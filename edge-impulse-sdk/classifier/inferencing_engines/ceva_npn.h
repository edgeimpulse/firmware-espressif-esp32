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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_CEVA_NPN_H
#define _EI_CLASSIFIER_INFERENCING_ENGINE_CEVA_NPN_H

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_CEVA_NPN)

#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include <ceva_neupro_nano_tflm_api.h>

static bool init_done = false;
CEVA_MODEL_STATE model_state;

static EI_IMPULSE_ERROR init_ceva_npn(ei_learning_block_config_tflite_graph_t *block_config,
    ei_unique_ptr_t& p_tensor_arena,
    int8_t** input_data,
	int8_t** output_data,
    bool debug)
{
    ei_config_ceva_npn_graph_t *graph_config = (ei_config_ceva_npn_graph_t*)block_config->graph_config;
    int output_len = 0;

#ifdef EI_CLASSIFIER_ALLOCATION_STATIC
    // Assign a no-op lambda to the "free" function in case of static arena
    static uint8_t tensor_arena[EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE] ALIGN(16) DEFINE_SECTION(STRINGIZE_VALUE_OF(EI_TENSOR_ARENA_LOCATION));
    p_tensor_arena = ei_unique_ptr_t(tensor_arena, [](void*){});
#else
    // Create an area of memory to use for input, output, and intermediate arrays.
    uint8_t *tensor_arena = (uint8_t*)ei_aligned_calloc(16, graph_config->arena_size);
    if (tensor_arena == NULL) {
        EI_LOGE("Failed to allocate TFLite arena (%zu bytes)\n", graph_config->arena_size);
        return EI_IMPULSE_TFLITE_ARENA_ALLOC_FAILED;
    }
    p_tensor_arena = ei_unique_ptr_t(tensor_arena, ei_aligned_free);
#endif

    // not required since 24.1.4, left for possible fallback to 24.1.3
    // ceva_init_model_state(&model_state);

    //ceva_neupro_nano_tflm_model_init is a TFLM wrapper function for ease of use
	//setup model will populate output_len with the expected output size
	//model init can take a while, it is normal
    ceva_neupro_nano_tflm_model_init((char*)graph_config->model,
                                    &output_len,
                                    static_cast<uint8_t*>(p_tensor_arena.get()),
                                    graph_config->arena_size,
                                    input_data,
                                    output_data,
                                    &model_state);

    init_done = true;
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
    bool debug)
{
    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;
    ei_config_ceva_npn_graph_t *graph_config = (ei_config_ceva_npn_graph_t*)block_config->graph_config;

    ei_unique_ptr_t p_tensor_arena(nullptr, ei_aligned_free);
    int8_t* input_data;
	int8_t* output_data;
    ei::matrix_t* matrix;

    if (!init_done) {
        if(init_ceva_npn(block_config, p_tensor_arena, &input_data, &output_data, debug) != EI_IMPULSE_OK) {
            return EI_IMPULSE_DEVICE_INIT_ERROR;
        }
    }

    // get the matrix with input features
    for (size_t i = 0; i < input_block_ids_size; i++) {
#if EI_CLASSIFIER_SINGLE_FEATURE_INPUT == 0
        uint16_t cur_mtx = input_block_ids[i];
        matrix = NULL;

        if (!find_mtx_by_idx(fmatrix, &matrix, cur_mtx, impulse->dsp_blocks_size + impulse->learning_blocks_size)) {
            EI_LOGE("Cannot find matrix with id %zu\n", cur_mtx);
            return EI_IMPULSE_INVALID_SIZE;
        }
#else
        matrix = fmatrix[0].matrix;
#endif
    }

    // copy rescale the input features to int8 and copy to input buffer
    for (size_t i = 0; i < matrix->rows * matrix->cols; i++) {
        //TODO: get scale and zero point from the model
        input_data[i] = (int8_t)((matrix->buffer[i] / graph_config->input_scale) + graph_config->input_zeropoint);
    }
    // get start time of the inference
    uint64_t ctx_start_us = ei_read_timer_us();

    // not required since 24.1.4, left for possible fallback to 24.1.3
    // ceva_set_model_state_for_inference(&model_state);

    //invoke inference using TFLM wrapper function
	ceva_neupro_nano_tflm_model_invoke(&model_state);
    // get end time of the inference
    uint64_t ctx_end_us = ei_read_timer_us();

    // calculate inference time
    result->timing.classification_us = ctx_end_us - ctx_start_us;
    result->timing.classification = (int)(result->timing.classification_us / 1000);

    size_t output_size = graph_config->output_features_count;

    result->_raw_outputs[learn_block_index].matrix_i8 = new matrix_i8_t(1, output_size);
    memcpy(result->_raw_outputs[learn_block_index].matrix_i8->buffer, output_data, output_size * sizeof(int8_t));

    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;

    return EI_IMPULSE_OK;
}


#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_CEVA_NPN)

#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_CEVA_NPN_H