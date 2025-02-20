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

#include "../ei_classifier_porting.h"
#if EI_PORTING_SONY_SPRESENSE == 1

#include <stdarg.h>
#include <stdlib.h>
#include <cstdio>

extern "C" void spresense_time_cb(uint32_t *sec, uint32_t *nano);
extern "C" void spresense_putchar(char cChar);
extern "C" char spresense_getchar(void);

__attribute__((weak)) EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

/**
 * Cancelable sleep, can be triggered with signal from other thread
 */
__attribute__((weak)) EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {

    uint64_t end_ms = ei_read_timer_ms() + time_ms;

    while(end_ms > ei_read_timer_ms()){};

    return EI_IMPULSE_OK;
}

uint64_t ei_read_timer_ms() {

    uint64_t time_ms;
    uint32_t seconds, nano_seconds;

    spresense_time_cb(&seconds, &nano_seconds);

    time_ms = (seconds * 1000) + (nano_seconds / 1000000);
    return time_ms;
}

uint64_t ei_read_timer_us() {

    uint64_t time_us;
    uint32_t seconds, nano_seconds;

    spresense_time_cb(&seconds, &nano_seconds);

    time_us = (seconds * 1000000) + (nano_seconds / 1000);
    return time_us;
}

__attribute__((weak)) void ei_printf(const char *format, ...) {

    char buffer[256];
    int length;
    va_list myargs;
    va_start(myargs, format);
    length = vsprintf(buffer, format, myargs);
    va_end(myargs);

    for(int i = 0; i < length; i++) {
        spresense_putchar(buffer[i]);
    }
}

__attribute__((weak)) void ei_printf_float(float f) {
    ei_printf("%f", f);
}

/**
 * @brief      Write single character to serial output
 *
 * @param[in]  cChar  The character
 */
__attribute__((weak)) void ei_putchar(char cChar)
{
    spresense_putchar(cChar);
}

__attribute__((weak)) char ei_getchar(void)
{
    return spresense_getchar();
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

#endif // EI_PORTING_SONY_SPRESENSE == 1
