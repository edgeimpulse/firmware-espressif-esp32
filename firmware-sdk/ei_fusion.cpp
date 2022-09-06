/* Edge Impulse firmware SDK
 * Copyright (c) 2020 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Include ----------------------------------------------------------------- */
#include "ei_fusion.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "ei_device_info_lib.h"
#include "ei_sampler.h"
#include <iomanip>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

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

/* Private function prototypes --------------------------------------------- */
static void create_fusion_list(
    int min_length,
    int i,
    int curr_length,
    bool check[],
    int max_length,
    uint32_t available_bytes);
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
    bool is_fusion;

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

/**
 * @brief      Create list of all sensor combinations
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

    if (fusion_cb_sampler == NULL) {
        return false;
    }
    else {
        dev->start_sample_thread(ei_fusion_read_axis_data, sample_interval_ms);
        return true;
    }
}

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
            "ERR: Sample length is too long. Maximum allowed is %ims at %.1fHz.\r\n",
            (int)floor(
                available_bytes /
                (((sizeof(fusion_sample_format_t) * num_fusion_axis) * 2) /
                 dev->get_sample_interval_ms())),
            (1.f / dev->get_sample_interval_ms()));
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

    bool ret = ei_sampler_start_sampling(
        &payload,
        &ei_fusion_sample_start,
        (sizeof(fusion_sample_format_t) * num_fusion_axis));

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
    int data[r];
    int arr[fusable_sensor_list.size()];

    for (unsigned int i = 0; i < fusable_sensor_list.size(); i++) {
        arr[i] = i;
    }

    /* Print all combination using temporary array 'data[]' */
    print_all_combinations(arr, data, 0, 0, r, ingest_memory_size);
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
    stringstream buf;
    fused_sensors_t sens;
    /* Print sensor string if requested combinations found */
    if (index == r) {
        int num_fusion_axis = 0;
        for (int j = 0; j < r; j++) {
            buf << fusable_sensor_list[data[j]].name;
            num_fusion_axis += fusable_sensor_list[data[j]].num_axis;

            if (j + 1 < r) {
                buf << " + ";
            }
        }
        sens.name = buf.str();

        if (index == 1) {
            float frequency =
                highest_frequency(&fusable_sensor_list[data[0]].frequencies[0], EI_MAX_FREQUENCIES);
            sens.max_sample_length =
                (int)(ingest_memory_size / (frequency * (sizeof(fusion_sample_format_t) * num_fusion_axis) * 2));
            for (int j = 0; j < EI_MAX_FREQUENCIES; j++) {
                if (fusable_sensor_list[data[0]].frequencies[j] != 0.0f) {
                    sens.frequencies.push_back(fusable_sensor_list[data[0]].frequencies[j]);
                }
            }
        }
        else { // fusion, use set freq
            sens.max_sample_length =
                (int)(ingest_memory_size / (FUSION_FREQUENCY * (sizeof(fusion_sample_format_t) * num_fusion_axis) * 2));
            sens.frequencies.push_back(FUSION_FREQUENCY);
        }
        fused_sensors.push_back(sens);
        return;
    }

    /* Replace index with all possible combinations */
    for (int i = start;
         i <= fusable_sensor_list.size() - 1 && fusable_sensor_list.size() - i >= r - index;
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

    if (strstr(name_buffer, fusable_sensor_list[sensor_ix].name)) { // is a matching sensor
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
