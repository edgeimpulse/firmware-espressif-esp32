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

#include "stai.h"
#include "stai_network.h"
#include "stai_ext.h"
#include "app_config.h"

//LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default);
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(network)
STAI_NETWORK_CONTEXT_DECLARE(network_context, STAI_NETWORK_CONTEXT_SIZE)

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef EI_STAI_MODE
#warning "EI_STAI_MODE not defined, defaulting to STAI_MODE_ASYNC"
#define EI_STAI_MODE STAI_MODE_ASYNC
#endif

/* Private variables ------------------------------------------------------- */
static uint32_t nn_in_len = 0;
static stai_size number_output = 0;

static stai_ptr buffer_in_ptr;
static stai_ptr buffer_out_ptr[STAI_NETWORK_OUT_NUM];
static int32_t nn_out_len[STAI_NETWORK_OUT_NUM] = {0};

//static stai_size buffer_in_len;
//static stai_ptr buffer_in_ptr[STAI_NETWORK_IN_NUM];

//
static uint32_t x;

static bool aton_run_inference(void);
static void aton_neural_network_init(void);

static void _rt_callback(
  void* cb_cookie,
  stai_event_type event_id,
  const void* event_payload)
{
    // Runtime callback, do something
    ei_printf("Event %d received\n", event_id);
}

static void _epoch_callback(
  void* cb_cookie,
  stai_event_type event_id,
  const void* event_payload)
{
    //Epoch callback, do something
    ei_printf("Epoch event %d received\n", event_id);
}

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

    // this needs to be changed for multi-model, multi-impulse
    static bool first_run = true;

    uint64_t ctx_start_us = ei_read_timer_us();

    if (first_run == true) {
        aton_neural_network_init();
        first_run = false;
    }

    signal->get_data(0, impulse->nn_input_frame_size, (float*) buffer_in_ptr);
#ifdef USE_DCACHE
    SCB_CleanInvalidateDCache_by_Addr(buffer_in_ptr, nn_in_len);
#endif

    /* run ATON inference */
    if (aton_run_inference() != true) {
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    /* Discard all buffer_out_ptr regions to avoid Dcache evictions during nn inference */
#ifdef USE_DCACHE
    for (int i = 0; i < number_output; i++) {
        void *tmp = buffer_out_ptr[i];
        SCB_InvalidateDCache_by_Addr(tmp, nn_out_len[i]);
    }
#endif

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    for (int i = 0; i < number_output; i++) {
        size_t output_size = nn_out_len[i];

        switch (graph_config->quant_type) {
            case kTfLiteFloat32: {
                result->_raw_outputs[learn_block_index].matrix = new matrix_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index].matrix->buffer, (float *)buffer_out_ptr[i], output_size * sizeof(float));
                break;
            }
            case kTfLiteInt8: {
                result->_raw_outputs[learn_block_index].matrix_i8 = new matrix_i8_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index].matrix_i8->buffer, (int8_t *)buffer_out_ptr[i], output_size * sizeof(int8_t));
                break;
            }
            case kTfLiteUInt8: {
                result->_raw_outputs[learn_block_index].matrix_u8 = new matrix_u8_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index].matrix_u8->buffer, (uint8_t *)buffer_out_ptr[i], output_size * sizeof(uint8_t));
                break;
            }
            default: {
                ei_printf("ERR: Cannot handle output type (%d)\n", graph_config->quant_type);
                return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
            }
        }

        result->_raw_outputs[learn_block_index].blockId = block_config->block_id;  
    }

    return EI_IMPULSE_OK;
}


