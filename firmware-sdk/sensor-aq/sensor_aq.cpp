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



#include <stdint.h>
#include <float.h>
//#include "qcbor.h"
//#include "setup.h"
#include "sensor_aq.h"


extern void ei_printf(const char *format, ...);

// Valid SenML Units (as per https://www.iana.org/assignments/senml/senml.xhtml)
static const char* valid_senml_units[] = { "m", "kg", "s", "A", "K", "cd", "mol", "Hz", "rad",
                                            "sr", "N", "Pa", "J", "W", "C", "V", "F", "Ohm", "S", "Wb",
                                            "T", "H", "Cel", "lm", "lx", "Bq", "Gy", "Sv", "kat", "m2",
                                            "m3", "m/s", "m/s2", "m3/s", "W/m2", "cd/m2",
                                            "bit", "bit/s", "lat", "lon", "pH", "db", "dBW",
                                            "count", "//", "%RH", "%EL", "EL", "1/s",
                                            "S/m", "B", "VA", "var", "J/m" };



/**
 * Write a buffer to the stream
 */
static size_t ei_fwrite(sensor_aq_ctx *ctx, const void *ptr, size_t size, size_t count) {
    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    size_t items_written = ctx->fwrite(ptr, size, count, ctx->stream);

    return items_written;
}

/**
 * Set position of the stream
 */
static int ei_fseek(sensor_aq_ctx *ctx, long int offset, int origin) {
    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    return ctx->fseek(ctx->stream, offset, origin);
}

/**
 * Determine if the unit is a valid SenML unit
 */
__attribute__((unused)) static bool is_valid_senml_unit(const char *unit) {
    for (size_t ix = 0; ix < sizeof(valid_senml_units) / sizeof(valid_senml_units[0]); ix++) {
        if (strcmp(valid_senml_units[ix], unit) == 0) {
            return true;
        }
    }
    return false;
}

static int sensor_aq_update_sig_and_write_to_file(sensor_aq_ctx *ctx, uint8_t *ptr, size_t size) {
    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    // Update the signature
    int ctx_err = ctx->signature_ctx->update(ctx->signature_ctx, ptr, size);
    if (ctx_err != 0) {
        return ctx_err;
    }

    // write to file
    if (ei_fwrite(ctx, ptr, 1, size) != size) {
        return AQ_STREAM_WRITE_FAILED;
    }

    // clear memory
    memset(ptr, 0, size);

    return AQ_OK;
}

static int sensor_aq_flush_buffer(sensor_aq_ctx *ctx) {
    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    UsefulBufC encoded = UsefulOutBuf_OutUBuf(&(ctx->encode_context.OutBuf));

    QCBORError res = QCBOREncode_Finish(&ctx->encode_context, &encoded);
    if (res != QCBOR_SUCCESS) {
        return res;
    }

    int err = sensor_aq_update_sig_and_write_to_file(ctx, (uint8_t*)encoded.ptr, encoded.len);
    if (err != AQ_OK) {
        return err;
    }

    // re-initialize
    QCBOREncode_Init(&ctx->encode_context, ctx->cbor_buffer);

    return AQ_OK;
}

/**
 * Initialize a sensor acquisition context
 *
 * @param ctx An uninitialized context
 * @param stream An opened stream (e.g. to a file) with write permissions
 * @param
 *
 * @returns Status code (0 = OK, -60xx error from aq_init, positive number is error from QCBOR)
 */
