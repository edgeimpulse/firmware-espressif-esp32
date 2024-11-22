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

#ifndef EI_POSTPROCESSING_COMMON_H
#define EI_POSTPROCESSING_COMMON_H

#include "edge-impulse-sdk/classifier/ei_model_types.h"

int16_t get_block_number(ei_impulse_handle_t *handle, void *init_func)
{
    for (size_t i = 0; i < handle->impulse->postprocessing_blocks_size; i++) {
        if (handle->impulse->postprocessing_blocks[i].init_fn == init_func) {
            return i;
        }
    }
    return -1;
}

#endif // EI_POSTPROCESSING_COMMON_H