/**
 * @brief      Do neural network_context inferencing over the processed feature matrix
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
    ei_config_aton_graph_t *graph_config = (ei_config_aton_graph_t*)block_config->graph_config;

    // this needs to be changed for multi-model, multi-impulse
    static bool first_run = true;

    uint64_t ctx_start_us = ei_read_timer_us();

    if (first_run == true) {
        aton_neural_network_init();
        first_run = false;
    }

    // fill input
    ei::matrix_t* matrix = fmatrix[0].matrix;
    size_t mtx_size = impulse->dsp_blocks_size + impulse->learning_blocks_size;
    for (size_t i = 0; i < input_block_ids_size; i++) {
#if EI_CLASSIFIER_SINGLE_FEATURE_INPUT == 0
        uint16_t cur_mtx = input_block_ids[i];
        ei::matrix_t* matrix = NULL;

        if (!find_mtx_by_idx(fmatrix, &matrix, cur_mtx, mtx_size)) {
            ei_printf("ERR: Cannot find matrix with id %zu\n", cur_mtx);
            return EI_IMPULSE_INVALID_SIZE;
        }
#else
        ei::matrix_t* matrix = fmatrix[0].matrix;
#endif

    }
    // copy rescale the input features to int8 and copy to input buffer
    size_t matrix_els = matrix->rows * matrix->cols;
    for (size_t ix = 0; ix < matrix_els; ix++) {
        //TODO: get scale and zero point from the model
        buffer_in_ptr[ix] = (int8_t)((matrix->buffer[ix] / graph_config->input_scale) + graph_config->input_zeropoint);
    }

#ifdef USE_DCACHE
    SCB_CleanInvalidateDCache_by_Addr(buffer_in_ptr, nn_in_len);
#endif

    /* run ATON inference */
        if (aton_run_inference() != true) {
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    /* Discard all buffer_out_ptr regions to avoid Dcache evictions during nn inference */
#ifdef USE_DCACHE
    for (int i = 0; i < number_output; i++) {
        void *tmp = buffer_out_ptr[i];
        SCB_InvalidateDCache_by_Addr(tmp, nn_out_len[i]);
    }
    //SCB_InvalidateDCache_by_Addr(tmp, nn_out_len);
#endif

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    // fill output
    for (uint32_t output_ix = 0; output_ix < block_config->output_tensors_size; output_ix++) {

        size_t output_size = nn_out_len[output_ix];
        //size_t output_size = nn_out_len;
        switch (graph_config->quant_type) {
            case kTfLiteInt8: {
                    result->_raw_outputs[learn_block_index + output_ix].matrix_i8 = new matrix_i8_t(1, output_size);
                    memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_i8->buffer, (int8_t *)buffer_out_ptr[output_ix], output_size * sizeof(int8_t));
            }
            break;
            case kTfLiteUInt8: {
                result->_raw_outputs[learn_block_index + output_ix].matrix_u8 = new matrix_u8_t(1, output_size);
                memcpy(result->_raw_outputs[learn_block_index + output_ix].matrix_u8->buffer, (uint8_t *)buffer_out_ptr[output_ix], output_size * sizeof(uint8_t));
            }
            break;
            default: {
                ei_printf("ERR: Cannot handle output type (%d)\n", graph_config->quant_type);
                return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
            }
        }

        result->_raw_outputs[learn_block_index + output_ix].blockId = block_config->block_id + output_ix;
    }

    return EI_IMPULSE_OK;
}

static bool aton_run_inference(void)
{
#if (EI_STAI_MODE == STAI_MODE_SYNC)
    stai_return_code res = stai_network_run(network_context, STAI_MODE_SYNC);

    return (res == STAI_SUCCESS);
#elif (EI_STAI_MODE == STAI_MODE_ASYNC)
    stai_return_code ret;

    ret = stai_ext_network_new_inference(network);

    ret = stai_network_run(network_context, STAI_MODE_ASYNC);
    if (STAI_SUCCESS != ret) {
        return false;
    }

    do {
        switch(ret)
        {
        case STAI_RUNNING_WFE:
            stai_ext_wfe();       // Status requested WFE before continuing
            stai_ext_network_run_continue(network_context); // Run epoch block
            break;
        case STAI_RUNNING_NO_WFE:  
            stai_ext_network_run_continue(network_context); // Run epoch block
            break;
        case STAI_DONE:
            break;
        default:
            break;
        }
        ret = stai_ext_network_get_nn_run_status(network_context);
    } while (ret == STAI_RUNNING_WFE || ret == STAI_RUNNING_NO_WFE);

    ret = stai_ext_network_new_inference(network_context);
    assert(ret == STAI_SUCCESS);

    return (ret == STAI_DONE);
#else
    stai_return_code rc;

    stai_ext_network_new_inference(network_context);
    rc = stai_network_run(network_context, EI_STAI_MODE);
    if (STAI_SUCCESS == rc) {
    do {
        switch(rc)
        {
        case STAI_RUNNING_WFE:
            stai_ext_wfe();       // Status requested WFE before continuing
            stai_ext_network_run_continue(network_context); // Run epoch block
            break;
        case STAI_RUNNING_NO_WFE:  
            stai_ext_network_run_continue(network_context); // Run epoch block
            break;
        case STAI_DONE:
            break;
        default:
            break;
        }
        rc = stai_ext_network_get_nn_run_status(network_context);
        } while ((rc != STAI_DONE) && 
            (((rc &STAI_ERROR_GENERIC) & (rc & STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE) & (rc & STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR)) == 0) );
    }

    return (rc == STAI_DONE);
#endif
}

static void aton_neural_network_init(void)
{
    stai_return_code rc;
    stai_network_info info;

    rc = stai_runtime_init();
    assert(rc == STAI_SUCCESS);

    rc = stai_network_init(network_context);
    assert(rc == STAI_SUCCESS);

    rc = stai_network_get_info(network_context, &info);
    assert(rc == STAI_SUCCESS);

    number_output = STAI_NETWORK_OUT_NUM;
    nn_in_len = info.inputs[0].size_bytes;

    rc = stai_network_get_inputs(network_context, &buffer_in_ptr, (stai_size *)&info.n_inputs);
    assert(rc == STAI_SUCCESS);

    rc = stai_network_get_outputs(network_context, buffer_out_ptr, &number_output);
    assert(rc == STAI_SUCCESS);
    for (int i = 0; i < number_output; i++) {
        nn_out_len[i] = info.outputs[i].size_bytes;
    }

#if (EI_STAI_MODE == STAI_MODE_ASYNC)
    //stai_runtime_set_callback(_rt_callback, NULL);
    //stai_network_set_callback(network_context, _epoch_callback, &x);
#endif

}

#ifdef __cplusplus
}
#endif

#endif // EI_CLASSIFIER_INFERENCING_ENGINE
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_ATON_H
