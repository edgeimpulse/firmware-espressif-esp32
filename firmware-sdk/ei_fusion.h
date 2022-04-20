/* Edge Impulse firmware SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
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

#ifndef EI_FUSION_H
#define EI_FUSION_H

/* Include ----------------------------------------------------------------- */
#include "ei_config_types.h"
#include "ei_fusion_sensors_config.h"
#include "sensor_aq.h"
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

#endif /* EI_FUSION_H */