int sensor_aq_init(sensor_aq_ctx *ctx, sensor_aq_payload_info *payload_info, EI_SENSOR_AQ_STREAM *stream, bool allow_empty_stream = false) {
    if (ctx == NULL) {
        return AQ_CTX_IS_NULL;
    }
    if (payload_info == NULL) {
        return AQ_PAYLOAD_INFO_IS_NULL;
    }
    if (stream == NULL && !allow_empty_stream) {
        return AQ_STREAM_IS_NULL;
    }

    // split up the buffers
    if (ctx->signature_ctx->signature_length >= ctx->buffer.size) {
        return AQ_SIGNATURE_BUFFER_DOES_NOT_FIT;
    }

    // reserve upper bytes for the hash buffer
    ctx->hash_buffer.size = ctx->signature_ctx->signature_length * 2; // encoding as hex, so times 2
    ctx->hash_buffer.buffer = (ctx->buffer.buffer + ctx->buffer.size) - ctx->hash_buffer.size;

   // empty out the ctx buffer
    memset(ctx->buffer.buffer, 0, ctx->buffer.size);

    // clear the hash, as we're inserting this into the CBOR representation and need to find it back later
    memset(ctx->hash_buffer.buffer, '0', ctx->hash_buffer.size);

    // and the rest for the CBOR buffer
    ctx->cbor_buffer.len = ctx->buffer.size - ctx->hash_buffer.size;
    ctx->cbor_buffer.ptr = ctx->buffer.buffer;

    // store the stream
    ctx->stream = stream;

    // verify that all methods are wired
    if (!ctx->fwrite) {
        return AQ_FWRITE_IS_NULL;
    }
    if (!ctx->fseek) {
        return AQ_FSEEK_IS_NULL;
    }
    if (ctx->signature_ctx == NULL) {
        return AQ_SIGNATURE_CTX_IS_NULL;
    }
    if (ctx->signature_ctx->init == NULL) {
        return AQ_SIGNATURE_INIT_IS_NULL;
    }
    if (ctx->signature_ctx->update == NULL) {
        return AQ_SIGNATURE_UPDATE_IS_NULL;
    }
    if (ctx->signature_ctx->finish == NULL) {
        return AQ_SIGNATURE_FINISH_IS_NULL;
    }
    if (payload_info->device_type == NULL) {
        return AQ_DEVICE_TYPE_IS_NULL;
    }

    int ctx_err = ctx->signature_ctx->init(ctx->signature_ctx);
    if (ctx_err != 0) {
        return ctx_err;
    }
    //int ctx_err;

    ctx->axis_count = 0;

    QCBOREncode_Init(&ctx->encode_context, ctx->cbor_buffer);
    QCBOREncode_OpenMap(&ctx->encode_context);

    // protected
    {
        QCBOREncode_OpenMapInMap(&ctx->encode_context, "protected");
        UsefulBufC version = { "v1", strlen("v1") };
        QCBOREncode_AddTextToMap(&ctx->encode_context, "ver", version);
        UsefulBufC alg = { ctx->signature_ctx->alg, strlen(ctx->signature_ctx->alg) };
        QCBOREncode_AddTextToMap(&ctx->encode_context, "alg", alg);

        // @todo: bring back time to the SDK
        //if (ctx->time != NULL) {
        //    time_t t = 12345678;//ctx->time(NULL);
        //    QCBOREncode_AddInt64ToMap(&ctx->encode_context, "iat", static_cast<int64_t>(t));
        //}

        if (ctx->signature_ctx->set_protected != NULL) {
            ctx->signature_ctx->set_protected(ctx->signature_ctx, &ctx->encode_context);
        }

        QCBOREncode_CloseMap(&ctx->encode_context);
    }

    // signature (empty at the moment)
    {
        UsefulBufC sig = { ctx->hash_buffer.buffer, ctx->hash_buffer.size };
        QCBOREncode_AddTextToMap(&ctx->encode_context, "signature", sig);
    }

    // start payload
    {
        QCBOREncode_OpenMapInMap(&ctx->encode_context, "payload");

        if (payload_info->device_name != NULL) {
            UsefulBufC name = { payload_info->device_name, strlen(payload_info->device_name) };
            QCBOREncode_AddTextToMap(&ctx->encode_context, "device_name", name);
        }
        UsefulBufC devtype = { payload_info->device_type, strlen(payload_info->device_type) };
        QCBOREncode_AddTextToMap(&ctx->encode_context, "device_type", devtype);
        QCBOREncode_AddDoubleToMap(&ctx->encode_context, "interval_ms", payload_info->interval_ms);

        QCBOREncode_OpenArrayInMap(&ctx->encode_context, "sensors");

        for (size_t ix = 0; ix < EI_MAX_SENSOR_AXES; ix++) {
            if (payload_info->sensors[ix].name == NULL || strlen(payload_info->sensors[ix].name) == 0 || payload_info->sensors[ix].units == NULL) {
                continue;
            }

            // @todo: WAV is not a valid SenML type but I still want it, let's skip this for now
            // if (!is_valid_senml_unit(payload_info->sensors[ix].units)) {
            //     return AQ_NOT_VALID_SENML_TYPE;
            // }

            ctx->axis_count++;

            UsefulBufC units = { payload_info->sensors[ix].units, strlen(payload_info->sensors[ix].units) };

            QCBOREncode_OpenMap(&ctx->encode_context);
            UsefulBufC name = { payload_info->sensors[ix].name, strlen(payload_info->sensors[ix].name) };
            QCBOREncode_AddTextToMap(&ctx->encode_context, "name", name);
            QCBOREncode_AddTextToMap(&ctx->encode_context, "units", units);
            QCBOREncode_CloseMap(&ctx->encode_context);
        }

        QCBOREncode_CloseArray(&ctx->encode_context);

        QCBOREncode_OpenArrayIndefiniteLengthInMap(&ctx->encode_context, "values");

        // we're making this empty array here...
        // the reason is that the decoder will now emit 9F (start indefinite size array) FF (break) at the end of the file
        // this is great because we can just strip off the last FF byte and start appending arrays
        // this way we don't have to hold anything in memory

        QCBOREncode_CloseArrayIndefiniteLength(&ctx->encode_context);

        QCBOREncode_CloseMap(&ctx->encode_context);
    }

    QCBOREncode_CloseMap(&ctx->encode_context);

    UsefulBufC encoded = UsefulOutBuf_OutUBuf(&(ctx->encode_context.OutBuf));

    QCBORError res = QCBOREncode_Finish(&ctx->encode_context, &encoded);
    if (res != QCBOR_SUCCESS) {
        return res;
    }

    // we'll skip over the last byte (the break byte of the array)
    encoded.len -= 1;

    // OK, one last thing (while we have everything in memory) is to find out at which position the signature is located...
    size_t sig_ix = 0;
    for (size_t ix = 0; ix < encoded.len - ctx->hash_buffer.size; ix++) {
        if (memcmp(((uint8_t*)encoded.ptr) + ix, ctx->hash_buffer.buffer, ctx->hash_buffer.size) == 0) {
            sig_ix = ix;
            break;
        }
    }

    if (sig_ix == 0) {
        return AQ_SIGNATURE_NOT_FOUND_IN_CBOR;
    }

    ctx->signature_index = sig_ix;

    // Update the signature
    ctx_err = ctx->signature_ctx->update(ctx->signature_ctx, (const uint8_t*)encoded.ptr, encoded.len);
    if (ctx_err != 0) {
        return ctx_err;
    }

    if (stream != NULL) {
        // write the first part to the file
        if (ei_fwrite(ctx, encoded.ptr, 1, encoded.len) != encoded.len) {
            return AQ_STREAM_WRITE_FAILED;
        }
    }

    return AQ_OK;
}

