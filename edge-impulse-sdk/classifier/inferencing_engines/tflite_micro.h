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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_MICRO_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_MICRO_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED != 1)

#include "model-parameters/model_metadata.h"
#if EI_CLASSIFIER_HAS_MODEL_VARIABLES == 1
#include "model-parameters/model_variables.h"
#endif

#include <cmath>
#include "edge-impulse-sdk/tensorflow/lite/micro/all_ops_resolver.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/micro_error_reporter.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/micro_interpreter.h"
#include "edge-impulse-sdk/tensorflow/lite/schema/schema_generated.h"
#include "edge-impulse-sdk/classifier/ei_aligned_malloc.h"
#include "edge-impulse-sdk/classifier/ei_fill_result_struct.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_helper.h"

#if defined(EI_CLASSIFIER_HAS_TFLITE_OPS_RESOLVER) && EI_CLASSIFIER_HAS_TFLITE_OPS_RESOLVER == 1
#include "tflite-model/tflite-resolver.h"
#endif // EI_CLASSIFIER_HAS_TFLITE_OPS_RESOLVER

static tflite::MicroErrorReporter micro_error_reporter;
static tflite::ErrorReporter* error_reporter = &micro_error_reporter;

#ifdef EI_CLASSIFIER_ALLOCATION_STATIC
#if defined __GNUC__
#define ALIGN(X) __attribute__((aligned(X)))
#elif defined _MSC_VER
#define ALIGN(X) __declspec(align(X))
#elif defined __TASKING__
#define ALIGN(X) __align(X)
#endif
#endif

// Subset of things required from ei_impulse_t to invoke a TFLite Micro model
typedef struct {
    uint32_t project_id;
    uint32_t block_id;

    uint32_t nn_input_frame_size;
    float frequency;

    bool object_detection;
    int8_t object_detection_last_layer;
    uint8_t tflite_output_data_tensor;
    uint8_t tflite_output_labels_tensor;
    uint8_t tflite_output_score_tensor;

    uint32_t tflite_arena_size;
    const uint8_t *model_arr;
    size_t model_arr_size;
} ei_nn_tflite_micro_t;

