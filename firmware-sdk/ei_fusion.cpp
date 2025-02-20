/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Include ----------------------------------------------------------------- */
#include "ei_fusion.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "ei_device_info_lib.h"
#include "ei_sampler.h"
#include <iomanip>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <cfloat>
#include <algorithm>

// offset for unknown header size (can be 0-3)
#define CBOR_HEADER_OFFSET 0x02
// number of CBOR bytes for sensor (ie {"name": "...", "units": "..."})
#define SENSORS_BYTE_OFFSET 14

using namespace std;

/* Private variables ------------------------------------------------------- */
static int payload_bytes; // counts bytes sensor fusion adds
static sampler_callback fusion_cb_sampler;

/*
** @brief list of fusable sensors
*/
vector<ei_device_fusion_sensor_t> fusable_sensor_list;
vector<fused_sensors_t> fused_sensors;
/*
** @brief list of sensors to fuse
*/
static vector<ei_device_fusion_sensor_t *> fusion_sensors;
int num_fusions, num_fusion_axis;
#if MULTI_FREQ_ENABLED == 1
#define MULTI_FREQ_MAX_FREQ_NOT_SET     (-1.0f)

#ifndef MULTI_FREQ_MAX_INC_FACTOR
#define MULTI_FREQ_MAX_INC_FACTOR       (10)
#endif

static float multi_sampling_freq[NUM_MAX_FUSIONS];
static float multi_freq_combination[NUM_MAX_FUSIONS][EI_MAX_FREQUENCIES];
static fusion_sample_format_t* old_data;    // store old samples for multi
#endif

/* Private function prototypes --------------------------------------------- */
static void print_fusion_list(int r, uint32_t ingest_memory_size);
static void print_all_combinations(
    int arr[],
    int data[],
    int start,
    int index,
    int r,
    uint32_t ingest_memory_size);
static int generate_bit_flags(int dec);
static bool add_sensor(int sensor_ix, char *name_buffer);
static bool add_axis(int sensor_ix, char *name_buffer);
static float highest_frequency(float *frequencies, size_t size);
#if MULTI_FREQ_ENABLED == 1
static float calc_gcd(float time1, float time2);
static void get_multi_freq_combinations(int row, int col, float* mat_period, float* actual_comb, int ix, vector<float>* freq_comb, vector<int>* mem_fact, float allowed_period);
static void clean_multi_freq_combinations(int n, int col, float* mat_period, float* actual_comb, int ix, float* actual_max);
static bool ei_fusion_calc_optimal_frequencies(uint8_t row, uint8_t col, float freq_objective);
#endif
/**
 * @brief Add sensor to fusion list
 * @return false if list is full
 */
bool ei_add_sensor_to_fusion_list(ei_device_fusion_sensor_t sensor)
{
    fusable_sensor_list.push_back(sensor);

    return true;
}

/**
 * @brief Check if requested input list is valid sensor fusion, create sensor buffer
 *
 * @param[in]  input_list      Sensor list to sample (ie. "Inertial + Environmental")
 *                             or Axes list to sample (ie. "accX + gyrY + magZ")
 * @param[in]  format          Format can be either SENSOR_FORMAT or AXIS_FORMAT
 *
 * @retval  false if invalid sensor_list
 */
bool ei_connect_fusion_list(const char *input_list, ei_fusion_list_format format)
{
    char *buff;
    bool is_fusion = false;

    num_fusions = 0;
    num_fusion_axis = 0;
    fusion_sensors.clear();

    for (unsigned int i = 0; i < fusable_sensor_list.size(); i++) {
        fusion_sensors.push_back(nullptr);
    } // clear fusion list

    // Copy const string in heap mem
    char *input_string = (char *)ei_malloc(strlen(input_list) + 1);
    if (input_string == NULL) {
        return false;
    }
    memset(input_string, 0, strlen(input_list) + 1);
    strncpy(input_string, input_list, strlen(input_list));

    buff = strtok(input_string, "+");

    while (buff != NULL) { // Run through buffer
        is_fusion = false;
        for (unsigned int i = 0; i < fusable_sensor_list.size();
             i++) { // check for axis name in list of fusable sensors

            if (format == SENSOR_FORMAT) {
                is_fusion = add_sensor(i, buff);
            }
            else {
                is_fusion = add_axis(i, buff);
            }

            if (is_fusion) {
                break;
            }
        }

        if (!is_fusion) { // no matching axis or sensor found
            break;
        }

        if (num_fusions >= NUM_MAX_FUSIONS) {
            break;
        }

        buff = strtok(NULL, "+");
    }

    ei_free(input_string);

    return is_fusion;
}

