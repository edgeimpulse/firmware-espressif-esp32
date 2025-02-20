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

#include "at_base64_lib.h"
#include "ei_device_lib.h"
#include "ei_device_info_lib.h"
#include "ei_device_memory.h"
#include "ei_device_interface.h"

#include "edge-impulse-sdk/classifier/ei_classifier_types.h"
#include "edge-impulse-sdk/classifier/ei_signal_with_axes.h"

extern "C" EI_IMPULSE_ERROR run_classifier(
    signal_t *signal,
    ei_impulse_result_t *result,
    bool debug = false);

float *features;
extern char* ei_classifier_inferencing_categories[];

/**
 * @brief      Call this function periocally during inference to
 *             detect a user stop command
 *
 * @return     true if user requested stop
 */
__attribute__((weak)) bool ei_user_invoke_stop_lib(void)
{
    char ch;
    while(1) {
        ch = ei_getchar();
        if(ch == 0) { return false; }
        if(ch == 'b') { return true; }
    }
}

/**
 * @brief Helper function for sending a data from memory over the
 * serial port. Data are encoded into base64 on the fly.
 *
 * @param address address of samples
 * @param length number of samples (bytes)
 * @return true if eferything went fin
 * @return false if some error occured (error during samples read)
 */
__attribute__((weak)) bool read_encode_send_sample_buffer(size_t address, size_t length)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    EiDeviceMemory *memory = dev->get_memory();
    // we are encoiding data into base64, so it needs to be divisible by 3
    const int buffer_size = 513;
    uint8_t* buffer = (uint8_t*)ei_malloc(buffer_size);

    while (1) {
        size_t bytes_to_read = buffer_size;

        if (bytes_to_read > length) {
            bytes_to_read = length;
        }

        if (bytes_to_read == 0) {
            ei_free(buffer);
            return true;
        }

        if (memory->read_sample_data(buffer, address, bytes_to_read) != bytes_to_read) {
            ei_free(buffer);
            return false;
        }

        base64_encode((char *)buffer, bytes_to_read, ei_putchar);

        address += bytes_to_read;
        length -= bytes_to_read;
    }

    return true;
}

bool run_impulse_static_data(bool debug, size_t length, size_t buf_len)
{
    size_t cur_pos = 0;
    uint32_t buf_pos = 0;
    uint64_t start_time = 0;

    static float *data_pt = NULL;
    static uint8_t *temp_buf = NULL;

    if(buf_len < 6) {
        ei_printf("ERR: Minimum buffer length should be 6\r\n");
        return false;
    }

    data_pt = (float*)ei_malloc(length*sizeof(float));
    if (data_pt == NULL) {
        ei_printf("ERR: Memory allocation for data buffer failed\r\n");
        return false;
    }

    temp_buf = (uint8_t*)ei_calloc(buf_len + 1, sizeof(uint8_t));
    if (temp_buf == NULL) {
        ei_printf("ERR: Memory allocation for serial read buffer failed\r\n");
        ei_free(data_pt);
        data_pt = NULL;
        return false;
    }

    ei_printf("OK CHUNK=%d\r\n", (int)buf_len);

    while (cur_pos < length) {

        start_time = ei_read_timer_ms();
        while (buf_pos < buf_len) {
            if (ei_read_timer_ms() - start_time > 100) {
                ei_printf("TIMEOUT\r\n");
                ei_free(data_pt);
                ei_free(temp_buf);
                data_pt = NULL;
                temp_buf = NULL;
                ei_printf("END OUTPUT\r\n");
                return false;
            }
            uint8_t rec = ei_getchar();
            if (rec != 0) {
                temp_buf[buf_pos++] = rec;
            }
        }

        std::vector<unsigned char> decoded = base64_decode((const char*)temp_buf);

        int copylength = decoded.size() > (length - cur_pos) * sizeof(float)
                       ? (length - cur_pos) * sizeof(float)
                       : decoded.size();

        memcpy((void*)(data_pt + cur_pos), decoded.data(), copylength);

        cur_pos = cur_pos + copylength/sizeof(float);
        buf_pos = 0;
        ei_printf("OK %d \r\n", (int)cur_pos);
    }

    ei_printf("TRANSFER COMPLETED %d\r\n", (int)cur_pos);
    uint32_t res = (uint32_t)ei_start_impulse_static_data(debug, data_pt, cur_pos);
    cur_pos = 0;
    ei_free(data_pt);
    ei_free(temp_buf);
    data_pt = NULL;
    temp_buf = NULL;
    ei_printf("RESULT %d\r\n", res);
    ei_printf("END OUTPUT\r\n");

    return true;
}

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

EI_IMPULSE_ERROR ei_start_impulse_static_data(bool debug, float* data, size_t size) {

    features = data;

    signal_t signal;            // Wrapper for raw input buffer
    ei_impulse_result_t result = {0}; // Used to store inference output
    EI_IMPULSE_ERROR res;       // Return code from inference

    // Make sure that the length of the buffer matches expected input length
    if (size != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        ei_printf("ERROR: The size of the input buffer is not correct.\r\n");
        ei_printf("Expected %d items, but got %d\r\n",
                EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
                (int)size);
        return EI_IMPULSE_ERROR_SHAPES_DONT_MATCH;
    }

    // Assign callback function to fill buffer used for preprocessing/inference
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &raw_feature_get_data;

    // Perform DSP pre-processing and inference
    res = run_classifier(&signal, &result, debug);

    // Print return code and how long it took to perform inference
    if(result.timing.dsp_us != 0) {
        ei_printf("Timing: DSP %.3f ms, inference %.3f ms, anomaly %.3f ms\r\n",
                result.timing.dsp_us / 1000.f,
                result.timing.classification_us / 1000.f,
                result.timing.anomaly_us / 1000.f);
    }
    else {
        ei_printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
                result.timing.dsp,
                result.timing.classification,
                result.timing.anomaly);
    }

    // Print the prediction results (object detection)
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    ei_printf("Object detection bounding boxes:\r\n");
    for (uint32_t i = 0; i < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);
    }

    // Print the prediction results (classification)
#else
    ei_printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
        ei_printf_float(result.classification[i].value);
        ei_printf("\r\n");
    }
#endif

    // Print anomaly result (if it exists)
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("Anomaly prediction: ");
    ei_printf_float(result.anomaly);
    ei_printf("\r\n");
#endif
    return res;
}
