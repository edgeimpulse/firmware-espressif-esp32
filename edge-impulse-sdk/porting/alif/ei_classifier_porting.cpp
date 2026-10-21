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

#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#if (EI_PORTING_ALIF == 1)

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "peripheral/ei_uart.h"
#include "peripheral/timer.h"

#include "FreeRTOS.h"
#include "task.h"

/* Extern function prototypes ---------------------------------------------- */

EI_IMPULSE_ERROR ei_run_impulse_check_canceled()
{
    return EI_IMPULSE_OK;
}

/**
 * Cancelable sleep, can be triggered with signal from other thread
 */
EI_IMPULSE_ERROR ei_sleep(int32_t time_ms)
{
    if (time_ms < 0) { 
        return EI_IMPULSE_OK; 
    }

    uint64_t start_time = ei_read_timer_ms();
    // cast so that we get correct wrap around behavior
    while ((ei_read_timer_ms() - start_time) < (uint64_t) time_ms)
        ;
    return EI_IMPULSE_OK;
}

uint64_t ei_read_timer_us()
{
    return timer_get_us();
}

uint64_t ei_read_timer_ms()
{
    return ei_read_timer_us() / 1000;
}

void ei_printf(const char *format, ...)
{
    char buffer[256] = {0};
    int length;

    va_list myargs;
    va_start(myargs, format);
    length = vsnprintf(buffer, sizeof(buffer), format, myargs);
    va_end(myargs);
    
    if (length > 0) {
        ei_uart_send(buffer, length);
    }
}

void ei_printf_float(float f)
{
    float n = f;

    static double PRECISION = 0.001;
    static int MAX_NUMBER_STRING_SIZE = 32;

    char s[MAX_NUMBER_STRING_SIZE];

    if (n == 0.0) {
        strcpy(s, "0");
    }
    else {
        int digit, m;
        char *c = s;
        int neg = (n < 0);
        if (neg) {
            n = -n;
        }
        // calculate magnitude
        m = log10(n);
        if (neg) {
            *(c++) = '-';
        }
        if (m < 1.0) {
            m = 0;
        }
        // convert the number
        while (n > PRECISION || m >= 0) {
            double weight = pow(10.0, m);
            if (weight > 0 && !isinf(weight)) {
                digit = floor(n / weight);
                n -= (digit * weight);
                *(c++) = '0' + digit;
            }
            if (m == 0 && n > 0) {
                *(c++) = '.';
            }
            m--;
        }
        *(c) = '\0';
    }

    ei_printf("%s", s);
}

void ei_putchar(char c) 
{ 
    ei_uart_send(&c, 1);
    fflush(stdout);
}

char ei_getchar(void)
{
    char c = 0xFF;

    c = ei_get_serial_byte();

    if (c == 0xFF ) { 
        return 0; //weird ei convention
    }

    return c;
}

void *ei_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    return pvPortMalloc(size);
}

void *ei_calloc(size_t nitems, size_t size)
{
    if ((size == 0) || (nitems == 0)) {
        return NULL;
    }

    return pvPortCalloc(nitems, size);
}

void ei_free(void *ptr)
{
    vPortFree(ptr);
}

#if defined(__cplusplus) && EI_C_LINKAGE == 1
extern "C"
#endif

void * operator new( size_t size )
{
    if (size == 0) {
        return NULL;
    }
    void* new_data = pvPortMalloc( size );
    if (new_data != NULL) {
        memset(new_data, 0, size);
    }
    return new_data;
}

void * operator new[]( size_t size )
{
    if (size == 0) {
        return NULL;
    }
    void* new_data = pvPortMalloc( size );
    if (new_data != NULL) {
        memset(new_data, 0, size);
    }
    return new_data;
}

void operator delete( void * ptr )
{
    if (ptr == NULL) {
        return;
    }
    vPortFree ( ptr );
}

void operator delete[]( void * ptr )
{
    if (ptr == NULL) {
        return;
    }
    vPortFree ( ptr );
}

#endif
