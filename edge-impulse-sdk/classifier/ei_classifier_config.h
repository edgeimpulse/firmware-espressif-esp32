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

#ifndef _EI_CLASSIFIER_CONFIG_H_
#define _EI_CLASSIFIER_CONFIG_H_

// clang-format off

// This is a file that's only used in benchmarking to override HW optimized kernels
#ifdef __has_include
    #if __has_include("source/benchmark.h")
    #include "source/benchmark.h"
    #endif
#endif

#if EI_CLASSIFIER_TFLITE_ENABLE_SILABS_MVP == 1
    #define EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN        0
    #define EI_CLASSIFIER_TFLITE_LOAD_CMSIS_NN_SOURCES  1
#endif

#ifndef EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN
#if defined(__MBED__)
    #include "mbed_version.h"
    #if (MBED_VERSION < MBED_ENCODE_VERSION((5), (7), (0)))
        #define EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN      0
    #else
        #define EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN      1
    #endif // Mbed OS 5.7 version check

// __ARM_ARCH_PROFILE is a predefine of arm-gcc.  __TARGET_* is armcc
#elif __ARM_ARCH_PROFILE == 'M' || defined(__TARGET_CPU_CORTEX_M0) || defined(__TARGET_CPU_CORTEX_M0PLUS) || defined(__TARGET_CPU_CORTEX_M3) || defined(__TARGET_CPU_CORTEX_M4) || defined(__TARGET_CPU_CORTEX_M7) || defined(ARDUINO_NRF52_ADAFRUIT)
    #define EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN      1
#else
    #define EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN      0
#endif
#endif // EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN

#if EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN == 1
#define CMSIS_NN                                     1
#define EI_CLASSIFIER_TFLITE_LOAD_CMSIS_NN_SOURCES   1
#endif

#ifndef EI_CLASSIFIER_TFLITE_ENABLE_ARC
#ifdef CPU_ARC
#define EI_CLASSIFIER_TFLITE_ENABLE_ARC             1
#else
#define EI_CLASSIFIER_TFLITE_ENABLE_ARC             0
#endif // CPU_ARC
#endif // EI_CLASSIFIER_TFLITE_ENABLE_ARC

#ifndef EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN
    #if defined(ESP32)
        #include "sdkconfig.h"
        #define EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN      1
        #define ESP_NN                                  1
    #endif // ESP32 check
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
        #define EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_S3      1
    #endif // ESP32S3 check
    #if defined(CONFIG_IDF_TARGET_ESP32P4)
        #define EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_P4      1
    #endif // ESP32S3 check
#else
    #define ESP_NN                                  1
#endif

// no include checks in the compiler? then just include metadata and then ops_define (optional if on EON model)
#ifndef __has_include
    #include "model-parameters/model_metadata.h"
    #if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED == 1)
        #include "tflite-model/trained_model_ops_define.h"
    #endif
#else
    #if __has_include("tflite-model/trained_model_ops_define.h")
    #include "tflite-model/trained_model_ops_define.h"
    #endif
#endif // __has_include

// clang-format on
#endif // _EI_CLASSIFIER_CONFIG_H_
