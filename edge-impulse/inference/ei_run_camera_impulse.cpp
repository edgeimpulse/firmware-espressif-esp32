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

/* Include ----------------------------------------------------------------- */
#include "model-parameters/model_metadata.h"

#if defined(EI_CLASSIFIER_SENSOR) && EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "edge-impulse-sdk/classifier/ei_print_results.h"
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "ei_camera.h"
#include "firmware-sdk/at_base64_lib.h"
#include "firmware-sdk/jpeg/encode_as_jpg.h"
#include "stdint.h"
#include "ei_device_espressif_esp32.h"
#include "ei_run_impulse.h"

#include "esp_timer.h"

#define DWORD_ALIGN_PTR(a)   ((a & 0x3) ?(((uintptr_t)a + 0x4) & ~(uintptr_t)0x3) : a)

typedef enum {
    INFERENCE_STOPPED,
    INFERENCE_WAITING,
    INFERENCE_SAMPLING,
    INFERENCE_DATA_READY
} inference_state_t;

static inference_state_t state = INFERENCE_STOPPED;
static uint64_t last_inference_ts = 0;

static bool debug_mode = false;
static bool continuous_mode = false;

static uint8_t *snapshot_buf = nullptr;
static uint32_t snapshot_buf_size;

static ei_device_snapshot_resolutions_t snapshot_resolution;
static ei_device_snapshot_resolutions_t fb_resolution;

static bool resize_required = false;
static uint32_t inference_delay;

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix + 2];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }

    // and done!
    return 0;
}

void ei_run_impulse(void)
{
    switch(state) {
        case INFERENCE_STOPPED:
            // nothing to do
            return;
        case INFERENCE_WAITING:
            if(ei_read_timer_ms() < (last_inference_ts + inference_delay)) {
                return;
            }
            state = INFERENCE_DATA_READY;
            break;
        case INFERENCE_SAMPLING:
        case INFERENCE_DATA_READY:
            if(continuous_mode == true) {
                state = INFERENCE_WAITING;
            }
            break;
        default:
            break;
    }

    uint8_t *jpeg_image;
    uint32_t jpeg_image_size = 0;

    EiCameraESP32 *camera = static_cast<EiCameraESP32*>(EiCameraESP32::get_camera());

    ei_printf("Taking photo...\n");

    if(camera->ei_camera_capture_jpeg(&jpeg_image, &jpeg_image_size) == false) {
        ei_printf("ERR: Failed to take a snapshot!\n");
        return;
    }

    snapshot_buf = (uint8_t*)ei_malloc(snapshot_buf_size);

    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    if(camera->ei_camera_jpeg_to_rgb888(jpeg_image, jpeg_image_size, snapshot_buf) == false) {
        ei_printf("ERR: Failed to decode JPEG image\n");
        ei_free(snapshot_buf);
        ei_free(jpeg_image);
        return;
    }

    ei_free(jpeg_image);
    jpeg_image_size = 0;

    int64_t fr_start = esp_timer_get_time();

    if (resize_required) {
        ei::image::processing::crop_and_interpolate_rgb888(
            snapshot_buf,
            fb_resolution.width,
            fb_resolution.height,
            snapshot_buf,
            snapshot_resolution.width,
            snapshot_resolution.height);
    }
    int64_t fr_end = esp_timer_get_time();

    if (debug_mode) {
        ei_printf("Time resizing: %d\n", (uint32_t)((fr_end - fr_start)/1000));
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    // print and discard JPEG buffer before inference to free some memory
    if (debug_mode) {
        ei_printf("Begin output\n");
        ei_printf("Framebuffer: ");
        // base64_encode((const char*)jpeg_image, jpeg_image_size, &ei_putchar);
        int ret = encode_rgb888_signal_as_jpg_and_output_base64(&signal, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
        ei_printf("\r\n");
        if(ret != 0) {
            ei_printf("ERR: Failed to encode frame as JPEG (%d)\n", ret);
        }
    }

    // run the impulse: DSP, neural network and the Anomaly algorithm
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR ei_error = run_classifier(&signal, &result, false);
    if (ei_error != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run impulse (%d)\n", ei_error);
        ei_free(snapshot_buf);
        return;
    }
    ei_free(snapshot_buf);

    ei_print_results(&ei_default_impulse, &result);

    if (debug_mode) {
        ei_printf("\r\n----------------------------------\r\n");
        ei_printf("End output\r\n");
    }

    if(continuous_mode == false) {
        ei_printf("Starting inferencing in %d seconds...\n", inference_delay / 1000);
    }
}

void ei_start_impulse(bool continuous, bool debug, bool use_max_uart_speed)
{
    snapshot_resolution.width = EI_CLASSIFIER_INPUT_WIDTH;
    snapshot_resolution.height = EI_CLASSIFIER_INPUT_HEIGHT;

    debug_mode = debug;
    continuous_mode = continuous;

    EiDeviceESP32* dev = static_cast<EiDeviceESP32*>(EiDeviceESP32::get_device());
    EiCameraESP32 *camera = static_cast<EiCameraESP32*>(EiCameraESP32::get_camera());

    // check if minimum suitable sensor resolution is the same as
    // desired snapshot resolution
    // if not we need to resize later
    fb_resolution = camera->search_resolution(snapshot_resolution.width, snapshot_resolution.height);

    if (snapshot_resolution.width != fb_resolution.width || snapshot_resolution.height != fb_resolution.height) {
        resize_required = true;
    }

    if (!camera->init(snapshot_resolution.width, snapshot_resolution.height)) {
        ei_printf("Failed to init camera, check if camera is connected!\n");
        return;
    }

    snapshot_buf_size = fb_resolution.width * fb_resolution.height * 3;

    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tImage resolution: %dx%d\n", EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

    if(continuous_mode == true) {
        inference_delay = 0;
        state = INFERENCE_DATA_READY;
    }
    else {
        inference_delay = 2000;
        state = INFERENCE_WAITING;
        ei_printf("Starting inferencing in %d seconds...\n", inference_delay / 1000);
    }

    if (use_max_uart_speed) {
        ei_printf("OK\r\n");
        ei_sleep(100);
        dev->set_max_data_output_baudrate();
        ei_sleep(100);
    }

    while(!ei_user_invoke_stop()) {
        ei_run_impulse();
        ei_sleep(1);
    }

    ei_stop_impulse();

    if (use_max_uart_speed) {
        ei_printf("\r\nOK\r\n");
        ei_sleep(100);
        dev->set_default_data_output_baudrate();
        ei_sleep(100);
    }

}

void ei_stop_impulse(void)
{
    state = INFERENCE_STOPPED;
}

bool is_inference_running(void)
{
    return (state != INFERENCE_STOPPED);
}

#endif /* defined(EI_CLASSIFIER_SENSOR) && EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA */