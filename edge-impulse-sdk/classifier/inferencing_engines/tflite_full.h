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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_FULL_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_FULL_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL)

#include "model-parameters/model_metadata.h"
#if EI_CLASSIFIER_HAS_MODEL_VARIABLES == 1
#include "model-parameters/model_variables.h"
#endif

#include <thread>
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include "edge-impulse-sdk/classifier/ei_fill_result_struct.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_helper.h"

// Subset of things required from ei_impulse_t to invoke TFLite model
typedef struct {
    uint32_t block_id;
    const uint8_t *model_arr;
    size_t model_arr_size;
} ei_nn_tflite_full_t;

typedef struct {
    std::unique_ptr<tflite::FlatBufferModel> model;
    std::unique_ptr<tflite::Interpreter> interpreter;
} ei_tflite_state_t;

std::map<uint32_t, ei_tflite_state_t*> ei_tflite_instances;

/**
 * Construct a tflite interpreter (creates it if needed)
 */
static EI_IMPULSE_ERROR get_interpreter(const ei_nn_tflite_full_t *config, tflite::Interpreter ** interpreter) {
    // not in the map yet...
    if (!ei_tflite_instances.count(config->block_id)) {
        ei_tflite_state_t *new_state = new ei_tflite_state_t();

        auto new_model = tflite::FlatBufferModel::BuildFromBuffer((const char*)config->model_arr, config->model_arr_size);
        new_state->model = std::move(new_model);
        if (!new_state->model) {
            ei_printf("Failed to build TFLite model from buffer\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }

        tflite::ops::builtin::BuiltinOpResolver resolver;
        tflite::InterpreterBuilder builder(*new_state->model, resolver);
        builder(&new_state->interpreter);

        if (!new_state->interpreter) {
            ei_printf("Failed to construct interpreter\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }

        if (new_state->interpreter->AllocateTensors() != kTfLiteOk) {
            ei_printf("AllocateTensors failed\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }

        int hw_thread_count = (int)std::thread::hardware_concurrency();
        hw_thread_count -= 1; // leave one thread free for the other application
        if (hw_thread_count < 1) {
            hw_thread_count = 1;
        }

        if (new_state->interpreter->SetNumThreads(hw_thread_count) != kTfLiteOk) {
            ei_printf("SetNumThreads failed\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }

        ei_tflite_instances.insert(std::make_pair(config->block_id, new_state));
    }

    auto tflite_state = ei_tflite_instances[config->block_id];
    *interpreter = tflite_state->interpreter.get();
    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR run_nn_inference_from_dsp(
    const ei_nn_tflite_full_t *config,
    signal_t *signal,
    matrix_t *output_matrix)
{
    tflite::Interpreter *interpreter;
    auto interpreter_ret = get_interpreter(config, &interpreter);
    if (interpreter_ret != EI_IMPULSE_OK) {
        return interpreter_ret;
    }

    TfLiteTensor *input = interpreter->input_tensor(0);
    TfLiteTensor *output = interpreter->output_tensor(0);

    if (!input) {
        return EI_IMPULSE_INPUT_TENSOR_WAS_NULL;
    }
    if (!output) {
        return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
    }

    EI_IMPULSE_ERROR input_res = fill_input_tensor_from_signal(signal, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    TfLiteStatus status = interpreter->Invoke();
    if (status != kTfLiteOk) {
        ei_printf("ERR: interpreter->Invoke() failed with %d\n", status);
        return EI_IMPULSE_TFLITE_ERROR;
    }

    EI_IMPULSE_ERROR output_res = fill_output_matrix_from_tensor(output, output_matrix);
    if (output_res != EI_IMPULSE_OK) {
        return output_res;
    }

    // on Linux we're not worried about free'ing (for now)

    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR run_nn_inference(
    const ei_impulse_t *impulse,
    ei::matrix_t *fmatrix,
    ei_impulse_result_t *result,
    bool debug = false)
{

    const ei_nn_tflite_full_t config = {
        0xFFFFFFFF, // block ID (we don't pass this in yet, so just put it here)
        impulse->model_arr,
        impulse->model_arr_size
    };
    tflite::Interpreter *interpreter;
    auto interpreter_ret = get_interpreter(&config, &interpreter);
    if (interpreter_ret != EI_IMPULSE_OK) {
        return interpreter_ret;
    }

    TfLiteTensor *input = interpreter->input_tensor(0);
    TfLiteTensor *output = interpreter->output_tensor(impulse->tflite_output_data_tensor);

    if (!input) {
        return EI_IMPULSE_INPUT_TENSOR_WAS_NULL;
    }
    if (!output) {
        return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
    }

    auto input_res = fill_input_tensor_from_matrix(fmatrix, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    uint64_t ctx_start_us = ei_read_timer_us();

    TfLiteStatus status = interpreter->Invoke();
    if (status != kTfLiteOk) {
        ei_printf("ERR: interpreter->Invoke() failed with %d\n", status);
        return EI_IMPULSE_TFLITE_ERROR;
    }

    uint64_t ctx_end_us = ei_read_timer_us();

    result->timing.classification_us = ctx_end_us - ctx_start_us;
    result->timing.classification = (int)(result->timing.classification_us / 1000);

    if (debug) {
        ei_printf("Predictions (time: %d ms.):\n", result->timing.classification);
    }

    TfLiteTensor *scores_tensor = interpreter->output_tensor(impulse->tflite_output_score_tensor);
    TfLiteTensor *labels_tensor = interpreter->output_tensor(impulse->tflite_output_labels_tensor);

    EI_IMPULSE_ERROR fill_res = fill_result_struct_from_output_tensor_tflite(
        impulse, output, labels_tensor, scores_tensor, result, debug);

    if (fill_res != EI_IMPULSE_OK) {
        return fill_res;
    }

    // on Linux we're not worried about free'ing (for now)

    return EI_IMPULSE_OK;
}

__attribute__((unused)) int extract_tflite_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr, const float frequency) {
    ei_dsp_config_tflite_t config = *((ei_dsp_config_tflite_t*)config_ptr);

    const ei_nn_tflite_full_t nn_config = {
        config.block_id, // block_id
        config.model, // model_arr
        config.model_size // model_arr_size
    };
    EI_IMPULSE_ERROR x = run_nn_inference_from_dsp(&nn_config, signal, output_matrix);
    if (x != EI_IMPULSE_OK) {
        ei_printf("run_nn_inference_from_dsp failed with code %d\n", x);
        EIDSP_ERR(EIDSP_INFERENCE_ERROR);
    }

    return EIDSP_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_FULL_H_
