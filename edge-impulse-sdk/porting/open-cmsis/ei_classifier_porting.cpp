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

/* Includes */
#include "../ei_classifier_porting.h"

#if (EI_PORTING_OPEN_CMSIS == 1)

#include <stdarg.h>
#include <stdlib.h>
#include <cstdio>
#include <string.h>
#include <math.h>

uint32_t volatile msTicks; /* counts 1ms timeTicks */

__attribute__((weak)) void SysTick_Handler(void)
{
    msTicks++;
}

__attribute__((weak)) EI_IMPULSE_ERROR ei_run_impulse_check_canceled() 
{
    return EI_IMPULSE_OK;
}

/**
 * @brief sleep for the given time in ms
 * 
 * @param time_ms time in ms
 * 
 */
__attribute__((weak)) EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {

    uint64_t start_time = ei_read_timer_ms();

    while ((start_time + time_ms) > ei_read_timer_ms()){};

    return EI_IMPULSE_OK;
}

/**
 * @brief Read the time in ms
 * 
 * @return uint64_t time in ms
 */
__attribute__((weak)) uint64_t ei_read_timer_ms()
{
    return (uint64_t)msTicks;
}

/**
 * @brief Read the time in us
 * 
 * 
 * @return uint64_t time in us
 */
__attribute__((weak)) uint64_t ei_read_timer_us()
{
    return (uint64_t)(msTicks * 1000);
}

/**
 * @brief Read a character from the standard input.
 * 
 */
__attribute__((weak)) char ei_getchar()
{
    char ch = 0;
    return ch;
}

/**
 * @brief Wrapper for printf function. The string is to be printed on the console. 
 * 
 * @note This function is weakly linked and can be overridden by the user.
 * 
 */
__attribute__((weak)) void ei_printf(const char *format, ...) 
{
    va_list myargs;
    va_start(myargs, format);
    vprintf(format, myargs);
    va_end(myargs);
}

/**
 * @brief Print a float on console. Needed if printf of float number is not supported by the platform
 * 
 * @note This function is weakly linked and can be overridden by the user.
 * 
 * @param f float number to be printed
 */
__attribute__((weak)) void ei_printf_float(float f) {
    ei_printf("%f", f);
}

/**
 * @brief Print char on console
 * 
 * @param c 
 */
__attribute__((weak)) void ei_putchar(char c)
{
    ei_printf("%c", c);
}

/**
 * @brief malloc wrapper
 * 
 */
__attribute__((weak)) void *ei_malloc(size_t size) {
    return malloc(size);
}

/**
 * @brief calloc wrapper
 * 
 */
__attribute__((weak)) void *ei_calloc(size_t nitems, size_t size) {
    return calloc(nitems, size);
}

/**
 * @brief free wrapper
 * 
 */
__attribute__((weak)) void ei_free(void *ptr) {
    free(ptr);
}

#if defined(__cplusplus) && EI_C_LINKAGE == 1
extern "C"
#endif
__attribute__((weak)) void DebugLog(const char* s) {
    ei_printf("%s", s);
}

#endif  // EI_PORTING_OPEN_CMSIS