/**
 * @brief Get sensor data and extract needed sensors
 * Callback function writes data to mem
 */
void ei_fusion_read_axis_data(void)
{
    EiDeviceInfo* dev = EiDeviceInfo::get_device();
    fusion_sample_format_t *sensor_data;
    fusion_sample_format_t *data;
    uint32_t loc = 0;

    data = (fusion_sample_format_t *)ei_malloc(sizeof(fusion_sample_format_t) * num_fusion_axis);
    if (data == NULL) {
        return;
    }

    for (int i = 0; i < num_fusions; i++) {

        sensor_data = NULL;
        if (fusion_sensors[i]->read_data != NULL) {
            sensor_data = fusion_sensors[i]->read_data(
                fusion_sensors[i]->num_axis); // read sensor data from sensor file
        }

        if (sensor_data != NULL) {
            for (int j = 0; j < fusion_sensors[i]->num_axis; j++) {
                if (fusion_sensors[i]->axis_flag_used & (1 << j)) {
                    data[loc++] = *(sensor_data + j); // add sensor data to fusion data
                }
            }
        }
        else { // No data, zero fill
            for (int j = 0; j < fusion_sensors[i]->num_axis; j++) {
                if (fusion_sensors[i]->axis_flag_used & (1 << j)) {
                    data[loc++] = 0;
                }
            }
        }
    }

    if (fusion_cb_sampler(
            (const void *)&data[0],
            (sizeof(fusion_sample_format_t) * num_fusion_axis))) // send fusion data to sampler
        dev->stop_sample_thread(); // if last sample detach

    ei_free(data);
}

#if MULTI_FREQ_ENABLED == 1
/**
 * @brief Get sensor data and extract needed sensors
 * @param flag_read which callback should be called
 */
void ei_fusion_multi_read_axis_data(uint8_t flag_read)
{
   EiDeviceInfo* dev = EiDeviceInfo::get_device();
   fusion_sample_format_t *sensor_data;
   fusion_sample_format_t *data;
   uint32_t loc = 0;

   if (flag_read != 0) {
       data = (fusion_sample_format_t *)ei_malloc(sizeof(fusion_sample_format_t) * num_fusion_axis);
       if (data == NULL) {
           return;
       }

       for (int i = 0; i < num_fusions; i++) {

           sensor_data = NULL;
           if ((fusion_sensors[i]->read_data != NULL)
                   && ((flag_read & (1 << i)) == (1 << i)) ) {
               sensor_data = fusion_sensors[i]->read_data(
                   fusion_sensors[i]->num_axis); // read sensor data from sensor file
           }

           if (sensor_data != NULL) {
               for (int j = 0; j < fusion_sensors[i]->num_axis; j++, loc++) {
                   if (fusion_sensors[i]->axis_flag_used & (1 << j)) {
                       data[loc] = *(sensor_data + j); // add sensor data to fusion data
                       if (old_data != nullptr) {

                        old_data[loc] = data[loc];       // store in old structure
                        }
                   }
               }
           }
           else { // No data, zero fill
               for (int j = 0; j < fusion_sensors[i]->num_axis; j++, loc++) {
                   if (fusion_sensors[i]->axis_flag_used & (1 << j)) {
                       data[loc] = old_data[loc];       // not sampled, use last value
                   }
               }
           }
       }

       if (fusion_cb_sampler(
               (const void *)&data[0],
               (sizeof(fusion_sample_format_t) * num_fusion_axis))) {
           dev->stop_sample_thread(); // if last sample detach
           ei_free(old_data);
           // send fusion data to sampler
       }

       ei_free(data);
   }
   else {
       if (fusion_cb_sampler(nullptr, 0)) {
           dev->stop_sample_thread(); // if last sample detach
           ei_free(old_data);
       }
   }

}
#endif

