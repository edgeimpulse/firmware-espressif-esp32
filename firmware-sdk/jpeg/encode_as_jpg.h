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
#ifndef ENCODE_AS_JPG_H_
#define ENCODE_AS_JPG_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "JPEGENC.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"
#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "firmware-sdk/at_base64_lib.h"

using namespace ei;

int encode_as_jpg(uint8_t *framebuffer, size_t framebuffer_size, int width, int height, uint8_t *out_buffer, size_t out_buffer_size, size_t *out_size) {
    static JPEGClass jpg;
    JPEGENCODE jpe;

    int rc = jpg.open(out_buffer, out_buffer_size);
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    rc = jpg.encodeBegin(&jpe, width, height, JPEG_PIXEL_GRAYSCALE, JPEG_SUBSAMPLE_444, JPEG_Q_BEST);
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    int imcuCount = ((width + jpe.cx-1)/ jpe.cx) * ((height + jpe.cy-1) / jpe.cy);

    int bytePp = 1;
    int pitch = bytePp * width;

    for (int i=0; i < imcuCount && rc == JPEG_SUCCESS; i++) {
        // pass a pointer to the upper left corner of each MCU
        // the JPEGENCODE structure is updated by addMCU() after
        // each call
        rc = jpg.addMCU(&jpe, &framebuffer[(jpe.x * bytePp) + (jpe.y * pitch)], pitch);
        if (rc != JPEG_SUCCESS) {
            return rc;
        }
    }

    *out_size = jpg.close();

    return 0;
}

int32_t jpeg_write_callback (JPEGFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    base64_encode_chunk((const char *)pBuf, iLen, ei_putchar);
    return 0;
}

void jpeg_close_callback(JPEGFILE *pFile) {
    base64_encode_finish(ei_putchar);
}

void* jpeg_open_callback (const char *szFilename) {
    // file handle isn't used in the internals, just return non NULL.
    return (void *)1;
}

static int encode_bw_signal_as_jpg_common(signal_t *signal, int width, int height, uint8_t *out_buffer, size_t out_buffer_size, size_t *out_size, bool output_directly) {
    static JPEGClass jpg;
    JPEGENCODE jpe;
    float *encode_buffer = NULL;
    uint8_t *encode_buffer_u8 = NULL;

    int rc;
    if (output_directly) {
        rc = jpg.open("image.jpg", jpeg_open_callback, jpeg_close_callback, NULL, jpeg_write_callback, NULL);
    } else {
        rc = jpg.open(out_buffer, out_buffer_size);
    }
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    rc = jpg.encodeBegin(&jpe, width, height, JPEG_PIXEL_GRAYSCALE, JPEG_SUBSAMPLE_444, JPEG_Q_BEST);
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    int imcu_count = ((width + jpe.cx-1)/ jpe.cx) * ((height + jpe.cy-1) / jpe.cy);

    int bytePp = 1;
    int pitch = bytePp * width;

    // We read through the signal paged...
    int buf_len = width * 8;

    int last_offset = 0;
    int max_offset_diff = 0;

    encode_buffer = (float*)ei_malloc(buf_len * 4);
    if (!encode_buffer) {
        rc = JPEG_MEM_ERROR;
        goto cleanup;
    }
    encode_buffer_u8 = (uint8_t*)ei_malloc(buf_len * bytePp);
    if (!encode_buffer_u8) {
        rc = JPEG_MEM_ERROR;
        goto cleanup;
    }

    for (int i = 0; i < imcu_count; i++) {
        // pass a pointer to the upper left corner of each MCU
        // the JPEGENCODE structure is updated by addMCU() after
        // each call

        int offset = jpe.x + (jpe.y * width);

        int available_pixels_to_read = signal->total_length - offset;
        int pixels_to_read = (available_pixels_to_read < buf_len) ? available_pixels_to_read : buf_len;

        rc = signal->get_data(offset, pixels_to_read, encode_buffer);
        if (rc != 0) {
            goto cleanup;
        }

        for (int ix = 0; ix < buf_len; ix++) {
            encode_buffer_u8[ix] = static_cast<uint32_t>(encode_buffer[ix]) & 0xff;
        }

        rc = jpg.addMCU(&jpe, encode_buffer_u8, pitch);
        if (rc != JPEG_SUCCESS) {
            goto cleanup;
        }

        if (offset - last_offset > max_offset_diff) {
            max_offset_diff = offset - last_offset;
        }

        last_offset = offset;
    }

    //ei_printf("Max_offset_diff %d\n", max_offset_diff);

    rc = JPEG_SUCCESS;

cleanup:
    if (output_directly)
        jpg.close();
    else
        *out_size = jpg.close();

    if (encode_buffer) ei_free(encode_buffer);
    if (encode_buffer_u8) ei_free(encode_buffer_u8);

    return rc;
}

