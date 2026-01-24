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

#ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_ETHOS_LINUX_H_
#define _EI_CLASSIFIER_INFERENCING_ENGINE_ETHOS_LINUX_H_

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_ETHOS_LINUX)
// check if the model is quantized on the build stage to fail quickly
#if EI_CLASSIFIER_QUANTIZATION_ENABLED == 0
#error "Ethos does not support float32 models"
#endif

#include "model-parameters/model_metadata.h"
#include <string>
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "ethos-u-driver-stack-imx/driver_library/include/ethosu.hpp"
#include "ethos-u-driver-stack-imx/kernel_driver/include/uapi/ethosu.h"

std::shared_ptr<EthosU::Network> network;
// by default Device uses /dev/ethosu0
static EthosU::Device device;
static bool init_done = false;

bool init_ethos(const uint8_t *model_arr, size_t model_arr_size, bool debug)
{
    int ret;
    try {
        // eioctl thros exception if ioctl returns negative value
        ret = device.ioctl(ETHOSU_IOCTL_PING);
    }
    catch (EthosU::Exception &e) {
        ei_printf("ERR: EthosU device not found: %s\n", e.what());
        return false;
    }

    try {
        // eioctl thros exception if ioctl returns negative value
        ret = device.ioctl(ETHOSU_IOCTL_VERSION_REQ);
    }
    catch (EthosU::Exception &e) {
        ei_printf("ERR: EthosU version request failed: %s\n", e.what());
        return false;
    }

    if (debug) {
        try {
            EthosU::Capabilities capabilities = device.capabilities();
        }
        catch (EthosU::Exception &e) {
            ei_printf("ERR: EthosU capabilities request failed: %s\n", e.what());
            return false;
        }

        // std::cout << "Capabilities:" << std::endl
        //     << "\tversion_status:" << unsigned(capabilities.hwId.versionStatus) << std::endl
        //     << "\tversion:" << capabilities.hwId.version << std::endl
        //     << "\tproduct:" << capabilities.hwId.product << std::endl
        //     << "\tarchitecture:" << capabilities.hwId.architecture << std::endl
        //     << "\tdriver:" << capabilities.driver << std::endl
        //     << "\tmacs_per_cc:" << unsigned(capabilities.hwCfg.macsPerClockCycle) << std::endl
        //     << "\tcmd_stream_version:" << unsigned(capabilities.hwCfg.cmdStreamVersion) << std::endl
        //     << "\tcustom_dma:" << std::boolalpha << capabilities.hwCfg.customDma << std::endl;
    }

    std::shared_ptr<EthosU::Buffer> modelBuffer = std::make_shared<EthosU::Buffer>(device, model_arr_size);
    // TODO: not sure why we need to do that, but Buffer constructor calls mmap
    // while resize calls ioct ETHOSU_IOCTL_BUFFER_SET on the ethos device
    modelBuffer->resize(model_arr_size);
    std::copy(model_arr, model_arr + model_arr_size, modelBuffer->data());

    // std::shared_ptr<EthosU::Buffer> networkBuffer = allocAndFill(device, "lamp-plant-int8_vela.tflite");
    network = std::make_shared<EthosU::Network>(device, modelBuffer);

    if(network->getIfmDims().size() > 1) {
        ei_printf("ERR: Only single input models are supported\n");
        return false;
    }

    init_done = true;

    return true;
}

