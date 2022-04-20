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

#ifndef EI_SENSOR_AQ_H
#define EI_SENSOR_AQ_H


/* Include ----------------------------------------------------------------- */
#include "qcbor.h"
#include <stdio.h>

// detect POSIX, and use FILE* in that case
#if !defined(EI_SENSOR_AQ_STREAM) && (defined (__unix__) || (defined (__APPLE__) && defined (__MACH__)))
#include <time.h>
#define EI_SENSOR_AQ_STREAM     FILE
#elif !defined(EI_SENSOR_AQ_STREAM)
#error "EI_SENSOR_AQ_STREAM not defined, and not on POSIX system. Please specify the EI_SENSOR_AQ_STREAM macro"
#endif

#ifdef __MBED__
#include "mbed.h"
#endif

#define EI_MAX_SENSOR_AXES                 20

/**  Sample format communicate with serial daemon */
typedef enum {
     EI_UINT8 = 0,
     EI_UINT16 = 1,
     EI_UINT32 = 2,
     EI_INT8 = 3,
     EI_INT16 = 4,
     EI_INT32 = 5,
     EI_FLOAT32 = 6
 } ei_content_type_t;

typedef enum {
    AQ_OK = 0,
    AQ_SIGNATURE_BUFFER_DOES_NOT_FIT = -6001,
    AQ_FREAD_IS_NULL = -6002,
    AQ_FWRITE_IS_NULL = -6003,
    AQ_FSEEK_IS_NULL = -6004,
    AQ_FTELL_IS_NULL = -6005,
    AQ_SIGNATURE_INIT_IS_NULL = -6006,
    AQ_SIGNATURE_UPDATE_IS_NULL = -6007,
    AQ_SIGNATURE_FINISH_IS_NULL = -6008,
    AQ_CTX_IS_NULL = -6009,
    AQ_PAYLOAD_INFO_IS_NULL = -6010,
    AQ_STREAM_IS_NULL = -6011,
    AQ_DEVICE_TYPE_IS_NULL = -6012,
    AQ_NOT_VALID_SENML_TYPE = -6013,
    AQ_SIGNATURE_NOT_FOUND_IN_CBOR = -6014,
    AQ_STREAM_WRITE_FAILED = -6015,
    AQ_VALUES_SIZE_DOES_NOT_MATCH_AXIS_COUNT = -6016,
    AQ_STREAM_FSEEK_FAILED = -6017,
    AQ_SIGNATURE_CTX_IS_NULL = -6018,
    AQ_BATCH_ONLY_SUPPORTS_SINGLE_AXIS = -6019,
    AQ_OUT_OF_MEM = -6020
} sensor_aq_status;

/**
 * Buffer context
 */
typedef struct {
    unsigned char*   buffer;
    size_t           size;
} sensor_aq_buffer_t;

/**
 * Signing context
 * All data needs to be signed, e.g. by HMAC or by public/private key pair
 * Data will be streamed in whenever it's available
 */
typedef struct sensor_aq_signing_ctx {
    // JWT algorithm (e.g. HS256 or none)
    const char *alg;

    // Length of the signature (e.g. 32 bytes for HS256, or 72 bytes for ECDSA-SHA256)
    // a buffer of this size will be passed in during initialization
    size_t signature_length;

    // Initialization function, return 0 if everything is OK
    // First argument is the current context
    int (*init)(struct sensor_aq_signing_ctx*);

    // Set fields in the 'protected' object of the CBOR context
    // here you can place information that is required for verifying the context signature
    // (e.g. expiration date or scope)
    int (*set_protected)(struct sensor_aq_signing_ctx*, QCBOREncodeContext*);

    // Update function, return 0 if everything is OK
    // First argument is the current context, second is a buffer with the new data, third is the size of the buffer
    int (*update)(struct sensor_aq_signing_ctx*, const uint8_t*, size_t);

    // Finalize function, return 0 if everything is OK
    // First argument is the current context, second a buffer to place the signature in (of size 'signature_length')
    int (*finish)(struct sensor_aq_signing_ctx*, uint8_t*);

    // Pointer to a different context, you can use this to get a reference back to yourself
    void *ctx;
} sensor_aq_signing_ctx_t;

/**
 * Context for a stream
 */
typedef struct {
    // Main buffer, will be split up by the library
    // The upper X bytes are allocated for the signing context (see signature_length)
    // the rest is used as CBOR buffer
    // Advice is to allocate at least 512 bytes
    sensor_aq_buffer_t buffer;

    // Signature context, used to sign the data
    sensor_aq_signing_ctx_t *signature_ctx;

    // function signatures
#ifdef __MBED__
    mbed::Callback<size_t(const void*, size_t, size_t, EI_SENSOR_AQ_STREAM*)> fwrite;
    mbed::Callback<int(EI_SENSOR_AQ_STREAM*, long int, int)> fseek;
    mbed::Callback<time_t(time_t*)> time;
#else
    size_t (*fwrite)(const void*, size_t, size_t, EI_SENSOR_AQ_STREAM*);
    int    (*fseek)(EI_SENSOR_AQ_STREAM*, long int, int);
    time_t (*time)(time_t*);
#endif

    // internal buffers (reference the main buffer, so not separate memory!)
    UsefulBuf cbor_buffer;
    sensor_aq_buffer_t hash_buffer;

    // cbor encode context
    QCBOREncodeContext encode_context;

    // axis count (how many sensor streams are we expecting?)
    size_t axis_count;

    // index of the signature in the file
    size_t signature_index;

    // active stream
    EI_SENSOR_AQ_STREAM *stream;
} sensor_aq_ctx;

/**
 * An axis of a sensor, e.g. accelerometer has X/Y/Z axis, so you'll have three instances here
 */
typedef struct {
    // Name of the sensor (e.g. accX)
    const char *name;
    // SenML unit type, e.g. m/s2 (see https://www.iana.org/assignments/senml/senml.xhtml for valid units)
    const char *units;
} sensor_aq_sensor;

/**
 * Information about the payload, who sent the data, what was the interval, and what kind of data is in this file?
 */
typedef struct {
    // Optional: globally unique name for your device, e.g. use MAC address or Device EUI if you have one
    const char *device_name;
    // Device type, group similar devices with the same sensors through this field
    const char *device_type;
    // Frequency that new data will come in, e.g. 100Hz = data every 10ms.
    float interval_ms;
    // Sensor axes, note that I declare this not as a pointer to have a more fluent interface
    sensor_aq_sensor sensors[EI_MAX_SENSOR_AXES];
} sensor_aq_payload_info;



/* Function prototypes ----------------------------------------------------- */

int sensor_aq_init(sensor_aq_ctx *ctx, sensor_aq_payload_info *payload_info, EI_SENSOR_AQ_STREAM *stream, bool allow_empty_stream);
int sensor_aq_add_data(sensor_aq_ctx *ctx, float values[], size_t values_size);
int sensor_aq_add_data_i16(sensor_aq_ctx *ctx, int16_t values[], size_t values_size);
int sensor_aq_add_data_batch(sensor_aq_ctx *ctx, int16_t values[], size_t values_size);
int sensor_aq_finish(sensor_aq_ctx *ctx);

#endif /* EI_SENSOR_AQ_H */
