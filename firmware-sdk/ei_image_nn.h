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

#ifndef __EIIMAGENN__H__
#define __EIIMAGENN__H__

#include "firmware-sdk/ei_camera_interface.h"
#include "firmware-sdk/ei_device_interface.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "firmware-sdk/at_base64_lib.h"
#include "firmware-sdk/jpeg/encode_as_jpg.h"

static void respond_and_change_to_max_baud()
{
    auto device = EiDeviceInfo::get_device();
    // sleep a little to let the daemon attach on the new baud rate...
    ei_printf("\r\nOK");
    ei_sleep(100);
    device->set_max_data_output_baudrate();
    ei_sleep(100);
}

static void change_to_normal_baud()
{
    auto device = EiDeviceInfo::get_device();
    // lower baud rate
    ei_printf("\r\nOK\r\n");
    ei_sleep(100);
    device->set_default_data_output_baudrate();
    // sleep a little to let the daemon attach on baud rate 115200 again...
    ei_sleep(100);
}

class EiImageNN {
public:
    EiImageNN(
        uint8_t* image, //let the user provide memory b/c of alignment issues on some platforms
        uint32_t image_size,
        uint32_t image_width,
        uint32_t image_height,
        uint32_t dsp_input_frame_size,
        int classifier_label_count)
        : image(image)
        , image_size(image_size)
        , image_width(image_width)
        , image_height(image_height)
        , dsp_input_frame_size(dsp_input_frame_size)
        , classifier_label_count(classifier_label_count)
    {
    }

    void run_nn(bool debug, int delay_ms, bool use_max_baudrate);
    int cutout_get_data(uint32_t offset, uint32_t length, float *out_ptr);

private:
    uint8_t *image;
    uint32_t image_size;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t dsp_input_frame_size;
    int classifier_label_count;
};

int EiImageNN::cutout_get_data(uint32_t offset, uint32_t length, float *out_ptr)
{
    uint32_t out_ptr_ix = 0;

    //change offset from float pointer (size 4) to packed RGB (1B ptr)
    offset = offset*3;

    while (length != 0) {
        const int R_OFFSET = 0, G_OFFSET = 1, B_OFFSET = 2;
        // clang-format off
        out_ptr[out_ptr_ix] =
            (image[offset + R_OFFSET] << 16) +
            (image[offset + G_OFFSET] << 8) +
            image[offset + B_OFFSET];
        // clang-format on

        // and go to the next pixel
        out_ptr_ix++;
        offset+=3;
        length--;
    }

    // and done!
    return 0;
}

void EiImageNN::run_nn(bool debug, int delay_ms, bool use_max_baudrate)
{
    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tImage resolution: %dx%d\n", image_width, image_height);
    ei_printf("\tFrame size: %d\n", dsp_input_frame_size);
    ei_printf("\tNo. of classes: %d\n", classifier_label_count);

    auto camera = EiCamera::get_camera();
    if (!camera->init(image_width, image_height)) {
        ei_printf("ERR: Failed to initialize image sensor\r\n");
        return;
    }

    if (use_max_baudrate) {
        respond_and_change_to_max_baud();
    }

    while (!ei_user_invoke_stop_lib()) {
        ei::signal_t signal;
        signal.total_length = image_height * image_width; // length of OUTPUT, not input
        signal.get_data = [this](size_t offset, size_t length, float *out_ptr) {
            return this->cutout_get_data(offset, length, out_ptr);
        };

        ei_printf("Taking photo...\n");

        if (!camera->ei_camera_capture_rgb888_packed_big_endian(
                image,
                image_size)) {
            ei_printf("Failed to capture image\r\n");
            break;
        }

        // run the impulse: DSP, neural network and the Anomaly algorithm
        ei_impulse_result_t result = { 0 };

        EI_IMPULSE_ERROR ei_error = run_classifier(&signal, &result, false);
        if (ei_error != EI_IMPULSE_OK) {
            ei_printf("Failed to run impulse (%d)\n", ei_error);
            break;
        }


        // Print framebuffer as JPG during debugging
        if(debug) {
            ei_printf("Begin output\n");
            ei_printf("Framebuffer: ");

            int x = encode_rgb888_signal_as_jpg_and_output_base64(&signal, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
            if (x != 0) {
                ei_printf("Failed to encode frame as JPEG (%d)\n", x);
                break;
            }
            ei_printf("\r\n");
        }

        display_results(&result);

        if (debug) {
            ei_printf("End output\n");
        }


        if (delay_ms != 0) {
            ei_printf("Starting inferencing in %d seconds...\n", delay_ms / 1000);
        }

        // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
        uint64_t end_ms = ei_read_timer_ms() + delay_ms;
        while (end_ms > ei_read_timer_ms()) {
            if (ei_user_invoke_stop_lib()) {
                ei_printf("Inferencing stopped by user\r\n");
                goto CLOSE_AND_EXIT;
            }
        }
    }
CLOSE_AND_EXIT:

    if (use_max_baudrate) {
        change_to_normal_baud();
    }
    camera->deinit();
}

#endif  //!__EIIMAGENN__H__
