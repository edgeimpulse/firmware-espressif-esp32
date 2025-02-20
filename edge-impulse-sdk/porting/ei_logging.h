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

#ifndef _EI_LOGGING_H_
#define _EI_LOGGING_H_

#include <stdint.h>
#include <stdarg.h>

#include "ei_classifier_porting.h"

#define EI_LOG_LEVEL_NONE 0 /*!< No log output */
#define EI_LOG_LEVEL_ERROR 1 /*!< Critical errors, software module can not recover on its own */
#define EI_LOG_LEVEL_WARNING 2 /*!< Error conditions from which recovery measures have been taken */
#define EI_LOG_LEVEL_INFO 3 /*!< Information messages which describe normal flow of events */
#define EI_LOG_LEVEL_DEBUG 4 /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */

// if we do not want ANY logging, setting EI_LOG_LEVEL to EI_LOG_LEVEL_NONE
// will not generate any code according to

#define EI_LOGE(format, ...) (void)0
#define EI_LOGW(format, ...) (void)0
#define EI_LOGI(format, ...) (void)0
#define EI_LOGD(format, ...) (void)0

#ifndef EI_LOG_LEVEL
    #define EI_LOG_LEVEL EI_LOG_LEVEL_INFO
#endif

__attribute__((unused)) static const char *debug_msgs[] =
{
    "NONE", // this one will never show
    "ERR",
    "WARNING",
    "INFO",
    "DEBUG"
};

#if EI_LOG_LEVEL >= EI_LOG_LEVEL_ERROR
    #ifdef EI_LOGE
    #undef EI_LOGE
    #endif // EI_LOGE
    #define EI_LOGE(format, ...) ei_printf("%s: ",debug_msgs[EI_LOG_LEVEL_ERROR]); ei_printf(format, ##__VA_ARGS__);
#endif

#if EI_LOG_LEVEL >= EI_LOG_LEVEL_WARNING
    #ifdef EI_LOGW
    #undef EI_LOGW
    #endif // EI_LOGW
    #define EI_LOGW(format, ...) ei_printf("%s: ",debug_msgs[EI_LOG_LEVEL_WARNING]); ei_printf(format, ##__VA_ARGS__);
#endif

#if EI_LOG_LEVEL >= EI_LOG_LEVEL_INFO
    #ifdef EI_LOGI
    #undef EI_LOGI
    #endif // EI_LOGI
    #define EI_LOGI(format, ...) ei_printf("%s: ",debug_msgs[EI_LOG_LEVEL_INFO]); ei_printf(format, ##__VA_ARGS__);
#endif

#if EI_LOG_LEVEL >= EI_LOG_LEVEL_DEBUG
    #ifdef EI_LOGD
    #undef EI_LOGD
    #endif // EI_LOGD
    #define EI_LOGD(format, ...) ei_printf("%s: ",debug_msgs[EI_LOG_LEVEL_DEBUG]); ei_printf(format, ##__VA_ARGS__);
#endif

#endif // _EI_LOGGING_H_