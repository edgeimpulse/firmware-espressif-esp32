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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_FULL_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_FULL_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL)

#include "model-parameters/model_metadata.h"
#include "tflite-model/trained_model_ops_define.h"

#include <thread>
#include "tensorflow-lite/tensorflow/lite/c/common.h"
#include "tensorflow-lite/tensorflow/lite/interpreter.h"
#include "tensorflow-lite/tensorflow/lite/kernels/register.h"
#include "tensorflow-lite/tensorflow/lite/model.h"
#include "tensorflow-lite/tensorflow/lite/optional_debug_tools.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/custom/tree_ensemble_classifier.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_helper.h"
#ifdef EI_CLASSIFIER_USE_QNN_DELEGATES
#include "QNN/TFLiteDelegate/QnnTFLiteDelegate.h"
#elif EI_CLASSIFIER_USE_GPU_DELEGATES==1
#include "tensorflow-lite/tensorflow/lite/delegates/gpu/delegate.h"
#endif

typedef struct {
    std::unique_ptr<tflite::FlatBufferModel> model;
    std::unique_ptr<tflite::Interpreter> interpreter;
} ei_tflite_state_t;

std::map<uint32_t, ei_tflite_state_t*> ei_tflite_instances;

/**
 * Construct a tflite interpreter (creates it if needed)
 */
