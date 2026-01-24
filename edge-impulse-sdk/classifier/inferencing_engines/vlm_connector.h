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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_VLM_CONNECTOR_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_VLM_CONNECTOR_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_VLM_CONNECTOR)

#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/porting/ei_logging.h"
#include <curl/curl.h>
#include <sstream>
#include <string>
#include "json/json.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <regex>
#include <base64/at_base64_lib.h>
#include "jpeg/encode_as_jpg.h"

std::string build_json_payload_clip(const ei_impulse_t *impulse,
                                    const ei_learning_block_config_vlm_connection_t& block_config,
                                    uint8_t* image_base64,
                                    size_t size_out)
{
    nlohmann::json payload;
    std::string json_str;
    std::vector<std::string> class_descs;

    for (uint32_t i = 0; i < impulse->label_count; i++) {
        class_descs.push_back(std::string(block_config.class_descriptions[i]));
    }

    try {
        payload["class_descriptions"] = class_descs;
        payload["image_url"] =  "data:image/jpeg;base64," + std::string((char*)image_base64, size_out);
        json_str = payload.dump();
    }
    catch (const std::exception& e) {
        EI_LOGE("Building JSON payload: %s\n", e.what());
        return "";
    }

    return json_str;
}

std::string build_json_payload_vlm(const ei_learning_block_config_vlm_connection_t& block_config,
                                   uint8_t* image_base64,
                                   size_t size_out)
{
    nlohmann::json payload;
    std::string json_str;

    try {
        payload["model"] = block_config.model;
        payload["max_tokens"] = block_config.max_tokens;
        payload["temperature"] = block_config.temperature;
        payload["image_url"] =  "data:image/jpeg;base64," + std::string((char*)image_base64, size_out);
        // logit bias example (disable new line token)
        //payload["logit_bias"] = { "1734", -100 }; // disable new line token
        // structured response
        payload["response_format"] = {
            {"type", "json_schema"},
            {"schema", {
                {"properties", {
                    {"label", {{"type", "string"}}},
                },
                {"required", {"label"}}
                }
            }}
        };
        payload["messages"] = {
            {
                {"role", "system"},
                {"content", "You always respond with the following JSON structure, regardless of the prompt: { \"label\": \"XXX\" }. Replace XXX with the requested answer."}
            },
            {
                {"role", "user"},
                {"content",
                    {
                        {
                            {"type", "image_url"},
                            {"image_url",
                                {{"url", payload["image_url"]}}
                            }
                        },
                        {
                            {"type", "text"},
                            {"text", block_config.prompt}
                        },
                    }
                }
            }
        };
        json_str = payload.dump();
    }
    catch (const std::exception& e) {
        EI_LOGE("Building JSON payload: %s\n", e.what());
        return "";
    }

    return json_str;
}

bool replace_all(std::string& input, const std::string& replace_word, const std::string& replace_by) {

    // Find the first occurrence of the substring
    size_t pos = input.find(replace_word);

    // Iterate through the string and replace all
    // occurrences
    while (pos != std::string::npos) {
        // Replace the substring with the specified string
        input.replace(pos, replace_word.size(), replace_by);

        // Find the next occurrence of the substring
        pos = input.find(replace_word,
                         pos + replace_by.size());
    }
    return true;
}

