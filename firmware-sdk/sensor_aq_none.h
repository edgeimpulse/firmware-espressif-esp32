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

#ifndef _EDGE_IMPULSE_SIGNING_NONE_H_
#define _EDGE_IMPULSE_SIGNING_NONE_H_

/**
 * None signing context
 * This is implemented here as it's a required part of JWT, and perhaps some people that don't have Mbed TLS might find it useful
 */

#include <string.h>
#include "sensor_aq.h"

static int sensor_aq_signing_none_init(sensor_aq_signing_ctx_t *aq_ctx) {
    return 0;
}

static int sensor_aq_signing_none_set_protected(struct sensor_aq_signing_ctx* aq_ctx, QCBOREncodeContext* cbor_ctx) {
    // example of adding new fields to the header
    QCBOREncode_AddInt64ToMap(cbor_ctx, "exp", 1564127560);

    return 0;
}

static int sensor_aq_signing_none_update(sensor_aq_signing_ctx_t *aq_ctx, const uint8_t *buffer, size_t buffer_size) {
    return 0;
}

static int sensor_aq_signing_none_finish(sensor_aq_signing_ctx_t *aq_ctx, uint8_t *buffer) {
    // signature will always be zero
    memset(buffer, 0, aq_ctx->signature_length);
    return 0;
}

/**
 * Construct a new signing context for none security
 * **NOTE:** This will provide zero verification for your data and data might be rejected by your provider
 *
 * @param aq_ctx An empty signing context (can declare it without arguments)
 */
void sensor_aq_init_none_context(sensor_aq_signing_ctx_t *aq_ctx) {
    aq_ctx->alg = "none"; // JWS algorithm
    aq_ctx->signature_length = 1;
    aq_ctx->ctx = NULL;
    aq_ctx->init = sensor_aq_signing_none_init;
    aq_ctx->set_protected = sensor_aq_signing_none_set_protected;
    aq_ctx->update = sensor_aq_signing_none_update;
    aq_ctx->finish = sensor_aq_signing_none_finish;
}

#endif // _EDGE_IMPULSE_SIGNING_NONE_H_