int encode_bw_signal_as_jpg(signal_t *signal, int width, int height, uint8_t *out_buffer, size_t out_buffer_size, size_t *out_size) {
    return encode_bw_signal_as_jpg_common(signal, width, height, out_buffer, out_buffer_size, out_size, false);
}

int encode_bw_signal_as_jpg_and_output_base64(signal_t *signal, int width, int height) {
    return encode_bw_signal_as_jpg_common(signal, width, height, NULL, 0, NULL, true);
}


static int encode_rgb888_signal_as_jpg_common(signal_t *signal, int width, int height, uint8_t *out_buffer, size_t out_buffer_size, size_t *out_size, bool output_directly) {
    static JPEGClass jpg;
    JPEGENCODE jpe;
    float *encode_buffer = NULL;
    uint8_t *encode_buffer_u8 = NULL;

    int rc;
    if (output_directly) {
        rc = jpg.open("image.jpg", jpeg_open_callback, jpeg_close_callback, NULL, jpeg_write_callback, NULL);
    } else {
        rc = jpg.open(out_buffer, out_buffer_size);
    }
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    rc = jpg.encodeBegin(&jpe, width, height, JPEG_PIXEL_RGB888, JPEG_SUBSAMPLE_444, JPEG_Q_BEST);
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    int imcu_count = ((width + jpe.cx-1)/ jpe.cx) * ((height + jpe.cy-1) / jpe.cy);

    int bytePp = 3;
    int pitch = bytePp * width;

    // We read through the signal paged...
    int buf_len = width * 8;

    int last_offset = 0;
    int max_offset_diff = 0;

    // encode_buffer in 4 BPP (float32)
    encode_buffer = (float*)ei_malloc(buf_len * 4);
    if (!encode_buffer) {
        rc = JPEG_MEM_ERROR;
        goto cleanup;
    }
    //encode_buffer_u8 in 3 BPP
    encode_buffer_u8 = (uint8_t*)ei_malloc(buf_len * bytePp);
    if (!encode_buffer_u8) {
        rc = JPEG_MEM_ERROR;
        goto cleanup;
    }

    for (int i = 0; i < imcu_count; i++) {
        // pass a pointer to the upper left corner of each MCU
        // the JPEGENCODE structure is updated by addMCU() after
        // each call

        // pixel offset
        int offset = jpe.x  + (jpe.y * width);

        int available_pixels_to_read = signal->total_length - offset;
        int pixels_to_read = (available_pixels_to_read < buf_len) ? available_pixels_to_read : buf_len;

        rc = signal->get_data(offset, pixels_to_read, encode_buffer);
        if (rc != 0) {
            goto cleanup;
        }

        for (int ix = 0; ix < buf_len; ix++) {
            uint32_t pixel = static_cast<uint32_t>(encode_buffer[ix]);
            // pixel pointer to byte pointer
            size_t out_pix_ptr = ix * bytePp;

            // jpeg library expects BGR (LE)
            encode_buffer_u8[out_pix_ptr + 2] = pixel >> 16 & 0xff;  // r
            encode_buffer_u8[out_pix_ptr + 1] = pixel >> 8  & 0xff;  // g
            encode_buffer_u8[out_pix_ptr + 0] = pixel       & 0xff;  // b
        }

        rc = jpg.addMCU(&jpe, encode_buffer_u8, pitch);

        if (rc != JPEG_SUCCESS) {
            goto cleanup;
        }

        if (offset - last_offset > max_offset_diff) {
            max_offset_diff = offset - last_offset;
        }

        last_offset = offset;
    }

    //ei_printf("Max_offset_diff %d\n", max_offset_diff);

    rc = JPEG_SUCCESS;

cleanup:
    if (output_directly)
        jpg.close();
    else
        *out_size = jpg.close();

    if (encode_buffer) ei_free(encode_buffer);
    if (encode_buffer_u8) ei_free(encode_buffer_u8);

    return rc;
}

int encode_rgb888_signal_as_jpg(signal_t *signal, int width, int height, uint8_t *out_buffer, size_t out_buffer_size, size_t *out_size) {
    return encode_rgb888_signal_as_jpg_common(signal, width, height, out_buffer, out_buffer_size, out_size, false);
}

