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

#ifndef _EDGE_IMPULSE_ANOMALY_H_
#define _EDGE_IMPULSE_ANOMALY_H_

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "model-parameters/anomaly_types.h"

#ifdef __cplusplus
namespace {
#endif // __cplusplus

/**
 * Standard scaler, scales all values in the input vector
 * Note that this *modifies* the array in place!
 * @param input Array of input values
 * @param scale Array of scale values (obtain from StandardScaler in Python)
 * @param mean Array of mean values (obtain from StandardScaler in Python)
 * @param input_size Size of input, scale and mean arrays
 */
void standard_scaler(float *input, const float *scale, const float *mean, size_t input_size) {
    for (size_t ix = 0; ix < input_size; ix++) {
        input[ix] = (input[ix] - mean[ix]) / scale[ix];
    }
}

/**
 * Calculate the distance between input vector and the cluster
 * @param input Array of input values (already scaled by standard_scaler)
 * @param input_size Size of the input array
 * @param cluster A cluster (number of centroids should match input_size)
 */
float calculate_cluster_distance(float *input, size_t input_size, const ei_classifier_anom_cluster_t *cluster) {
    // todo: check input_size and centroid size?

    float dist = 0.0f;
    for (size_t ix = 0; ix < input_size; ix++) {
        dist += pow(input[ix] - cluster->centroid[ix], 2);
    }
    return sqrt(dist) - cluster->max_error;
}

/**
 * Get minimum distance to a cluster
 * @param input Array of input values (already scaled by standard_scaler)
 * @param input_size Size of the input array
 * @param clusters Array of clusters
 * @param cluster_size Size of cluster array
 */
float get_min_distance_to_cluster(float *input, size_t input_size, const ei_classifier_anom_cluster_t *clusters, size_t cluster_size) {
    float min = 1000.0f;
    for (size_t ix = 0; ix < cluster_size; ix++) {
        float dist = calculate_cluster_distance(input, input_size, &clusters[ix]);
        if (dist < min) {
            min = dist;
        }
    }
    return min;
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _EDGE_IMPULSE_ANOMALY_H_
