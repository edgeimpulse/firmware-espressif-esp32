#ifndef __EI_ARM_CMSIS_DSP__H__
#define __EI_ARM_CMSIS_DSP__H__

#include "edge-impulse-sdk/CMSIS/DSP/Include/arm_const_structs.h"
#include "edge-impulse-sdk/CMSIS/DSP/Include/arm_math.h"
#include "edge-impulse-sdk/CMSIS/DSP/Include/dsp/transform_functions.h"
#include "edge-impulse-sdk/dsp/memory.hpp"
#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/dsp/returntypes.hpp"

/**
* Initialize a CMSIS-DSP fast rfft structure
* We do it this way as this means we can compile out fast_init calls which hints the compiler
* to which tables can be removed
*/
static int cmsis_rfft_init_f32(arm_rfft_fast_instance_f32 *rfft_instance, const size_t n_fft)
{
// ARM cores (ex M55) with Helium extensions (MVEF) need special treatment (Issue 2843)
#if EI_CLASSIFIER_HAS_FFT_INFO == 1 && !defined(ARM_MATH_MVEF) &&                                  \
    !defined(EI_CLASSIFIER_LOAD_ALL_FFTS)
    arm_status status;
    switch (n_fft) {
#if EI_CLASSIFIER_LOAD_FFT_32 == 1
    case 32: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 16U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len16.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len16.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len16.pTwiddle;
        rfft_instance->fftLenRFFT = 32U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_32;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
#if EI_CLASSIFIER_LOAD_FFT_64 == 1
    case 64: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 32U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len32.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len32.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len32.pTwiddle;
        rfft_instance->fftLenRFFT = 64U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_64;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
#if EI_CLASSIFIER_LOAD_FFT_128 == 1
    case 128: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 64U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len64.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len64.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len64.pTwiddle;
        rfft_instance->fftLenRFFT = 128U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_128;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
#if EI_CLASSIFIER_LOAD_FFT_256 == 1
    case 256: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 128U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len128.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len128.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len128.pTwiddle;
        rfft_instance->fftLenRFFT = 256U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_256;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
#if EI_CLASSIFIER_LOAD_FFT_512 == 1
    case 512: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 256U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len256.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len256.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len256.pTwiddle;
        rfft_instance->fftLenRFFT = 512U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_512;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
#if EI_CLASSIFIER_LOAD_FFT_1024 == 1
    case 1024: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 512U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len512.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len512.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len512.pTwiddle;
        rfft_instance->fftLenRFFT = 1024U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_1024;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
#if EI_CLASSIFIER_LOAD_FFT_2048 == 1
    case 2048: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 1024U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len1024.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len1024.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len1024.pTwiddle;
        rfft_instance->fftLenRFFT = 2048U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_2048;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
#if EI_CLASSIFIER_LOAD_FFT_4096 == 1
    case 4096: {
        arm_cfft_instance_f32 *S = &(rfft_instance->Sint);
        S->fftLen = 2048U;
        S->pTwiddle = NULL;
        S->bitRevLength = arm_cfft_sR_f32_len2048.bitRevLength;
        S->pBitRevTable = arm_cfft_sR_f32_len2048.pBitRevTable;
        S->pTwiddle = arm_cfft_sR_f32_len2048.pTwiddle;
        rfft_instance->fftLenRFFT = 4096U;
        rfft_instance->pTwiddleRFFT = (float32_t *)twiddleCoef_rfft_4096;
        status = ARM_MATH_SUCCESS;
        break;
    }
#endif
    default:
        return ei::EIDSP_FFT_TABLE_NOT_LOADED;
    }

    return status;
#else
    return arm_rfft_fast_init_f32(rfft_instance, n_fft);
#endif
}

static bool can_do_fft(size_t n_fft)
{
    return n_fft == 32 || n_fft == 64 || n_fft == 128 || n_fft == 256 || n_fft == 512 ||
        n_fft == 1024 || n_fft == 2048 || n_fft == 4096;
}

static int arm_rfft(const float *input, float *output, size_t n_fft)
{
    // hardware acceleration only works for the powers above...
    arm_rfft_fast_instance_f32 rfft_instance;
    int status = cmsis_rfft_init_f32(&rfft_instance, n_fft);
    if (status != ARM_MATH_SUCCESS) {
        return status;
    }

    arm_rfft_fast_f32(&rfft_instance, const_cast<float *>(input), output, 0);
    return 0;
}

static int hw_r2c_fft(const float *input, ei::fft_complex_t *output, size_t n_fft)
{
    if(!can_do_fft(n_fft)) { EIDSP_ERR(ei::EIDSP_NOT_SUPPORTED); }

    float *arm_fft_out;
    auto allocator = EI_MAKE_TRACKED_POINTER(arm_fft_out, n_fft);

    if (!arm_fft_out)
    {
        EIDSP_ERR(ei::EIDSP_OUT_OF_MEM);
    }

    // non zero is fail
    if(arm_rfft(input, arm_fft_out, n_fft)) { EIDSP_ERR(ei::EIDSP_PARAMETER_INVALID); }

    const size_t n_fft_out_features = n_fft / 2 + 1;
    output[0].r = arm_fft_out[0];
    output[0].i = 0.0f;
    output[n_fft_out_features - 1].r = arm_fft_out[1];
    output[n_fft_out_features - 1].i = 0.0f;

    size_t fft_output_buffer_ix = 2;
    for (size_t ix = 1; ix < n_fft_out_features - 1; ix += 1) {
        output[ix].r = arm_fft_out[fft_output_buffer_ix];
        output[ix].i = arm_fft_out[fft_output_buffer_ix + 1];

        fft_output_buffer_ix += 2;
    }
    return ei::EIDSP_OK;
}

static int hw_r2r_fft(const float *input, float *output, size_t n_fft)
{
    if(!can_do_fft(n_fft)) { return ei::EIDSP_NOT_SUPPORTED; }

    float *arm_fft_out;
    auto allocator = EI_MAKE_TRACKED_POINTER(arm_fft_out, n_fft);

    if (!arm_fft_out)
    {
        EIDSP_ERR(ei::EIDSP_OUT_OF_MEM);
    }

    // non zero is fail
    if(arm_rfft(input, arm_fft_out, n_fft)) { EIDSP_ERR(ei::EIDSP_PARAMETER_INVALID); }

    const size_t n_fft_out_features = n_fft / 2 + 1;
    output[0] = arm_fft_out[0]; // DC component
    output[n_fft_out_features - 1] = arm_fft_out[1]; // ARM puts the nyquist at the beginning

    size_t fft_output_buffer_ix = 2;
    for (size_t ix = 1; ix < n_fft_out_features - 1; ix += 1) {
        float rms_result;
        arm_rms_f32(arm_fft_out + fft_output_buffer_ix, 2, &rms_result);
        output[ix] = rms_result * sqrt(2);

        fft_output_buffer_ix += 2;
    }
    return ei::EIDSP_OK;
}

#endif //!__EI_ARM_CMSIS_DSP__H__
