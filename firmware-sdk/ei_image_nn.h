/* Edge Impulse firmware SDK
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

            size_t jpeg_buffer_size = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT >= 128 * 128 ?
                8192 * 3 :
                4096 * 4;
            uint8_t *jpeg_buffer = NULL;
            jpeg_buffer = (uint8_t*)ei_malloc(jpeg_buffer_size);
            if (!jpeg_buffer) {
                ei_printf("ERR: Failed to allocate JPG buffer\r\n");
                return;
            }

            size_t out_size;
            int x = encode_rgb888_signal_as_jpg(&signal, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, jpeg_buffer, jpeg_buffer_size, &out_size);
            if (x != 0) {
                ei_printf("Failed to encode frame as JPEG (%d)\n", x);
                break;
            }

            ei_printf("Framebuffer: ");
            base64_encode((char*)jpeg_buffer, out_size, ei_putc);
            ei_printf("\r\n");

            if (jpeg_buffer) {
                ei_free(jpeg_buffer);
            }
        }

        // print the predictions
        ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                  result.timing.dsp, result.timing.classification, result.timing.anomaly);
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
        bool bb_found = result.bounding_boxes[0].value > 0;
        for (size_t ix = 0; ix < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; ix++) {
            auto bb = result.bounding_boxes[ix];
            if (bb.value == 0) {
                continue;
            }

            ei_printf("    %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\n", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
        }

        if (!bb_found) {
            ei_printf("    No objects found\n");
        }
#else
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            ei_printf("    %s: %.5f\n", result.classification[ix].label,
                                        result.classification[ix].value);
        }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif
#endif

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
