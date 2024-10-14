#ifndef __EI_NO_HW_DSP__H__
#define __EI_NO_HW_DSP__H__

#include <cstddef>
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/dsp/numpy_types.h"

// Make these constexpr to let compiler optimize the if statements out

constexpr int hw_r2r_fft(float* input, float* output, size_t n_fft) {
    return ei::EIDSP_NO_HW_ACCEL;
}

constexpr int hw_r2c_fft(const float *input, ei::fft_complex_t *output, size_t n_fft) {
    return ei::EIDSP_NO_HW_ACCEL;
}

#endif  //!__EI_NO_HW_DSP__H__