/**
 * @brief      Wrapper for start_sample_thread
 *
 * @param[in]  callsampler          callback function from ei_sampler
 * @param[in]  sample_interval_ms   sample interval from ei_sampler
 *
 * @retval  false if initialisation failed
 */
bool ei_fusion_sample_start(sampler_callback callsampler, float sample_interval_ms)
{
    EiDeviceInfo* dev = EiDeviceInfo::get_device();
    fusion_cb_sampler = callsampler; // connect cb sampler (used in ei_fusion_read_data())
    bool started = false;

    if (fusion_cb_sampler != nullptr) {
#if MULTI_FREQ_ENABLED == 1
        if (num_fusions == 1) {
            started = dev->start_sample_thread(ei_fusion_read_axis_data, sample_interval_ms);
        }
#else
        started = dev->start_sample_thread(ei_fusion_read_axis_data, sample_interval_ms);
#endif
    }
    return started;
}

#if MULTI_FREQ_ENABLED == 1
/**
 *
 * @param callsampler
 * @param multi_sample_interval_ms
 * @return
 */
bool ei_multi_fusion_sample_start(sampler_callback callsampler, float multi_sample_interval_ms)
{
    EiDeviceInfo* dev = EiDeviceInfo::get_device();
    fusion_cb_sampler = callsampler; // connect cb sampler (used in ei_fusion_read_data())

    if ((fusion_cb_sampler == NULL) || (num_fusions < 2)) {   /* */
        return false;
    }
    else {
        if (ei_fusion_calc_optimal_frequencies(num_fusions, EI_MAX_FREQUENCIES, (1000.0f/multi_sample_interval_ms)) == false) {
            ei_printf("ERR: Unable to calculate the optimal frequency\n");
            return false;
        }
        // TODO calc of optimal frequencies.
        dev->start_multi_sample_thread(ei_fusion_multi_read_axis_data, multi_sampling_freq, num_fusions); // what about multi_sample_interval_ms ?
        return true;
    }
}
#endif

/**
 * @brief      Create payload for sampling list, pad, start sampling
 */
