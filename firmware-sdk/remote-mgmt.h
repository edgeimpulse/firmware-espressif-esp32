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