/**
 * Add data to the sensor file for a single interval
 * @param ctx The context
 * @param values Values for the current frame
 * @param values_size Size of the values
 */
int sensor_aq_add_data(sensor_aq_ctx *ctx, float values[], size_t values_size) {
    if (values_size != ctx->axis_count) {
        return AQ_VALUES_SIZE_DOES_NOT_MATCH_AXIS_COUNT;
    }

    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    // clear memory
    memset(ctx->cbor_buffer.ptr, 0, ctx->cbor_buffer.len);

    // re-initialize
    QCBOREncode_Init(&ctx->encode_context, ctx->cbor_buffer);

    // If we only have a single axis then emit flattened array (saves space)
    if (values_size == 1) {
        QCBOREncode_AddDouble(&ctx->encode_context, values[0]);
    }
    else {
        // otherwise create an array
        QCBOREncode_OpenArray(&ctx->encode_context);

        for (size_t ix = 0; ix < values_size; ix++) {
            QCBOREncode_AddDouble(&ctx->encode_context, values[ix]);
        }

        QCBOREncode_CloseArray(&ctx->encode_context);
    }

    return sensor_aq_flush_buffer(ctx);
}

/**
 * Add data to the sensor file for a single interval
 * @param ctx The context
 * @param values Values for the current frame
 * @param values_size Size of the values
 */
int sensor_aq_add_data_i16(sensor_aq_ctx *ctx, int16_t values[], size_t values_size) {
    if (values_size != ctx->axis_count) {
        return AQ_VALUES_SIZE_DOES_NOT_MATCH_AXIS_COUNT;
    }

    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    // clear memory
    memset(ctx->cbor_buffer.ptr, 0, ctx->cbor_buffer.len);

    // re-initialize
    QCBOREncode_Init(&ctx->encode_context, ctx->cbor_buffer);

    // If we only have a single axis then emit flattened array (saves space)
    if (values_size == 1) {
        QCBOREncode_AddInt64(&ctx->encode_context, values[0]);
    }
    else {
        // otherwise create an array
        QCBOREncode_OpenArray(&ctx->encode_context);

        for (size_t ix = 0; ix < values_size; ix++) {
            QCBOREncode_AddInt64(&ctx->encode_context, values[ix]);
        }

        QCBOREncode_CloseArray(&ctx->encode_context);
    }

    return sensor_aq_flush_buffer(ctx);
}

