/*
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _EDGE_IMPULSE_INFERENCING_ANOMALY_H_
#define _EDGE_IMPULSE_INFERENCING_ANOMALY_H_

#if (EI_CLASSIFIER_HAS_ANOMALY == 1)

#include <cmath>
#include <stdlib.h>

#include "edge-impulse-sdk/classifier/ei_classifier_types.h"
#include "edge-impulse-sdk/classifier/ei_aligned_malloc.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

EI_IMPULSE_ERROR inference_anomaly_invoke(const ei_impulse_t *impulse,
                                          ei::matrix_t *fmatrix,
                                          ei_impulse_result_t *result,
                                          bool debug = false)
{

    uint64_t anomaly_start_ms = ei_read_timer_ms();

    float input[EI_CLASSIFIER_ANOM_AXIS_SIZE];
    for (size_t ix = 0; ix < EI_CLASSIFIER_ANOM_AXIS_SIZE; ix++) {
        input[ix] = fmatrix->buffer[EI_CLASSIFIER_ANOM_AXIS[ix]];
    }
    standard_scaler(input, ei_classifier_anom_scale, ei_classifier_anom_mean, EI_CLASSIFIER_ANOM_AXIS_SIZE);
    float anomaly = get_min_distance_to_cluster(
        input, EI_CLASSIFIER_ANOM_AXIS_SIZE, ei_classifier_anom_clusters, EI_CLASSIFIER_ANOM_CLUSTER_COUNT);

    uint64_t anomaly_end_ms = ei_read_timer_ms();

    if (debug) {
        ei_printf("Anomaly score (time: %d ms.): ", static_cast<int>(anomaly_end_ms - anomaly_start_ms));
        ei_printf_float(anomaly);
        ei_printf("\n");
    }

    result->timing.anomaly = anomaly_end_ms - anomaly_start_ms;

    result->anomaly = anomaly;

    return EI_IMPULSE_OK;
}

#endif //#if (EI_CLASSIFIER_HAS_ANOMALY == 1)
#endif // _EDGE_IMPULSE_INFERENCING_ANOMALY_H_
