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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_NORDIC_AXON_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_NORDIC_AXON_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_NORDIC_AXON)
// check if the model is quantized on the build stage to fail quickly
#if EI_CLASSIFIER_QUANTIZATION_ENABLED == 0
#error "Nordic axon does not support float32 models"
#endif

#include "model-parameters/model_metadata.h"
#include <string>
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "axon_platform.h"
#include "axon_driver.h"
#include "axon_nn_infer.h"
#include "axon_nn_infer_test.h"
#include "axon_stringization.h"
#include <inttypes.h>


/*
* Create the model include header file name and structure from
* the model name.
*/
#define AXON_MODEL_FILE_NAME_ROOT axon_model_
#define AXON_MODEL_LAYERS_FILE_NAME_ROOT AXON_MODEL_FILE_NAME_ROOT
#define AXON_MODEL_TEST_VECTORS_FILE_NAME_ROOT AXON_MODEL_FILE_NAME_ROOT
#define AXON_MODEL_TEST_VECTORS_FILE_NAME_END _test_vectors_.h
#define AXON_MODEL_LAYERS_FILE_NAME_TAIL _layers_.h
#define AXON_MODEL_DOT_H _.h

#define AXON_MODEL_FILE_NAME STRINGIZE_3_CONCAT(AXON_MODEL_FILE_NAME_ROOT, AXON_MODEL_NAME, AXON_MODEL_DOT_H)
#define AXON_MODEL_FILE_LAYERS_NAME STRINGIZE_3_CONCAT(AXON_MODEL_LAYERS_FILE_NAME_ROOT, AXON_MODEL_NAME, AXON_MODEL_LAYERS_FILE_NAME_TAIL)
#define AXON_MODEL_TEST_VECTORS_FILE_NAME STRINGIZE_3_CONCAT(AXON_MODEL_TEST_VECTORS_FILE_NAME_ROOT, AXON_MODEL_NAME, AXON_MODEL_TEST_VECTORS_FILE_NAME_END)

// // generate structure name model_<model_name>
#define THE_REAL_MODEL_STRUCT_NAME(model_name) model_##model_name
#define THE_MODEL_STRUCT_NAME(model_name) THE_REAL_MODEL_STRUCT_NAME(model_name)

// // generate structure name model_<model_name>_layer_list
#define THE_REAL_MODEL_LAYERS_STRUCT_NAME(model_name) model_##model_name##_layer_list
#define THE_MODEL_LAYERS_STRUCT_NAME(model_name) THE_REAL_MODEL_LAYERS_STRUCT_NAME(model_name)

// // generate structure name model_<model_name>_test_vectors
#define THE_REAL_MODEL_TEST_VECTORS_STRUCT_NAME(model_name) model_##model_name##_test_vectors
#define THE_MODEL_TEST_VECTORS_STRUCT_NAME(model_name) THE_REAL_MODEL_TEST_VECTORS_STRUCT_NAME(model_name)

// generate structure name <model_name>_input_test_vectors
#define THE_REAL_TEST_INPUT_VECTORS_LIST_NAME(model_name) model_name##_input_test_vectors
#define THE_TEST_INPUT_VECTORS_LIST_NAME(model_name) THE_REAL_TEST_INPUT_VECTORS_LIST_NAME(model_name)

// generate structure name <model_name>_expected_output_test_vectors
#define THE_REAL_expected_output_vectors_NAME(model_name) model_name##_expected_output_vectors
#define THE_expected_output_vectors_NAME(model_name) THE_REAL_expected_output_vectors_NAME(model_name)

// generate structure name <model_name>_layer_vectors
#define THE_REAL_layer_vectors_NAME(model_name) model_name##_layer_expected_output_vectors
#define THE_layer_vectors_NAME(model_name) THE_REAL_layer_vectors_NAME(model_name)

#include AXON_MODEL_FILE_NAME

const axon_nn_compiled_model_struct *model_static_info;

EI_IMPULSE_ERROR run_nn_inference(
    const ei_impulse_t *impulse,
    ei_feature_t *fmatrix,
    uint32_t learn_block_index,
    uint32_t* input_block_ids,
    uint32_t input_block_ids_size,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug)
{
    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;
    ei_config_nordic_axon_graph_t *graph_config = (ei_config_nordic_axon_graph_t*)block_config->graph_config;
    ei::matrix_t* matrix = fmatrix[0].matrix;

    if (debug) {
        ei_printf("INFO: Running %s device opened.\n", "Nordic Axon");
        ei_printf("INFO: Start Platform!\n");
    }

    AxonResultEnum result_axon = axon_platform_init();

    if (result_axon != kAxonResultSuccess) {
        ei_printf("ERR: axon_platform_init failed!\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }
    void *axon_handle = axon_driver_get_handle();

    if (debug) {
        ei_printf("INFO: Prepare and run Axon!\n");
    }

    axon_nn_model_inference_wrapper_struct model_wrapper;
    model_static_info = &THE_MODEL_STRUCT_NAME(AXON_MODEL_NAME);

    int init_result = axon_nn_model_init(&model_wrapper, model_static_info);
    if (init_result != 0) {
        ei_printf("ERR: axon_nn_model_init failed: %d\n", init_result);
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    axon_nn_model_init_vars(&model_wrapper);

    uint32_t vector_size = model_static_info->inputs[0].dimensions.height * model_static_info->inputs[0].dimensions.width;
    int8_t input_vector[vector_size];

    uint64_t ctx_start_us = ei_read_timer_us();

    // copy rescale the input features to int8 and copy to input buffer
    for (size_t i = 0; i < matrix->rows * matrix->cols; i++) {
        //TODO: get scale and zero point from the model
        input_vector[i] = (int8_t)((matrix->buffer[i] / graph_config->input_scale) + graph_config->input_zeropoint);
    }

    AxonResultEnum result_axon_infer = axon_nn_model_infer_sync(
        axon_handle, // your hardware handle
        model_static_info,
        &model_wrapper.cmd_buf_info,
        input_vector,
        vector_size
    );

    if (result_axon_infer != kAxonResultSuccess) {
        printf("ERR: Inference failed!\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    // ei_sleep(3); // let the axon finish processing
    result->timing.classification_us = (int64_t)(ei_read_timer_us() - ctx_start_us);

    const char *label;
    int32_t score;
    int16_t class_idx = axon_nn_get_classification(&model_wrapper, NULL, &label, &score, NULL);

    if (class_idx >= 0) {
        if (debug) {
            ei_printf("INFO: Axon inference successful.\n");
            printf("INFO: Predicted class: %d\n", class_idx);
            printf("INFO: Label: %s\n", label);
            printf("INFO: Score: %d\n", score);
        }
        // here channel is always one and byte width is always 1 for quantized models
        uint32_t output_size = model_static_info->output_dimensions.height * model_static_info->output_dimensions.width;
        result->_raw_outputs[learn_block_index + 0].matrix_i8 = new matrix_i8_t(1, output_size);
        memcpy(result->_raw_outputs[learn_block_index + 0].matrix_i8->buffer, (int8_t *)model_static_info->output_ptr, output_size * sizeof(int8_t));
        result->_raw_outputs[learn_block_index].blockId = block_config->block_id;
    } else {
        printf("ERR: axon Classification failed!\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    axon_platform_close();

    return EI_IMPULSE_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_ETHOS_LINUX)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_ETHOS_LINUX_H_
