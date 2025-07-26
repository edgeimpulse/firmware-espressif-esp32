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

 #ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_ATON_H

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_ATON)

/* Include ----------------------------------------------------------------- */
#include "edge-impulse-sdk/tensorflow/lite/kernels/custom/tree_ensemble_classifier.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/ei_run_dsp.h"
#include "edge-impulse-sdk/porting/ei_logging.h"

#include "ll_aton_runtime.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Private variables ------------------------------------------------------- */
static uint8_t *nn_in;
static uint8_t *nn_out;

static const LL_Buffer_InfoTypeDef *nn_in_info;
static const LL_Buffer_InfoTypeDef *nn_out_info;

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default);


EI_IMPULSE_ERROR run_nn_inference_image_quantized(
    const ei_impulse_t *impulse,
    signal_t *signal,
    uint32_t learn_block_index,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;
    ei_config_aton_graph_t *graph_config = (ei_config_aton_graph_t*)block_config->graph_config;

    extern uint8_t *global_camera_buffer;
    extern uint8_t *snapshot_buf;
    // this needs to be changed for multi-model, multi-impulse
    static bool first_run = true;

    uint64_t ctx_start_us = ei_read_timer_us();

    #if DATA_OUT_FORMAT_FLOAT32
    static float32_t *nn_out;
    #else
    static uint8_t *nn_out;
    #endif
    static uint32_t nn_out_len;

    if(first_run == true) {

        nn_in_info = LL_ATON_Input_Buffers_Info_Default();
        nn_out_info = LL_ATON_Output_Buffers_Info_Default();

        nn_in = (uint8_t *) LL_Buffer_addr_start(&nn_in_info[0]);
        uint32_t nn_in_len = LL_Buffer_len(&nn_in_info[0]);

        #if DATA_OUT_FORMAT_FLOAT32
        nn_out = (float32_t *) nn_out_info[0].addr_base.p;
        #else
        nn_out = (uint8_t *) LL_Buffer_addr_start(&nn_out_info[0]);
        #endif
        nn_out_len = LL_Buffer_len(&nn_out_info[0]);

        first_run = false;
    }

    memcpy(nn_in, snapshot_buf, impulse->input_width * impulse->input_height * 3);
    #ifdef USE_DCACHE
    SCB_CleanInvalidateDCache_by_Addr(nn_in, impulse->input_width * impulse->input_height * 3);
    #endif

    LL_ATON_RT_Main(&NN_Instance_Default);

    /* Discard all nn_out regions to avoid Dcache evictions during nn inference */
    #ifdef USE_DCACHE
    int i = 0;
    while (nn_out_info[i].name != NULL) {
            SCB_InvalidateDCache_by_Addr((float32_t *) LL_Buffer_addr_start(&nn_out_info[i]), LL_Buffer_len(&nn_out_info[i]));
            i++;
    }
    #endif

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    size_t output_size = nn_out_len;

    result->_raw_outputs[learn_block_index].matrix = new matrix_t(1, output_size);
    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;

    switch (graph_config->quant_type) {
        case kTfLiteFloat32: {
            result->_raw_outputs[learn_block_index].matrix = new matrix_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix->buffer, (float *)nn_out, output_size * sizeof(float));
            break;
        }
        case kTfLiteInt8: {
            result->_raw_outputs[learn_block_index].matrix_i8 = new matrix_i8_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix_i8->buffer, (int8_t *)nn_out, output_size * sizeof(int8_t));
            break;
        }
        case kTfLiteUInt8: {
            result->_raw_outputs[learn_block_index].matrix_u8 = new matrix_u8_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix_u8->buffer, (uint8_t *)nn_out, output_size * sizeof(uint8_t));
            break;
        }
        default: {
            ei_printf("ERR: Cannot handle output type (%d)\n", graph_config->quant_type);
            return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
        }
    }

    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;

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


    return EI_IMPULSE_OK;
}

#ifdef __cplusplus
}
#endif

#endif // EI_CLASSIFIER_INFERENCING_ENGINE
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_ATON_H
