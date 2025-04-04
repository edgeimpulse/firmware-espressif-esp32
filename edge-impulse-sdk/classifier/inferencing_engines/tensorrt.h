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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_TENSORRT_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_TENSORRT_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSORRT)

#if (EI_CLASSIFIER_HAS_ANOMALY == EI_ANOMALY_TYPE_VISUAL_GMM)

#include <thread>
#include "tensorflow-lite/tensorflow/lite/c/common.h"
#include "tensorflow-lite/tensorflow/lite/interpreter.h"
#include "tensorflow-lite/tensorflow/lite/kernels/register.h"
#include "tensorflow-lite/tensorflow/lite/model.h"
#include "tensorflow-lite/tensorflow/lite/optional_debug_tools.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/custom/tree_ensemble_classifier.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_helper.h"

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

EI_IMPULSE_ERROR run_nn_inference_tflite_full(
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

    TfLiteTensor *input = interpreter->input_tensor(0);
    TfLiteTensor *output = interpreter->output_tensor(block_config->output_data_tensor);

    if (!input) {
        return EI_IMPULSE_INPUT_TENSOR_WAS_NULL;
    }
    if (!output) {
        return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
    }

    size_t mtx_size = impulse->dsp_blocks_size + impulse->learning_blocks_size;
    auto input_res = fill_input_tensor_from_matrix(fmatrix, input, input_block_ids, input_block_ids_size, mtx_size);
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

    if (result->copy_output) {
        auto output_res = fill_output_matrix_from_tensor(output, fmatrix[impulse->dsp_blocks_size + learn_block_index].matrix);
        if (output_res != EI_IMPULSE_OK) {
            return output_res;
        }
    }

    if (debug) {
        ei_printf("Predictions (time: %d ms.):\n", result->timing.classification);
    }

    TfLiteTensor *scores_tensor = interpreter->output_tensor(block_config->output_score_tensor);
    TfLiteTensor *labels_tensor = interpreter->output_tensor(block_config->output_labels_tensor);

    EI_IMPULSE_ERROR fill_res = fill_result_struct_from_output_tensor_tflite(
        impulse, block_config, output, labels_tensor, scores_tensor, result, debug);

    if (fill_res != EI_IMPULSE_OK) {
        return fill_res;
    }

    // on Linux we're not worried about free'ing (for now)

    return EI_IMPULSE_OK;
}
#endif // (EI_CLASSIFIER_HAS_ANOMALY == EI_ANOMALY_TYPE_VISUAL_GMM)

#include "model-parameters/model_metadata.h"

#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "edge-impulse-sdk/classifier/ei_fill_result_struct.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <filesystem>
#include <stdlib.h>
#include <map>
#include "tflite/linux-jetson-nano/libeitrt.h"

#if __APPLE__
#include <mach-o/dyld.h>
#else
#include <linux/limits.h>
#endif

EiTrt* ei_trt_handle;
std::map<int,bool> ei_trt_models_init;

inline bool file_exists(char *model_file_name)
{
    if (FILE *file = fopen(model_file_name, "r")) {
        fclose(file);
        return true;
    }
    else {
        return false;
    }
}

