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

#ifndef EI_FUSION_H
#define EI_FUSION_H

/* Include ----------------------------------------------------------------- */
#include "ei_config_types.h"
#if defined(__has_include)
  #if __has_include("ei_fusion_sensors_config.h")
    #include "ei_fusion_sensors_config.h"
  #else
    //use predefined values and display warning
    #warning "ei_fusion_sensors_config.h is missing! Using a preset values"
    #define NUM_MAX_FUSIONS       1
    #define FUSION_FREQUENCY      10.0f
    #define MULTI_FREQ_ENABLED    0
    // TODO: this is deprecated and will be removed in next releases
    #define NUM_MAX_FUSION_AXIS   20
    /** Format used for fusion */
    typedef float fusion_sample_format_t;
  #endif
#else
  // let's assume the file is there
  #include "ei_fusion_sensors_config.h"
#endif
#include "sensor-aq/sensor_aq.h"
#include <string>
#include <vector>

#define EI_MAX_FREQUENCIES 5

/** Format used in input list. Can either contain sensor names or axes names */
typedef enum
{
    SENSOR_FORMAT = 0,
    AXIS_FORMAT

} ei_fusion_list_format;

/**
 * Information about the fusion structure, name, number of axis, sampling frequencies, axis name, and reference to read sensor function
 */
typedef struct {
    // Name of sensor to show up in Studio
    const char *name;
    // Number of sensor axis to sample
    int num_axis;
    // List of sensor sampling frequencies
    float frequencies[EI_MAX_FREQUENCIES];
    // Sensor axes, note that I declare this not as a pointer to have a more fluent interface
    sensor_aq_sensor sensors[EI_MAX_SENSOR_AXES];
    // Reference to read sensor function that should return pointer to float array of raw sensor data
    fusion_sample_format_t *(*read_data)(int n_samples);
    // Axis used
    int axis_flag_used;
} ei_device_fusion_sensor_t;

typedef struct {
    std::string name;
    unsigned int max_sample_length;
    std::vector<float> frequencies;
} fused_sensors_t;

/* Function prototypes ----------------------------------------------------- */
bool ei_add_sensor_to_fusion_list(ei_device_fusion_sensor_t sensor);

/**
 * @brief Function combines all sensors AND prints results using ei_printf
 * 
 */
void ei_built_sensor_fusion_list(void);

/**
 * @brief Function combines all sensors AND returns const reference to list as a result
 * 
 * @return const std::vector<fused_sensors_t>& 
 */
const std::vector<fused_sensors_t> &ei_get_sensor_fusion_list(void);

bool ei_connect_fusion_list(const char *input_list, ei_fusion_list_format format);
void ei_fusion_read_axis_data(void);
bool ei_fusion_sample_start(sampler_callback callsampler, float sample_interval_ms);
bool ei_fusion_setup_data_sampling(void);
#if MULTI_FREQ_ENABLED == 1
bool ei_multi_fusion_sample_start(sampler_callback callsampler, float multi_sample_interval_ms);
void ei_fusion_multi_read_axis_data(uint8_t flag_read);
float ei_fusion_calc_multi_gcd(float* numbers, uint8_t how_many);
bool ei_is_fusion(void);
#endif


#endif /* EI_FUSION_H */
