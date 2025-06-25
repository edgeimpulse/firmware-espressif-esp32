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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_MICRO_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_MICRO_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED != 1)

#include "model-parameters/model_metadata.h"

#include <cmath>
#include "edge-impulse-sdk/tensorflow/lite/micro/all_ops_resolver.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/micro_interpreter.h"
#include "edge-impulse-sdk/tensorflow/lite/schema/schema_generated.h"
#include "edge-impulse-sdk/tensorflow/lite/schema/schema_generated_full.h"
#include "edge-impulse-sdk/classifier/ei_aligned_malloc.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_helper.h"

#if defined(EI_CLASSIFIER_HAS_TFLITE_OPS_RESOLVER) && EI_CLASSIFIER_HAS_TFLITE_OPS_RESOLVER == 1
#include "tflite-model/tflite-resolver.h"
#endif // EI_CLASSIFIER_HAS_TFLITE_OPS_RESOLVER

#ifdef EI_CLASSIFIER_ENABLE_PROFILER
#include "edge-impulse-sdk/tensorflow/lite/micro/micro_profiler.h"
#endif

#ifdef EI_CLASSIFIER_ALLOCATION_STATIC
#if defined __GNUC__
#define ALIGN(X) __attribute__((aligned(X)))
#elif defined _MSC_VER
#define ALIGN(X) __declspec(align(X))
#elif defined __TASKING__
#define ALIGN(X) __align(X)
#endif
#endif

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

