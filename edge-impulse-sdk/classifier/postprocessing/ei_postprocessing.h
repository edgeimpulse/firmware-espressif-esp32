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

    for (size_t i = 0; i < impulse->postprocessing_blocks_size; i++) {
        void* state = NULL;
        if (handle->post_processing_state != NULL) {
            state = handle->post_processing_state[i];
        }

        EI_IMPULSE_ERROR res = impulse->postprocessing_blocks[i].postprocess_fn(handle,
                                                                                result,
                                                                                impulse->postprocessing_blocks[i].config,
                                                                                state);
        if (res != EI_IMPULSE_OK) {
            return res;
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