bool ei_fusion_setup_data_sampling(void)
{
    EiDeviceInfo* dev = EiDeviceInfo::get_device();
    EiDeviceMemory* mem = dev->get_memory();
    payload_bytes = 0;

    if (num_fusion_axis == 0) {
        return false;
    }

    // Calculate number of bytes available on flash for sampling, reserve 1 block for header + overhead
    uint32_t available_bytes = (mem->get_available_sample_blocks() - 1) * mem->block_size;
    // Check available sample size before sampling for the selected frequency
    uint32_t requested_bytes = ceil(
        (dev->get_sample_length_ms() / dev->get_sample_interval_ms()) *
        (sizeof(fusion_sample_format_t) * num_fusion_axis) * 2);
    if (requested_bytes > available_bytes) {
        ei_printf(
            "ERR: Sample length is too long. Maximum allowed is %ims at ",
            (int)floor(
                available_bytes /
                (((sizeof(fusion_sample_format_t) * num_fusion_axis) * 2) /
                 dev->get_sample_interval_ms())));
        ei_printf_float(1.f / dev->get_sample_interval_ms());
        ei_printf("Hz.\r\n");
        return false;
    }

    int index = 0;
    // create header payload from individual sensors
    sensor_aq_payload_info payload = { dev->get_device_id().c_str(),
                                       dev->get_device_type().c_str(),
                                       dev->get_sample_interval_ms(),
                                       {} };
    for (int i = 0; i < num_fusions; i++) {
        for (int j = 0; j < fusion_sensors[i]->num_axis; j++) {
            payload.sensors[index].name = fusion_sensors[i]->sensors[j].name;
            payload.sensors[index++].units = fusion_sensors[i]->sensors[j].units;

            payload_bytes += strlen(fusion_sensors[i]->sensors[j].name) +
                strlen(fusion_sensors[i]->sensors[j].units) + SENSORS_BYTE_OFFSET;
        }
    }

    // use heap for unit name, add 4 bytes for padding
    char *unit_name = (char *)ei_malloc(payload_bytes + sizeof(uint32_t));
    if (unit_name == NULL) {
        return false;
    }

    // counts bytes payload adds, pads if not 32 bits
    int32_t fill = (CBOR_HEADER_OFFSET + payload_bytes) & 0x03;
    if (fill != 0x00) {
        strncpy(unit_name, payload.sensors[num_fusion_axis - 1].units, payload_bytes);
        for (int32_t i = fill; i < 4; i++) {
            strcat(unit_name, " ");
        }
        payload.sensors[num_fusion_axis - 1].units = unit_name;
    }

    bool ret = false;

#if MULTI_FREQ_ENABLED == 1
    old_data = (fusion_sample_format_t *)ei_malloc(sizeof(fusion_sample_format_t) * num_fusion_axis);
    memset(old_data, 0, sizeof(fusion_sample_format_t) * num_fusion_axis);

    if (num_fusions == 1) {
        ret = ei_sampler_start_sampling(
                &payload,
                &ei_fusion_sample_start,
                (sizeof(fusion_sample_format_t) * num_fusion_axis));
    }
    else {
        ret = ei_sampler_start_sampling(
                &payload,
                &ei_multi_fusion_sample_start,
                (sizeof(fusion_sample_format_t) * num_fusion_axis));
    }

    if (ret == false) {
        ei_free(old_data);  //
    }

#else
    ret = ei_sampler_start_sampling(
            &payload,
            &ei_fusion_sample_start,
            (sizeof(fusion_sample_format_t) * num_fusion_axis));
#endif

    ei_free(unit_name);
    return ret;
}

/**
 * @brief   Builds list of possible sensor combinations from fusable_sensor_list
 */
void ei_built_sensor_fusion_list(void)
{
    const vector<fused_sensors_t> sens_list = ei_get_sensor_fusion_list();

    /*
     * Print in the form:
     * Name: Sensor1 + Sensor2, Max sample length: 12345s, Frequencies: [123.45Hz]
     */
    for (auto it = sens_list.begin(); it != sens_list.end(); it++) {
        ei_printf("Name: %s, ", it->name.c_str());
        ei_printf("Max sample length: %us, ", it->max_sample_length);
        ei_printf("Frequencies: [");
        for (auto freq = it->frequencies.begin(); freq != it->frequencies.end();) {
            ei_printf_float(*freq);
            ei_printf("Hz");
            freq++;
            if (freq != it->frequencies.end()) {
                ei_printf(", ");
            }
        }
        ei_printf("]\n");
    }
}

/**
 *
 * @return
 */
const vector<fused_sensors_t> &ei_get_sensor_fusion_list(void)
{
    EiDeviceInfo* dev = EiDeviceInfo::get_device();
    EiDeviceMemory* mem = dev->get_memory();
    // Calculate number of bytes available on flash for sampling, reserve 1 block for header + overhead
    uint32_t available_bytes = (mem->get_available_sample_blocks() - 1) * mem->block_size;

    fused_sensors.clear();

    /* For number of different combinations */
    for (int i = 0; i < NUM_MAX_FUSIONS; i++) {
        print_fusion_list(i + 1, available_bytes);
    }

    return fused_sensors;
}

/**
 * @brief The main function that prints all combinations of size r
 * in arr[] of size n. This function mainly uses print_all_combinations()
 * @param r
 * @param ingest_memory_sizes
 */
