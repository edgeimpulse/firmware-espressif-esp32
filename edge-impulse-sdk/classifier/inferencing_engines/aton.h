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
#include "edge-impulse-sdk/classifier/ei_fill_result_struct.h"
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
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    EI_IMPULSE_ERROR fill_res = EI_IMPULSE_OK;
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

    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t *)impulse->learning_blocks[0].config;
    if (block_config->classification_mode == EI_CLASSIFIER_CLASSIFICATION_MODE_OBJECT_DETECTION) {
        switch (block_config->object_detection_last_layer) {

            case EI_CLASSIFIER_LAST_LAYER_YOLOV5:
                #if MODEL_OUTPUT_IS_FLOAT
                fill_res = fill_result_struct_f32_yolov5(
                    ei_default_impulse.impulse,
                    &result,
                    6, // hard coded for now
                    (float *)&data,//output.data.uint8,
                    // output.params.zero_point,
                    // output.params.scale,
                    ei_default_impulse.impulse->tflite_output_features_count);
                #else
                fill_res = fill_result_struct_quantized_yolov5(
                    impulse,        
                    block_config,
                    result,
                    6, // hard coded for now
                    (uint8_t *)nn_out,
                    nn_out_info[0].offset[0],
                    nn_out_info[0].scale[0],
                    nn_out_len);
                #endif
                break;

            case EI_CLASSIFIER_LAST_LAYER_YOLO_PRO:
                #if MODEL_OUTPUT_IS_FLOAT
                fill_res = fill_result_struct_f32_yolo_pro(
                    ei_default_impulse.impulse,
                    &result,
                    (float *)&data,
                    ei_default_impulse.impulse->tflite_output_features_count);
                #else
                fill_res = fill_result_struct_quantized_yolo_pro(
                    impulse,
                    block_config,
                    result,
                    (uint8_t *)nn_out,
                    nn_out_info[0].offset[0],
                    nn_out_info[0].scale[0],
                    nn_out_len);
                #endif
                break;

            case EI_CLASSIFIER_LAST_LAYER_FOMO:
                fill_res = fill_result_struct_i8_fomo(
                    impulse,
                    block_config,
                    result,
                    (int8_t *)nn_out,
                    nn_out_info[0].offset[0],
                    nn_out_info[0].scale[0],
                    impulse->fomo_output_size,
                    impulse->fomo_output_size);
                break;
            case EI_CLASSIFIER_LAST_LAYER_YOLOV11:
            case EI_CLASSIFIER_LAST_LAYER_YOLOV11_ABS: {
                bool is_coord_normalized = block_config->object_detection_last_layer == EI_CLASSIFIER_LAST_LAYER_YOLOV11 ?
                    true : false;
                #if MODEL_OUTPUT_IS_FLOAT
                fill_res = fill_result_struct_f32_yolov11(
                    ei_default_impulse.impulse,
                    &result,
                    is_coord_normalized,
                    (float *)&data,
                    ei_default_impulse.impulse->tflite_output_features_count);
                #else
                fill_res = fill_result_struct_quantized_yolov11(
                    impulse,
                    block_config,
                    result,
                    is_coord_normalized,
                    (uint8_t *)nn_out,
                    nn_out_info[0].offset[0],
                    nn_out_info[0].scale[0],
                    nn_out_len);
                #endif
                break;
            }

        
            default:
                ei_printf("ERR: Unsupported object detection last layer (%d)\n",
                    block_config->object_detection_last_layer);
                fill_res = EI_IMPULSE_UNSUPPORTED_INFERENCING_ENGINE;
                break;
        }

    }
    // if we copy the output, we don't need to process it as classification
    else
    {
        if (!result->copy_output) {
            bool int8_output = 1; //quantized hardcoded for now
            if (int8_output) {
                fill_res = fill_result_struct_i8(impulse, result, (int8_t *)nn_out, nn_out_info[0].offset[0], nn_out_info[0].scale[0], debug);
            }
            else {
                fill_res = fill_result_struct_f32(impulse, result,(float *)nn_out, debug);
            }
        }
    }

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    return fill_res;
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
