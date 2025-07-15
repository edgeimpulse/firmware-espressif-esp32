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

#ifndef __EI_DATA_NORMALIZATION_H__
#define __EI_DATA_NORMALIZATION_H__

#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/ei_classifier_types.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

#if EI_CLASSIFIER_HAS_DATA_NORMALIZATION

extern "C" EI_IMPULSE_ERROR init_data_normalization(ei_impulse_handle_t *handle) {
    if (!handle) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }

    auto impulse = handle->impulse;
    for (size_t i = 0; i < impulse->dsp_blocks_size; i++) {
        if(impulse->dsp_blocks[i].data_normalization_config) {
            auto dn_config = impulse->dsp_blocks[i].data_normalization_config;
            if (dn_config->init_fn) {
                EI_IMPULSE_ERROR res = dn_config->init_fn(handle);
                if (res != EI_IMPULSE_OK) {
                    return res;
                }
            }
        }
    }

    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR deinit_data_normalization(ei_impulse_handle_t *handle) {
    if (!handle) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }

    auto impulse = handle->impulse;
    for (size_t i = 0; i < impulse->dsp_blocks_size; i++) {
        if(impulse->dsp_blocks[i].data_normalization_config) {
            auto dn_config = impulse->dsp_blocks[i].data_normalization_config;
            if (dn_config->deinit_fn) {
                EI_IMPULSE_ERROR res = dn_config->deinit_fn(handle);
                if (res != EI_IMPULSE_OK) {
                    return res;
                }
            }
        }
    }

    return EI_IMPULSE_OK;
}

extern "C" EI_IMPULSE_ERROR run_data_normalization(ei_impulse_handle_t *handle,
                                                   ei_feature_t *features) {

    if (!handle) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }

    auto impulse = handle->impulse;
    for (size_t i = 0; i < impulse->dsp_blocks_size; i++) {
        auto dsp_block = impulse->dsp_blocks[i];
        if(dsp_block.data_normalization_config
           && dsp_block.data_normalization_config->config) {
            auto dn_config = impulse->dsp_blocks[i].data_normalization_config;
            if (dn_config->exec_fn) {
                EI_IMPULSE_ERROR res = dn_config->exec_fn((void*)&handle->impulse->dsp_blocks[i], features[i].matrix);
                if (res != EI_IMPULSE_OK) {
                    return res;
                }
            }
        }
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR data_normalization_standard_scaler(
    void *dsp_block,
    matrix_t *input_matrix)
{
    // Standard scaler implements:
    //     z = (x - u) * s'
    //
    //     where:
    //        x is the feature
    //        u is the mean
    //        s' is 1/s
    //        s is the standard deviation

    if (dsp_block == NULL) {
        return EI_IMPULSE_DATA_NORMALIZATION_ERROR;
    }

    EI_IMPULSE_ERROR ret = EI_IMPULSE_DATA_NORMALIZATION_ERROR;

    ei_model_dsp_t *block = (ei_model_dsp_t *)dsp_block;
    ei_data_normalization_t *dn_config = (ei_data_normalization_t *)block->data_normalization_config;

    // We only use mean and scale here.
    if (dn_config->config) {
        ei_data_normalization_standard_scaler_config_t *sc_config = (ei_data_normalization_standard_scaler_config_t *) dn_config->config;

        if(sc_config->mean_data && sc_config->scale_data && sc_config->var_data
           && (sc_config->mean_data_len > 0)
           && (sc_config->scale_data_len > 0) && (sc_config->var_data_len > 0)) {

            if (input_matrix->rows != 1) {
                ei_printf("ERR: data normalization: input matrix invalid num of rows, expected: (1), got (%d)\n", input_matrix->rows);
                return EI_IMPULSE_INVALID_SIZE;
            }

            const uint32_t numb_els_input = input_matrix->rows * input_matrix->cols;
            if (block->n_output_features != numb_els_input) {
                ei_printf("ERR: data normalization: input matrix size, expected (%ld), got (%d)\n", block->n_output_features, numb_els_input);
                return EI_IMPULSE_INVALID_SIZE;
            }

            uint32_t numb_els = 0;
            numb_els = sc_config->mean_data_len;
            if (numb_els != numb_els_input) {
                ei_printf("ERR: data normalization: mean size mismatch, expected (%d), got (%d)\n", numb_els_input, numb_els);
                return EI_IMPULSE_INVALID_SIZE;
            }

            numb_els = sc_config->scale_data_len;
            if (numb_els != numb_els_input) {
                ei_printf("ERR: data normalization: scale size mismatch, expected (%d), got (%d)\n", numb_els_input, numb_els);
                return EI_IMPULSE_INVALID_SIZE;
            }

            // note: (N, 1)
            matrix_t mean_matrix(input_matrix->cols, 1, sc_config->mean_data);
            matrix_t scale_matrix(input_matrix->cols, 1, sc_config->scale_data);

            // transpose the input matrix from (1, N) -> (N, 1)
            matrix_t temp_input_matrix(input_matrix->cols, 1, input_matrix->buffer);

            int err = numpy::subtract(&temp_input_matrix, &mean_matrix);
            if (err != EIDSP_OK) {
                return EI_IMPULSE_DATA_NORMALIZATION_ERROR;
            }

            err = numpy::scale(&temp_input_matrix, &scale_matrix);
            if (err != EIDSP_OK) {
                return EI_IMPULSE_DATA_NORMALIZATION_ERROR;
            }

            ret = EI_IMPULSE_OK;
        }
    }

    return ret;
}

#endif // EI_CLASSIFIER_HAS_DATA_NORMALIZATION

#endif // __EI_DATA_NORMALIZATION_H__
