/* The Clear BSD License
 *
 * Copyright (c) 2026 EdgeImpulse Inc.
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
#include "axon/nrf_axon_platform.h"
#include "drivers/axon/nrf_axon_driver.h"
#include "drivers/axon/nrf_axon_nn_infer.h"
#include "drivers/axon/nrf_axon_nn_infer_test.h"
#include "axon/nrf_axon_stringization.h"
#include <inttypes.h>

#if !defined (NRF_AXON_MODEL_NAME)
#error "NRF_AXON_MODEL_NAME is not defined. Please define NRF_AXON_MODEL_NAME"
#endif

/*
* Create the model include header file name and structure from
* the model name.
*/
#define AXON_MODEL_FILE_NAME_ROOT nrf_axon_model_

#define AXON_MODEL_DOT_H _.h

#define AXON_MODEL_FILE_NAME STRINGIZE_3_CONCAT(AXON_MODEL_FILE_NAME_ROOT, NRF_AXON_MODEL_NAME, AXON_MODEL_DOT_H)

#define THE_REAL_MODEL_STRUCT_NAME(model_name) model_##model_name
#define THE_MODEL_STRUCT_NAME(model_name) THE_REAL_MODEL_STRUCT_NAME(model_name)

#define NRF_AXON_MODEL_ALLOCATE_PACKED_OUTPUT_BUFFER  1 // model allocates a dedicated buffer for the output.

#include AXON_MODEL_FILE_NAME

const nrf_axon_nn_compiled_model_s *nrf_axon_compiled_model;

static bool nrf_axon_init = false;

/**
 * This code needs to be run one-time only.
 */
int one_time_init()
{
    nrf_axon_platform_init();
    nrf_axon_result_e result_axon = nrf_axon_platform_init();

    if (result_axon != NRF_AXON_RESULT_SUCCESS){
        ei_printf("ERR: nrf_axon_platform_init failed!\n");
        return EI_IMPULSE_NORDIC_AXON_ERROR;
    }

    nrf_axon_compiled_model = &THE_MODEL_STRUCT_NAME(NRF_AXON_MODEL_NAME);

    result_axon = nrf_axon_nn_model_validate(nrf_axon_compiled_model);
    if (result_axon != NRF_AXON_RESULT_SUCCESS){
        ei_printf("ERR: nrf_axon_nn_model_validate failed!\n");
        return EI_IMPULSE_NORDIC_AXON_ERROR;
    }
    return EI_IMPULSE_OK;
}

int one_time_cleanup()
{
    nrf_axon_platform_close();
    return EI_IMPULSE_OK;
}

/**
 * @brief Performs session initialization
 * 
 * A "session" is when inference is occuring regularlly on continuously streaming input.
 * This function is needed for streaming style models that have internal state variables that need
 * to be initialized at the start of a session.
 */
int streaming_session_init()
{
    return nrf_axon_nn_model_init_vars(nrf_axon_compiled_model);
}

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

    if (nrf_axon_init == false) {
        if (one_time_init() != EI_IMPULSE_OK) {
            return EI_IMPULSE_NORDIC_AXON_ERROR;
        }
        nrf_axon_init = true;
    }

    if (debug) {
        ei_printf("INFO: Prepare and run Axon!\n");
    }

    uint32_t vector_size = nrf_axon_compiled_model->inputs[nrf_axon_compiled_model->external_input_ndx].dimensions.height * nrf_axon_compiled_model->inputs[nrf_axon_compiled_model->external_input_ndx].dimensions.width;
    int8_t input_vector[vector_size];

    // copy rescale the input features to int8 and copy to input buffer
    /**
     * @FIXME!!! CHECK IF MODEL IS TRANSPOSED! THIS INFO IS NOT STORED IN THE MODEL, BUT SHOULD BE!
     */
    for (size_t i = 0; i < matrix->rows * matrix->cols; i++) {
        //TODO: get scale and zero point from the model
        input_vector[i] = (int8_t)((matrix->buffer[i] / graph_config->input_scale) + graph_config->input_zeropoint);
    }

    uint64_t ctx_start_us = ei_read_timer_us();

    nrf_axon_result_e result_axon_infer = nrf_axon_nn_model_infer_sync(
        nrf_axon_compiled_model,
        input_vector,
        nrf_axon_compiled_model->packed_output_buf);

    if (result_axon_infer != NRF_AXON_RESULT_SUCCESS) {
        ei_printf("ERR: Inference failed!\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    // ei_sleep(3); // let the axon finish processing
    result->timing.classification_us = (int64_t)(ei_read_timer_us() - ctx_start_us);
    result->timing.classification = (int)(result->timing.classification_us / 1000);

#if 0
   /**
    * get_classification isn't necessary to validate the result. It only applies to single 
    * channel classification networks
    */
    const char *label;
    int32_t score;
    int16_t class_idx = axon_nn_get_classification(&model_wrapper, NULL, &label, &score, NULL);

    if (class_idx >= 0) {
        if (debug) {
            ei_printf("INFO: Axon inference successful.\n");
            ei_printf("INFO: Predicted class: %d\n", class_idx);
            ei_printf("INFO: Label: %s\n", label);
            ei_printf("INFO: Score: %d\n", score);
        }
        // here channel is always one and byte width is always 1 for quantized models
        uint32_t output_size = nrf_axon_compiled_model->output_dimensions.height * nrf_axon_compiled_model->output_dimensions.width * nrf_axon_compiled_model->output_dimensions.channel_cnt;
        result->_raw_outputs[learn_block_index + 0].matrix_i8 = new matrix_i8_t(1, output_size);
        memcpy(result->_raw_outputs[learn_block_index + 0].matrix_i8->buffer, (int8_t *)nrf_axon_compiled_model->packed_output_buf, output_size * sizeof(int8_t));
        result->_raw_outputs[learn_block_index].blockId = block_config->block_id;
    } else {
        ei_printf("ERR: axon Classification failed!\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }
#else
    // here channel is always one and byte width is always 1 for quantized models
    /**
     * Not sure how output channels will always be 1... fomo models have multiple output channels.
     * Also, is new matrix_i8_t(1, output_size) a memory leak? Or does it get freed elsewhere?
     * If the matrix_i8 was pre-allocated, it could be passed to the infer function directly, saving an extra memcpy.
     */
    uint32_t output_size = nrf_axon_compiled_model->output_dimensions.height * nrf_axon_compiled_model->output_dimensions.width * nrf_axon_compiled_model->output_dimensions.channel_cnt;
    result->_raw_outputs[learn_block_index + 0].matrix_i8 = new matrix_i8_t(1, output_size);
    memcpy(result->_raw_outputs[learn_block_index + 0].matrix_i8->buffer, (int8_t *)nrf_axon_compiled_model->packed_output_buf, output_size * sizeof(int8_t));
    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;
#endif
    
    return EI_IMPULSE_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_ETHOS_LINUX)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_ETHOS_LINUX_H_
