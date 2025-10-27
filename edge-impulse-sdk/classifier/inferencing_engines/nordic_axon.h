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
// #include "model-parameters/model_variables.h"
#include <string>
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "axon_platform.h"
#include "axon_driver.h"
#include "axon_nn_infer.h"
#include "axon_nn_infer_test.h"
#include "axon_stringization.h"

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

// #define THE_TEST_NAME_ROOT base_inference_

// const axon_nn_compiled_model_struct *the_full_model_static_info;
// const axon_nn_compiled_model_struct **the_model_layers_static_info = NULL;
// uint16_t model_layers_count = 0;
// axon_nn_model_test_info_struct the_test_vectors = {0};

#include AXON_MODEL_FILE_NAME

// int AxonnnModelPrepare() {
//   the_full_model_static_info = &THE_MODEL_STRUCT_NAME(AXON_MODEL_NAME);
// #if INCLUDE_VECTORS
//   axon_nn_populate_model_test_info_struct(
//     &the_test_vectors, // structure to populate
//     STRINGIZE_2_CONCAT(THE_TEST_NAME_ROOT, AXON_MODEL_NAME),
//     (const int8_t**)THE_TEST_INPUT_VECTORS_LIST_NAME(AXON_MODEL_NAME),  // test vectors for layer 1 (full model)
//     (const int8_t**)THE_expected_output_vectors_NAME(AXON_MODEL_NAME), // expected output vectors, one for each input vector (full model)
//     sizeof(THE_expected_output_vectors_NAME(AXON_MODEL_NAME))/sizeof(*THE_expected_output_vectors_NAME(AXON_MODEL_NAME)), // number of full model test/expected_output vector pairs.
//     (const int8_t**)THE_layer_vectors_NAME(AXON_MODEL_NAME),  // individual layer outputs. for each n, layer_models[n] input is layer_vectors[n-1] (except n=0, input is full_model_input_vectors[0]), expected output is layer_vectors[n]
//     sizeof(THE_layer_vectors_NAME(AXON_MODEL_NAME))/sizeof(*THE_layer_vectors_NAME(AXON_MODEL_NAME)), // number of elements in layer_vectors
//     sizeof(**THE_expected_output_vectors_NAME(AXON_MODEL_NAME)));
// # if AXON_LAYER_TEST_VECTORS
//   the_model_layers_static_info = THE_MODEL_LAYERS_STRUCT_NAME(AXON_MODEL_NAME);
//   model_layers_count = sizeof(THE_MODEL_LAYERS_STRUCT_NAME(AXON_MODEL_NAME)) / sizeof(*THE_MODEL_LAYERS_STRUCT_NAME(AXON_MODEL_NAME));
// # endif
// #endif

//   return 0;
// }

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
    // ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;
    ei_printf("Running %s device opened.\n", "Nordic Axon");

    ei_printf("Start Platform!\n");

    uint64_t ctx_start_us = ei_read_timer_us();

    AxonResultEnum result_axon = axon_platform_init();

    if (result_axon != kAxonResultSuccess) {
        ei_printf("axon_platform_init failed!\n");
    }
    void *axon_handle = axon_driver_get_handle();

    ei_printf("Prepare and run Axon!\n");

    axon_nn_model_inference_wrapper_struct model_wrapper;
    model_static_info = &THE_MODEL_STRUCT_NAME(AXON_MODEL_NAME); // your compiled model info

    int init_result = axon_nn_model_init(&model_wrapper, model_static_info);
    if (init_result != 0) {
        ei_printf("axon_nn_model_init failed: %d\n", init_result);
    }

    axon_nn_model_init_vars(&model_wrapper);

    const int8_t* input_vector = (const int8_t*)fmatrix->matrix->buffer; // transpose data to (channel, height, width)
    uint32_t vector_size = fmatrix->matrix->rows * fmatrix->matrix->cols;

    AxonResultEnum result_axon_infer = axon_nn_model_infer_sync(
        axon_handle, // your hardware handle
        model_static_info,
        &model_wrapper.cmd_buf_info,
        input_vector,
        vector_size
    );

    if (result_axon_infer != kAxonResultSuccess) {
        printf("Inference failed!\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    const char *label;
    int32_t score;
    int16_t class_idx = axon_nn_get_classification(&model_wrapper, &label, &score, NULL);

    if (class_idx >= 0) {
        printf("Predicted class: %d\n", class_idx);
        printf("Label: %s\n", label);
        printf("Score: %d\n", score);
    } else {
        printf("Classification failed!\n");
    }

    axon_platform_close();

    return EI_IMPULSE_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_ETHOS_LINUX)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_ETHOS_LINUX_H_