extern "C" {
    static const ei_nn_tflite_micro_t get_nn_config_from_impulse(const ei_impulse_t *impulse) {
        const ei_nn_tflite_micro_t config = {
            .project_id = impulse->project_id,
            .block_id = 0xffffffff, // we don't have one for learn blocks, so just set to max value here
            .nn_input_frame_size = impulse->nn_input_frame_size,
            .frequency = impulse->frequency,
            .object_detection = impulse->object_detection,
            .object_detection_last_layer = impulse->object_detection_last_layer,
            .tflite_output_data_tensor = impulse->tflite_output_data_tensor,
            .tflite_output_labels_tensor = impulse->tflite_output_labels_tensor,
            .tflite_output_score_tensor = impulse->tflite_output_score_tensor,
            .tflite_arena_size = impulse->tflite_arena_size,
            .model_arr = impulse->model_arr,
            .model_arr_size = impulse->model_arr_size
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
 * @param      micro_interpreter  Pointer to interpreter (for non-compiled models)
 * @param      micro_tensor_arena Pointer to the arena that will be allocated
 *
 * @return  EI_IMPULSE_OK if successful
 */
static EI_IMPULSE_ERROR inference_tflite_setup(
    const ei_nn_tflite_micro_t *config,
    uint64_t *ctx_start_us,
    TfLiteTensor** input,
    TfLiteTensor** output,
    TfLiteTensor** output_labels,
    TfLiteTensor** output_scores,
    tflite::MicroInterpreter** micro_interpreter,
    ei_unique_ptr_t& p_tensor_arena) {

    *ctx_start_us = ei_read_timer_us();

#ifdef EI_CLASSIFIER_ALLOCATION_STATIC
    // Assign a no-op lambda to the "free" function in case of static arena
    static uint8_t tensor_arena[EI_CLASSIFIER_TFLITE_ARENA_SIZE] ALIGN(16);
    p_tensor_arena = ei_unique_ptr_t(tensor_arena, [](void*){});
#else
    // Create an area of memory to use for input, output, and intermediate arrays.
    uint8_t *tensor_arena = (uint8_t*)ei_aligned_calloc(16, config->tflite_arena_size);
    if (tensor_arena == NULL) {
        ei_printf("Failed to allocate TFLite arena (%d bytes)\n", config->tflite_arena_size);
        return EI_IMPULSE_TFLITE_ARENA_ALLOC_FAILED;
    }
    p_tensor_arena = ei_unique_ptr_t(tensor_arena, ei_aligned_free);
#endif

    static bool tflite_first_run = true;
    static uint32_t last_project_id = 0;
    static uint32_t last_block_id = 0;

    if (last_project_id != config->project_id) {
        tflite_first_run = true;
        last_project_id = config->project_id;
    }
    if (last_block_id != config->block_id) {
        tflite_first_run = true;
        last_block_id = config->block_id;
    }

    static const tflite::Model* model = nullptr;

    // ======
    // Initialization code start
    // This part can be run once, but that would require the TFLite arena
    // to be allocated at all times, which is not ideal (e.g. when doing MFCC)
    // ======
    if (tflite_first_run) {
        // Map the model into a usable data structure. This doesn't involve any
        // copying or parsing, it's a very lightweight operation.
        model = tflite::GetModel(config->model_arr);
        if (model->version() != TFLITE_SCHEMA_VERSION) {
            error_reporter->Report(
                "Model provided is schema version %d not equal "
                "to supported version %d.",
                model->version(), TFLITE_SCHEMA_VERSION);
            return EI_IMPULSE_TFLITE_ERROR;
        }
        tflite_first_run = false;
    }

#ifdef EI_TFLITE_RESOLVER
    EI_TFLITE_RESOLVER
#else
    tflite::AllOpsResolver resolver;
#endif
#if defined(EI_CLASSIFIER_ENABLE_DETECTION_POSTPROCESS_OP)
    resolver.AddCustom("TFLite_Detection_PostProcess", &post_process_op);
#endif

    // Build an interpreter to run the model with.
    tflite::MicroInterpreter *interpreter = new tflite::MicroInterpreter(
        model, resolver, tensor_arena, config->tflite_arena_size, error_reporter);

    *micro_interpreter = interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        error_reporter->Report("AllocateTensors() failed");
        return EI_IMPULSE_TFLITE_ERROR;
    }

    // Obtain pointers to the model's input and output tensors.
    *input = interpreter->input(0);
    *output = interpreter->output(config->tflite_output_data_tensor);

    if (config->object_detection_last_layer == EI_CLASSIFIER_LAST_LAYER_SSD) {
        *output_scores = interpreter->output(config->tflite_output_score_tensor);
        *output_labels = interpreter->output(config->tflite_output_labels_tensor);
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
static EI_IMPULSE_ERROR inference_tflite_run(const ei_impulse_t *impulse,
    uint64_t ctx_start_us,
    TfLiteTensor* output,
    TfLiteTensor* labels_tensor,
    TfLiteTensor* scores_tensor,
    tflite::MicroInterpreter* interpreter,
    uint8_t* tensor_arena,
    ei_impulse_result_t *result,
    bool debug) {

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        delete interpreter;
        error_reporter->Report("Invoke failed (%d)\n", invoke_status);
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

    delete interpreter;

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
    const ei_nn_tflite_micro_t *config,
    signal_t *signal,
    matrix_t *output_matrix)
{
    TfLiteTensor* input;
    TfLiteTensor* output;
    TfLiteTensor* output_scores;
    TfLiteTensor* output_labels;
    uint64_t ctx_start_us = ei_read_timer_us();
    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    tflite::MicroInterpreter* interpreter;
    EI_IMPULSE_ERROR init_res = inference_tflite_setup(
        config,
        &ctx_start_us,
        &input, &output,
        &output_labels,
        &output_scores,
        &interpreter, p_tensor_arena);

    if (init_res != EI_IMPULSE_OK) {
        return init_res;
    }

    EI_IMPULSE_ERROR input_res = fill_input_tensor_from_signal(signal, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        error_reporter->Report("Invoke failed (%d)\n", invoke_status);
        return EI_IMPULSE_TFLITE_ERROR;
    }

    EI_IMPULSE_ERROR output_res = fill_output_matrix_from_tensor(output, output_matrix);
    if (output_res != EI_IMPULSE_OK) {
        return output_res;
    }

    delete interpreter;

    return EI_IMPULSE_OK;
}

/**
 * @brief      Do neural network inferencing over the processed feature matrix
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
    const ei_nn_tflite_micro_t config = get_nn_config_from_impulse(impulse);

    TfLiteTensor* input;
    TfLiteTensor* output;
    TfLiteTensor* output_scores;
    TfLiteTensor* output_labels;
    uint64_t ctx_start_us = ei_read_timer_us();
    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    tflite::MicroInterpreter* interpreter;
    EI_IMPULSE_ERROR init_res = inference_tflite_setup(
        &config,
        &ctx_start_us,
        &input, &output,
        &output_labels,
        &output_scores,
        &interpreter,
        p_tensor_arena);

    if (init_res != EI_IMPULSE_OK) {
        return init_res;
    }

    uint8_t* tensor_arena = static_cast<uint8_t*>(p_tensor_arena.get());

    auto input_res = fill_input_tensor_from_matrix(fmatrix, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    EI_IMPULSE_ERROR run_res = inference_tflite_run(impulse,
        ctx_start_us,
        output,
        output_labels,
        output_scores,
        interpreter, tensor_arena, result, debug);

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
    bool debug = false)
{
    const ei_nn_tflite_micro_t config = get_nn_config_from_impulse(impulse);

    memset(result, 0, sizeof(ei_impulse_result_t));

    uint64_t ctx_start_us;
    TfLiteTensor* input;
    TfLiteTensor* output;
    TfLiteTensor* output_scores;
    TfLiteTensor* output_labels;
    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    tflite::MicroInterpreter* interpreter;
    EI_IMPULSE_ERROR init_res = inference_tflite_setup(&config,
        &ctx_start_us,
        &input, &output,
        &output_labels,
        &output_scores,
        &interpreter,
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
    int ret = extract_image_features_quantized(impulse, signal, &features_matrix, impulse->dsp_blocks[0].config, impulse->frequency);
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
            ei_printf_float((features_matrix.buffer[ix] - impulse->tflite_input_zeropoint) * impulse->tflite_input_scale);
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
        interpreter,
        static_cast<uint8_t*>(p_tensor_arena.get()),
        result, debug);

    if (run_res != EI_IMPULSE_OK) {
        return run_res;
    }

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    return EI_IMPULSE_OK;
}
#endif // EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1

__attribute__((unused)) int extract_tflite_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr, const float frequency) {
    ei_dsp_config_tflite_t config = *((ei_dsp_config_tflite_t*)config_ptr);

    const ei_nn_tflite_micro_t nn_config = {
        0, // project_id
        config.block_id, // block_id
        static_cast<uint32_t>(signal->total_length), // nn_input_frame_size
        frequency, // frequency
        false, // object_detection
        EI_CLASSIFIER_LAST_LAYER_UNKNOWN, // object_detection_last_layer
        0, // tflite_output_data_tensor
        255, // tflite_output_labels_tensor
        255, // tflite_output_score_tensor
        static_cast<uint32_t>(config.arena_size), // tflite_arena_size
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

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED != 1)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_MICRO_H_
