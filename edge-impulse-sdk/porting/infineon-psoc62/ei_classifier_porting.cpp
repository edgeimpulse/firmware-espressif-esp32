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
#if EI_PORTING_INFINEONPSOC62 == 1

#include <stdarg.h>
#include <stdlib.h>
#include <cstdio>
#include "unistd.h"
#include "cyhal.h"
#ifdef FREERTOS_ENABLED
#include <FreeRTOS.h>
#include <timers.h>
#include <task.h>
#else /* bare-metal */
#include "cyhal_lptimer.h"

static bool timer_init = false;
static volatile uint64_t tick = 0;

static void systick_isr(void)
{
    tick++;
}
#endif

__attribute__((weak)) EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

#ifdef FREERTOS_ENABLED
__attribute__((weak)) EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
    vTaskDelay(time_ms / portTICK_PERIOD_MS);

    return EI_IMPULSE_OK;
}

__attribute__((weak)) uint64_t ei_read_timer_ms() {

    return xTaskGetTickCount();
}

__attribute__((weak)) uint64_t ei_read_timer_us() {

    return xTaskGetTickCount()*1000;
}
#else /* Bare-metal */
__attribute__((weak)) EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
    cyhal_system_delay_ms(time_ms);
    return EI_IMPULSE_OK;
}

uint64_t ei_read_timer_ms() {
    if(timer_init == false) {
        cyhal_clock_t clock;
        uint32_t freq;

        // get IMO clock frequency
        cyhal_clock_reserve(&clock, &CYHAL_CLOCK_IMO);
        freq = cyhal_clock_get_frequency(&clock);
        cyhal_clock_free(&clock);

        // set SysTick to 1 ms
        Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_IMO, (freq / 1000) - 1);
        Cy_SysTick_SetCallback(0, systick_isr);
        timer_init = true;
        return 0;
    }
    return tick;
}

uint64_t ei_read_timer_us() {
    return ei_read_timer_ms() * 1000;
}
#endif /* FREERTOS_ENABLED */

void ei_putchar(char c)
{
    putchar(c);
}

__attribute__((weak)) char ei_getchar(void)
{
    return getchar();
}

__attribute__((weak)) void ei_printf(const char *format, ...) {

    char buffer[256];
    va_list myargs;
    va_start(myargs, format);
    vsnprintf(buffer, 256, format, myargs);
    va_end(myargs);

    printf("%s", buffer);
}

__attribute__((weak)) void ei_printf_float(float f) {
    ei_printf("%f", f);
}

#ifdef FREERTOS_ENABLED
__attribute__((weak)) void *ei_malloc(size_t size) {
    return pvPortMalloc(size);
}

__attribute__((weak)) void *ei_calloc(size_t nitems, size_t size) {
    void *mem = NULL;

    /* Infineon port of FreeRTOS does not support pvPortCalloc */
    mem = pvPortMalloc(nitems * size);
    if (mem) {
        /* zero the memory */
        memset(mem, 0, nitems * size);
    }
    return mem;
}

__attribute__((weak)) void ei_free(void *ptr) {
    vPortFree(ptr);
}
#else
__attribute__((weak)) void *ei_malloc(size_t size) {
    return malloc(size);
}

__attribute__((weak)) void *ei_calloc(size_t nitems, size_t size) {
    return calloc(nitems, size);
}

__attribute__((weak)) void ei_free(void *ptr) {
    free(ptr);
}
#endif

#if defined(__cplusplus) && EI_C_LINKAGE == 1
extern "C"
#endif
__attribute__((weak)) void DebugLog(const char* s) {
    ei_printf("%s", s);
}

#endif // EI_PORTING_INFINEONPSOC62