/**
 * Add data to the sensor file for many intervals at the same time
 * This only works if there is only a single sensor
 * @param ctx The context
 * @param values Values
 * @param values_size Size of the values array
 */
// int sensor_aq_add_data_batch(sensor_aq_ctx *ctx, float values[], size_t values_size) {
//     if (ctx->axis_count != 1) {
//         return AQ_BATCH_ONLY_SUPPORTS_SINGLE_AXIS;
//     }

//     for (size_t ix = 0; ix < values_size; ix++) {
//         float new_values[1] = { values[ix] };
//         int r = sensor_aq_add_data(ctx, new_values, 1);
//         if (r != AQ_OK) {
//             return r;
//         }
//     }

//     return AQ_OK;
// }

/**
 * Add data to the sensor file for many intervals at the same time
 * This only works if there is only a single sensor
 * @param ctx The context
 * @param values Values
 * @param values_size Size of the values array
 */
int sensor_aq_add_data_batch(sensor_aq_ctx *ctx, int16_t values[], size_t values_size) {
    if (ctx->axis_count != 1) {
        return AQ_BATCH_ONLY_SUPPORTS_SINGLE_AXIS;
    }

    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    // clear memory
    memset(ctx->cbor_buffer.ptr, 0, ctx->cbor_buffer.len);

    // re-initialize
    QCBOREncode_Init(&ctx->encode_context, ctx->cbor_buffer);

    for (size_t ix = 0; ix < values_size; ix++) {
        QCBOREncode_AddInt64(&ctx->encode_context, values[ix]);

        // within 8 bytes of the size of the buffer...
        if (ctx->encode_context.OutBuf.data_len >= ctx->cbor_buffer.len - 8) {
            int fr = sensor_aq_flush_buffer(ctx);
            if (fr != AQ_OK) {
                return fr;
            }
        }
    }

    return sensor_aq_flush_buffer(ctx);
}

int sensor_aq_finish(sensor_aq_ctx *ctx) {
    uint8_t final_byte[] = { 0xff };

    if (ctx->stream == NULL) {
        return AQ_STREAM_IS_NULL;
    }

    // Update the signature
    int ctx_err = ctx->signature_ctx->update(ctx->signature_ctx, final_byte, 1);
    if (ctx_err != 0) {
        return ctx_err;
    }

    // write to file
    if (ei_fwrite(ctx, final_byte, 1, 1) != 1) {
        return AQ_STREAM_WRITE_FAILED;
    }

    // finish the signing
    ctx_err = ctx->signature_ctx->finish(ctx->signature_ctx, ctx->hash_buffer.buffer);
    if (ctx_err != 0) {
        return ctx_err;
    }

    if (ei_fseek(ctx, ctx->signature_index, SEEK_SET) != 0) {
        return AQ_STREAM_FSEEK_FAILED;
    }

    // update the hash
    uint8_t *hash = ctx->hash_buffer.buffer;
    // we have allocated twice as much for this data (because we also want to be able to represent in hex)
    // thus only loop over the first half of the bytes as the signature_ctx has written to those
    for (size_t hash_ix = 0; hash_ix < ctx->hash_buffer.size / 2; hash_ix++) {
        // this might seem convoluted, but snprintf() with %02x is not always supported e.g. by newlib-nano
        // we encode as hex... first ASCII char encodes top 4 bytes
        uint8_t first = (hash[hash_ix] >> 4) & 0xf;
        // second encodes lower 4 bytes
        uint8_t second = hash[hash_ix] & 0xf;

        // if 0..9 -> '0' (48) + value, if >10, then use 'a' (97) - 10 + value
        char first_c = first >= 10 ? 87 + first : 48 + first;
        char second_c = second >= 10 ? 87 + second : 48 + second;

        char buf[] = { first_c, second_c };
        if (ei_fwrite(ctx, buf, 1, 2) != 2) {
            return AQ_STREAM_WRITE_FAILED;
        }
    }

    return AQ_OK;
}
