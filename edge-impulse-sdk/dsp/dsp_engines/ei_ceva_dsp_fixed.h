#ifndef EI_CEVA_DSP_H
#define EI_CEVA_DSP_H

#include <cstddef>
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/dsp/numpy_types.h"
#define CEVA_DSP_LIB_MAX_FFT 1024 // doesn't actually change link stage.  But gives us all protos

extern "C" {
#include "ceva/ceva_dsp_lib_fft.h"
}


extern "C" float_32 CEVA_DSP_LIB_FLOAT_VEC_MAX_ABS(float_32 *inp1, uint32 size_buf);

namespace ei {
namespace fft {

constexpr int MIN_FFT_SIZE = 32;
constexpr int MAX_FFT_SIZE = 1024;

static bool can_do_fft(size_t n_fft)
{
    return n_fft == 32 || n_fft == 64 || n_fft == 128 || n_fft == 256 || n_fft == 512 ||
        n_fft == 1024;
}

static int16 const *twiddle_table_for_n_fft(size_t log2_n_fft)
{
// First, the cases where we need all tables
#if EI_CLASSIFIER_HAS_FFT_INFO == 1  && !defined(EI_CLASSIFIER_LOAD_ALL_FFTS)
// Intentionally empty
#else
#define EI_LOAD_ALL_FFT_TABLES 1
#endif

    switch (log2_n_fft) {
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_32
        case 5: return twi_table_16_rfft_32;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_64
        case 6: return twi_table_16_rfft_64;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_128
        case 7: return twi_table_16_rfft_128;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_256
        case 8: return twi_table_16_rfft_256;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_512
        case 9: return twi_table_16_rfft_512;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_1024
        case 10: return twi_table_16_rfft_1024;
#endif
        default: return nullptr;
    }
}

static int hw_r2c_fft(const float *input, ei::fft_complex_t *output, size_t n_fft)
{
    using namespace ei;
    if(!can_do_fft(n_fft)) { return EIDSP_FFT_SIZE_NOT_SUPPORTED; }

    size_t log2_n_fft = 0;
    while ((1 << log2_n_fft) < n_fft) {
        log2_n_fft++;
    }

    int16 const *twiddle_table = twiddle_table_for_n_fft(log2_n_fft);
    if (!twiddle_table) { return EIDSP_FFT_TABLE_NOT_LOADED; }

    int16* temp = nullptr;
    auto ptr = EI_MAKE_TRACKED_POINTER(temp, n_fft*2 + log2_n_fft*4);
//     auto ptr = EI_MAKE_TRACKED_POINTER(temp, 1000);
    EI_ERR_AND_RETURN_ON_NULL(temp, EIDSP_OUT_OF_MEM);

    float max_val = CEVA_DSP_LIB_FLOAT_VEC_MAX_ABS(const_cast<float*>(input), n_fft);
    // ei_printf("max_val: %f\n", max_val);
    int16* output_as_int16 = (int16*)output;
    float scale_factor = (32767.0f) / max_val;
    for (size_t i = 0; i < n_fft; i++) {
        output_as_int16[i] = static_cast<int16>(input[i] * scale_factor);
    }

    // for (size_t i = 0; i < n_fft; i++) {
    //     ei_printf("%d,", ((int16*)output)[i]);
    // }
    // ei_printf("\n");

    // Scale at every stage, max 10 stages for 1024
    int8 scale[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    scale[log2_n_fft] = 0;


    CEVA_DSP_LIB_INT16_FFT(
        log2_n_fft,
        output_as_int16,
        output_as_int16,
        CEVA_DSP_LIB_cos_sin_fft_16,
        twiddle_table,
        bitrev_16_1024,
        temp,
        scale,
        1 // bit reverse
    );

    // Scale back, double for real and imaginary parts
    // n_fft>>1 accounts for 1 right shift per stage (see scale array)
    scale_factor = (n_fft >> 1) / scale_factor;

    // Go backwards b/c floats are bigger than source int16
    // Cast avoids warning. Need int so we can quit at -1
    for (int i = (int)n_fft + 1; i >= 0; i--) {
        ((float*)output)[i] = (static_cast<float>(output_as_int16[i]) * scale_factor);
    }

    return EIDSP_OK;
}

} // namespace fft

} // namespace ei

#endif // EI_CEVA_DSP_H
