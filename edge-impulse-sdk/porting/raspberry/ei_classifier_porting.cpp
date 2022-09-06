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
