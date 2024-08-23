/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#include <stdint.h>

#include "edge-impulse-sdk/tensorflow/lite/kernels/internal/reference/scatter_nd.h"

#include "edge-impulse-sdk/tensorflow/lite/core/c/common.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/kernel_util.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/kernels/kernel_util.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/internal/types.h"

namespace tflite {
namespace scatter_nd {

constexpr int kIndices = 0;
constexpr int kUpdates = 1;
constexpr int kShape = 2;
constexpr int kOutputTensor = 0;

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {

  TF_LITE_ENSURE_EQ(context, NumInputs(node), 3);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);

  MicroContext* micro_context = GetMicroContext(context);

  TfLiteTensor* indices =
      micro_context->AllocateTempInputTensor(node, kIndices);
  TfLiteTensor* updates =
      micro_context->AllocateTempInputTensor(node, kUpdates);
  TfLiteTensor* shape =
      micro_context->AllocateTempInputTensor(node, kShape);
  TF_LITE_ENSURE(context, indices != nullptr);
  TF_LITE_ENSURE(context, updates != nullptr);
  TF_LITE_ENSURE(context, shape != nullptr);

  switch (updates->type) {
    case kTfLiteFloat32:
    case kTfLiteUInt8:
    case kTfLiteBool:
    case kTfLiteInt8:
    case kTfLiteInt64:
    case kTfLiteInt32:
      break;
    default:
      TF_LITE_KERNEL_LOG(
          context, "Updates of type '%s' are not supported by scatter_nd.",
          TfLiteTypeGetName(updates->type));
      return kTfLiteError;
  }
  if (indices->type != shape->type) {
    TF_LITE_KERNEL_LOG(context, "Indices and shape must have the same type.");
    return kTfLiteError;
  }

  TfLiteTensor* output =
      micro_context->AllocateTempOutputTensor(node, kOutputTensor);
  TF_LITE_ENSURE(context, output != nullptr);
  output->type = updates->type;

  TfLiteStatus ret = kTfLiteError;

  if (IsDynamicTensor(output)) {
    TF_LITE_KERNEL_LOG(context, "DynamicTensor is not yet supported by scatter_nd.");
    return ret;
  }

  const int shape_rank = SizeOfDimension(shape, 0);
  const auto* shape_data = GetTensorData<int32_t>(shape);

  if (IsConstantOrPersistentTensor(shape)) {
    switch (indices->type) {
      case kTfLiteInt32:
        // check if output tensor needs resizing
        // throw an error if it does
        if (output->dims->size != shape_rank) {
          TF_LITE_KERNEL_LOG(context, "Tensor resizing is not supported by scatter_nd.");
          return kTfLiteError;
        }
        for (int i = 0; i < shape_rank; i++) {
          if (output->dims->data[i] != shape_data[i]) {
            TF_LITE_KERNEL_LOG(context, "Tensor resizing is not supported by scatter_nd.");
            return kTfLiteError;
          }
        }
        break;
      default:
        TF_LITE_KERNEL_LOG(
            context, "Indices of type '%s' are not supported by scatter_nd.",
            TfLiteTypeGetName(indices->type));
        return ret;
    }
  } else {
    TF_LITE_KERNEL_LOG(context, "DynamicTensor is not yet supported by scatter_nd.");
    return ret;
  }

  micro_context->DeallocateTempTfLiteTensor(indices);
  micro_context->DeallocateTempTfLiteTensor(updates);
  micro_context->DeallocateTempTfLiteTensor(shape);
  micro_context->DeallocateTempTfLiteTensor(output);
  return kTfLiteOk;
}

template <typename IndicesT, typename UpdatesT>
TfLiteStatus ScatterNd(const TfLiteEvalTensor* indices, const TfLiteEvalTensor* updates,
                       TfLiteEvalTensor* output) {
  return reference_ops::ScatterNd(
      tflite::micro::GetTensorShape(indices), tflite::micro::GetTensorData<IndicesT>(indices),
      tflite::micro::GetTensorShape(updates), tflite::micro::GetTensorData<UpdatesT>(updates),
      tflite::micro::GetTensorShape(output), tflite::micro::GetTensorData<UpdatesT>(output));
}

template <typename IndicesT>
TfLiteStatus EvalScatterNd(TfLiteContext* context, const TfLiteEvalTensor* indices,
                           const TfLiteEvalTensor* updates,
                           const TfLiteEvalTensor* shape, TfLiteEvalTensor* output) {

  TfLiteStatus status = kTfLiteError;
  switch (updates->type) {
    case kTfLiteFloat32:
      status = ScatterNd<IndicesT, float>(indices, updates, output);
      break;
    case kTfLiteUInt8:
      status = ScatterNd<IndicesT, uint8_t>(indices, updates, output);
      break;
    case kTfLiteBool:
      status = ScatterNd<IndicesT, bool>(indices, updates, output);
      break;
    case kTfLiteInt8:
      status = ScatterNd<IndicesT, int8_t>(indices, updates, output);
      break;
    case kTfLiteInt32:
      status = ScatterNd<IndicesT, int32_t>(indices, updates, output);
      break;
    case kTfLiteInt64:
      status = ScatterNd<IndicesT, int64_t>(indices, updates, output);
      break;
    default:
      TF_LITE_KERNEL_LOG(
          context, "Updates of type '%s' are not supported by scatter_nd.",
          TfLiteTypeGetName(updates->type));
      return kTfLiteError;
  }
  if (status != kTfLiteOk) {
    TF_LITE_KERNEL_LOG(context, "scatter_nd index out of bounds");
  }
  return status;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {


  const TfLiteEvalTensor* indices =
      tflite::micro::GetEvalInput(context, node, kIndices);
  const TfLiteEvalTensor* updates =
      tflite::micro::GetEvalInput(context, node, kUpdates);
  const TfLiteEvalTensor* shape =
      tflite::micro::GetEvalInput(context, node, kShape);
  TfLiteEvalTensor* output =
      tflite::micro::GetEvalOutput(context, node, kOutputTensor);

  switch (indices->type) {
    case kTfLiteInt32:
      return EvalScatterNd<int32_t>(context, indices, updates, shape, output);
    default:
      TF_LITE_KERNEL_LOG(
          context, "Indices of type '%s' are not supported by scatter_nd.",
          TfLiteTypeGetName(indices->type));
      return kTfLiteError;
  }
}

}  // namespace scatter_nd

TfLiteRegistration Register_SCATTER_ND() {
  return tflite::micro::RegisterOp(nullptr, scatter_nd::Prepare, scatter_nd::Eval);
}

}  // namespace tflite
