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

#ifndef EI_POSTPROCESSING_TYPES_H
#define EI_POSTPROCESSING_TYPES_H

#include "edge-impulse-sdk/classifier/ei_model_types.h"

EI_IMPULSE_ERROR init_perfcal(ei_impulse_handle_t *handle, void *config);
EI_IMPULSE_ERROR deinit_perfcal(ei_impulse_handle_t *handle, void *config);
EI_IMPULSE_ERROR process_perfcal(ei_impulse_handle_t *handle,
                                 ei_impulse_result_t *result,
                                 void *config,
                                 bool debug);

#endif // EI_POSTPROCESSING_TYPES_H