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
#ifndef REMOTE_MGMGT_H
#define REMOTE_MGMGT_H
#include <cstdint>
#include <string>
#include <memory>
#include "ei_device_info_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DECODE_OK = 0,
    ERR_MAP_EXPECTED,
    ERR_UNKNOWN_FIELD,
    ERR_UNEXPECTED_TYPE,
    ERR_UNEXPECTED_VALUE,
    ERR_UNKNOWN
} decode_result_t;

enum class MessageType {
    DecoderErrorType,
    HelloResponseType,
    ErrorResponseType,
    SampleRequestType,
    StreamingStartRequestType,
    StreamingStopRequestType,
};

class DecodedMessage {
public:
    virtual ~DecodedMessage() {}
    virtual MessageType getType() const = 0;
};

class DecoderError : public DecodedMessage {
public:
    decode_result_t err_code;
    std::string err_message;
    virtual MessageType getType() const override {
        return MessageType::DecoderErrorType;
    }
};

class HelloResponse : public DecodedMessage {
public:
    bool status;
    std::string err_message;
    virtual MessageType getType() const override {
        return MessageType::HelloResponseType;
    }
};

class ErrorResponse : public DecodedMessage {
public:
    std::string err_message;
    virtual MessageType getType() const override {
        return MessageType::ErrorResponseType;
    }
};

class SampleRequest : public DecodedMessage {
public:
    std::string sensor;
    virtual MessageType getType() const override {
        return MessageType::SampleRequestType;
    }
};

class StreamingStartRequest : public DecodedMessage {
public:
    bool status;
    virtual MessageType getType() const override {
        return MessageType::StreamingStartRequestType;
    }
};

class StreamingStopRequest : public DecodedMessage {
public:
    bool status;
    virtual MessageType getType() const override {
        return MessageType::StreamingStopRequestType;
    }
};

/**
 * @brief This message should be sent after receiving SampleRequest (it is ack message)
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @return actual message length
 */
int get_sample_start_msg(uint8_t* buf, size_t buf_len);

/**
 * @brief This message should be sent after receiving SampleRequest
 * if there is any error on the device side (e.g. sensor error etc.)
 * Put an error description in the msg argument
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @param msg Error message
 * @return actual message length
 */
int get_sample_failed_msg(uint8_t* buf, size_t buf_len, const char* error_msg);

/**
 * @brief This message should be sent right after starting sampling
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @return actual message length
 */
int get_sample_started_msg(uint8_t* buf, size_t buf_len);

/**
 * @brief Send this message if the device is done sampling, but is processing the sample before
 * uploading, such as a device preprocessing audio or signing the file.
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @return actual message length
 */
int get_sample_processing_msg(uint8_t* buf, size_t buf_len);

/**
 * @brief Send this message before you start the upload of sample to ingestion service
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @return actual message length
 */
int get_sample_uploading_msg(uint8_t* buf, size_t buf_len);

/**
 * @brief Send this message after the sample has been uploaded to ingestion service
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @return actual message length
 */
int get_sample_finished_msg(uint8_t* buf, size_t buf_len);

/**
 * @brief Create a message with base64 encoded snapshot frame
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @param frame Base64 encoded snapshot frame
 * @return actual message length
 */
int get_snapshot_frame_msg(uint8_t* buf, size_t buf_len, const char* frame);

/**
 * @brief Create a hello message (send as a first message to Remote Management Service)
 * @param buf Buffer to write the message to
 * @param buf_len Length of the buffer
 * @param device device instance, the required data will be extracted from this instance
 * @return actual message length
*/
int get_hello_msg(uint8_t* buf, size_t buf_len, EiDeviceInfo* device);

std::unique_ptr<DecodedMessage> decode_message(const uint8_t* buf, size_t buf_len, EiDeviceInfo *device);

#ifdef __cplusplus
};
#endif

#endif /* REMOTE_MGMGT_H */