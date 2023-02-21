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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_EON_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_EON_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED == 1)

#include "model-parameters/model_metadata.h"
#if EI_CLASSIFIER_HAS_MODEL_VARIABLES == 1
#include "model-parameters/model_variables.h"
#endif

#include "edge-impulse-sdk/tensorflow/lite/c/common.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tflite-model/trained_model_compiled.h"
#include "edge-impulse-sdk/classifier/ei_aligned_malloc.h"
#include "edge-impulse-sdk/classifier/ei_fill_result_struct.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_helper.h"
#include "model-parameters/dsp_blocks.h"
#include "edge-impulse-sdk/classifier/ei_run_dsp.h"

// Subset of things required from ei_impulse_t to invoke an EON Compiled model
typedef struct {
    bool object_detection;
    int8_t object_detection_last_layer;
    uint8_t tflite_output_data_tensor;
    uint8_t tflite_output_labels_tensor;
    uint8_t tflite_output_score_tensor;

    TfLiteTensor* (*model_input)(int);
    TfLiteTensor* (*model_output)(int);
    TfLiteStatus (*model_init)(void*(*alloc_fnc)(size_t,size_t));
    TfLiteStatus (*model_invoke)();
    TfLiteStatus (*model_reset)(void (*free)(void* ptr));
} ei_nn_tflite_eon_t;

extern "C" {
    static const ei_nn_tflite_eon_t get_nn_config_from_impulse(const ei_impulse_t *impulse) {
        const ei_nn_tflite_eon_t config = {
            .object_detection = impulse->object_detection,
            .object_detection_last_layer = impulse->object_detection_last_layer,
            .tflite_output_data_tensor = impulse->tflite_output_data_tensor,
            .tflite_output_labels_tensor = impulse->tflite_output_labels_tensor,
            .tflite_output_score_tensor = impulse->tflite_output_score_tensor,
            .model_input = impulse->model_input,
            .model_output = impulse->model_output,
            .model_init = impulse->model_init,
            .model_invoke = impulse->model_invoke,
            .model_reset = impulse->model_reset
        };
        return config;
    }
}

/**
 * Setup the TFLite runtime
 *
 * @param      ctx_start_us       Pointer to the start time
 * @param      input              Pointer to input tensor
 * @param      output             Pointer to output tensor
 * @param      micro_tensor_arena Pointer to the arena that will be allocated
 *
 * @return  EI_IMPULSE_OK if successful
 */
static EI_IMPULSE_ERROR inference_tflite_setup(
    const ei_nn_tflite_eon_t *config,
    uint64_t *ctx_start_us,
    TfLiteTensor** input,
    TfLiteTensor** output,
    TfLiteTensor** output_labels,
    TfLiteTensor** output_scores,
    ei_unique_ptr_t& p_tensor_arena) {

    *ctx_start_us = ei_read_timer_us();

    TfLiteStatus init_status = config->model_init(ei_aligned_calloc);
    if (init_status != kTfLiteOk) {
        ei_printf("Failed to allocate TFLite arena (error code %d)\n", init_status);
        return EI_IMPULSE_TFLITE_ARENA_ALLOC_FAILED;
    }

    *input = config->model_input(0);
    *output = config->model_output(config->tflite_output_data_tensor);

    if (config->object_detection_last_layer == EI_CLASSIFIER_LAST_LAYER_SSD) {
        *output_scores = config->model_output(config->tflite_output_score_tensor);
        *output_labels = config->model_output(config->tflite_output_labels_tensor);
    }

    return EI_IMPULSE_OK;
}

/**
 * Run TFLite model
 *
 * @param   ctx_start_us    Start time of the setup function (see above)
 * @param   output          Output tensor
 * @param   interpreter     TFLite interpreter (non-compiled models)
 * @param   tensor_arena    Allocated arena (will be freed)
 * @param   result          Struct for results
 * @param   debug           Whether to print debug info
 *
 * @return  EI_IMPULSE_OK if successful
 */