static void print_fusion_list(int r, uint32_t ingest_memory_size)
{
    /* A temporary array to store all combination one by one */
    auto data = new int[r];
    auto arr = new int[fusable_sensor_list.size()];

    for (unsigned int i = 0; i < fusable_sensor_list.size(); i++) {
        arr[i] = i;
    }

    /* Print all combination using temporary array 'data[]' */
    print_all_combinations(arr, data, 0, 0, r, ingest_memory_size);
    delete[] data;
    delete[] arr;
}

/**
 * @brief Run recursive to find all combinations
 * print sensor string of requested number of combinations is found
 * @param arr Input array
 * @param data Temperory data array, holds indexes
 * @param start Start idx of arr[]
 * @param index Current index in data[]
 * @param r Size of combinations
 * @param ingest_memory_size Available mem for sample data
 */
static void print_all_combinations(
    int arr[],
    int data[],
    int start,
    int index,
    int r,
    uint32_t ingest_memory_size)
{
    string buf;
    fused_sensors_t sens;
    /* Print sensor string if requested combinations found */
    if (index == r) {
        int local_num_fusion_axis = 0;
        for (int j = 0; j < r; j++) {

            string full_name = fusable_sensor_list[data[j]].name;

            int axis_name_start = full_name.find("(");

            /* Print axes info if not fusioned with other sensors */
            if(axis_name_start && index != 1) {
                buf += full_name.substr(0, axis_name_start - 1);
            }
            else {
                buf += full_name;
            }

            local_num_fusion_axis += fusable_sensor_list[data[j]].num_axis;

            if (j + 1 < r) {
                buf += " + ";
            }
        }
        sens.name = buf;

        if (index == 1) {
            float frequency =
                highest_frequency(&fusable_sensor_list[data[0]].frequencies[0], EI_MAX_FREQUENCIES);
            sens.max_sample_length =
                (int)(ingest_memory_size / (frequency * (sizeof(fusion_sample_format_t) * local_num_fusion_axis) * 2));
            for (int j = 0; j < EI_MAX_FREQUENCIES; j++) {
                if (fusable_sensor_list[data[0]].frequencies[j] != 0.0f) {
                    sens.frequencies.push_back(fusable_sensor_list[data[0]].frequencies[j]);
                }
            }
        }
        else {
#if (MULTI_FREQ_ENABLED == 1)
            float mat_period[NUM_MAX_FUSIONS][EI_MAX_FREQUENCIES] = {{0.0}};
            float mat_freq[NUM_MAX_FUSIONS][EI_MAX_FREQUENCIES] = {{0.0}};
            float combination[NUM_MAX_FUSIONS] = {0.0};
            float max_freq = MULTI_FREQ_MAX_FREQ_NOT_SET;
            int starting_ix = 0;
            const int mem_inc_threshold = MULTI_FREQ_MAX_INC_FACTOR;
            int how_many_under_threshold = 0;

            vector<float> found_freq_combinations;
            vector<int> mem_increase_factor;

            for (int j = 0; j < r; j++) {                         // per sensors
                for (int z = 0; z < EI_MAX_FREQUENCIES; z++) {     // per freq
                    if (fusable_sensor_list[data[j]].frequencies[z] != 0.0) {
                        mat_period[j][z] = 1.f/fusable_sensor_list[data[j]].frequencies[z];
                        mat_freq[j][z] = fusable_sensor_list[data[j]].frequencies[z];
                    }
                }
            }

            clean_multi_freq_combinations(r, EI_MAX_FREQUENCIES, (float*)mat_freq, combination, starting_ix, &max_freq);
            get_multi_freq_combinations(r, EI_MAX_FREQUENCIES, (float*)mat_period, combination, starting_ix, &found_freq_combinations, &mem_increase_factor, max_freq);

            for (size_t j = 0; j < mem_increase_factor.size(); j++) {
                if (mem_increase_factor.at(j) < mem_inc_threshold) {
                    how_many_under_threshold++;
                }
            }

            for (size_t j = 0; j < found_freq_combinations.size(); j++) {
                if ((how_many_under_threshold > 0) && (mem_increase_factor.at(j) < mem_inc_threshold)) {
                    sens.frequencies.push_back(found_freq_combinations.at(j));
                }
                else if (how_many_under_threshold == 0) {
                    sens.frequencies.push_back(found_freq_combinations.at(j));
                }
            }

            if (sens.frequencies.size() > 0) {
                float frequency = highest_frequency(&sens.frequencies[0], sens.frequencies.size());
                sens.max_sample_length =
                    (int)(ingest_memory_size / (frequency * (sizeof(fusion_sample_format_t) * local_num_fusion_axis) * 2));
            }
            else {
                sens.max_sample_length  = 0;
            }
#else
            // fusion, use set freq
            sens.max_sample_length =
                (int)(ingest_memory_size / (FUSION_FREQUENCY * (sizeof(fusion_sample_format_t) * local_num_fusion_axis) * 2));
            sens.frequencies.push_back(FUSION_FREQUENCY);
#endif
        }
        if (sens.max_sample_length > 0) {
            fused_sensors.push_back(sens);
        }
        return;
    }

    /* Replace index with all possible combinations */
    for (size_t i = start;
         i <= fusable_sensor_list.size() - 1 && (int)(fusable_sensor_list.size() - i) >= r - index;
         i++) {
        data[index] = arr[i];
        print_all_combinations(arr, data, i + 1, index + 1, r, ingest_memory_size);
    }
}

