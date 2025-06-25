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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_HELPER_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_HELPER_H_

#include "edge-impulse-sdk/classifier/ei_quantize.h"
#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL) || (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) || (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSORRT) || (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_TIDL)

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL) || (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_TIDL)
#include <thread>
#include "tensorflow-lite/tensorflow/lite/c/common.h"
#include "tensorflow-lite/tensorflow/lite/interpreter.h"
#include "tensorflow-lite/tensorflow/lite/kernels/register.h"
#include "tensorflow-lite/tensorflow/lite/model.h"
#include "tensorflow-lite/tensorflow/lite/optional_debug_tools.h"
#endif // EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL

#if EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE
#include <cmath>
#include "edge-impulse-sdk/tensorflow/lite/micro/all_ops_resolver.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/micro_interpreter.h"
#include "edge-impulse-sdk/tensorflow/lite/schema/schema_generated.h"
#endif // EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE

EI_IMPULSE_ERROR fill_input_tensor_from_matrix(
    ei_feature_t *fmatrix,
    ei_feature_t *omatrix,
    TfLiteTensor *input,
    uint32_t* input_block_ids,
    uint32_t input_block_ids_size,
    size_t fmtx_size,
    size_t omtx_size
) {
    size_t matrix_els = 0;
    uint32_t input_idx = 0;

    for (size_t i = 0; i < input_block_ids_size; i++) {
#if EI_CLASSIFIER_SINGLE_FEATURE_INPUT == 0
        size_t cur_mtx = input_block_ids[i];
        ei::matrix_t* matrix = NULL;

        if (!find_mtx_by_idx(fmatrix, &matrix, cur_mtx, fmtx_size)) {
            if (!find_mtx_by_idx(omatrix, &matrix, cur_mtx, omtx_size)) {
                ei_printf("ERR: Cannot find matrix with id %zu\n", cur_mtx);
                return EI_IMPULSE_INVALID_SIZE;
            }
        }
#else
        ei::matrix_t* matrix = fmatrix[0].matrix;
#endif

        matrix_els += matrix->rows * matrix->cols;

        switch (input->type) {
            case kTfLiteFloat32: {
                for (size_t ix = 0; ix < matrix->rows * matrix->cols; ix++) {
                    input->data.f[input_idx++] = matrix->buffer[ix];
                }
                break;
            }
            case kTfLiteInt8: {
                for (size_t ix = 0; ix < matrix->rows * matrix->cols; ix++) {
                    float val = (float)matrix->buffer[ix];
                    input->data.int8[input_idx++] = static_cast<int8_t>(
                        pre_cast_quantize(val, input->params.scale, input->params.zero_point, true));
                }
                break;
            }
            case kTfLiteUInt8: {
                for (size_t ix = 0; ix < matrix->rows * matrix->cols; ix++) {
                    float val = (float)matrix->buffer[ix];
                    input->data.uint8[input_idx++] = static_cast<uint8_t>(
                        pre_cast_quantize(val, input->params.scale, input->params.zero_point, false));            }
                break;
            }
            default: {
                ei_printf("ERR: Cannot handle input type (%d)\n", input->type);
                return EI_IMPULSE_INPUT_TENSOR_WAS_NULL;
            }
        }
    }

    if (input->bytes / 4 != matrix_els && input->bytes != matrix_els) {
        ei_printf("ERR: input tensor has size %d bytes, but input matrix has has size %d bytes\n",
            (int)input->bytes, (int)matrix_els);
        return EI_IMPULSE_INVALID_SIZE;
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR fill_input_tensor_from_signal(
    signal_t *signal,
    TfLiteTensor *input
) {
    switch (input->type) {
        case kTfLiteFloat32: {
            if (input->bytes / 4 != signal->total_length) {
                ei_printf("ERR: input tensor has size %d, but signal has size %d\n",
                    (int)input->bytes / 4, (int)signal->total_length);
                return EI_IMPULSE_INVALID_SIZE;
            }

            auto x = signal->get_data(0, signal->total_length, input->data.f);
            if (x != EIDSP_OK) {
                return EI_IMPULSE_DSP_ERROR;
            }
            break;
        }
        case kTfLiteInt8:
        case kTfLiteUInt8: {
            // we don't have a good signaling way here (this is DSP blocks where
            // we don't understand the input very well; guess whether this is an RGB input)
            bool is_rgb = input->bytes / 3 == signal->total_length;

            if (!is_rgb) {
                // otherwise expect an exact match in length
                if (input->bytes != signal->total_length) {
                    ei_printf("ERR: input tensor has size %d, but signal has size %d\n",
                        (int)input->bytes, (int)signal->total_length);
                    return EI_IMPULSE_INVALID_SIZE;
                }
            }

            float scale = input->params.scale;
            int zero_point = input->params.zero_point;
            if (scale == 0.0f) { // not quantized?
                if (is_rgb) {
                    scale = 0.003921568859368563f;
                }
                else {
                    scale = 1.0f;
                }

                if (input->type == kTfLiteInt8 && zero_point == 0) {
                    zero_point = -128;
                }
            }

            size_t output_ix = 0;
            const size_t page_size = 1024;

            // buffered read from the signal
            size_t bytes_left = signal->total_length;
            for (size_t ix = 0; ix < signal->total_length; ix += page_size) {
                size_t elements_to_read = bytes_left > page_size ? page_size : bytes_left;

                matrix_t input_matrix(elements_to_read, 1);
                if (!input_matrix.buffer) {
                    return EI_IMPULSE_ALLOC_FAILED;
                }
                signal->get_data(ix, elements_to_read, input_matrix.buffer);

                for (size_t jx = 0; jx < elements_to_read; jx++) {
                    if (is_rgb) {
                        uint32_t value = static_cast<uint32_t>(input_matrix.buffer[jx]);

                        // fast code path
                        if (scale == 0.003921568859368563f && zero_point == -128) {
                            int32_t r = static_cast<int32_t>(value >> 16 & 0xff);
                            int32_t g = static_cast<int32_t>(value >> 8 & 0xff);
                            int32_t b = static_cast<int32_t>(value & 0xff);

                            if (input->type == kTfLiteInt8) {
                                input->data.int8[output_ix++] = static_cast<int8_t>(r + zero_point);
                                input->data.int8[output_ix++] = static_cast<int8_t>(g + zero_point);
                                input->data.int8[output_ix++] = static_cast<int8_t>(b + zero_point);
                            }
                            else {
                                input->data.uint8[output_ix++] = static_cast<uint8_t>(r + zero_point);
                                input->data.uint8[output_ix++] = static_cast<uint8_t>(g + zero_point);
                                input->data.uint8[output_ix++] = static_cast<uint8_t>(b + zero_point);
                            }
                        }
                        // slow code path
                        else {
                            float r = static_cast<float>(value >> 16 & 0xff) / 255.0f;
                            float g = static_cast<float>(value >> 8 & 0xff) / 255.0f;
                            float b = static_cast<float>(value & 0xff) / 255.0f;

                            if (input->type == kTfLiteInt8) {
                                input->data.int8[output_ix++] = static_cast<int8_t>(round(r / scale) + zero_point);
                                input->data.int8[output_ix++] = static_cast<int8_t>(round(g / scale) + zero_point);
                                input->data.int8[output_ix++] = static_cast<int8_t>(round(b / scale) + zero_point);
                            }
                            else {
                                input->data.uint8[output_ix++] = static_cast<uint8_t>(round(r / scale) + zero_point);
                                input->data.uint8[output_ix++] = static_cast<uint8_t>(round(g / scale) + zero_point);
                                input->data.uint8[output_ix++] = static_cast<uint8_t>(round(b / scale) + zero_point);
                            }
                        }
                    }
                    else {
                        float value = input_matrix.buffer[jx];
                        if (input->type == kTfLiteInt8) {
                            input->data.int8[output_ix++] = static_cast<int8_t>(round(value / scale) + zero_point);
                        }
                        else { // uint8
                            input->data.uint8[output_ix++] = static_cast<uint8_t>((value / scale) + zero_point);
                        }
                    }
                }
            }
            break;
        }
        default: {
            ei_printf("ERR: Cannot handle input type (%d)\n", input->type);
            return EI_IMPULSE_INPUT_TENSOR_WAS_NULL;
        }
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR fill_output_matrix_from_tensor(
    TfLiteTensor *output,
    matrix_t *output_matrix
) {
    const size_t matrix_els = output_matrix->rows * output_matrix->cols;

    switch (output->type) {
        case kTfLiteFloat32: {
            if (output->bytes / 4 != matrix_els) {
                ei_printf("ERR: output tensor has size %d, but input matrix has has size %d\n",
                    (int)output->bytes / 4, (int)matrix_els);
                return EI_IMPULSE_INVALID_SIZE;
            }

            memcpy(output_matrix->buffer, output->data.f, output->bytes);
            break;
        }
        case kTfLiteInt8: {
            if (output->bytes != matrix_els) {
                ei_printf("ERR: output tensor has size %d, but input matrix has has size %d\n",
                    (int)output->bytes, (int)matrix_els);
                return EI_IMPULSE_INVALID_SIZE;
            }

            for (size_t ix = 0; ix < output->bytes; ix++) {
                float value = static_cast<float>(output->data.int8[ix] - output->params.zero_point) * output->params.scale;
                output_matrix->buffer[ix] = value;
            }
            break;
        }
        case kTfLiteUInt8: {
            if (output->bytes != matrix_els) {
                ei_printf("ERR: output tensor has size %d, but input matrix has has size %d\n",
                    (int)output->bytes, (int)matrix_els);
                return EI_IMPULSE_INVALID_SIZE;
            }

            for (size_t ix = 0; ix < output->bytes; ix++) {
                float value = static_cast<float>(output->data.uint8[ix] - output->params.zero_point) * output->params.scale;
                output_matrix->buffer[ix] = value;
            }
            break;
        }
        default: {
            ei_printf("ERR: Cannot handle output type (%d)\n", output->type);
            return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
        }
    }

    return EI_IMPULSE_OK;
}

#endif // #if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL) || (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_TFLITE_HELPER_H_
