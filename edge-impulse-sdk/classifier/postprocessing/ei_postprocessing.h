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

#ifndef EI_POSTPROCESSING_H
#define EI_POSTPROCESSING_H

#include "edge-impulse-sdk/classifier/ei_model_types.h"

#if EI_CLASSIFIER_CALIBRATION_ENABLED
#include "edge-impulse-sdk/classifier/postprocessing/ei_performance_calibration.h"
#endif

#if EI_CLASSIFIER_OBJECT_TRACKING_ENABLED
#include "edge-impulse-sdk/classifier/postprocessing/ei_object_tracking.h"
#endif

#if EI_CLASSIFIER_OBJECT_COUNTING_ENABLED
#include "edge-impulse-sdk/classifier/postprocessing/ei_object_tracking.h"
#include "edge-impulse-sdk/classifier/postprocessing/ei_object_counting.h"
#endif

extern "C" EI_IMPULSE_ERROR init_postprocessing(ei_impulse_handle_t *handle) {
    if (!handle) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }
    auto impulse = handle->impulse;
    handle->post_processing_state = (void **)ei_malloc(impulse->postprocessing_blocks_size * sizeof(void *));

    for (size_t i = 0; i < impulse->postprocessing_blocks_size; i++) {

        if (impulse->postprocessing_blocks[i].init_fn == nullptr) {
            continue;
        }

        EI_IMPULSE_ERROR res = impulse->postprocessing_blocks[i].init_fn(handle, &handle->post_processing_state[i], impulse->postprocessing_blocks[i].config);
        if (res != EI_IMPULSE_OK) {
            return res;
        }
    }

    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR deinit_postprocessing(ei_impulse_handle_t *handle) {
    if (!handle) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }
    auto impulse = handle->impulse;

    for (size_t i = 0; i < impulse->postprocessing_blocks_size; i++) {
        void* state = NULL;
        if (handle->post_processing_state != NULL) {
            state = handle->post_processing_state[i];
        }

        if (impulse->postprocessing_blocks[i].deinit_fn == nullptr) {
            continue;
        }

        EI_IMPULSE_ERROR res = impulse->postprocessing_blocks[i].deinit_fn(state, impulse->postprocessing_blocks[i].config);
        if (res != EI_IMPULSE_OK) {
            return res;
        }
    }
    ei_free(handle->post_processing_state);
    handle->post_processing_state = NULL;

    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR run_postprocessing(ei_impulse_handle_t *handle,
                                               ei_impulse_result_t *result) {
    if (!handle) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }
    auto impulse = handle->impulse;

    for (size_t ix = 0; ix < impulse->postprocessing_blocks_size; ix++) {
        void* state = NULL;
        if (handle->post_processing_state != NULL) {
            state = handle->post_processing_state[ix];
        }

        EI_IMPULSE_ERROR res = impulse->postprocessing_blocks[ix].postprocess_fn(handle,
                                                                                ix,
                                                                                impulse->postprocessing_blocks[ix].input_block_id,
                                                                                result,
                                                                                impulse->postprocessing_blocks[ix].config,
                                                                                state);
        if (res != EI_IMPULSE_OK) {
            return res;
        }
    }

    // free raw results
    for (size_t ix = 0; ix < impulse->learning_blocks_size; ix++) {
        if (result->_raw_outputs[ix].matrix) {
            delete result->_raw_outputs[ix].matrix;
            result->_raw_outputs[ix].matrix = nullptr;
        }
    }

    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR display_postprocessing(ei_impulse_handle_t *handle,
                                                   ei_impulse_result_t *result) {
    if (!handle) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }
    auto impulse = handle->impulse;

    for (size_t i = 0; i < impulse->postprocessing_blocks_size; i++) {

        if (impulse->postprocessing_blocks[i].display_fn == nullptr) {
            continue;
        }

        EI_IMPULSE_ERROR res = impulse->postprocessing_blocks[i].display_fn(result, impulse->postprocessing_blocks[i].config);
        if (res != EI_IMPULSE_OK) {
            return res;
        }
    }

    return EI_IMPULSE_OK;
}

#endif