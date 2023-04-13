/*
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../ei_classifier_porting.h"
#if EI_PORTING_SILABS == 1

/* Include ----------------------------------------------------------------- */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
//TODO: use only headers after migrating Thunderboard2 firmware
#if defined(EFR32MG24B310F1536IM48) && EFR32MG24B310F1536IM48==1
#include "sl_sleeptimer.h"
#include "sl_stdio.h"
#elif defined(EFR32MG12P332F1024GL125) && EFR32MG12P332F1024GL125==1
extern "C" {
    void sl_sleeptimer_delay_millisecond(uint16_t time_ms);
    uint32_t sl_sleeptimer_get_tick_count(void);
    uint32_t sl_sleeptimer_tick_to_ms(uint32_t tick);
}
#endif

__attribute__((weak)) EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

/**
 * Cancelable sleep, can be triggered with signal from other thread
 */
__attribute__((weak)) EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
    sl_sleeptimer_delay_millisecond(time_ms);
    return EI_IMPULSE_OK;
}

uint64_t ei_read_timer_ms()
{
    return (uint32_t)sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
}

uint64_t ei_read_timer_us()
{
    return ei_read_timer_ms() * 1000;
}

void ei_serial_set_baudrate(int baudrate)
{
}

//TODO: after merging Thunderboard 2 firmware, use this function
#if defined(EFR32MG24B310F1536IM48) && EFR32MG24B310F1536IM48==1
void ei_putchar(char c)
{
    sl_putchar(c);
}
#endif

__attribute__((weak)) char ei_getchar()
{
#if defined(EFR32MG24B310F1536IM48) && EFR32MG24B310F1536IM48==1
    char ch = 0;

    if(sl_getchar(&ch) == SL_STATUS_OK) {
        return ch;
    }
    else {
        return 0;
    }
#else
    return getchar();
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

#endif // EI_PORTING_SILABS == 1
