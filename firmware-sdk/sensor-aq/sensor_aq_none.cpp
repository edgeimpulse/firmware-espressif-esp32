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

/**
 * None signing context
 * This is implemented here as it's a required part of JWT, and perhaps some people that don't have Mbed TLS might find it useful
 */
#include <string.h>
#include "sensor_aq.h"
#include "sensor_aq_none.h"

static int sensor_aq_signing_none_init(sensor_aq_signing_ctx_t *aq_ctx) {
    return 0;
}

static int sensor_aq_signing_none_update(sensor_aq_signing_ctx_t *aq_ctx, const uint8_t *buffer, size_t buffer_size) {
    return 0;
}

static int sensor_aq_signing_none_finish(sensor_aq_signing_ctx_t *aq_ctx, uint8_t *buffer) {
    return 0;
}

/**
 * Construct a new signing context for none security
 * **NOTE:** This will provide zero verification for your data and data might be rejected by your provider
 *
 * @param aq_ctx An empty signing context (can declare it without arguments)
 */
void sensor_aq_init_none_context(sensor_aq_signing_ctx_t *aq_ctx) {
    aq_ctx->alg = "none";
    aq_ctx->signature_length = 1;
    aq_ctx->ctx = NULL;
    aq_ctx->init = sensor_aq_signing_none_init;
    aq_ctx->set_protected = NULL;
    aq_ctx->update = sensor_aq_signing_none_update;
    aq_ctx->finish = sensor_aq_signing_none_finish;
}