static EI_IMPULSE_ERROR inference_tflite_run(
    const ei_impulse_t *impulse,
    uint64_t ctx_start_us,
    TfLiteTensor* output,
    TfLiteTensor* labels_tensor,
    TfLiteTensor* scores_tensor,
    uint8_t* tensor_arena,
    ei_impulse_result_t *result,
    bool debug) {

    if (impulse->model_invoke() != kTfLiteOk) {
        return EI_IMPULSE_TFLITE_ERROR;
    }

    uint64_t ctx_end_us = ei_read_timer_us();

    result->timing.classification_us = ctx_end_us - ctx_start_us;
    result->timing.classification = (int)(result->timing.classification_us / 1000);

    // Read the predicted y value from the model's output tensor
    if (debug) {
        ei_printf("Predictions (time: %d ms.):\n", result->timing.classification);
    }

    EI_IMPULSE_ERROR fill_res = fill_result_struct_from_output_tensor_tflite(
        impulse, output, labels_tensor, scores_tensor, result, debug);

    impulse->model_reset(ei_aligned_free);

    if (fill_res != EI_IMPULSE_OK) {
        return fill_res;
    }

    if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
        return EI_IMPULSE_CANCELED;
    }

    return EI_IMPULSE_OK;
}

/**
 * @brief      Do neural network inferencing over a signal (from the DSP)
 *
 * @param      fmatrix  Processed matrix
 * @param      result   Output classifier results
 * @param[in]  debug    Debug output enable
 *
 * @return     The ei impulse error.
 */
