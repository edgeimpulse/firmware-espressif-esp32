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

/* Include ----------------------------------------------------------------- */
#include <stdint.h>
#include <stdlib.h>

#include "ei_inertial_sensor.h"

#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

#include "LIS3DHTR.h"

/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2    9.80665f

static float imu_data[INERTIAL_AXIS_SAMPLED];

LIS3DHTR lis;

bool ei_inertial_init(void) {

    lis.begin(LIS3DHTR_DEFAULT_ADDRESS);

    if(lis.isConnection() == false) {
        ei_printf("ERR: failed to connect to inertial sensor!\n");
        return false;
    }

    ei_sleep(100);
    lis.setFullScaleRange(LIS3DHTR_RANGE_2G);
    lis.setOutputDataRate(LIS3DHTR_DATARATE_100HZ);

    if(ei_add_sensor_to_fusion_list(inertial_sensor) == false) {
        ei_printf("ERR: failed to register Inertial sensor!\n");
        return false;
    }

    return true;
}

float *ei_fusion_inertial_read_data(int n_samples)
{
    
    lis.getAcceleration(&imu_data[0], &imu_data[1], &imu_data[2]);
    imu_data[0] *= CONVERT_G_TO_MS2;
    imu_data[1] *= CONVERT_G_TO_MS2;
    imu_data[2] *= CONVERT_G_TO_MS2;
    
    return imu_data;
}