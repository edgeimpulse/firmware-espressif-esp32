#ifndef EI_CEVA_DSP_H
#define EI_CEVA_DSP_H

#include <cstddef>
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/dsp/numpy_types.h"
#define CEVA_DSP_LIB_MAX_FFT 1024 // doesn't actually change link stage.  But gives us all protos

extern "C" {
#include "ceva/ceva_dsp_lib_fft.h"
}

extern "C" void CEVA_DSP_LIB_FLOAT_FFT_REAL(int32 log2_buf_len,
		float_32 *in_buf,
		float_32 *out_buf,
		float_32 const *twi_table,
		float_32 const *last_stage_twi_table,
		int16 const *bitrev_tbl,
		float_32 *temp_buf,
		int32 br);


namespace ei {
namespace fft {

constexpr int MIN_FFT_SIZE = 32;
constexpr int MAX_FFT_SIZE = 1024;

static bool can_do_fft(size_t n_fft)
{
    return n_fft == 32 || n_fft == 64 || n_fft == 128 || n_fft == 256 || n_fft == 512 ||
        n_fft == 1024;
}

static float_32 const *twiddle_table_for_n_fft(size_t log2_n_fft)
{
// First, the cases where we need all tables
#if EI_CLASSIFIER_HAS_FFT_INFO == 1  && !defined(EI_CLASSIFIER_LOAD_ALL_FFTS)
// Intentionally empty
#else
#define EI_LOAD_ALL_FFT_TABLES 1
#endif

    switch (log2_n_fft) {
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_32
        case 5: return twi_table_float_rfft_32;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_64
        case 6: return twi_table_float_rfft_64;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_128
        case 7: return twi_table_float_rfft_128;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_256
        case 8: return twi_table_float_rfft_256;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_512
        case 9: return twi_table_float_rfft_512;
#endif
#if EI_LOAD_ALL_FFT_TABLES || EI_CLASSIFIER_LOAD_FFT_1024
        case 10: return twi_table_float_rfft_1024;
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

    float_32 const *twiddle_table = twiddle_table_for_n_fft(log2_n_fft);
    if (!twiddle_table) { return EIDSP_FFT_TABLE_NOT_LOADED; }

    float* temp = nullptr;
    auto ptr = EI_MAKE_TRACKED_POINTER(temp, n_fft);
    EI_ERR_AND_RETURN_ON_NULL(temp, EIDSP_OUT_OF_MEM);

    CEVA_DSP_LIB_FLOAT_FFT_REAL(
		log2_n_fft,
		const_cast<float*>(input),
		(float_32 *)output,
		CEVA_DSP_LIB_FLOAT_cos_sin,
		twiddle_table,
		bitrev_32_1024,
		temp,
		1 // bit reverse
	);
    return EIDSP_OK;
}

} // namespace fft

} // namespace ei

#endif // EI_CEVA_DSP_H