#if defined (__GNUC__)  /* GNU compiler */
#define ALIGN(X) __attribute__((aligned(X)))
#define DEFINE_SECTION(x) __attribute__((section(x)))
#elif defined (_MSC_VER)
#define ALIGN(X) __declspec(align(X))
#elif defined (__TASKING__) /* TASKING Compiler */
#define ALIGN(X) __align(X)
#define DEFINE_SECTION(x) __attribute__((section(x)))
#elif defined (__ARMCC_VERSION) /* Arm Compiler */
#define ALIGN(X) __ALIGNED(x)
#define DEFINE_SECTION(x) __attribute__((section(x)))
#elif defined (__ICCARM__) /* IAR Compiler */
#define ALIGN(x) __attribute__((aligned(x)))
#define DEFINE_SECTION(x) __attribute__((section(x)))
#elif defined (__clang__) /* LLVM/Clang Compiler */
#define ALIGN(X) __ALIGNED(x)
#define DEFINE_SECTION(x) __attribute__((section(x)))
#endif

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
    ei_learning_block_config_tflite_graph_t *block_config,
    uint64_t *ctx_start_us,
    TfLiteTensor** input,
    TfLiteTensor** outputs,
    tflite::MicroInterpreter** micro_interpreter,
    ei_unique_ptr_t& p_tensor_arena,
    void** micro_profiler) {

    *ctx_start_us = ei_read_timer_us();

    ei_config_tflite_graph_t *graph_config = (ei_config_tflite_graph_t*)block_config->graph_config;

#ifdef EI_CLASSIFIER_ALLOCATION_STATIC
    // Assign a no-op lambda to the "free" function in case of static arena
    static uint8_t tensor_arena[EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE] ALIGN(16) DEFINE_SECTION(STRINGIZE_VALUE_OF(EI_TENSOR_ARENA_LOCATION));
    p_tensor_arena = ei_unique_ptr_t(tensor_arena, [](void*){});
#else
    // Create an area of memory to use for input, output, and intermediate arrays.
    uint8_t *tensor_arena = (uint8_t*)ei_aligned_calloc(16, graph_config->arena_size);
    if (tensor_arena == NULL) {
        ei_printf("Failed to allocate TFLite arena (%zu bytes)\n", graph_config->arena_size);
        return EI_IMPULSE_TFLITE_ARENA_ALLOC_FAILED;
    }
    p_tensor_arena = ei_unique_ptr_t(tensor_arena, ei_aligned_free);
#endif

    static bool tflite_first_run = true;
    static uint8_t *model_arr = NULL;

    if (model_arr != graph_config->model) {
        tflite_first_run = true;
        model_arr = (uint8_t*)graph_config->model;
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
        model = tflite::GetModel(graph_config->model);
        if (model->version() != TFLITE_SCHEMA_VERSION) {
            ei_printf(
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
    static tflite::AllOpsResolver resolver; // needs static to match the life of the interpreter
#endif

    // Build an interpreter to run the model with.
    // only create profiler when enabled
#ifdef EI_CLASSIFIER_ENABLE_PROFILER
    tflite::MicroProfiler *profiler = new tflite::MicroProfiler;

    tflite::MicroInterpreter *interpreter = new tflite::MicroInterpreter(
        model, resolver, tensor_arena, graph_config->arena_size, nullptr, profiler);

    *micro_profiler = (void*)profiler;
#else
    tflite::MicroInterpreter *interpreter = new tflite::MicroInterpreter(
        model, resolver, tensor_arena, graph_config->arena_size, nullptr, nullptr);

    micro_profiler = nullptr;
#endif

    *micro_interpreter = interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors(true);
    if (allocate_status != kTfLiteOk) {
        ei_printf("AllocateTensors() failed");
        return EI_IMPULSE_TFLITE_ERROR;
    }

    // Obtain pointers to the model's input and output tensors.
    *input = interpreter->input(0);
    for (uint8_t i = 0; i < block_config->output_tensors_size; i++) {
        outputs[i] = interpreter->output(block_config->output_tensors_indices[i]);
    }

    if (tflite_first_run) {
        tflite_first_run = false;
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
    uint64_t ctx_start_us,
    tflite::MicroInterpreter* interpreter,
    ei_impulse_result_t *result,
    void* micro_profiler) {

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        delete interpreter;
        ei_printf("Invoke failed (%d)\n", invoke_status);
        return EI_IMPULSE_TFLITE_ERROR;
    }

    uint64_t ctx_end_us = ei_read_timer_us();

    result->timing.classification_us = ctx_end_us - ctx_start_us;
    result->timing.classification = (int)(result->timing.classification_us / 1000);

    EI_LOGD("Predictions (time: %d ms.):\n", result->timing.classification);

#ifdef EI_CLASSIFIER_ENABLE_PROFILER
    tflite::MicroProfiler *profiler = (tflite::MicroProfiler*)micro_profiler;

    ei_printf("Profiling per individual OP\n");
    profiler->LogCsv();
    ei_printf("\n");

    ei_printf("Profiling per OP group\n");
    profiler->LogTicksPerTagCsv();
    ei_printf("\n");
#endif

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
    ei_learning_block_config_tflite_graph_t *config,
    signal_t *signal,
    matrix_t *output_matrix)
{
    TfLiteTensor* input;
    TfLiteTensor* outputs;
    uint64_t ctx_start_us = ei_read_timer_us();
    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    tflite::MicroInterpreter* interpreter;
#ifdef EI_CLASSIFIER_ENABLE_PROFILER
    tflite::MicroProfiler* profiler;
#else
    void* profiler = nullptr;
#endif

    EI_IMPULSE_ERROR init_res = inference_tflite_setup(
        config,
        &ctx_start_us,
        &input,
        &outputs,
        &interpreter,
        p_tensor_arena,
        (void**)&profiler);

    if (init_res != EI_IMPULSE_OK) {
        return init_res;
    }

    auto input_res = fill_input_tensor_from_signal(signal, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        ei_printf("Invoke failed (%d)\n", invoke_status);
        return EI_IMPULSE_TFLITE_ERROR;
    }

    auto output_res = fill_output_matrix_from_tensor(&outputs[0], output_matrix);
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
    ei_feature_t *fmatrix,
    uint32_t learn_block_index,
    uint32_t* input_block_ids,
    uint32_t input_block_ids_size,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;

    TfLiteTensor* input;
    TfLiteTensor* outputs;
    uint64_t ctx_start_us = ei_read_timer_us();
    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    tflite::MicroInterpreter* interpreter;
#ifdef EI_CLASSIFIER_ENABLE_PROFILER
    tflite::MicroProfiler* profiler;
#else
    void* profiler = nullptr;
#endif

    EI_IMPULSE_ERROR init_res = inference_tflite_setup(
        block_config,
        &ctx_start_us,
        &input,
        &outputs,
        &interpreter,
        p_tensor_arena,
        (void**)&profiler);

    if (init_res != EI_IMPULSE_OK) {
        return init_res;
    }

    auto input_res = fill_input_tensor_from_matrix(fmatrix,
                                                   result->_raw_outputs,
                                                   input,
                                                   input_block_ids,
                                                   input_block_ids_size,
                                                   impulse->dsp_blocks_size,
                                                   impulse->learning_blocks_size);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    EI_IMPULSE_ERROR run_res = inference_tflite_run(
        ctx_start_us,
        interpreter,
        result,
        profiler);

    for (uint32_t output_ix = 0; output_ix < block_config->output_tensors_size; output_ix++) {
        TfLiteTensor* output = &outputs[output_ix];
        // calculate the size of the output by iterating through dims
        size_t output_size = 1;
        for (int dim_num = 0; dim_num < output->dims->size; dim_num++) {
            output_size *= output->dims->data[dim_num];
        }

        switch (output->type) {
            case kTfLiteFloat32: {
                result->_raw_outputs[learn_block_index + output_ix].matrix = new matrix_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix->buffer, output->data.f, output->bytes);
                break;
            }
            case kTfLiteInt8: {
                result->_raw_outputs[learn_block_index + output_ix].matrix_i8 = new matrix_i8_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_i8->buffer, output->data.int8, output->bytes);
                break;
            }
            case kTfLiteUInt8: {
                result->_raw_outputs[learn_block_index + output_ix].matrix_u8 = new matrix_u8_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_u8->buffer, output->data.uint8, output->bytes);
                break;
            }
            default: {
                ei_printf("ERR: Cannot handle output type (%d)\n", output->type);
                return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
            }
        }

        result->_raw_outputs[learn_block_index].blockId = block_config->block_id;
    }

    delete interpreter;

    if (run_res != EI_IMPULSE_OK) {
        return run_res;
    }

    return EI_IMPULSE_OK;
}