EI_IMPULSE_ERROR run_nn_inference(
    const ei_impulse_t *impulse,
    ei_feature_t *fmatrix,
    uint32_t learn_block_index,
    uint32_t* input_block_ids,
    uint32_t input_block_ids_size,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug)
{
    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;
    ei_config_ethos_graph_t *io_details = ((ei_config_ethos_graph_t*)block_config->graph_config);

    std::vector<std::shared_ptr<EthosU::Buffer>> ifm;
    std::vector<std::shared_ptr<EthosU::Buffer>> ofm;
    ei::matrix_t* matrix;


    if (!init_done) {
        if(!init_ethos(io_details->model, io_details->model_size, debug)) {
            return EI_IMPULSE_DEVICE_INIT_ERROR;
        }
    }

    // so far only single input models are supported
    // get the size of the first input tensor
    auto ifmSize = network->getIfmDims()[0];
    // create a buffer for the input tensor
    std::shared_ptr<EthosU::Buffer> buffer = std::make_shared<EthosU::Buffer>(device, ifmSize);
    // resize the buffer to the size of the input tensor
    // TODO: not sure if we need to call it, but the reference code does
    buffer->resize(ifmSize);
    // // inputType is an enum TensorType
    // auto inputType = network->getIfmTypes()[0];
    // // inputShape is a vecor of dims
    // auto inputShape = network->getIfmShapes()[0];

    // get the matrix with input features
    for (size_t i = 0; i < input_block_ids_size; i++) {
#if EI_CLASSIFIER_SINGLE_FEATURE_INPUT == 0
        uint16_t cur_mtx = input_block_ids[i];
        matrix = NULL;

        if (!find_mtx_by_idx(fmatrix, &matrix, cur_mtx, impulse->dsp_blocks_size)) {
            ei_printf("ERR: Cannot find matrix with id %zu\n", cur_mtx);
            return EI_IMPULSE_INVALID_SIZE;
        }
#else
        matrix = fmatrix[0].matrix;
#endif
    }

    if(ifmSize != matrix->rows * matrix->cols) {
        ei_printf("ERR: Input size mismatch\n");
        return EI_IMPULSE_INVALID_SIZE;
    }

    // get start time of the inference
    uint64_t ctx_start_us = ei_read_timer_us();

    // copy rescale the input features to int8 and copy to input buffer
    for (size_t i = 0; i < ifmSize; i++) {
        buffer->data()[i] = (int8_t)((matrix->buffer[i] / io_details->input_scale) + io_details->input_zeropoint);
    }

    // put the data into the input tensor
    ifm.push_back(buffer);

    // create a buffer for the output tensor
    auto ofmSize = network->getOfmDims()[0];
    ofm.push_back(std::make_shared<EthosU::Buffer>(device, ofmSize));

    // start inference
    EthosU::Inference inference(network, ifm.begin(), ifm.end(), ofm.begin(), ofm.end());

    // wait in 1 second (wait gets nanoseconds)
    if (inference.wait(1000 * 1000 * 1000)) {
        ei_printf("ERR: Inference timeout\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    // get end time of the inference
    uint64_t ctx_end_us = ei_read_timer_us();

    // check if inference ended successfully
    if (inference.status() != EthosU::InferenceStatus::OK) {
        ei_printf("ERR: Inference status: %d\n", inference.status());
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    // calculate inference time
    result->timing.classification_us = ctx_end_us - ctx_start_us;

    size_t output_size = io_details->output_features_count;

    if (network->getOfmTypes()[0] == EthosU::TensorType_INT8) {
        result->_raw_outputs[learn_block_index].matrix_i8 = new matrix_i8_t(1, output_size);
        memcpy(result->_raw_outputs[learn_block_index].matrix_i8->buffer, (int8_t *)inference.getOfmBuffers()[0]->data(), output_size * sizeof(int8_t));
    }
    else if (network->getOfmTypes()[0] == EthosU::TensorType_UINT8) {
        result->_raw_outputs[learn_block_index].matrix_u8 = new matrix_u8_t(1, output_size);
        memcpy(result->_raw_outputs[learn_block_index].matrix_u8->buffer, (uint8_t *)inference.getOfmBuffers()[0]->data(), output_size * sizeof(uint8_t));
    }
    else {
        ei_printf("ERR: Cannot handle output type (%d)\n", network->getOfmTypes()[0]);
        return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
    }

    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;

    return EI_IMPULSE_OK;
}

#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_ETHOS_LINUX)
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_ETHOS_LINUX_H_
