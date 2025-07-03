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

#ifndef EI_OBJECT_COUNTING_H
#define EI_OBJECT_COUNTING_H

/* Includes ---------------------------------------------------------------- */
#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/porting/ei_logging.h"

extern ei_impulse_handle_t & ei_default_impulse;

#if EI_CLASSIFIER_OBJECT_COUNTING_ENABLED == 1

class CrossingCounter {
public:
    CrossingCounter(std::vector<std::tuple<int, int, int, int>> segments) : segments(segments) {
        counts.resize(segments.size(), 0);
    }

    void update(std::tuple<int, int, int, int> other_segment) {
        for (size_t segment_idx = 0; segment_idx < segments.size(); segment_idx++) {
            if (_line_intersects(segments[segment_idx], other_segment)) {
                counts[segment_idx] += 1;
            }
        }
    }

    std::vector<uint32_t> counts;
    std::vector<std::tuple<int, int, int, int>> segments;
private:
    bool ccw(int A[2], int B[2], int C[2]) {
        return (C[1] - A[1]) * (B[0] - A[0]) > (B[1] - A[1]) * (C[0] - A[0]);
    }

    bool _line_intersects(std::tuple<int, int, int, int> L1, std::tuple<int, int, int, int> L2) {
        int A[2] = { std::get<0>(L1), std::get<1>(L1) };
        int B[2] = { std::get<2>(L1), std::get<3>(L1) };
        int C[2] = { std::get<0>(L2), std::get<1>(L2) };
        int D[2] = { std::get<2>(L2), std::get<3>(L2) };
// verbose debug
#if EI_LOG_LEVEL == 5
        ei_printf("A[0] = %d A[1] = %d\n", A[0], A[1]);
        ei_printf("B[0] = %d B[1] = %d\n", B[0], B[1]);
        ei_printf("C[0] = %d C[1] = %d\n", C[0], C[1]);
        ei_printf("D[0] = %d D[1] = %d\n", D[0], D[1]);
#endif
        return ccw(A, C, D) != ccw(B, C, D) && ccw(A, B, C) != ccw(A, B, D);
    }
};

EI_IMPULSE_ERROR init_object_counting(ei_impulse_handle_t *handle, void **state, void *config)
{
    // const ei_impulse_t *impulse = handle->impulse;
    const ei_object_counting_config_t *object_counting_config = (ei_object_counting_config_t*)config;

    // Allocate the object counter
    CrossingCounter *object_counter = new CrossingCounter(object_counting_config->segments);

    if (!object_counter) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }

    // Store the object counter in the handle
    *state = (void *)object_counter;

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR deinit_object_counting(void *state, void *config)
{
    CrossingCounter *object_counter = (CrossingCounter*)state;

    if (object_counter) {
        delete object_counter;
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR process_object_counting(ei_impulse_handle_t *handle,
                                         ei_impulse_result_t *result,
                                         void *config,
                                         void *state)
{
    const ei_impulse_t *impulse = handle->impulse;
    CrossingCounter *object_counter = (CrossingCounter*)state;

    if (impulse->sensor == EI_CLASSIFIER_SENSOR_CAMERA) {
        if((void *)object_counter != NULL) {
            for (size_t i = 0; i < result->postprocessed_output.object_tracking_output.open_traces_count; i++) {
                ei_object_tracking_trace_t trace = result->postprocessed_output.object_tracking_output.open_traces[i];
                object_counter->update(trace.last_centroid_segment);
            }

            result->postprocessed_output.object_counting_output.counts = object_counter->counts.data();
            result->postprocessed_output.object_counting_output.counter_num = object_counter->counts.size();
        }
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR display_object_counting(ei_impulse_result_t *result,
                                         void *config)
{
    // print the counts
    ei_printf("Counts:\r\n");
        for (uint32_t i = 0; i < result->postprocessed_output.object_counting_output.counter_num; i++) {
            ei_printf("  Counter %d: %d\r\n", i, result->postprocessed_output.object_counting_output.counts[i]);
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR set_post_process_params(ei_impulse_handle_t* handle, ei_object_counting_config_t* params) {
    int16_t block_number = get_block_number(handle, (void*)init_object_counting);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    CrossingCounter *object_counter = (CrossingCounter*)handle->post_processing_state[block_number];

    object_counter->segments = params->segments;
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_impulse_handle_t* handle, ei_object_counting_config_t* params) {
    int16_t block_number = get_block_number(handle, (void*)init_object_counting);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    CrossingCounter *object_counter = (CrossingCounter*)handle->post_processing_state[block_number];

    params->segments = object_counter->segments;
    return EI_IMPULSE_OK;
}

// versions that operate on the default impulse
EI_IMPULSE_ERROR set_post_process_params(ei_object_counting_config_t *params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        set_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_object_counting_config_t *params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        get_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

#endif // EI_CLASSIFIER_OBJECT_COUNTING_ENABLED
#endif // EI_OBJECT_COUNTING_H