/*
 * Copyright (c) 2024 EdgeImpulse Inc.
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

/* Include ----------------------------------------------------------------- */
#include "../ei_classifier_porting.h"
#if EI_PORTING_AMBIQ == 1

#include <stdarg.h>
#include <stdlib.h>
#include <cstdio>
#include "ns_timer.h"
#include "ns_ambiqsuite_harness.h"
#include "ns_malloc.h"
#include <cstring>

#include "peripheral/usb/ei_usb.h"

#define EI_WEAK_FN __attribute__((weak))

extern ns_timer_config_t ei_tickTimer;

EI_WEAK_FN EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

/**
 * Cancelable sleep, can be triggered with signal from other thread
 */
EI_WEAK_FN EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
    ns_delay_us(time_ms*1000);
    return EI_IMPULSE_OK;
}

uint64_t ei_read_timer_ms() {
    return ns_us_ticker_read(&ei_tickTimer) / 1000L;
}

uint64_t ei_read_timer_us() {
    return ns_us_ticker_read(&ei_tickTimer);

}

__attribute__((weak)) void ei_printf(const char *format, ...) {
    char buffer[1024] = {0};
    int length;
    va_list myargs;
    va_start(myargs, format);
    length = vsnprintf(buffer, sizeof(buffer), format, myargs);
    va_end(myargs);

    if (length > 0) {
        ei_usb_send((uint8_t *)buffer, length);
    }
}

__attribute__((weak)) void ei_printf_float(float f) {
    ei_printf("%f", f);
}

__attribute__((weak)) void *ei_malloc(size_t size) {

    void *p = ns_malloc(size);
    return p;
}

__attribute__((weak)) void *ei_calloc(size_t nitems, size_t size) {

    void *ret = ns_malloc(nitems*size);
    memset(ret, 0, nitems*size);
    return ret;
}

__attribute__((weak)) void ei_free(void *ptr) {

    ns_free(ptr);
}

void ei_putchar(char c) 
{ 
    ei_printf("%c", c);
}

char ei_getchar(void)
{
    char c = 0xFF;

    c = ei_get_serial_byte(false);

    if (c == 0xFF ) { 
        return 0; //weird ei convention
    }

    return c;
}

#if defined(__cplusplus) && EI_C_LINKAGE == 1
extern "C"
#endif
__attribute__((weak)) void DebugLog(const char* s) {
    ei_printf("%s", s);
}

#endif // EI_PORTING_AMBIQ == 1