EI_IMPULSE_ERROR write_model_to_file(
    const ei_impulse_t *impulse,
    uint32_t learn_block_index,
    char *model_file_name,
    const unsigned char *model,
    size_t model_size,
    bool debug = false)
{
    static char current_exe_path[PATH_MAX] = { 0 };

#if __APPLE__
    uint32_t len = PATH_MAX;
    if (_NSGetExecutablePath(current_exe_path, &len) != 0) {
        current_exe_path[0] = '\0'; // buffer too small
    }
    else {
        // resolve symlinks, ., .. if possible
        char *canonical_path = realpath(current_exe_path, NULL);
        if (canonical_path != NULL)
        {
            strncpy(current_exe_path, canonical_path, len);
            free(canonical_path);
        }
    }
#else
    int readlink_res = readlink("/proc/self/exe", current_exe_path, PATH_MAX);
    if (readlink_res < 0) {
        printf("readlink_res = %d\n", readlink_res);
        current_exe_path[0] = '\0'; // failed to find location
    }
#endif

    if (strlen(current_exe_path) == 0) {
        // could not determine current exe path, use /tmp for the engine file
        snprintf(
            model_file_name,
            PATH_MAX,
            "/tmp/ei-%d-%d-%d.engine",
            impulse->project_id,
            impulse->deploy_version,
            impulse->learning_blocks[learn_block_index].blockId);
    }
    else {
        std::filesystem::path p(current_exe_path);
        snprintf(
            model_file_name,
            PATH_MAX,
            "%s/%s-project%d-v%d-%d.engine",
            p.parent_path().c_str(),
            p.stem().c_str(),
            impulse->project_id,
            impulse->deploy_version,
            impulse->learning_blocks[learn_block_index].blockId);
    }

    bool fexists = file_exists(model_file_name);
    if (!fexists) {
        ei_printf("INFO: Model file '%s' does not exist, creating...\n", model_file_name);

        FILE *file = fopen(model_file_name, "w");
        if (!file) {
            ei_printf("ERR: TensorRT init failed to open '%s'\n", model_file_name);
            return EI_IMPULSE_TENSORRT_INIT_FAILED;
        }

        if (fwrite(model, model_size, 1, file) != 1) {
            ei_printf("ERR: TensorRT init fwrite failed.\n");
            return EI_IMPULSE_TENSORRT_INIT_FAILED;
        }

        if (fclose(file) != 0) {
            ei_printf("ERR: TensorRT init fclose failed.\n");
            return EI_IMPULSE_TENSORRT_INIT_FAILED;
        }
    }
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
    ei_config_tflite_graph_t *graph_config = (ei_config_tflite_graph_t*)block_config->graph_config;

#if (EI_CLASSIFIER_HAS_ANOMALY == EI_ANOMALY_TYPE_VISUAL_GMM)
    if (block_config->classification_mode == EI_CLASSIFIER_CLASSIFICATION_MODE_VISUAL_ANOMALY
       && !result->copy_output) {
        return run_nn_inference_tflite_full(impulse, fmatrix, learn_block_index, input_block_ids, input_block_ids_size, result, config_ptr);
    }
#endif

    #if EI_CLASSIFIER_QUANTIZATION_ENABLED == 1
    #error "TensorRT requires an unquantized network"
    #endif

    static char model_file_name[PATH_MAX];
    // writes the model file to filesystem (if and only it doesn't exist)
    write_model_to_file(impulse, learn_block_index, model_file_name, graph_config->model, graph_config->model_size);

    // create context for building and executing TensorRT engine(s)
    if (ei_trt_handle == nullptr) {
        ei_trt_handle = libeitrt::create_EiTrt(debug);
        libeitrt::setMaxWorkspaceSize(ei_trt_handle, 1<<29); // 512 MB

        if (debug) {
           ei_printf("Using EI TensorRT lib v%d.%d.%d\r\n", libeitrt::getMajorVersion(ei_trt_handle),
                     libeitrt::getMinorVersion(ei_trt_handle), libeitrt::getPatchVersion(ei_trt_handle));
        }
    }

    // lazy initialize TensorRT models and warmup only once per model
    if (ei_trt_models_init.count(learn_block_index) <= 0) {
        libeitrt::build(ei_trt_handle, learn_block_index, model_file_name);
        libeitrt::warmUp(ei_trt_handle, learn_block_index, 200);
        ei_trt_models_init[learn_block_index] = true;
    }

    int input_size = libeitrt::getInputSize(ei_trt_handle, learn_block_index);
    int output_size = libeitrt::getOutputSize(ei_trt_handle, learn_block_index);

#if EI_CLASSIFIER_SINGLE_FEATURE_INPUT == 0
    size_t mtx_size = impulse->dsp_blocks_size + impulse->learning_blocks_size;
    ei::matrix_t* matrix = NULL;

    size_t combined_matrix_size = get_feature_size(fmatrix, input_block_ids_size, input_block_ids, mtx_size);
    if ((input_size >= 0) && ((size_t)input_size != combined_matrix_size)) {
        ei_printf("ERR: Invalid input features size, %ld given (expected: %d)\n", combined_matrix_size, input_size);
        return EI_IMPULSE_INVALID_SIZE;
    }
    ei::matrix_t combined_matrix(1, combined_matrix_size);

    uint32_t buf_pos = 0;
    for (size_t i = 0; i < input_block_ids_size; i++) {
        size_t cur_mtx = input_block_ids[i];

        if (!find_mtx_by_idx(fmatrix, &matrix, cur_mtx, mtx_size)) {
            ei_printf("ERR: Cannot find matrix with id %zu\n", cur_mtx);
            return EI_IMPULSE_INVALID_SIZE;
        }

        for (size_t ix = 0; ix < matrix->rows * matrix->cols; ix++) {
            combined_matrix.buffer[buf_pos++] = matrix->buffer[ix];
        }
    }
    matrix = &combined_matrix;
#else
    ei::matrix_t* matrix = fmatrix[0].matrix;
#endif

    // copy input data to gpu
    libeitrt::copyInputToDevice(ei_trt_handle, learn_block_index, matrix->buffer,
                                input_size * sizeof(float));

    libeitrt::infer(ei_trt_handle, learn_block_index);

    float *out_data = (float*)ei_malloc(output_size * sizeof(float));
    if (out_data == nullptr) {
        ei_printf("ERR: Cannot allocate memory for output data \n");
        return EI_IMPULSE_ALLOC_FAILED;
    }

    // copy output data from gpu
    libeitrt::copyOutputToHost(ei_trt_handle, learn_block_index, out_data,
                               output_size * sizeof(float));


    // get inference time
    result->timing.classification_us = libeitrt::getInferenceUs(ei_trt_handle, learn_block_index);
    result->timing.classification = (int)(result->timing.classification_us / 1000);

    if (result->copy_output) {
        matrix_t *output_matrix = fmatrix[impulse->dsp_blocks_size + learn_block_index].matrix;
        const size_t matrix_els = output_matrix->rows * output_matrix->cols;

        if ((output_size >= 0) && ((size_t)output_size != matrix_els)) {
                ei_printf("ERR: output tensor has size %d, but input matrix has size %d\n",
                    output_size, (int)matrix_els);
                ei_free(out_data);
                return EI_IMPULSE_INVALID_SIZE;
        }
        memcpy(output_matrix->buffer, out_data, output_size * sizeof(float));
        ei_free(out_data);
        return EI_IMPULSE_OK;
    }

    EI_IMPULSE_ERROR fill_res = EI_IMPULSE_OK;

    if (block_config->object_detection) {
        switch (block_config->object_detection_last_layer) {
            case EI_CLASSIFIER_LAST_LAYER_FOMO: {
                fill_res = fill_result_struct_f32_fomo(
                    impulse,
                    block_config,
                    result,
                    out_data,
                    impulse->fomo_output_size,
                    impulse->fomo_output_size);
                break;
            }
            case EI_CLASSIFIER_LAST_LAYER_YOLOV5:
            case EI_CLASSIFIER_LAST_LAYER_YOLOV5_V5_DRPAI: {
                int version = block_config->object_detection_last_layer == EI_CLASSIFIER_LAST_LAYER_YOLOV5_V5_DRPAI ?
                    5 : 6;
                fill_res = fill_result_struct_f32_yolov5(
                    impulse,
                    block_config,
                    result,
                    version,
                    out_data,
                    impulse->tflite_output_features_count);
                break;
            }
            case EI_CLASSIFIER_LAST_LAYER_TAO_SSD:
            case EI_CLASSIFIER_LAST_LAYER_TAO_RETINANET: {
                fill_res = fill_result_struct_f32_tao_decode_detections(
                    impulse,
                    block_config,
                    result,
                    out_data,
                    impulse->tflite_output_features_count,
                    debug);
                break;
            }
            case EI_CLASSIFIER_LAST_LAYER_TAO_YOLOV3:
                fill_res = fill_result_struct_f32_tao_yolov3(
                    impulse,
                    block_config,
                    result,
                    out_data,
                    impulse->tflite_output_features_count,
                    debug);
                break;
            case EI_CLASSIFIER_LAST_LAYER_TAO_YOLOV4: {
                fill_res = fill_result_struct_f32_tao_yolov4(
                    impulse,
                    block_config,
                    result,
                    out_data,
                    impulse->tflite_output_features_count,
                    debug);
                break;
            }
            case EI_CLASSIFIER_LAST_LAYER_YOLO_PRO: {
                fill_res = fill_result_struct_f32_yolo_pro(
                    impulse,
                    block_config,
                    result,
                    out_data,
                    impulse->tflite_output_features_count,
                    debug);
                break;
            }
            default: {
                ei_printf(
                    "ERR: Unsupported object detection last layer (%d)\n",
                    block_config->object_detection_last_layer);
                return EI_IMPULSE_UNSUPPORTED_INFERENCING_ENGINE;
            }
        }
    }
    else if (block_config->classification_mode == EI_CLASSIFIER_CLASSIFICATION_MODE_VISUAL_ANOMALY) {
        if (!result->copy_output) {
            fill_res = fill_result_visual_ad_struct_f32(impulse, result, out_data, block_config, debug);
        }
    }
    // if we copy the output, we don't need to process it as classification
    else {
        if (!result->copy_output) {
            fill_res = fill_result_struct_f32(impulse, result, out_data, debug);
        }
    }

    ei_free(out_data);

    if (fill_res != EI_IMPULSE_OK) {
        return fill_res;
    }

    return EI_IMPULSE_OK;
}

/**
 * Special function to run the classifier on images for quantized models
 * that allocates a lot less memory by quantizing in place. This only works if 'can_run_classifier_image_quantized'
 * returns EI_IMPULSE_OK.
 */
EI_IMPULSE_ERROR run_nn_inference_image_quantized(
    const ei_impulse_t *impulse,
    signal_t *signal,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    return EI_IMPULSE_UNSUPPORTED_INFERENCING_ENGINE;
}


#endif // #if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSORRT)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_TENSORRT_H_