EI_IMPULSE_ERROR parse_json_to_result_vlm(std::string json_string_result,
                                          ei_impulse_result_t *result,
                                          ei_learning_block_config_vlm_connection_t* block_config,
                                          const ei_impulse_t *impulse,
                                          uint32_t learn_block_index) {

    // Parse the JSON response
    nlohmann::json json_result = nlohmann::json::parse(json_string_result);

    if (EI_LOG_LEVEL >= 5) {
        std::cout << "response JSON string: \n";
        std::cout << std::setw(4) << json_result << '\n';
    }

    auto json_content = json_result["choices"][0]["message"]["content"];

    std::string json_string = json_content.dump();

    EI_LOGD("Raw JSON String: %s\n", json_string.c_str());

    // remove ```json ... ``` if present
    replace_all(json_string, "```json", "");
    replace_all(json_string, "```", "");
    EI_LOGD("JSON String remove ```json ... ```: %s\n", json_string.c_str());

    replace_all(json_string, "\\n", " ");
    EI_LOGD("JSON String remove \\n: %s\n", json_string.c_str());

    // remove backslashes
    json_string.erase(std::remove(json_string.begin(), json_string.end(), '\\'), json_string.end());
    EI_LOGD("JSON String remove backslashes: %s\n", json_string.c_str());

    // remove leading and trailing quotes
    if (json_string.front() == '\"' && json_string.back() == '\"') {
        json_string = json_string.substr(1, json_string.size() - 2);
    }
    EI_LOGD("Final JSON String: %s\n", json_string.c_str());

    nlohmann::json json_response = nlohmann::json::parse(json_string);
    if (EI_LOG_LEVEL >= 5) {
        std::cout << std::setw(4) << json_response << '\n';
    }

    if (!json_response.contains("label")) {
        EI_LOGE("JSON response does not contain required fields.\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    auto label = json_response["label"];

    result->_raw_outputs[learn_block_index].matrix = new matrix_t(1, impulse->label_count);

    for (size_t j = 0; j < impulse->label_count; j++) {
        if (impulse->categories[j] == label) {
            result->_raw_outputs[learn_block_index].matrix->buffer[j] = 1.0f;
            break;
        }
    }

    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR parse_response_to_result_clip(std::string json_string_result,
                                               ei_impulse_result_t *result,
                                               ei_learning_block_config_vlm_connection_t* block_config,
                                               const ei_impulse_t *impulse,
                                               uint32_t learn_block_index) {

    nlohmann::json json_result = nlohmann::json::parse(json_string_result);

    if (EI_LOG_LEVEL >= 5) {
        std::cout << "response JSON string: \n";
        std::cout << std::setw(4) << json_result << '\n';
    }

    auto scores = json_result["scores"];
    auto indices = json_result["indices"];

    size_t num_labels = impulse->label_count;

    result->_raw_outputs[learn_block_index].matrix = new matrix_t(1, num_labels);
    for (size_t i = 0; i < num_labels; i++) {
        float score = scores[i];
        uint32_t index = indices[i];
        std::string label = impulse->categories[index];

        result->_raw_outputs[learn_block_index].matrix->buffer[index] = score;
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR check_response_code(CURLcode res, std::string& response) {
    if (res != CURLE_OK) {
        EI_LOGE("CURL error: %s\n", curl_easy_strerror(res));
        return EI_IMPULSE_INFERENCE_ERROR;
    }
    else {
        EI_LOGD(response.c_str());
        // Parse the JSON response
        nlohmann::json json_result = nlohmann::json::parse(response);
        if (json_result.contains("error")) {
            std::string error_message = json_result["error"]["message"];
            EI_LOGE("API Error: %s\n", error_message.c_str());
            return EI_IMPULSE_INFERENCE_ERROR;
        }
    }
    return EI_IMPULSE_OK;
}

// Callback function to handle the response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* buffer = static_cast<std::string*>(userp);
    size_t total_size = size * nmemb;
    buffer->append(static_cast<char*>(contents), total_size);
    return total_size;
}

EI_IMPULSE_ERROR run_vlm_inference(
    const ei_impulse_t *impulse,
    ei_feature_t *fmatrix,
    uint32_t learn_block_index,
    uint32_t* input_block_ids,
    uint32_t input_block_ids_size,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    // dummy function to match signature
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR run_vlm_inference(
    const ei_impulse_handle_t *handle,
    signal_t *signal,
    uint32_t learn_block_index,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    uint64_t ctx_start_us;
    uint32_t input_width;
    uint32_t input_height;
    uint32_t nn_input_frame_size;
    const ei_impulse_t *impulse = handle->impulse;

    uint64_t dsp_start_us = ei_read_timer_us();

    if (handle->input_params) {
        ei_input_params *params = handle->input_params;
        input_width = params->input_width;
        input_height = params->input_height;
        nn_input_frame_size = params->nn_input_frame_size;
    }
    else {
        input_width = impulse->input_width;
        input_height = impulse->input_height;
        nn_input_frame_size = impulse->nn_input_frame_size;
    }

    EI_LOGD("Running VLM Connector inference (%ux%u)\r\n", input_width, input_height);
    ei_learning_block_config_vlm_connection_t *block_config = (ei_learning_block_config_vlm_connection_t*)config_ptr;

    // allocate buffer for the jpeg image
    // we don't know the size of the jpeg image, but it will be smaller than the input image
    // or the same size in worst case
    std::unique_ptr<uint8_t> image_data_ptr(new uint8_t[nn_input_frame_size]);
    uint8_t* image_data = image_data_ptr.get();
    if (image_data == nullptr) {
        EI_LOGE("Failed to allocate memory for image\r\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    size_t jpeg_size = 0;

    // convert signal to image
    int jpeg_ret = encode_rgb888_signal_as_jpg(
        signal,
        input_width,
        input_height,
        image_data,
        nn_input_frame_size,
        &jpeg_size);

    if (jpeg_ret != 0) {
        EI_LOGE("Failed to convert signal to jpeg image (%d)\r\n", jpeg_ret);
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    // create output buffer
    // we are encoding data into base64, so it needs to be divisible by 3
    size_t output_size_check = ((jpeg_size + 2) / 3) * 4;

    std::unique_ptr<uint8_t[]> buffer_out_ptr(new uint8_t[output_size_check]);
    uint8_t* buffer_out = buffer_out_ptr.get();

    // convert image to base64
    size_t size_out = base64_encode_buffer((const char*)image_data,
                                    jpeg_size, /* input_size */
                                    (char*)buffer_out, /* output_buffer */
                                    output_size_check /* output_buffer_size */);

    if (size_out < 0) {
        EI_LOGE("Failed to convert image to base64 (%zu)\r\n", size_out);
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    result->timing.dsp_us = ei_read_timer_us() - dsp_start_us;
    result->timing.dsp = (int)(result->timing.dsp_us / 1000);

    // Initialize CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        EI_LOGE("Failed to initialize CURL\r\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    // Prepare the request payload (JSON)
    std::string payload;
    if (strcmp(block_config->model, "clip-vit-large-patch14_ggml-model-q8_0") == 0) {
        EI_LOGD("Using CLIP model payload builder\n");
        payload = build_json_payload_clip(impulse, *block_config, buffer_out, size_out);
    } else if (strcmp(block_config->model, "gemma-3-4b-it-q4_0") == 0) {
        EI_LOGD("Using VLM model payload builder\n");
        payload = build_json_payload_vlm(*block_config, buffer_out, size_out);
    }
    else {
        EI_LOGE("Unknown model type: %s\r\n", block_config->model);
        curl_easy_cleanup(curl);
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    // Set the request options
    curl_easy_setopt(curl, CURLOPT_URL, block_config->server_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size());

    // Set custom headers using a linked list
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Authorization: Bearer NONE");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set the write callback function to capture the response
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    ctx_start_us = ei_read_timer_us();

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Get the end time
    uint64_t ctx_end_us = ei_read_timer_us();

    result->timing.classification_us = ctx_end_us - ctx_start_us;
    result->timing.classification = (int)(result->timing.classification_us / 1000);

    EI_LOGD("VLM Connector DSP time: %d ms\r\n", result->timing.dsp);
    EI_LOGD("VLM Connector DSP time: %lld us\r\n", result->timing.dsp_us);
    EI_LOGD("VLM Connector inference time: %d ms\r\n", result->timing.classification);
    EI_LOGD("VLM Connector inference time: %lld us\r\n", result->timing.classification_us);

    EI_IMPULSE_ERROR curl_res = check_response_code(res, response);
    if (curl_res != EI_IMPULSE_OK) {
        curl_easy_cleanup(curl);
        return curl_res;
    }

    EI_IMPULSE_ERROR parse_res = EI_IMPULSE_OK;
    // Parse the JSON response and populate the result structure
    if (strcmp(block_config->model, "clip-vit-large-patch14_ggml-model-q8_0") == 0) {
        parse_res = parse_response_to_result_clip(response, result, block_config, impulse, learn_block_index);
    }
    else if (strcmp(block_config->model, "gemma-3-4b-it-q4_0") == 0) {
        parse_res = parse_json_to_result_vlm(response, result, block_config, impulse, learn_block_index);
    }

    if (parse_res != EI_IMPULSE_OK) {
        std::cerr << "Failed to parse JSON response for VLM model" << std::endl;
        return parse_res;
    }

    // Cleanup
    curl_easy_cleanup(curl);

    return EI_IMPULSE_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_VLM_CONNECTOR)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_VLM_CONNECTOR_H_