/**
 * @brief      Set n flags for decimal input parameter
 *
 * @param      dec decimal input
 * @return     int flags
 */
static int generate_bit_flags(int dec)
{
    return ((1 << dec) - 1);
}

/**
 * @brief      Compare name_buffer with name from fusable_sensor_list array
 *             If there's a match, add to fusion_sensor[] array
 * @param      sensor_ix index in fusable_sensor_list
 * @param      name_buffer sensor to search
 * @return     true if added to fusion_sensor[] array
 */
static bool add_sensor(int sensor_ix, char *name_buffer)
{
    bool added_loc;
    bool is_fusion = false;    

    char* buf = (char *)ei_malloc(strlen(fusable_sensor_list[sensor_ix].name) + 1);

    if (buf == NULL) {
        return false;
    }

    memset(buf, 0, strlen(fusable_sensor_list[sensor_ix].name) + 1);
    strncpy(buf, fusable_sensor_list[sensor_ix].name, strlen(fusable_sensor_list[sensor_ix].name));

    if (strstr(fusable_sensor_list[sensor_ix].name, "(")
        && strstr(name_buffer, "(") == NULL ) {  // full name has ( ) => BUT not the fusion string received, which probably means is using abbreviation
        buf[strcspn(fusable_sensor_list[sensor_ix].name , " (")] = '\0';
    }

    if (strstr(name_buffer, buf)) { // is a matching sensor
        added_loc = false;
        for (int j = 0; j < num_fusions; j++) {
            if (strstr(
                    name_buffer,
                    fusion_sensors[j]->name)) { // has already been added to sampling list
                added_loc = true;
                break;
            }
        }
        if (!added_loc) {
            fusion_sensors[num_fusions] =
                (ei_device_fusion_sensor_t *)&fusable_sensor_list[sensor_ix];
            num_fusion_axis += fusable_sensor_list[sensor_ix].num_axis;

            /* Add all axes for ingestions */
            fusion_sensors[num_fusions]->axis_flag_used =
                generate_bit_flags(fusable_sensor_list[sensor_ix].num_axis);
            num_fusions++;
        }
        is_fusion = true;
    }

    ei_free(buf);

    return is_fusion;
}

/**
 * @brief      Run through all axes names from sensor and compare with name_buffer
 *             It found add sensor to fusable_sensor_list[] array and set a axis flag
 * @param      sensor_ix index in fusable sensor_list[]
 * @param      name_buffer axis name
 * @return     true if added to fusion_sensor[] array
 */
