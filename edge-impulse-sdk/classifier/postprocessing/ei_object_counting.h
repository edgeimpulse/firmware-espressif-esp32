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

#ifndef EI_OBJECT_COUNTING_H
#define EI_OBJECT_COUNTING_H

#if EI_CLASSIFIER_OBJECT_COUNTING_ENABLED

/* Includes ---------------------------------------------------------------- */
#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/classifier/ei_model_types.h"

/* Private const types ----------------------------------------------------- */
#define MEM_ERROR   "ERR: Failed to allocate memory for performance calibration\r\n"

#define EI_PC_RET_NO_EVENT_DETECTED    -1
#define EI_PC_RET_MEMORY_ERROR         -2

extern ei_impulse_handle_t & ei_default_impulse;

class ObjectCounter {
public:
    ObjectCounter() {
        this->_count = 0;
    }

    void countObjectDetected() {
        this->_count++;
    }

    int getCount() {
        return this->_count;
    }

private:
    int _count;
};

EI_IMPULSE_ERROR init_objcnt(ei_impulse_handle_t *handle, void *config)
{
    const ei_impulse_t *impulse = handle->impulse;
    const ei_model_performance_calibration_t *calibration = (ei_model_performance_calibration_t*)config;

    // Allocate the object counter
    ObjectCounter *objcnt = new ObjectCounter();
    if (!objcnt) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }

    // Store the object counter in the handle
    handle->post_processing_state = (void *)objcnt;

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR deinit_objcnt(ei_impulse_handle_t *handle, void *config)
{
    ObjectCounter *objcnt = (ObjectCounter *)handle->post_processing_state;
    if (objcnt) {
        delete objcnt;
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR process_objcnt(ei_impulse_handle_t *handle,
                                            ei_impulse_result_t *result,
                                            void *config,
                                            bool debug) {

    const ei_impulse_t *impulse = handle->impulse;
    ObjectCounter *objcnt = (ObjectCounter *)handle->post_processing_state;

    if (impulse->sensor == EI_CLASSIFIER_SENSOR_CAMERA) {
        if((void *)objcnt != NULL) {
            objcnt->countObjectDetected();
        }
    }

    return EI_IMPULSE_OK;
}

typedef struct {
    float detection_threshold;
} ei_obj_cnt_params_t;

EI_IMPULSE_ERROR set_post_process_params(ei_impulse_handle_t* handle, ei_obj_cnt_params_t* params) {
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_impulse_handle_t* handle, ei_obj_cnt_params_t* params) {
    return EI_IMPULSE_OK;
}

// versions that operate on the default impulse
EI_IMPULSE_ERROR set_post_process_params(ei_obj_cnt_params_t *params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        // set params
    }
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_obj_cnt_params_t *params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        // get params
    }
    return EI_IMPULSE_OK;
}

#endif // EI_CLASSIFIER_OBJECT_COUNTING_ENABLED
#endif // EI_OBJECT_COUNTING_H