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

/**
 * HMAC SHA256 implementation using Mbed TLS
 */

#include <string.h>
#include "firmware-sdk/sensor-aq/sensor_aq.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

typedef struct {
    char hmac_key[33];
} sensor_aq_mbedtls_hs256_ctx_t;

static int sensor_aq_mbedtls_hs256_init(sensor_aq_signing_ctx_t *aq_ctx) {
    sensor_aq_mbedtls_hs256_ctx_t *hs_ctx = (sensor_aq_mbedtls_hs256_ctx_t*)aq_ctx->ctx;

    /* Signature disabled, return zero */
    return 0;
}

static int sensor_aq_mbedtls_hs256_update(sensor_aq_signing_ctx_t *aq_ctx, const uint8_t *buffer, size_t buffer_size) {
    sensor_aq_mbedtls_hs256_ctx_t *hs_ctx = (sensor_aq_mbedtls_hs256_ctx_t*)aq_ctx->ctx;

    /* Signature disabled, return zero */
    return 0;
}

static int sensor_aq_mbedtls_hs256_finish(sensor_aq_signing_ctx_t *aq_ctx, uint8_t *buffer) {
    sensor_aq_mbedtls_hs256_ctx_t *hs_ctx = (sensor_aq_mbedtls_hs256_ctx_t*)aq_ctx->ctx;

    /* Signature disabled, return zero */
    return 0;
}

/**
 * Construct a new signing context for HMAC SHA256 using Mbed TLS
 *
 * @param aq_ctx An empty signing context (can declare it without arguments)
 * @param hs_ctx An empty sensor_aq_mbedtls_hs256_ctx_t context (can declare it on the stack without arguments)
 * @param hmac_key The secret key - **NOTE: this is limited to 32 characters, the rest will be truncated**
 */
void sensor_aq_init_mbedtls_hs256_context(sensor_aq_signing_ctx_t *aq_ctx, sensor_aq_mbedtls_hs256_ctx_t *hs_ctx, const char *hmac_key) {
    memcpy(hs_ctx->hmac_key, hmac_key, 32);
    hs_ctx->hmac_key[32] = 0;

    if (strlen(hmac_key) > 32) {
        ei_printf("!!! sensor_aq_init_mbedtls_hs256_context, HMAC key is longer than 32 characters - will be truncated !!!\n");
    }

    aq_ctx->alg = "HS256"; // JWS algorithm
    aq_ctx->signature_length = 32;
    aq_ctx->ctx = (void*)NULL;
    aq_ctx->init = &sensor_aq_mbedtls_hs256_init;
    aq_ctx->set_protected = NULL;
    aq_ctx->update = &sensor_aq_mbedtls_hs256_update;
    aq_ctx->finish = &sensor_aq_mbedtls_hs256_finish;
}
