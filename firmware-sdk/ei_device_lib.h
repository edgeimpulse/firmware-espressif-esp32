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

#ifndef EI_DEVICE_LIB_H
#define EI_DEVICE_LIB_H

#include <cstdint>
#include <cstddef>
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

/**
 * @brief      Call this function periocally during inference to
 *             detect a user stop command
 *
 * @return     true if user requested stop
 */
bool ei_user_invoke_stop_lib(void);

/**
 * @brief Helper function for sending a data from memory over the
 * serial port. Data are encoded into base64 on the fly.
 *
 * @param address address of samples
 * @param length number of samples (bytes)
 * @return true if eferything went fin
 * @return false if some error occured (error during samples read)
 */
bool read_encode_send_sample_buffer(size_t address, size_t length);

bool run_impulse_static_data(bool debug, size_t length, size_t buf_len);

EI_IMPULSE_ERROR ei_start_impulse_static_data(bool debug, float* data, size_t size);

#endif /* EI_DEVICE_LIB_H */
