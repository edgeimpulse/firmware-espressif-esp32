// ESP-DSP engine integration for Edge Impulse SDK
// Copyright (c) 2025 Edge Impulse, Espressif Systems
// Licensed under Apache 2.0

#ifndef EI_ESP_DSP_H
#define EI_ESP_DSP_H

#include <stdint.h>
#include <stdbool.h>
#include "edge-impulse-sdk/porting/espressif/esp-dsp/modules/fft/include/dsps_fft2r.h"
#include "edge-impulse-sdk/porting/ei_logging.h"

namespace ei {
namespace fft {

constexpr int MIN_FFT_SIZE = 4;
constexpr int MAX_FFT_SIZE = 4096;

static bool init_done = false;

static bool can_do_fft(size_t n_fft) {
    // if power of 2 and within range
    if (n_fft < MIN_FFT_SIZE || n_fft > MAX_FFT_SIZE)
        return false;
    if ((n_fft & (n_fft - 1)) != 0)
        return false; // not a power of 2
    return true;
}

static bool init_fft(size_t n_fft) {
    if (init_done) {
        return true; // Already initialized
    }
    EI_LOGD("Initializing ESP-DSP FFT with size %zu\n", n_fft);
    if (!can_do_fft(n_fft)) { return false; /* EIDSP_FFT_SIZE_NOT_SUPPORTED */ }

    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        EI_LOGE("Not possible to initialize FFT. Error = %i\n", ret);
        return false;
    }
    return true;
}

static int hw_r2c_fft(const float *input, ei::fft_complex_t *output_as_complex, size_t n_fft) {
    if (!init_done) {
        if (!init_fft(n_fft)) {
            EI_LOGE("Failed to initialize FFT\n");
            return -1; // EIDSP_FFT_INIT_FAILED
        }
        init_done = true;
    }
    // ESP-DSP expects input/output as float arrays, length must be power of 2
    float *in = const_cast<float *>(input);
    float *output = (float*)output_as_complex;
    int err = 0;

    // Prepare input as complex numbers (real part, imaginary part)
    float *complex_input = (float*)ei_malloc(n_fft * sizeof(float) * 2);
    if (complex_input == nullptr) {
        EI_LOGE("Failed to allocate memory for complex input\n");
        goto out;
        return -1; // EIDSP_MEMORY_ALLOC_FAILED
    }

    for (size_t i = 0; i < n_fft; i++) {
        complex_input[i * 2 + 0] = in[i]; // Real part
        complex_input[i * 2 + 1] = 0.0f; // Imaginary part
    }

    err = dsps_fft2r_fc32(complex_input, n_fft);
    if (err != 0) {
        EI_LOGE("Error in dsps_fft2r_fc32: %d\n", err);
        return err;
    }
    // Rearrange output if needed (ESP-DSP uses bit-reversed order)
    dsps_bit_rev_fc32(complex_input, n_fft);

    // Copy result to output
    for (size_t i = 0; i < n_fft + 2; i++) {
        output[i] = complex_input[i];
    }
out:
    ei_free(complex_input);
    return 0;
}

} // namespace fft
} // namespace ei

#endif // EI_ESP_DSP_H