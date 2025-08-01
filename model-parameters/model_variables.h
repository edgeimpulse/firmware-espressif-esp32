/*
 * Copyright (c) 2024 EdgeImpulse Inc.
 *
 * Generated by Edge Impulse and licensed under the applicable Edge Impulse
 * Terms of Service. Community and Professional Terms of Service
 * (https://edgeimpulse.com/legal/terms-of-service) or Enterprise Terms of
 * Service (https://edgeimpulse.com/legal/enterprise-terms-of-service),
 * according to your product plan subscription (the “License”).
 *
 * This software, documentation and other associated files (collectively referred
 * to as the “Software”) is a single SDK variation generated by the Edge Impulse
 * platform and requires an active paid Edge Impulse subscription to use this
 * Software for any purpose.
 *
 * You may NOT use this Software unless you have an active Edge Impulse subscription
 * that meets the eligibility requirements for the applicable License, subject to
 * your full and continued compliance with the terms and conditions of the License,
 * including without limitation any usage restrictions under the applicable License.
 *
 * If you do not have an active Edge Impulse product plan subscription, or if use
 * of this Software exceeds the usage limitations of your Edge Impulse product plan
 * subscription, you are not permitted to use this Software and must immediately
 * delete and erase all copies of this Software within your control or possession.
 * Edge Impulse reserves all rights and remedies available to enforce its rights.
 *
 * Unless required by applicable law or agreed to in writing, the Software is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing
 * permissions, disclaimers and limitations under the License.
 */

#ifndef _EI_CLASSIFIER_MODEL_VARIABLES_H_
#define _EI_CLASSIFIER_MODEL_VARIABLES_H_

/**
 * @file
 *  Auto-generated complete impulse definitions. The ei_impulse_handle_t should
 *  be passed to ei_run_classifier() function to use this specific impulse.
 *  This file should only be included in ei_run_classifier.h file.
 */

#include <stdint.h>
#include "model_metadata.h"

#include "tflite-model/tflite_learn_13_compiled.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/inferencing_engines/engines.h"
#include "edge-impulse-sdk/classifier/postprocessing/ei_postprocessing_common.h"

const char* ei_classifier_inferencing_categories[] = { "helloworld", "noise", "unknown" };

uint8_t ei_dsp_config_12_axes[] = { 0 };
const uint32_t ei_dsp_config_12_axes_size = 1;
ei_dsp_config_mfcc_t ei_dsp_config_12 = {
    12, // uint32_t blockId
    4, // int implementationVersion
    1, // int length of axes
    NULL, // named axes
    0, // size of the named axes array
    13, // int num_cepstral
    0.02f, // float frame_length
    0.02f, // float frame_stride
    32, // int num_filters
    256, // int fft_length
    101, // int win_size
    0, // int low_frequency
    0, // int high_frequency
    0.98f, // float pre_cof
    1 // int pre_shift
};

const uint8_t ei_dsp_blocks_size = 1;
ei_model_dsp_t ei_dsp_blocks[ei_dsp_blocks_size] = {
    { // DSP block 12
        12,
        650, // output size
        &extract_mfcc_features, // DSP function pointer
        (void*)&ei_dsp_config_12, // pointer to config struct
        ei_dsp_config_12_axes, // array of offsets into the input stream, one for each axis
        ei_dsp_config_12_axes_size, // number of axes
        1, // version
        nullptr, // factory function
    }
};
const ei_config_tflite_eon_graph_t ei_config_tflite_graph_13 = {
    .implementation_version = 1,
    .model_init = &tflite_learn_13_init,
    .model_invoke = &tflite_learn_13_invoke,
    .model_reset = &tflite_learn_13_reset,
    .model_input = &tflite_learn_13_input,
    .model_output = &tflite_learn_13_output,
};

const uint8_t ei_output_tensors_indices_13[1] = { 0 };
const uint8_t ei_output_tensors_size_13 = 1;
const ei_learning_block_config_tflite_graph_t ei_learning_block_config_13 = {
    .implementation_version = 1,
    .block_id = 13,
    .output_tensors_indices = ei_output_tensors_indices_13,
    .output_tensors_size = ei_output_tensors_size_13,
    .quantized = 1,
    .compiled = 1,
    .graph_config = (void*)&ei_config_tflite_graph_13
};

const uint8_t ei_learning_blocks_size = 1;
const uint32_t ei_learning_block_13_inputs[1] = { 12 };
const uint8_t ei_learning_block_13_inputs_size = 1;
const ei_learning_block_t ei_learning_blocks[ei_learning_blocks_size] = {
    {
        13,
        &run_nn_inference,
        (void*)&ei_learning_block_config_13,
        EI_CLASSIFIER_IMAGE_SCALING_NONE,
        ei_learning_block_13_inputs,
        ei_learning_block_13_inputs_size,
    },
};

const ei_fill_result_classification_i8_config_t ei_fill_result_classification_i8_config_13 = {
    .zero_point = -128,
    .scale = 0.00390625
};
const size_t ei_postprocessing_blocks_size = 1;
const ei_postprocessing_block_t ei_postprocessing_blocks[ei_postprocessing_blocks_size] = {
    {
        .block_id = 13,
        .type = EI_CLASSIFIER_MODE_CLASSIFICATION,
        .init_fn = NULL,
        .deinit_fn = NULL,
        .postprocess_fn = &process_classification_i8,
        .display_fn = NULL,
        .config = (void*)&ei_fill_result_classification_i8_config_13,
        .input_block_id = 13
    },
};

const ei_impulse_t impulse_40_0 = {
    .project_id = 40,
    .project_owner = "Edge Impulse Profiling",
    .project_name = "Demo: Keyword Spotting",
    .impulse_id = 1,
    .impulse_name = "Impulse #1",
    .deploy_version = 3,

    .nn_input_frame_size = 650,
    .raw_sample_count = 16000,
    .raw_samples_per_frame = 1,
    .dsp_input_frame_size = 16000 * 1,
    .input_width = 0,
    .input_height = 0,
    .input_frames = 0,
    .interval_ms = 0.0625,
    .frequency = 16000,

    .dsp_blocks_size = ei_dsp_blocks_size,
    .dsp_blocks = ei_dsp_blocks,

    .learning_blocks_size = ei_learning_blocks_size,
    .learning_blocks = ei_learning_blocks,

    .postprocessing_blocks_size = 1,
    .postprocessing_blocks = ei_postprocessing_blocks,

    .inferencing_engine = EI_CLASSIFIER_TFLITE,

    .sensor = EI_CLASSIFIER_SENSOR_MICROPHONE,
    .fusion_string = "audio",
    .slice_size = (16000/4),
    .slices_per_model_window = 4,

    .has_anomaly = EI_ANOMALY_TYPE_UNKNOWN,
    .label_count = 3,
    .categories = ei_classifier_inferencing_categories
};

ei_impulse_handle_t impulse_handle_40_0 = ei_impulse_handle_t( &impulse_40_0 );
ei_impulse_handle_t& ei_default_impulse = impulse_handle_40_0;

#endif // _EI_CLASSIFIER_MODEL_VARIABLES_H_
