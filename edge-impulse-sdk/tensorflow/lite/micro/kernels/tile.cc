/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "edge-impulse-sdk/tensorflow/lite/kernels/kernel_util.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/kernels/kernel_util.h"

namespace tflite {
namespace tile {

constexpr int kInputTensor = 0;
constexpr int kInputMultipliers = 1;
constexpr int kOutputTensor = 0;

namespace {
struct OpData {
  // Indicates that 'Eval' is a noop as the output as written during 'Prepare'.
  bool noop;
};

template <typename T, typename M>
void CopyMultipleTimes(const T* in_data, int32_t in_size, M multiplier,
                       T* out_data) {
  for (M i = 0; i < multiplier; ++i) {
    const T* in_end = in_data + in_size;
    T* new_out_data = std::copy(in_data, in_end, out_data);
    in_data = out_data;
    out_data = new_out_data;
  }
}

template <typename T, typename M>
std::pair<int, int> TileOneDimension(const TfLiteIntArray& in_dimensions,
                                     const T* in_data, const M* multipliers,
                                     T* out_data, int dimension) {
  if (in_dimensions.size == 0) {
    // If input tensor is a scalar, then just copy it to output (no need to
    // multiply).
    *out_data = *in_data;
    return std::make_pair(0, 0);
  }

  const int dimension_size = in_dimensions.data[dimension];
  if (dimension == in_dimensions.size - 1) {
    CopyMultipleTimes(in_data, dimension_size, multipliers[dimension],
                      out_data);
    return std::make_pair(
        dimension_size,
        dimension_size * static_cast<int>(multipliers[dimension]));
  }
  int total_stride_size = 0, total_tiled_stride_size = 0;
  const T* copy_from_data = in_data;
  T* copy_to_data = out_data;
  for (int i = 0; i < dimension_size; ++i) {
    int stride_size = 0, tiled_stride_size = 0;
    std::tie(stride_size, tiled_stride_size) =
        TileOneDimension(in_dimensions, copy_from_data, multipliers,
                         copy_to_data, dimension + 1);
    copy_from_data += stride_size;
    copy_to_data += tiled_stride_size;
    total_stride_size += stride_size;
    total_tiled_stride_size += tiled_stride_size;
  }
  CopyMultipleTimes(out_data, total_tiled_stride_size,
                    multipliers[dimension] - 1,
                    out_data + total_tiled_stride_size);
  return std::make_pair(
      total_stride_size,
      static_cast<int>(total_tiled_stride_size * multipliers[dimension]));
}

template <typename T>
void Tile(const TfLiteIntArray& in_dimensions, const TfLiteEvalTensor* in_data,
          const TfLiteEvalTensor* multipliers, TfLiteEvalTensor* out_data) {
  // Doing recursively tiling from top to down dimension.
  switch (multipliers->type) {
    case kTfLiteInt32:
      TileOneDimension(in_dimensions, tflite::micro::GetTensorData<T>(in_data),
                       tflite::micro::GetTensorData<int32_t>(multipliers),
                       tflite::micro::GetTensorData<T>(out_data), 0);
      break;
    case kTfLiteInt64:
      TileOneDimension(in_dimensions, tflite::micro::GetTensorData<T>(in_data),
                       tflite::micro::GetTensorData<int64_t>(multipliers),
                       tflite::micro::GetTensorData<T>(out_data), 0);
      break;
    default:
      break;
  }
}

}  // namespace

TfLiteStatus EvalImpl(TfLiteContext* context, const TfLiteEvalTensor* input,
                      const TfLiteEvalTensor* multipliers, TfLiteEvalTensor* output) {
  if (tflite::micro::GetTensorShape(output).FlatSize() == 0) {
    return kTfLiteOk;
  }

  switch (output->type) {
    case kTfLiteInt8:
    case kTfLiteUInt8:
      Tile<int8_t>(*(input->dims), input, multipliers, output);
      break;
    case kTfLiteFloat32:
    case kTfLiteInt32:
      Tile<int32_t>(*(input->dims), input, multipliers, output);
      break;
    case kTfLiteInt64:
      Tile<int64_t>(*(input->dims), input, multipliers, output);
      break;
    case kTfLiteBool:
      Tile<bool>(*(input->dims), input, multipliers, output);
      break;
    default:
      TF_LITE_KERNEL_LOG(context, "Type '%s' is not supported by tile.",
                         TfLiteTypeGetName(output->type));
      return kTfLiteError;
  }
  return kTfLiteOk;
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TF_LITE_ENSURE_EQ(context, NumInputs(node), 2);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);
  OpData* op_data = reinterpret_cast<OpData*>(node->user_data);
  op_data->noop = false;

  MicroContext* micro_context = GetMicroContext(context);

  TfLiteTensor* input =
      micro_context->AllocateTempInputTensor(node, kInputTensor);
  TF_LITE_ENSURE(context, input != nullptr);
  TfLiteTensor* output =
      micro_context->AllocateTempOutputTensor(node, kOutputTensor);
  TF_LITE_ENSURE(context, output != nullptr);
  TfLiteTensor* multipliers =
      micro_context->AllocateTempInputTensor(node, kInputMultipliers);
  TF_LITE_ENSURE(context, multipliers != nullptr);

  // Only int32 and int64 multipliers type is supported.
  if (multipliers->type != kTfLiteInt32 && multipliers->type != kTfLiteInt64) {
    TF_LITE_KERNEL_LOG(context,
                       "Multipliers of type '%s' are not supported by tile.",
                       TfLiteTypeGetName(multipliers->type));
    micro_context->DeallocateTempTfLiteTensor(input);
    micro_context->DeallocateTempTfLiteTensor(output);
    micro_context->DeallocateTempTfLiteTensor(multipliers);
    return kTfLiteError;
  }

  if (IsDynamicTensor(output)) {
    TF_LITE_KERNEL_LOG(context, "Dynamic output tensor is not supported by tflite micro.");
    micro_context->DeallocateTempTfLiteTensor(input);
    micro_context->DeallocateTempTfLiteTensor(output);
    micro_context->DeallocateTempTfLiteTensor(multipliers);
    return kTfLiteError;
  }

  micro_context->DeallocateTempTfLiteTensor(input);
  micro_context->DeallocateTempTfLiteTensor(output);
  micro_context->DeallocateTempTfLiteTensor(multipliers);

  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {

  const TfLiteEvalTensor* input =
      tflite::micro::GetEvalInput(context, node, kInputTensor);
  const TfLiteEvalTensor* multipliers =
      tflite::micro::GetEvalInput(context, node, kInputMultipliers);
  TfLiteEvalTensor* output =
      tflite::micro::GetEvalOutput(context, node, kOutputTensor);

  if (reinterpret_cast<OpData*>(node->user_data)->noop) {
    return kTfLiteOk;
  }

  return EvalImpl(context, input, multipliers, output);
}

void* Init(TfLiteContext* context, const char* buffer, size_t length) {
  TFLITE_DCHECK(context->AllocatePersistentBuffer != nullptr);
  return context->AllocatePersistentBuffer(context, sizeof(OpData));
}

}  // namespace tile

TfLiteRegistration Register_TILE() {
  return tflite::micro::RegisterOp(tile::Init, tile::Prepare, tile::Eval);
}

}  // namespace tflite
