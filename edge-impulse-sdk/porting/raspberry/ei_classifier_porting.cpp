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
#if EI_PORTING_RASPBERRY == 1

#include "pico/stdlib.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef FREERTOS_ENABLED
// Include FreeRTOS for delay
#include <FreeRTOS.h>
#include <task.h>
#endif

#define EI_WEAK_FN __attribute__((weak))

EI_WEAK_FN EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

EI_WEAK_FN EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
#ifdef FREERTOS_ENABLED
    vTaskDelay(time_ms / portTICK_PERIOD_MS);
#else
    sleep_ms(time_ms);
#endif
    return EI_IMPULSE_OK;
}

uint64_t ei_read_timer_ms() {
    return to_ms_since_boot(get_absolute_time());
}

uint64_t ei_read_timer_us() {
    return to_us_since_boot(get_absolute_time());
}

void ei_putchar(char c)
{
    /* Send char to serial output */
    ei_printf("%c", c);
}

/**
 *  Printf function uses vsnprintf and output using USB Serial
 */
__attribute__((weak)) void ei_printf(const char *format, ...) {
    static char print_buf[1024] = { 0 };

    va_list args;
    va_start(args, format);
    int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);

    if (r > 0) {
       printf(print_buf);
    }
}

__attribute__((weak)) void ei_printf_float(float f) {
    printf("%f", f);
}

__attribute__((weak)) void *ei_malloc(size_t size) {
#ifdef FREERTOS_ENABLED
    return pvPortMalloc(size);
#else
    return malloc(size);
#endif
}

#ifdef FREERTOS_ENABLED
void *pvPortCalloc(size_t sNb, size_t sSize)
{
    void *vPtr = NULL;
    if (sSize > 0) {
        vPtr = pvPortMalloc(sSize * sNb); // Call FreeRTOS or other standard API
        if(vPtr)
           memset(vPtr, 0, (sSize * sNb)); // Must required
    }
    return vPtr;
}
#endif

__attribute__((weak)) void *ei_calloc(size_t nitems, size_t size) {
#ifdef FREERTOS_ENABLED
    return pvPortCalloc(nitems, size);
#else
    return calloc(nitems, size);
#endif
}

__attribute__((weak)) void ei_free(void *ptr) {
#ifdef FREERTOS_ENABLED
    vPortFree(ptr);
#else
    free(ptr);
#endif
}

#if defined(__cplusplus) && EI_C_LINKAGE == 1
extern "C"
#endif
__attribute__((weak)) void DebugLog(const char* s) {
    ei_printf("%s", s);
}

#endif // EI_PORTING_RASPBERRY == 1
