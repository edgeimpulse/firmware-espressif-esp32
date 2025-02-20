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
#include "ei_device_info_lib.h"
#include "ei_fusion.h"
#include "QCBOR/inc/qcbor.h"
#include "remote-mgmt.h"
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <memory>

#define REMOTE_MANAGEMENT_VERSION   3
//TODO: this is usually defined in target implementation of the EiDeviceInfo
#define EI_MAX_FREQUENCIES          5

using namespace std;

template<class T, class... Args>
static std::unique_ptr<T> make_unique_ptr(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

static int simple_bool_msg(uint8_t* buf, size_t buf_len, const char* msg, bool value)
{
    UsefulBuf cbor_buf = {
        .ptr = buf,
        .len = buf_len
    };
    QCBOREncodeContext ec;
    UsefulBufC encoded;

    QCBOREncode_Init(&ec, cbor_buf);
    QCBOREncode_OpenMap(&ec);
    QCBOREncode_AddBoolToMap(&ec, msg, value);
    QCBOREncode_CloseMap(&ec);

    if(QCBOREncode_Finish(&ec, &encoded)) {
        return 0;
    }

    return encoded.len;
}

int get_sample_started_msg(uint8_t* buf, size_t buf_len)
{
    return simple_bool_msg(buf, buf_len, "sampleStarted", true);
}

int get_sample_start_msg(uint8_t* buf, size_t buf_len)
{
    return simple_bool_msg(buf, buf_len, "sample", true);
}

int get_sample_processing_msg(uint8_t* buf, size_t buf_len)
{
    return simple_bool_msg(buf, buf_len, "sampleProcessing", true);
}

int get_sample_uploading_msg(uint8_t* buf, size_t buf_len)
{
    return simple_bool_msg(buf, buf_len, "sampleUploading", true);
}

int get_sample_finished_msg(uint8_t* buf, size_t buf_len)
{
    return simple_bool_msg(buf, buf_len, "sampleFinished", true);
}

int get_sample_failed_msg(uint8_t* buf, size_t buf_len, const char* error_msg)
{
    UsefulBuf cbor_buf = {
        .ptr = buf,
        .len = buf_len
    };
    QCBOREncodeContext ec;
    UsefulBufC encoded;

    QCBOREncode_Init(&ec, cbor_buf);
    QCBOREncode_OpenMap(&ec);
    QCBOREncode_AddBoolToMap(&ec, "sample", false);
    QCBOREncode_AddSZStringToMap(&ec, "error", error_msg);
    QCBOREncode_CloseMap(&ec);

    if(QCBOREncode_Finish(&ec, &encoded)) {
        return 0;
    }

    return encoded.len;
}

int get_snapshot_frame_msg(uint8_t* buf, size_t buf_len, const char* frame)
{
    UsefulBuf cbor_buf = {
        .ptr = buf,
        .len = buf_len
    };
    QCBOREncodeContext ec;
    UsefulBufC encoded;

    QCBOREncode_Init(&ec, cbor_buf);
    QCBOREncode_OpenMap(&ec);
    QCBOREncode_AddSZStringToMap(&ec, "snapshotFrame", frame);
    QCBOREncode_CloseMap(&ec);

    if(QCBOREncode_Finish(&ec, &encoded)) {
        return 0;
    }

    return encoded.len;
}

int get_hello_msg(uint8_t* buf, size_t buf_len, EiDeviceInfo* device)
{
    UsefulBuf cbor_buf = {
        .ptr = buf,
        .len = buf_len
    };
    QCBOREncodeContext ec;
    UsefulBufC encoded;

    QCBOREncode_Init(&ec, cbor_buf);
    QCBOREncode_OpenMap(&ec);
    QCBOREncode_OpenMapInMap(&ec, "hello");
    QCBOREncode_AddInt64ToMap(&ec, "version", REMOTE_MANAGEMENT_VERSION);
    QCBOREncode_AddSZStringToMap(&ec, "connection", "ip");
    QCBOREncode_AddSZStringToMap(&ec, "apiKey", device->get_upload_api_key().c_str());
    QCBOREncode_AddSZStringToMap(&ec, "deviceId", device->get_device_id().c_str());
    QCBOREncode_AddSZStringToMap(&ec, "deviceType", device->get_device_type().c_str());
    QCBOREncode_AddBoolToMap(&ec, "supportsSnapshotStreaming", false);

    const ei_device_sensor_t *sensor_list;
    size_t sensor_list_size;
    device->get_sensor_list((const ei_device_sensor_t **)&sensor_list, &sensor_list_size);

    QCBOREncode_OpenArrayInMap(&ec, "sensors");
    // iterate over 'standalone' sensors (old approach)
    for (size_t ix = 0; ix < sensor_list_size; ix++) {
        QCBOREncode_OpenMap(&ec);
        QCBOREncode_AddSZStringToMap(&ec, "name", sensor_list[ix].name);
        QCBOREncode_AddInt64ToMap(&ec, "maxSampleLengthS", sensor_list[ix].max_sample_length_s);
        QCBOREncode_OpenArrayInMap(&ec, "frequencies");
        for (size_t fx = 0; fx < EI_MAX_FREQUENCIES; fx++) {
            if (sensor_list[ix].frequencies[fx] != 0.0f) {
                QCBOREncode_AddDouble(&ec, sensor_list[ix].frequencies[fx]);
            }
        }
        QCBOREncode_CloseArray(&ec); // frequencies
        QCBOREncode_CloseMap(&ec); // map for this sensor
    }
    // now iterate over fusion sensors
    std::vector<fused_sensors_t> fusion_sensors = ei_get_sensor_fusion_list();
    for (std::vector<fused_sensors_t>::iterator it = fusion_sensors.begin(); it != fusion_sensors.end(); ++it) {
        QCBOREncode_OpenMap(&ec);
        QCBOREncode_AddSZStringToMap(&ec, "name", it->name.c_str());
        QCBOREncode_AddInt64ToMap(&ec, "maxSampleLengthS", it->max_sample_length);
        QCBOREncode_OpenArrayInMap(&ec, "frequencies");
        for (std::vector<float>::iterator f_it = it->frequencies.begin();  f_it != it->frequencies.end() ; ++f_it) {
            QCBOREncode_AddDouble(&ec, *f_it);
        }
        QCBOREncode_CloseArray(&ec); // frequencies
        QCBOREncode_CloseMap(&ec); // map for this sensor
    }
    QCBOREncode_CloseArray(&ec); // sensor array

    QCBOREncode_CloseMap(&ec); // hello map
    QCBOREncode_CloseMap(&ec); // main object map

    QCBORError err = QCBOREncode_Finish(&ec, &encoded);

    if(err) {
        printf("Error encoding hello message (%d)\n", err);
        return 0;
    }

    return encoded.len;
}

unique_ptr<DecodedMessage> decode_message(const uint8_t* buf, size_t buf_len, EiDeviceInfo *device)
{
    QCBORDecodeContext ctx;
    QCBORItem item;
    char buffer[128] = { 0 };
    char sensor_name[128] = { 0 };
    char label_buf[128] = { 0 };
    uint32_t length = 0;
    float interval = 0;

    QCBORDecode_Init(&ctx, (UsefulBufC){ buf, buf_len}, QCBOR_DECODE_MODE_NORMAL);

    // first one needs to be a map...
    if (QCBORDecode_GetNext(&ctx, &item) != QCBOR_SUCCESS || item.uDataType != QCBOR_TYPE_MAP) {
        QCBORDecode_Finish(&ctx);
        auto ret = make_unique_ptr<DecoderError>();
        ret->err_code = ERR_MAP_EXPECTED;
        ret->err_message = "Expected map on in main body";
        return ret;
    }

    // then we expect labels and handle them
    while (QCBORDecode_GetNext(&ctx, &item) == QCBOR_SUCCESS && item.uLabelType == QCBOR_TYPE_TEXT_STRING) {
        memset(label_buf, 0, sizeof(label_buf));
        memcpy(label_buf, item.label.string.ptr, item.label.string.len > 127 ? 127 : item.label.string.len);

        if (strcmp(label_buf, "hello") == 0) {
            auto ret = make_unique_ptr<HelloResponse>();
            if (item.uDataType == QCBOR_TYPE_TRUE) {
                ret->status = true;
            }
            else {
                ret->status = false;
                if(QCBORDecode_GetNext(&ctx, &item) == QCBOR_SUCCESS && item.uLabelType == QCBOR_TYPE_TEXT_STRING) {
                    memset(buffer, 0, sizeof(buffer));
                    memcpy(buffer, item.label.string.ptr, item.label.string.len > 127 ? 127 : item.label.string.len);
                    ret->err_message = buffer;
                }
            }
            QCBORDecode_Finish(&ctx);
            return ret;
        }
        else if (strcmp(label_buf, "err") == 0) {
            auto ret = make_unique_ptr<ErrorResponse>();
            if (item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                memset(buffer, 0, sizeof(buffer));
                memcpy(buffer, item.val.string.ptr, item.val.string.len >= 127 ? 127 : item.val.string.len);
                ret->err_message = buffer;
            }
            QCBORDecode_Finish(&ctx);
            return ret;
        }
        else if (strcmp(label_buf, "startSnapshot") == 0) {
            auto ret = make_unique_ptr<StreamingStartRequest>();
            if (item.uDataType == QCBOR_TYPE_TRUE) {
                ret->status = true;
            }
            else {
                ret->status = false;
            }
            QCBORDecode_Finish(&ctx);
            return ret;
        }
        else if (strcmp(label_buf, "stopSnapshot") == 0) {
            auto ret = make_unique_ptr<StreamingStopRequest>();
            if (item.uDataType == QCBOR_TYPE_TRUE) {
                ret->status = true;
            }
            else {
                ret->status = false;
            }
            QCBORDecode_Finish(&ctx);
            return ret;
        }
        else if (strcmp(label_buf, "sample") == 0) {
            if (item.uDataType != QCBOR_TYPE_MAP) {
                QCBORDecode_Finish(&ctx);
                auto ret = make_unique_ptr<DecoderError>();
                ret->err_code = ERR_UNEXPECTED_TYPE;
                ret->err_message = "Unexpected type for 'sample'";
                return ret;
            }

            while (QCBORDecode_GetNext(&ctx, &item) == QCBOR_SUCCESS && item.uLabelType == QCBOR_TYPE_TEXT_STRING) {
                memset(label_buf, 0, sizeof(label_buf));
                memcpy(label_buf, item.label.string.ptr, item.label.string.len > 127 ? 127 : item.label.string.len);

                if (strcmp(label_buf, "path") == 0 && item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                    memset(buffer, 0, sizeof(buffer));
                    memcpy(buffer, item.val.string.ptr, item.val.string.len >= sizeof(buffer) ? sizeof(buffer) : item.val.string.len);
                    device->set_upload_path(buffer, false);
                }
                else if (strcmp(label_buf, "label") == 0 && item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                    memset(buffer, 0, sizeof(buffer));
                    memcpy(buffer, item.val.string.ptr, item.val.string.len >= sizeof(buffer) ? sizeof(buffer) : item.val.string.len);
                    device->set_sample_label(buffer, false);
                }
                else if (strcmp(label_buf, "hmacKey") == 0 && item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                    memset(buffer, 0, sizeof(buffer));
                    memcpy(buffer, item.val.string.ptr, item.val.string.len >= sizeof(buffer) ? sizeof(buffer) : item.val.string.len);
                    device->set_sample_hmac_key(buffer, false);
                }
                else if (strcmp(label_buf, "interval") == 0 && item.uDataType == QCBOR_TYPE_INT64) {
                    interval = (float)item.val.int64;
                    device->set_sample_interval_ms(interval, false);
                }
                else if (strcmp(label_buf, "interval") == 0 && item.uDataType == QCBOR_TYPE_DOUBLE) {
                    interval = item.val.dfnum;
                    device->set_sample_interval_ms(interval, false);
                }
                else if (strcmp(label_buf, "length") == 0 && item.uDataType == QCBOR_TYPE_INT64) {
                    length = (uint32_t)item.val.int64;
                    device->set_sample_length_ms(length, false);
                }
                else if (strcmp(label_buf, "sensor") == 0 && item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                    memset(sensor_name, 0, sizeof(sensor_name));
                    memcpy(sensor_name, item.val.string.ptr, item.val.string.len >= sizeof(sensor_name) ? sizeof(sensor_name) : item.val.string.len);
                }
                else {
                    QCBORDecode_Finish(&ctx);
                    auto ret = make_unique_ptr<DecoderError>();
                    ret->err_code = ERR_UNKNOWN_FIELD;
                    ret->err_message = label_buf;
                    return ret;
                }
            }
            QCBORDecode_Finish(&ctx);
            auto ret = make_unique_ptr<SampleRequest>();
            ret->sensor = sensor_name;
            device->save_config();
            return ret;
        }
        else {
            QCBORDecode_Finish(&ctx);
            auto ret = make_unique_ptr<DecoderError>();
            ret->err_code = ERR_UNKNOWN_FIELD;
            ret->err_message = label_buf;
            return ret;
        }
    }

    QCBORDecode_Finish(&ctx);

    auto ret = make_unique_ptr<DecoderError>();
    ret->err_code = ERR_UNKNOWN;
    ret->err_message = "Decoder loop terminasted";
    return ret;
}
