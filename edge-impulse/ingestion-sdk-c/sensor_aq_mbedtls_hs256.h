/* Edge Impulse ingestion SDK
 * Copyright (c) 2020 EdgeImpulse Inc.
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

#ifndef _EDGE_IMPULSE_SIGNING_MBEDTLS_HMAC_SHA256_H_
#define _EDGE_IMPULSE_SIGNING_MBEDTLS_HMAC_SHA256_H_

/**
 * HMAC SHA256 implementation using Mbed TLS
 */

//#include <string.h>
#include "sensor_aq.h"
#include "mbedtls/md.h"
//#include "mbedtls/sha256.h"
//#include "mbed_trace.h"
//#include "ei_mbedtls_md.h"

//#ifdef MBEDTLS_MD_C

typedef struct {
    mbedtls_md_context_t md_ctx;
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

// #else

// #error "sensor_aq_mbedtls_hs256 loaded but Mbed TLS was not found, or MBEDTLS_MD_C was disabled"

// #endif // MBEDTLS_MD_C

#endif // _EDGE_IMPULSE_SIGNING_MBEDTLS_HMAC_SHA256_H_