static bool add_axis(int sensor_ix, char *name_buffer)
{
    bool added_loc;
    bool is_fusion = false;

    for (int y = 0; y < fusable_sensor_list[sensor_ix].num_axis; y++) {

        if (strstr(
                name_buffer,
                fusable_sensor_list[sensor_ix].sensors[y].name)) { // is a matching axis
            added_loc = false;
            for (int j = 0; j < num_fusions; j++) {
                if (strstr(
                        name_buffer,
                        fusion_sensors[j]->name)) { // has already been added to sampling list
                    added_loc = true;
                    break;
                }
            }
            if (!added_loc) { // Add sensor or axes
                for (int x = 0; x < num_fusions; x++) { // Find corresponding sensor
                    if (strstr(
                            fusion_sensors[x]->name,
                            fusable_sensor_list[sensor_ix]
                                .name)) { // has already been added to sampling list
                        fusion_sensors[x]->axis_flag_used |= (1 << y);
                        added_loc = true;
                        break;
                    }
                }

                if (!added_loc) { // New sensor, add to list
                    fusion_sensors[num_fusions] =
                        (ei_device_fusion_sensor_t *)&fusable_sensor_list[sensor_ix];
                    fusion_sensors[num_fusions]->axis_flag_used = (1 << y);
                    num_fusions++;
                }
                num_fusion_axis++;
            }
            is_fusion = true;
        }
    }

    return is_fusion;
}

/**
 * @brief Run trough freq array and return highest
 *
 * @param frequencies
 * @param size
 * @return float
 */
static float highest_frequency(float *frequencies, size_t size)
{
    float highest = 0.f;

    for (int i = 0; i < (int)size; i++) {
        if (highest < frequencies[i]) {
            highest = frequencies[i];
        }
    }
    return highest;
}
#if MULTI_FREQ_ENABLED == 1
/**
 *
 * @param time1
 * @param time2
 * @return
 */
static float calc_gcd(float fnum1, float fnum2)
{
    float temp;

    while(1)
    {
        if (fnum1 < fnum2)
        {
            temp = fnum2;
            fnum2 = fnum1;
            fnum1 = temp;
        }

        if (fabs(fnum2) < 0.001)
        {
            break;
        }

        temp = fnum1;
        fnum1 = fnum2;
        fnum2 = temp - floor(temp / fnum2) * fnum2;
    }

    return fnum1;
}

/**
 * @brief Get the multi freq combinations object
 *
 * @param n
 * @param mat_period
 * @param actual_comb
 * @param ix
 * @param psens
 */
static void get_multi_freq_combinations(int n, int col, float* mat_period, float* actual_comb, int ix, vector<float>* freq_comb, vector<int>* mem_fact, float allowed_period)
{
    int i;

    if (ix == n) {
        for (i = 0; i < n; i++) {
            if (actual_comb[i] == 0.0) {
                return; // make no sense to calc gcd for this combination
            }
        }
        float freq = ei_fusion_calc_multi_gcd(actual_comb, (uint8_t)n);
        int local_mem_fac = 0;

        freq = roundf(10.f/(freq))/10.f;

        if ((freq > allowed_period) && (allowed_period != MULTI_FREQ_MAX_FREQ_NOT_SET)) {
            return;
        }

        if (find(begin(*freq_comb), end(*freq_comb), freq) != end(*freq_comb)) {  /* already present, let's check if better mem increase */
            size_t j;
            for (j = 0; j < freq_comb->size(); j++) {
                if (freq_comb->at(j) == freq) {
                    break;
                }
            }
            // let's check if we can found a better combination with the same result.
            for (i = 0; i < n; i++) {
                int temp = (int)(freq*actual_comb[i]);

                if (temp == 1) {
                    local_mem_fac = 1;  // if equal of one of the starting freq, we want to keep it!
                    break;
                }
                else {
                    local_mem_fac += (int)(freq*actual_comb[i]);
                }

            }
            if (local_mem_fac < mem_fact->at(j)) {
                mem_fact->at(j) = local_mem_fac;
            }

            return; // element already present, no need to add it.
        }

        for (i = 0; i < n; i++) {
            int temp = (int)(freq*actual_comb[i]);

            if (temp == 1) {
                local_mem_fac = 1;  // if equal of one of the starting freq, we want to keep it!
                break;
            }
            else {
                local_mem_fac += (int)(freq*actual_comb[i]);
            }
        }

        mem_fact->push_back(local_mem_fac);
        freq_comb->push_back(freq);

    } else {
        for (i = 0; i < col; i++) {
            actual_comb[ix] = mat_period[ix*col + i];
            get_multi_freq_combinations(n, col, mat_period, actual_comb, ix + 1, freq_comb, mem_fact, allowed_period);
        }
    }
}