int encode_rgb888_signal_as_jpg_and_output_base64(signal_t *signal, int width, int height) {
    return encode_rgb888_signal_as_jpg_common(signal, width, height, NULL, 0, NULL, true);
}

static int encode_rgb565_signal_as_jpg_common(signal_t *signal, int width, int height, uint8_t *out_buffer, size_t out_buffer_size, size_t *out_size, bool output_directly) {
    static JPEGClass jpg;
    JPEGENCODE jpe;
    float *encode_buffer = NULL;
    uint8_t *encode_buffer_u8 = NULL;

    int rc;
    if (output_directly) {
        rc = jpg.open("image.jpg", jpeg_open_callback, jpeg_close_callback, NULL, jpeg_write_callback, NULL);
    } else {
        rc = jpg.open(out_buffer, out_buffer_size);
    }
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    rc = jpg.encodeBegin(&jpe, width, height, JPEG_PIXEL_RGB565, JPEG_SUBSAMPLE_444, JPEG_Q_BEST);
    if (rc != JPEG_SUCCESS) {
        return rc;
    }

    int imcu_count = ((width + jpe.cx-1)/ jpe.cx) * ((height + jpe.cy-1) / jpe.cy);

    int bytePp = 2;
    int pitch = bytePp * width;

    // We read through the signal paged...
    int buf_len = width * 8;

    int last_offset = 0;
    int max_offset_diff = 0;

    // encode_buffer in 4 BPP (float32)
    encode_buffer = (float*)ei_malloc(buf_len * 4);
    if (!encode_buffer) {
        rc = JPEG_MEM_ERROR;
        goto cleanup;
    }
    //encode_buffer_u8 in 2 BPP
    encode_buffer_u8 = (uint8_t*)ei_malloc(buf_len * bytePp);
    if (!encode_buffer_u8) {
        rc = JPEG_MEM_ERROR;
        goto cleanup;
    }

    for (int i = 0; i < imcu_count; i++) {
        // pass a pointer to the upper left corner of each MCU
        // the JPEGENCODE structure is updated by addMCU() after
        // each call

        // pixel offset
        int offset = jpe.x  + (jpe.y * width);

        int available_pixels_to_read = signal->total_length - offset;
        int pixels_to_read = (available_pixels_to_read < buf_len) ? available_pixels_to_read : buf_len;

        rc = signal->get_data(offset, pixels_to_read, encode_buffer);
        if (rc != 0) {
            goto cleanup;
        }

        for (int ix = 0; ix < pixels_to_read; ix++) {
            uint32_t pixel = static_cast<uint32_t>(encode_buffer[ix]);
            // pixel pointer to byte pointer
            size_t out_pix_ptr = ix * bytePp;

            uint8_t r, g, b;
            r = (pixel >> 19) & 0x1f;
            g = (pixel >> 10) & 0x3f;
            b = (pixel >>  3) & 0x1f;

            encode_buffer_u8[out_pix_ptr + 1] = (r << 3) | ((g >> 3) & 0x7);
            encode_buffer_u8[out_pix_ptr + 0] = ((g & 0x7) << 5) | b ;
        }

        rc = jpg.addMCU(&jpe, encode_buffer_u8, pitch);

        if (rc != JPEG_SUCCESS) {
            goto cleanup;
        }

        if (offset - last_offset > max_offset_diff) {
            max_offset_diff = offset - last_offset;
        }

        last_offset = offset;
    }

    //ei_printf("Max_offset_diff %d\n", max_offset_diff);

    rc = JPEG_SUCCESS;

cleanup:
    if (output_directly)
        jpg.close();
    else
        *out_size = jpg.close();

    if (encode_buffer) ei_free(encode_buffer);
    if (encode_buffer_u8) ei_free(encode_buffer_u8);

    return rc;
}

int encode_rgb565_signal_as_jpg(signal_t *signal, int width, int height, uint8_t *out_buffer, size_t out_buffer_size, size_t *out_size) {
    return encode_rgb565_signal_as_jpg_common(signal, width, height, out_buffer, out_buffer_size, out_size, false);
}

int encode_rgb565_signal_as_jpg_and_output_base64(signal_t *signal, int width, int height) {
    return encode_rgb565_signal_as_jpg_common(signal, width, height, NULL, 0, NULL, true);
}


#endif // ENCODE_AS_JPG_H
