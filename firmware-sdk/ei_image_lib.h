/* Edge Impulse firmware SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
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