static EI_IMPULSE_ERROR get_interpreter(ei_learning_block_config_tflite_graph_t *block_config, tflite::Interpreter **interpreter) {
    // not in the map yet...
    if (!ei_tflite_instances.count(block_config->block_id)) {
        ei_config_tflite_graph_t *graph_config = (ei_config_tflite_graph_t*)block_config->graph_config;
        ei_tflite_state_t *new_state = new ei_tflite_state_t();

        auto new_model = tflite::FlatBufferModel::BuildFromBuffer((const char*)graph_config->model, graph_config->model_size);
        new_state->model = std::move(new_model);
        if (!new_state->model) {
            ei_printf("Failed to build TFLite model from buffer\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }

        tflite::ops::builtin::BuiltinOpResolver resolver;
#if EI_CLASSIFIER_HAS_TREE_ENSEMBLE_CLASSIFIER
        resolver.AddCustom("TreeEnsembleClassifier",
            tflite::ops::custom::Register_TREE_ENSEMBLE_CLASSIFIER());
#endif
        tflite::InterpreterBuilder builder(*new_state->model, resolver);
        builder(&new_state->interpreter);

        if (!new_state->interpreter) {
            ei_printf("Failed to construct interpreter\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }
#ifdef EI_CLASSIFIER_USE_QNN_DELEGATES
        // Create QNN Delegate options structure.
        TfLiteQnnDelegateOptions options = TfLiteQnnDelegateOptionsDefault();

        // Set the mandatory backend_type option. All other options have default values.
        options.backend_type = kHtpBackend;

        // Instantiate delegate. Must not be freed until interpreter is freed.
        TfLiteDelegate* delegate = TfLiteQnnDelegateCreate(&options);

        if (new_state->interpreter->ModifyGraphWithDelegate(delegate) != kTfLiteOk) {
            ei_printf("ERROR: ModifyGraphWithDelegate failed\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }
#elif EI_CLASSIFIER_USE_GPU_DELEGATES==1
        TfLiteGpuDelegateOptionsV2 options = TfLiteGpuDelegateOptionsV2Default();

        TfLiteDelegate* delegate = TfLiteGpuDelegateV2Create(&options);

        if (new_state->interpreter->ModifyGraphWithDelegate(delegate) != kTfLiteOk) {
            ei_printf("ERROR: ModifyGraphWithDelegate (GPU) failed\n");
            return EI_IMPULSE_TFLITE_ERROR;
        }
#endif

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

        ei_tflite_instances.insert(std::make_pair(block_config->block_id, new_state));
    }

    auto tflite_state = ei_tflite_instances[block_config->block_id];
    *interpreter = tflite_state->interpreter.get();
    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR run_nn_inference_from_dsp(
    ei_learning_block_config_tflite_graph_t *block_config,
    signal_t *signal,
    matrix_t *output_matrix)
{
    tflite::Interpreter *interpreter;
    auto interpreter_ret = get_interpreter(block_config, &interpreter);
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

    auto input_res = fill_input_tensor_from_signal(signal, input);
    if (input_res != EI_IMPULSE_OK) {
        return input_res;
    }

    TfLiteStatus status = interpreter->Invoke();
    if (status != kTfLiteOk) {
        ei_printf("ERR: interpreter->Invoke() failed with %d\n", status);
        return EI_IMPULSE_TFLITE_ERROR;
    }

    auto output_res = fill_output_matrix_from_tensor(output, output_matrix);
    if (output_res != EI_IMPULSE_OK) {
        return output_res;
    }

    // on Linux we're not worried about free'ing (for now)

    return EI_IMPULSE_OK;
}

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

    tflite::Interpreter *interpreter;
    auto interpreter_ret = get_interpreter(block_config, &interpreter);
    if (interpreter_ret != EI_IMPULSE_OK) {
        return interpreter_ret;
    }

    TfLiteTensor* input = nullptr;
    TfLiteTensor** outputs = nullptr;

    // allocate outputs
    outputs = (TfLiteTensor**)ei_malloc(block_config->output_tensors_size * sizeof(TfLiteTensor*));

    // Obtain pointers to the model's input and output tensors.
    input = interpreter->input_tensor(0);

    for (uint8_t i = 0; i < block_config->output_tensors_size; i++) {
        outputs[i] = interpreter->output_tensor(block_config->output_tensors_indices[i]);
        if (&outputs[i] == nullptr) {
            return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
        }
        memset(outputs[i]->data.raw, 0, outputs[i]->bytes);
    }

    if (!input) {
        return EI_IMPULSE_INPUT_TENSOR_WAS_NULL;
    }

    uint64_t ctx_start_us = ei_read_timer_us();

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

    TfLiteStatus status = interpreter->Invoke();
    if (status != kTfLiteOk) {
        ei_printf("ERR: interpreter->Invoke() failed with %d\n", status);
        return EI_IMPULSE_TFLITE_ERROR;
    }

    uint64_t ctx_end_us = ei_read_timer_us();

    result->timing.classification_us = ctx_end_us - ctx_start_us;

    for (uint32_t output_ix = 0; output_ix < block_config->output_tensors_size; output_ix++) {
        TfLiteTensor* output = outputs[output_ix];
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
                if (block_config->dequantize_output) {
                    result->_raw_outputs[learn_block_index + output_ix].matrix = new matrix_t(1, output_size);
                    fill_output_matrix_from_tensor(output, result->_raw_outputs[learn_block_index + output_ix].matrix);
                }
                else {
                    result->_raw_outputs[learn_block_index + output_ix].matrix_i8 = new matrix_i8_t(1, output_size);
                    memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_i8->buffer, output->data.int8, output->bytes);
                }
                break;
            }
            case kTfLiteUInt8: {
                if (block_config->dequantize_output) {
                    result->_raw_outputs[learn_block_index + output_ix].matrix = new matrix_t(1, output_size);
                    fill_output_matrix_from_tensor(output, result->_raw_outputs[learn_block_index + output_ix].matrix);
                }
                else {
                    result->_raw_outputs[learn_block_index + output_ix].matrix_u8 = new matrix_u8_t(1, output_size);
                    memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_u8->buffer, output->data.uint8, output->bytes);
                }
                break;
            }
            default: {
                ei_printf("ERR: Cannot handle output type (%d)\n", output->type);
                return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
            }
        }

        result->_raw_outputs[learn_block_index + output_ix].blockId = block_config->block_id + output_ix;
    }

    EI_LOGD("Predictions (time: %d ms.):\n", result->timing.classification);

    ei_free(outputs);

    // on Linux we're not worried about free'ing (for now)

    return EI_IMPULSE_OK;
}

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
        .compiled = 1,
        .graph_config = &ei_config_tflite_graph_0
    };

    auto x = run_nn_inference_from_dsp(&ei_learning_block_config, signal, output_matrix);
    if (x != 0) {
        return x;
    }

    return EIDSP_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_FULL_H_
