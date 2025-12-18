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
 *   *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   *   notice, this list of conditions and the following disclaimer in the
 *   *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   *   contributors may be used to endorse or promote products derived from this
 *   *   software without specific prior written permission.
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
#ifndef _EIDSP_CMSIS_NUMPY_H_
#define _EIDSP_CMSIS_NUMPY_H_

#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include <stdint.h>

// CMSIS-DSP includes
#include "edge-impulse-sdk/CMSIS/DSP/Include/dsp/matrix_functions.h"
#include "edge-impulse-sdk/CMSIS/DSP/Include/dsp/statistics_functions.h"

#ifndef EI_MAX_UINT16
#define EI_MAX_UINT16 65535
#endif

namespace ei {

static inline float hw_sqrt(float x)
{
    float temp;
    arm_sqrt_f32(x, &temp);
    return temp;
}

static inline int hw_dot(matrix_t *matrix1, matrix_t *matrix2, matrix_t *out_matrix)
{
    if (matrix1->rows > EI_MAX_UINT16 || matrix1->cols > EI_MAX_UINT16 ||
        matrix2->rows > EI_MAX_UINT16 || matrix2->cols > EI_MAX_UINT16 ||
        out_matrix->rows > EI_MAX_UINT16 || out_matrix->cols > EI_MAX_UINT16) {
        return EIDSP_NARROWING;
    }

    const arm_matrix_instance_f32 m1 = { static_cast<uint16_t>(matrix1->rows),
                                         static_cast<uint16_t>(matrix1->cols),
                                         matrix1->buffer };
    const arm_matrix_instance_f32 m2 = { static_cast<uint16_t>(matrix2->rows),
                                         static_cast<uint16_t>(matrix2->cols),
                                         matrix2->buffer };
    arm_matrix_instance_f32 mo = { static_cast<uint16_t>(out_matrix->rows),
                                   static_cast<uint16_t>(out_matrix->cols),
                                   out_matrix->buffer };
    int status = arm_mat_mult_f32(&m1, &m2, &mo);
    if (status != ARM_MATH_SUCCESS) {
        return status;
    }
    return EIDSP_OK;
}

static inline int
hw_dot_by_row(int i, float *row, uint32_t matrix1_cols, matrix_t *matrix2, matrix_t *out_matrix)
{
    if (matrix1_cols > EI_MAX_UINT16 || matrix2->rows > EI_MAX_UINT16 ||
        matrix2->cols > EI_MAX_UINT16 || out_matrix->cols > EI_MAX_UINT16) {
        return EIDSP_NARROWING;
    }

    const arm_matrix_instance_f32 m1 = { 1, static_cast<uint16_t>(matrix1_cols), row };
    const arm_matrix_instance_f32 m2 = { static_cast<uint16_t>(matrix2->rows),
                                         static_cast<uint16_t>(matrix2->cols),
                                         matrix2->buffer };
    arm_matrix_instance_f32 mo = { 1,
                                   static_cast<uint16_t>(out_matrix->cols),
                                   out_matrix->buffer + (i * out_matrix->cols) };
    int status = arm_mat_mult_f32(&m1, &m2, &mo);
    if (status != ARM_MATH_SUCCESS) {
        return status;
    }
    return EIDSP_OK;
}

static inline int
hw_mat_transpose(const float *in_matrix, float *out_matrix, uint16_t in_rows, uint16_t in_cols)
{
    if (in_rows > EI_MAX_UINT16 || in_cols > EI_MAX_UINT16) {
        return EIDSP_NARROWING;
    }
    const arm_matrix_instance_f32 i_m = { in_cols, in_rows, const_cast<float *>(in_matrix) };
    arm_matrix_instance_f32 o_m = { in_rows, in_cols, out_matrix };
    arm_status status = arm_mat_trans_f32(&i_m, &o_m);
    if (status != ARM_MATH_SUCCESS) {
        return status;
    }
    return EIDSP_OK;
}

static inline int hw_mat_scale_inplace(float *data, uint16_t rows, uint16_t cols, float scale)
{
    if (rows > EI_MAX_UINT16 || cols > EI_MAX_UINT16) {
        return EIDSP_NARROWING;
    }
    const arm_matrix_instance_f32 mi = { rows, cols, data };
    arm_matrix_instance_f32 mo = { rows, cols, data };
    int status = arm_mat_scale_f32(&mi, scale, &mo);
    if (status != ARM_MATH_SUCCESS) {
        return status;
    }
    return EIDSP_OK;
}

static inline int hw_rms_array(const float *array, uint32_t len, float *out_value)
{
    arm_rms_f32(array, len, out_value);
    return EIDSP_OK;
}

static inline int hw_mean_array(const float *array, uint32_t len, float *out_value)
{
    arm_mean_f32(array, len, out_value);
    return EIDSP_OK;
}

static inline int hw_min_array(const float *array, uint32_t len, float *out_value)
{
    float minv;
    uint32_t ix;
    arm_min_f32(array, len, &minv, &ix);
    *out_value = minv;
    return EIDSP_OK;
}

static inline int hw_max_array(const float *array, uint32_t len, float *out_value)
{
    float maxv;
    uint32_t ix;
    arm_max_f32(array, len, &maxv, &ix);
    *out_value = maxv;
    return EIDSP_OK;
}

// Variance with NumPy semantics (divide by N)
static inline void hw_variance_np(const float32_t *pSrc, uint32_t blockSize, float32_t *pResult)
{
    uint32_t blkCnt;
    float32_t sum = 0.0f;
    float32_t fSum = 0.0f;
    float32_t fMean, fValue;
    const float32_t *pInput = pSrc;

    if (blockSize <= 1U) {
        *pResult = 0;
        return;
    }
    blkCnt = blockSize >> 2U;

    while (blkCnt > 0U) {
        sum += *pInput++;
        sum += *pInput++;
        sum += *pInput++;
        sum += *pInput++;
        blkCnt--;
    }

    blkCnt = blockSize % 0x4U;
    while (blkCnt > 0U) {
        sum += *pInput++;
        blkCnt--;
    }

    fMean = sum / (float32_t)blockSize;
    pInput = pSrc;
    blkCnt = blockSize >> 2U;
    while (blkCnt > 0U) {
        fValue = *pInput++ - fMean;
        fSum += fValue * fValue;
        fValue = *pInput++ - fMean;
        fSum += fValue * fValue;
        fValue = *pInput++ - fMean;
        fSum += fValue * fValue;
        fValue = *pInput++ - fMean;
        fSum += fValue * fValue;
        blkCnt--;
    }

    blkCnt = blockSize % 0x4U;
    while (blkCnt > 0U) {
        fValue = *pInput++ - fMean;
        fSum += fValue * fValue;
        blkCnt--;
    }
    *pResult = fSum / (float32_t)(blockSize);
}

static inline void hw_variance(const float32_t *pSrc, uint32_t blockSize, float32_t *pResult)
{
    arm_var_f32(pSrc, blockSize, pResult);
}

static inline void
hw_third_moment(const float32_t *pSrc, uint32_t blockSize, float32_t mean, float32_t *pResult)
{
    uint32_t blkCnt;
    float32_t sum = 0.0f;
    float32_t in;

    blkCnt = blockSize >> 2U;
    while (blkCnt > 0U) {
        in = *pSrc++;
        in = in - mean;
        sum += in * in * in;
        in = *pSrc++;
        in = in - mean;
        sum += in * in * in;
        in = *pSrc++;
        in = in - mean;
        sum += in * in * in;
        in = *pSrc++;
        in = in - mean;
        sum += in * in * in;
        blkCnt--;
    }

    blkCnt = blockSize % 0x4U;
    while (blkCnt > 0U) {
        in = *pSrc++;
        in = in - mean;
        sum += in * in * in;
        blkCnt--;
    }
    sum = sum / blockSize;
    *pResult = sum;
}

static inline void
hw_fourth_moment(const float32_t *pSrc, uint32_t blockSize, float32_t mean, float32_t *pResult)
{
    uint32_t blkCnt;
    float32_t sum = 0.0f;
    float32_t in;

    blkCnt = blockSize >> 2U;
    while (blkCnt > 0U) {
        in = *pSrc++;
        in = in - mean;
        float square = in * in;
        sum += square * square;
        in = *pSrc++;
        in = in - mean;
        square = in * in;
        sum += square * square;
        in = *pSrc++;
        in = in - mean;
        square = in * in;
        sum += square * square;
        in = *pSrc++;
        in = in - mean;
        square = in * in;
        sum += square * square;
        blkCnt--;
    }

    blkCnt = blockSize % 0x4U;
    while (blkCnt > 0U) {
        in = *pSrc++;
        in = in - mean;
        float square = in * in;
        sum += square * square;
        blkCnt--;
    }
    sum = sum / blockSize;
    *pResult = sum;
}

static inline int hw_stdev_array(const float *array, uint32_t len, float *out_value)
{
    float var;
    hw_variance_np(array, len, &var);
    arm_sqrt_f32(var, out_value);
    return EIDSP_OK;
}

static inline int hw_skew_array(const float *array, uint32_t len, float *out_value)
{
    float mean;
    arm_mean_f32(array, len, &mean);
    float m3;
    hw_third_moment(array, len, mean, &m3);
    float var;
    hw_variance_np(array, len, &var);
    float denom;
    arm_sqrt_f32(var * var * var, &denom);
    if (denom == 0.0f) {
        *out_value = 0.0f;
    }
    else {
        *out_value = m3 / denom;
    }
    return EIDSP_OK;
}

static inline int hw_kurtosis_array(const float *array, uint32_t len, float *out_value)
{
    float mean;
    arm_mean_f32(array, len, &mean);
    float m4;
    hw_fourth_moment(array, len, mean, &m4);
    float var;
    hw_variance_np(array, len, &var);
    var = var * var;
    if (var == 0.0f) {
        *out_value = -3.0f;
    }
    else {
        *out_value = (m4 / var) - 3.0f;
    }
    return EIDSP_OK;
}

static inline int hw_std_axis0(matrix_t *input_matrix, matrix_t *output_matrix)
{
    arm_matrix_instance_f32 arm_in_matrix, arm_transposed_matrix;

    if (input_matrix->cols != output_matrix->rows) {
        return EIDSP_MATRIX_SIZE_MISMATCH;
    }
    if (output_matrix->cols != 1) {
        return EIDSP_MATRIX_SIZE_MISMATCH;
    }

    arm_in_matrix.numRows = input_matrix->rows;
    arm_in_matrix.numCols = input_matrix->cols;
    arm_in_matrix.pData = &input_matrix->buffer[0];

    arm_transposed_matrix.numRows = input_matrix->cols;
    arm_transposed_matrix.numCols = input_matrix->rows;
    auto alloc = EI_MAKE_TRACKED_POINTER(
        arm_transposed_matrix.pData,
        input_matrix->cols * input_matrix->rows);
    if (arm_transposed_matrix.pData == NULL) {
        return EIDSP_OUT_OF_MEM;
    }

    int ret = arm_mat_trans_f32(&arm_in_matrix, &arm_transposed_matrix);
    if (ret != EIDSP_OK) {
        return ret;
    }

    for (size_t row = 0; row < arm_transposed_matrix.numRows; row++) {
        float std;
        float var;
        hw_variance_np(
            arm_transposed_matrix.pData + (row * arm_transposed_matrix.numCols),
            arm_transposed_matrix.numCols,
            &var);
        arm_sqrt_f32(var, &std);
        output_matrix->buffer[row] = std;
    }

    return EIDSP_OK;
}

#define EI_RETURN_IF_ERROR(status)                                                                 \
    if (status != EIDSP_OK) {                                                                      \
        EI_LOGE("ARM CMSIS Error: %d", status);                                                    \
        return status;                                                                             \
    }

} // namespace ei

#endif // _EIDSP_CMSIS_NUMPY_H_
