/* Edge Impulse inferencing library
 * Copyright (c) 2021 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../ei_classifier_porting.h"
#if EI_PORTING_MBED == 1

#include "mbed.h"
#include <stdarg.h>
#include <stdlib.h>
#include "us_ticker_api.h"

#define EI_WEAK_FN __attribute__((weak))

EI_WEAK_FN EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

/**
 * Cancelable sleep, can be triggered with signal from other thread
 */
EI_WEAK_FN EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
#if MBED_VERSION >= MBED_ENCODE_VERSION((5), (11), (0))
    rtos::ThisThread::sleep_for(time_ms);
#else
    wait_ms(time_ms);
#endif // MBED_VERSION >= MBED_ENCODE_VERSION((5), (11), (0))
    return EI_IMPULSE_OK;
}

uint64_t ei_read_timer_ms() {
#if DEVICE_USTICKER
    return us_ticker_read() / 1000L;
#elif DEVICE_LPTICKER
    return ei_read_timer_us() / 1000L;
#else
    #error "Target does not have DEVICE_LPTICKER nor DEVICE_USTICKER"
#endif
}

uint64_t ei_read_timer_us() {
#if DEVICE_USTICKER
    return us_ticker_read();
#elif DEVICE_LPTICKER
	const ticker_info_t *info = lp_ticker_get_info();
	uint32_t n_ticks = lp_ticker_read();
    return (uint64_t)n_ticks * (1000000UL / info->frequency);
#else
    #error "Target does not have DEVICE_LPTICKER nor DEVICE_USTICKER"
#endif
}

__attribute__((weak)) void ei_printf(const char *format, ...) {
    va_list myargs;
    va_start(myargs, format);
    vprintf(format, myargs);
    va_end(myargs);
}

__attribute__((weak)) void ei_printf_float(float f) {
    ei_printf("%f", f);
}

__attribute__((weak)) void *ei_malloc(size_t size) {
    return malloc(size);
}

__attribute__((weak)) void *ei_calloc(size_t nitems, size_t size) {
    return calloc(nitems, size);
}

__attribute__((weak)) void ei_free(void *ptr) {
    free(ptr);
}

#if defined(__cplusplus) && EI_C_LINKAGE == 1
extern "C"
#endif
__attribute__((weak)) void DebugLog(const char* s) {
    ei_printf("%s", s);
}

#endif // EI_PORTING_MBED == 1