/**
 * @brief
 *
 * @param n
 * @param col
 * @param ix
 * @param actual_comb
 * @param actual_max
 */
static void clean_multi_freq_combinations(int n, int col,float* mat_period, float* actual_comb, int ix, float* actual_max)
{
    if (ix == n) {

        bool natural_mult = true;

        for (int i = 0; i < (n-1); i++) {
            for (int j = i + 1; j < n; j++) {
                if (actual_comb[j] < actual_comb[i]) {

                    if (fmodf(actual_comb[i], actual_comb[j]) != 0.0f) {
                        return;
                    }
                }
                else {
                    if (fmodf(actual_comb[j], actual_comb[i]) != 0.0f) {
                        return;
                    }
                }
            }
        }

        if (natural_mult == true) {
            float temp_max = 0.0f;

            for (int i = 0; i < (n-1); i++) {
                temp_max = actual_comb[i];

                for (int j = i + 1; j < n; j++) {
                    if (temp_max < actual_comb[j]) {
                        temp_max = actual_comb[j];
                    }
                }
            }

            if (temp_max > *actual_max) {
                *actual_max = temp_max;
            }
        }

        return;
    } else {
        for (int i = 0; i < col; i++) {
            actual_comb[ix] = mat_period[ix*col + i];

            clean_multi_freq_combinations(n, col, mat_period, actual_comb, ix + 1, actual_max);
        }
    }

}

/**
 * @brief
 *
 * @param mat_period
 * @param actual_comb
 * @return true
 * @return false
 */
static bool ei_fusion_calc_optimal_frequencies(uint8_t row, uint8_t col, float freq_objective)
{
    uint8_t found = 0;
    int32_t optimal_values = 0;

    if (freq_objective == 0.0) {
        return false;
    }

    memset(multi_sampling_freq, 0, sizeof(multi_sampling_freq));
    memset(multi_freq_combination, 0, sizeof(multi_freq_combination));

    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            //
            multi_freq_combination[i][j] = fusion_sensors[i]->frequencies[j];
        }
    }

    for (int i = 0; i < row; i++) {  // for each sensors
        optimal_values = INT32_MAX;

        for (int j = 0; j < col; j++) {  // for each freq
            if ((freq_objective >= multi_freq_combination[i][j])
                && (multi_freq_combination[i][j] != 0.0f)) {
                if (fmodf(freq_objective, multi_freq_combination[i][j]) == 0.0f) {
                    int32_t temp = freq_objective/multi_freq_combination[i][j];
                    if (temp < optimal_values) {
                        optimal_values = temp;
                        multi_sampling_freq[i] = multi_freq_combination[i][j];
                    }
                }
            }
        }

        if (optimal_values != INT32_MAX) {
            found++;
        }
    }

    if (found == row) {
        return true;
    }
    else{
        return false;
    }
}

/**
 * @brief
 *
 * @param numbers
 * @param how_may
 * @return float the gcd of the array
 */
float ei_fusion_calc_multi_gcd(float* numbers, uint8_t how_many)
{
    uint8_t i;
    float inttime1;


    if (how_many < 2) {
        return 0.f;
    }

    inttime1 = numbers[0];

    for (i = 1; i < how_many; i++) {
        inttime1 = calc_gcd(inttime1, (numbers[i]));
    }

    return (inttime1);
}

bool ei_is_fusion(void)
{
    return (num_fusions > 1);
}

#endif