#if EI_CLASSIFIER_QUANTIZATION_ENABLED == 1
/**
 * Special function to run the classifier on images, only works on TFLite models (either interpreter or EON or for tensaiflow)
 * that allocates a lot less memory by quantizing in place. This only works if 'can_run_classifier_image_quantized'
 * returns EI_IMPULSE_OK.
 */
EI_IMPULSE_ERROR run_nn_inference_image_quantized(
    const ei_impulse_t *impulse,
    signal_t *signal,
    uint32_t learn_block_index,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;

    memset(result, 0, sizeof(ei_impulse_result_t));

    uint64_t ctx_start_us;
    TfLiteTensor* input;
    TfLiteTensor* outputs;
    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);

    tflite::MicroInterpreter* interpreter;
#ifdef EI_CLASSIFIER_ENABLE_PROFILER
    tflite::MicroProfiler* profiler;
#else
    void* profiler = nullptr;
#endif

    EI_IMPULSE_ERROR init_res = inference_tflite_setup(
        block_config,
        &ctx_start_us,
        &input,
        &outputs,
        &interpreter,
        p_tensor_arena,
        (void**)&profiler);

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
    int ret = extract_image_features_quantized(signal, &features_matrix, impulse->dsp_blocks[0].config, input->params.scale, input->params.zero_point,
        impulse->frequency, impulse->learning_blocks[0].image_scaling);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to run DSP process (%d)\n", ret);
        return EI_IMPULSE_DSP_ERROR;
    }

    if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
        return EI_IMPULSE_CANCELED;
    }

    result->timing.dsp_us = ei_read_timer_us() - dsp_start_us;
    result->timing.dsp = (int)(result->timing.dsp_us / 1000);

#if EI_LOG_LEVEL == EI_LOG_LEVEL_DEBUG
    ei_printf("Features (%d ms.): ", result->timing.dsp);
    for (size_t ix = 0; ix < features_matrix.cols; ix++) {
        ei_printf_float((features_matrix.buffer[ix] - input->params.zero_point) * input->params.scale);
        ei_printf(" ");
    }
    ei_printf("\n");
#endif

    ctx_start_us = ei_read_timer_us();

    EI_IMPULSE_ERROR run_res = inference_tflite_run(
        ctx_start_us,
        interpreter,
        result,
        profiler);

    for (uint32_t output_ix = 0; output_ix < block_config->output_tensors_size; output_ix++) {
        TfLiteTensor* output = &outputs[output_ix];
        // calculate the size of the output by iterating through dims
        size_t output_size = 1;
        for (int dim_num = 0; dim_num < output->dims->size; dim_num++) {
            output_size *= output->dims->data[dim_num];
        }

        switch (output->type) {
            case kTfLiteFloat32: {
                result->_raw_outputs[learn_block_index + output_ix].matrix = new matrix_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix->buffer, output->data.f, output->bytes);
                break;
            }
            case kTfLiteInt8: {
                result->_raw_outputs[learn_block_index + output_ix].matrix_i8 = new matrix_i8_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_i8->buffer, output->data.int8, output->bytes);
                break;
            }
            case kTfLiteUInt8: {
                result->_raw_outputs[learn_block_index + output_ix].matrix_u8 = new matrix_u8_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_u8->buffer, output->data.uint8, output->bytes);
                break;
            }
            default: {
                ei_printf("ERR: Cannot handle output type (%d)\n", output->type);
                return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
            }
        }

        result->_raw_outputs[learn_block_index].blockId = block_config->block_id;
    }

    delete interpreter;

    if (run_res != EI_IMPULSE_OK) {
        return run_res;
    }

    return EI_IMPULSE_OK;
}
#endif // EI_CLASSIFIER_QUANTIZATION_ENABLED == 1

__attribute__((unused)) int extract_tflite_features(signal_t *signal, matrix_t *output_matrix, void *config_ptr, const float frequency) {
    ei_dsp_config_tflite_t *dsp_config = (ei_dsp_config_tflite_t*)config_ptr;

    ei_config_tflite_graph_t ei_config_tflite_graph_0 = {
        .implementation_version = 1,
        .model = dsp_config->model,
        .model_size = dsp_config->model_size,
        .arena_size = dsp_config->arena_size
    };

    const uint8_t ei_output_tensor_indices[1] = { 0 };
    const uint8_t ei_output_tensor_size = 1;

    ei_learning_block_config_tflite_graph_t ei_learning_block_config = {
        .implementation_version = 1,
        .block_id = dsp_config->block_id,
        .output_tensors_indices = ei_output_tensor_indices,
        .output_tensors_size = ei_output_tensor_size,
        .quantized = 0,
        .compiled = 0,
        .graph_config = &ei_config_tflite_graph_0
    };

    auto x = run_nn_inference_from_dsp(&ei_learning_block_config, signal, output_matrix);
    if (x != 0) {
        return x;
    }

    return EIDSP_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED != 1)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_MICRO_H_
