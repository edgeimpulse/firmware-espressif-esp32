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

#ifndef EI_IMAGE_LIB_H
#define EI_IMAGE_LIB_H

#include "stdint.h"

// ********* Functions for AT commands

/**
 * @brief Use to output an image as base64.  Assign this to an AT command
 *
 * @param width Width in pixels
 * @param height Height in pixels
 * @param use_max_baudrate Use the fast baud rate for transfer
 * @return true If successful
 * @return false If failure
 */
bool ei_camera_take_snapshot_output_on_serial(size_t width, size_t height, bool use_max_baudrate);


/**
 * @brief Use to output an image as base64, over and over.  Assign this to an AT command
 * Calls ei_camera_take_snapshot_encode_and_output() in a loop until a char is received on the UART
 *
 * @param width Width in pixels
 * @param height Height in pixels
 * @param use_max_baudrate Use the fast baud rate for transfer
 * @return true If successful
 * @return false If failure
 */
bool ei_camera_start_snapshot_stream(size_t width, size_t height, bool use_max_baudrate);




#endif /* EI_IMAGE_LIB_H */