EI_IMPULSE_ERROR run_nn_inference_from_dsp(
    const ei_nn_tflite_eon_t *config,
    signal_t *signal,
    matrix_t *output_matrix)
{
    TfLiteTensor* input;
    TfLiteTensor* output;
    TfLiteTensor* output_scores;
    TfLiteTensor* output_labels;

    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    uint64_t ctx_start_us;

    EI_IMPULSE_ERROR init_res = inference_tflite_setup(config,
        &ctx_start_us,
        &input,
        &output,
        &output_labels,
        &output_scores,
        p_tensor_arena);

    if (init_res != EI_IMPULSE_OK) {
        return init_res;
    }

    EI_IMPULSE_ERROR input_res = fill_input_tensor_from_signal(signal, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    // invoke the model
    if (config->model_invoke() != kTfLiteOk) {
        return EI_IMPULSE_TFLITE_ERROR;
    }

    EI_IMPULSE_ERROR output_res = fill_output_matrix_from_tensor(output, output_matrix);
    if (output_res != EI_IMPULSE_OK) {
        return output_res;
    }

    if (config->model_reset(ei_aligned_free) != kTfLiteOk) {
        return EI_IMPULSE_TFLITE_ERROR;
    }

    return EI_IMPULSE_OK;
}

/**
 * @brief      Do neural network inferencing over a feature matrix
 *
 * @param      fmatrix  Processed matrix
 * @param      result   Output classifier results
 * @param[in]  debug    Debug output enable
 *
 * @return     The ei impulse error.
 */
EI_IMPULSE_ERROR run_nn_inference(
    const ei_impulse_t *impulse,
    ei::matrix_t *fmatrix,
    ei_impulse_result_t *result,
    bool debug = false)
{
    const ei_nn_tflite_eon_t config = get_nn_config_from_impulse(impulse);

    TfLiteTensor* input;
    TfLiteTensor* output;
    TfLiteTensor* output_scores;
    TfLiteTensor* output_labels;

    uint64_t ctx_start_us = ei_read_timer_us();
    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    EI_IMPULSE_ERROR init_res = inference_tflite_setup(&config,
        &ctx_start_us,
        &input,
        &output,
        &output_labels,
        &output_scores,
        p_tensor_arena);

    if (init_res != EI_IMPULSE_OK) {
        return init_res;
    }

    uint8_t* tensor_arena = static_cast<uint8_t*>(p_tensor_arena.get());

    auto input_res = fill_input_tensor_from_matrix(fmatrix, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    EI_IMPULSE_ERROR run_res = inference_tflite_run(impulse, ctx_start_us,
                                                    output, output_labels, output_scores,
                                                    tensor_arena, result, debug);

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    if (run_res != EI_IMPULSE_OK) {
        return run_res;
    }

    return EI_IMPULSE_OK;
}

#if EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1
/**
 * Special function to run the classifier on images, only works on TFLite models (either interpreter or EON or for tensaiflow)
 * that allocates a lot less memory by quantizing in place. This only works if 'can_run_classifier_image_quantized'
 * returns EI_IMPULSE_OK.
 */
EI_IMPULSE_ERROR run_nn_inference_image_quantized(
    const ei_impulse_t *impulse,
    signal_t *signal,
    ei_impulse_result_t *result,
    bool debug = false) {

    const ei_nn_tflite_eon_t config = get_nn_config_from_impulse(impulse);

    memset(result, 0, sizeof(ei_impulse_result_t));

    uint64_t ctx_start_us;
    TfLiteTensor* input;
    TfLiteTensor* output;
    TfLiteTensor* output_scores;
    TfLiteTensor* output_labels;

    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    EI_IMPULSE_ERROR init_res = inference_tflite_setup(&config,
        &ctx_start_us, &input, &output,
        &output_labels,
        &output_scores,
        p_tensor_arena);
    if (init_res != EI_IMPULSE_OK) {
        return init_res;
    }

    if (input->type != TfLiteType::kTfLiteInt8 && input->type != TfLiteType::kTfLiteUInt8) {
        return EI_IMPULSE_ONLY_SUPPORTED_FOR_IMAGES;
    }

    uint64_t dsp_start_us = ei_read_timer_us();

    // features matrix maps around the input tensor to not allocate any memory
    ei::matrix_i8_t features_matrix(1, impulse->nn_input_frame_size, input->data.int8);

    // run DSP process and quantize automatically
    int ret = extract_image_features_quantized(impulse, signal, &features_matrix, ei_dsp_blocks[0].config, impulse->frequency);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to run DSP process (%d)\n", ret);
        return EI_IMPULSE_DSP_ERROR;
    }

    if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
        return EI_IMPULSE_CANCELED;
    }

    result->timing.dsp_us = ei_read_timer_us() - dsp_start_us;
    result->timing.dsp = (int)(result->timing.dsp_us / 1000);


    if (debug) {
        ei_printf("Features (%d ms.): ", result->timing.dsp);
        for (size_t ix = 0; ix < features_matrix.cols; ix++) {
            ei_printf_float((features_matrix.buffer[ix] - input->params.zero_point) * input->params.scale);
            ei_printf(" ");
        }
        ei_printf("\n");
    }

    ctx_start_us = ei_read_timer_us();

    EI_IMPULSE_ERROR run_res = inference_tflite_run(impulse,
        ctx_start_us,
        output,
        output_labels,
        output_scores,
        static_cast<uint8_t*>(p_tensor_arena.get()),
        result, debug);

    if (run_res != EI_IMPULSE_OK) {
        return run_res;
    }

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    return EI_IMPULSE_OK;
}
#endif // EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1

__attribute__((unused)) int extract_tflite_eon_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr, const float frequency) {
    ei_dsp_config_tflite_eon_t config = *((ei_dsp_config_tflite_eon_t*)config_ptr);

    const ei_nn_tflite_eon_t nn_config = {
        false, // object_detection
        EI_CLASSIFIER_LAST_LAYER_UNKNOWN, // object_detection_last_layer
        0, // tflite_output_data_tensor
        255, // tflite_output_labels_tensor
        255, // tflite_output_score_tensor
        config.input_fn, // model_input
        config.output_fn, // model_output
        config.init_fn, // model_init
        config.invoke_fn, // model_invoke
        config.reset_fn // model_reset
    };

    EI_IMPULSE_ERROR x = run_nn_inference_from_dsp(&nn_config, signal, output_matrix);
    if (x != EI_IMPULSE_OK) {
        ei_printf("run_nn_inference_from_dsp failed with code %d\n", x);
        EIDSP_ERR(EIDSP_INFERENCE_ERROR);
    }

    return EIDSP_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED == 1)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_EON_H_
