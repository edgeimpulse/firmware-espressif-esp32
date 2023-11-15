/*
 * Copyright (c) 2022 EdgeImpulse Inc.
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

#ifndef _EDGE_IMPULSE_SIGNING_NONE_H_
#define _EDGE_IMPULSE_SIGNING_NONE_H_

/**
 * None signing context
 * This is implemented here as it's a required part of JWT, and perhaps some people that don't have Mbed TLS might find it useful
 */

#include <string.h>
#include "sensor_aq.h"

/**
 * Construct a new signing context for none security
 * **NOTE:** This will provide zero verification for your data and data might be rejected by your provider
 *
 * @param aq_ctx An empty signing context (can declare it without arguments)
 */
void sensor_aq_init_none_context(sensor_aq_signing_ctx_t *aq_ctx);

#endif // _EDGE_IMPULSE_SIGNING_NONE_H_
