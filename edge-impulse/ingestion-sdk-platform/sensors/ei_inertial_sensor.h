/* Edge Impulse ingestion SDK
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

#ifndef EI_INERTIAL_SENSOR_H
#define EI_INERTIAL_SENSOR_H

/* Include ----------------------------------------------------------------- */
#include "firmware-sdk/ei_fusion.h"

/** Number of axis used and sample data format */
#define INERTIAL_AXIS_SAMPLED       3

/* Function prototypes ----------------------------------------------------- */
bool ei_inertial_init(void);
float *ei_fusion_inertial_read_data(int n_samples);

static const ei_device_fusion_sensor_t inertial_sensor = {
    // name of sensor module to be displayed in fusion list
    "Inertial",
    // number of sensor module axis
    INERTIAL_AXIS_SAMPLED,
    // sampling frequencies
    { 20.0f, 62.5f, 100.0f },
    // axis name and units payload (must be same order as read in)
    { {"accX", "m/s2"}, {"accY", "m/s2"}, {"accZ", "m/s2"} }, 
    // reference to read data function
    &ei_fusion_inertial_read_data,
    0
};

#endif /* EI_INERTIAL_SENSOR_H */
