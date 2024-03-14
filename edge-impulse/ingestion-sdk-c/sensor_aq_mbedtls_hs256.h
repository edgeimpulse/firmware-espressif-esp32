/*
 * Copyright (c) 2023 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _EDGE_IMPULSE_SIGNING_MBEDTLS_HMAC_SHA256_H_
#define _EDGE_IMPULSE_SIGNING_MBEDTLS_HMAC_SHA256_H_

/**
 * HMAC SHA256 implementation using Mbed TLS
 */
#include "firmware-sdk/sensor-aq/sensor_aq.h"

typedef struct {
    char hmac_key[33];
} sensor_aq_mbedtls_hs256_ctx_t;

/**
 * Construct a new signing context for HMAC SHA256 using Mbed TLS
 *
 * @param aq_ctx An empty signing context (can declare it without arguments)
 * @param hs_ctx An empty sensor_aq_mbedtls_hs256_ctx_t context (can declare it on the stack without arguments)
 * @param hmac_key The secret key - **NOTE: this is limited to 32 characters, the rest will be truncated**
 */
void sensor_aq_init_mbedtls_hs256_context(sensor_aq_signing_ctx_t *aq_ctx, sensor_aq_mbedtls_hs256_ctx_t *hs_ctx, const char *hmac_key);

#endif // _EDGE_IMPULSE_SIGNING_MBEDTLS_HMAC_SHA256_H_
