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

#define FLATBUFFERS_LOCALE_INDEPENDENT 0
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <new>
#include <string>

#include <algorithm>
#include <initializer_list>
#include <numeric>
#include <vector>

#include "edge-impulse-sdk/third_party/flatbuffers/include/flatbuffers/flexbuffers.h"  // from @flatbuffers
#include "edge-impulse-sdk/tensorflow/lite/c/common.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/micro_context.h"
#include "edge-impulse-sdk/tensorflow/lite/micro/kernels/kernel_util.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/internal/compatibility.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "edge-impulse-sdk/tensorflow/lite/kernels/kernel_util.h"

#define FEATURE_TYPE float

namespace tflite {
namespace micro {
namespace tree_ensemble_classifier {

struct IndexTypes {
  bool feature_id_uint32;
  bool tree_index_uint32;
  bool tree_index_type_valid;
};

// FlexBuffers aligns a Blob only to the width of its size prefix (often a
// single byte), so typed payloads inside custom options may start at any
// address. Dereferencing a misaligned float* / uint16_t* / uint32_t* is UB and
// faults on e.g. Cortex-M FPU loads (VLDR) or Cortex-M0. This view keeps the
// raw byte pointer and reads each element via memcpy, which compiles to a
// single load on targets that allow unaligned access.
template <typename T>
struct UnalignedArray {
  const uint8_t* data;

  T operator[](uint32_t index) const {
    T value;
    std::memcpy(&value, data + static_cast<size_t>(index) * sizeof(T), sizeof(T));
    return value;
  }
};

template <typename T>
static UnalignedArray<T> GetBlobArray(const flexbuffers::Map& m, const char* key) {
  return {m[key].AsBlob().data()};
}

// TfLiteNode::user_data is a void*. We store a pointer to this common base so
// Prepare/Eval can read the index-width tags before knowing the derived type.
// These tags select ordinary function calls; no virtual dispatch or RTTI is used.
struct OpData {
  IndexTypes index_types;
};

// The compiler instantiates the types/functions for the four uint16_t/uint32_t
// combinations referenced below. No C++ types or machine code are generated on
// the device. At runtime, each operator creates just one state object of the
// selected type; its array pointers read the model's existing buffers directly.
template <typename FeatureIdType, typename TreeIndexType>
struct OpDataTree : OpData {
  uint32_t num_leaf_nodes;
  uint32_t num_internal_nodes;
  uint32_t num_trees;
  UnalignedArray<uint16_t> nodes_modes;
  UnalignedArray<FeatureIdType> nodes_featureids;
  UnalignedArray<float> nodes_values;
  UnalignedArray<TreeIndexType> nodes_truenodeids;
  UnalignedArray<TreeIndexType> nodes_falsenodeids;
  UnalignedArray<float> nodes_weights;
  const uint8_t* nodes_classids;
  UnalignedArray<TreeIndexType> tree_root_ids;
  const uint8_t* buffer_t;
  size_t buffer_length;
};

static IndexTypes GetIndexTypes(const char* buffer, size_t length) {
  const uint8_t* buffer_t = reinterpret_cast<const uint8_t*>(buffer);
  const flexbuffers::Map& m = flexbuffers::GetRoot(buffer_t, length).AsMap();
  const std::string tree_index_type = m["tree_index_type"].AsString().str();
  const size_t feature_id_size = m["nodes_featureids"].AsBlob().size();
  const uint32_t num_internal_nodes = m["num_internal_nodes"].AsUInt32();
  return {
      feature_id_size == static_cast<size_t>(num_internal_nodes) * sizeof(uint32_t),
      tree_index_type == "uint32",
      tree_index_type == "uint16" || tree_index_type == "uint32"};
}

template <typename FeatureIdType, typename TreeIndexType>
static void* InitTyped(TfLiteContext* context, const char* buffer, size_t length,
                       IndexTypes types) {

  const uint8_t* buffer_t = reinterpret_cast<const uint8_t*>(buffer);
  const flexbuffers::Map& m = flexbuffers::GetRoot(buffer_t, length).AsMap();

  // Allocate one state object in Micro's persistent arena. sizeof is known at
  // compile time for this specialization; the allocation itself occurs in Init.
  void* storage = context->AllocatePersistentBuffer(
      context, sizeof(OpDataTree<FeatureIdType, TreeIndexType>));
  if (storage == nullptr) {
    return nullptr;
  }
  // Placement new constructs/value-initializes the selected C++ object in the
  // already allocated storage and starts its lifetime (including its base).
  // Unlike ordinary new, it makes no heap allocation. This trivially destructible
  // state needs no delete; its storage remains owned by the arena.
  auto* data = new (storage) OpDataTree<FeatureIdType, TreeIndexType>();
  data->index_types = types;

  data->buffer_t = buffer_t;
  data->buffer_length = length;

  data->num_leaf_nodes = m["num_leaf_nodes"].AsUInt32();
  data->num_internal_nodes = m["num_internal_nodes"].AsUInt32();
  data->num_trees = m["num_trees"].AsUInt32();

  data->nodes_modes = GetBlobArray<uint16_t>(m, "nodes_modes");
  data->nodes_featureids = GetBlobArray<FeatureIdType>(m, "nodes_featureids");
  data->nodes_values = GetBlobArray<float>(m, "nodes_values");
  data->nodes_truenodeids = GetBlobArray<TreeIndexType>(m, "nodes_truenodeids");
  data->nodes_falsenodeids = GetBlobArray<TreeIndexType>(m, "nodes_falsenodeids");
  data->nodes_weights = GetBlobArray<float>(m, "nodes_weights");
  data->nodes_classids = m["nodes_classids"].AsBlob().data();
  data->tree_root_ids = GetBlobArray<TreeIndexType>(m, "tree_root_ids");

  // Convert to the common base before returning it as void*. MicroGraph stores
  // this return value in node->user_data. This changes the pointer's type, not
  // the object: the complete OpDataTree still occupies the same arena storage.
  return static_cast<OpData*>(data);
}

void* Init(TfLiteContext* context, const char* buffer, size_t length) {
  const IndexTypes types = GetIndexTypes(buffer, length);
  if (!types.tree_index_type_valid) {
    return nullptr;
  }
  // Runtime model metadata selects one of four already-compiled InitTyped
  // functions. The template arguments at each call site are compile-time types.
  if (types.tree_index_uint32) {
    if (types.feature_id_uint32) {
      return InitTyped<uint32_t, uint32_t>(context, buffer, length, types);
    }
    return InitTyped<uint16_t, uint32_t>(context, buffer, length, types);
  }
  if (types.feature_id_uint32) {
    return InitTyped<uint32_t, uint16_t>(context, buffer, length, types);
  }
  return InitTyped<uint16_t, uint16_t>(context, buffer, length, types);
}

template <typename IndexType>
static uint32_t ReadIndex(const UnalignedArray<IndexType>& indices, uint32_t index) {
  return static_cast<uint32_t>(indices[index]);
}

static bool HasExpectedByteSize(const flexbuffers::Blob& blob, uint32_t element_count,
                                size_t element_size) {
  const uint64_t blob_size = static_cast<uint64_t>(blob.size());
  const uint64_t count = static_cast<uint64_t>(element_count);
  // FlexBuffers versions used by EON may report a typed blob's element count,
  // while other versions report its byte count.
  return blob_size == count || blob_size == count * element_size;
}

template <typename FeatureIdType, typename TreeIndexType>
static TfLiteStatus PrepareTyped(TfLiteContext* context, TfLiteNode* node) {

  MicroContext* micro_context = GetMicroContext(context);
  // Reverse InitTyped's conversion: void* -> OpData* -> the original derived
  // type. Prepare selected this specialization using the stored tags, so it
  // matches the type constructed in Init. static_cast does not check those tags
  // itself, allocate memory, or copy the object.
  const auto* data = static_cast<const OpDataTree<FeatureIdType, TreeIndexType>*>(
      static_cast<const OpData*>(node->user_data));
  const flexbuffers::Map& m = flexbuffers::GetRoot(data->buffer_t, data->buffer_length).AsMap();

  // The OOB checks below are very important to prevent vulnerabilities where an adversary sends
  // us a malicious TFLite model, similar to: https://nvd.nist.gov/vuln/detail/CVE-2022-23560

  // Keep this calculation unsigned: a forest can contain more nodes than
  // fit in a signed 16-bit (or signed int) value.
  const uint32_t num_nodes = data->num_leaf_nodes + data->num_internal_nodes;

  TF_LITE_ENSURE_EQ(context, strncmp(m["node_value_type"].AsString().c_str(), "float32", 7), 0);
  TF_LITE_ENSURE_EQ(context, strncmp(m["class_index_type"].AsString().c_str(), "uint8", 5), 0);
  TF_LITE_ENSURE_EQ(context, strncmp(m["class_weight_type"].AsString().c_str(), "float32", 7), 0);
  TF_LITE_ENSURE_EQ(context, strncmp(m["equality_operator"].AsString().c_str(), "leq", 3), 0);

  TF_LITE_ENSURE(context, HasExpectedByteSize(m["nodes_featureids"].AsBlob(),
                                                data->num_internal_nodes, sizeof(FeatureIdType)));
  TF_LITE_ENSURE(context, HasExpectedByteSize(m["nodes_values"].AsBlob(),
                                                data->num_internal_nodes, sizeof(float)));
  TF_LITE_ENSURE(context, HasExpectedByteSize(m["nodes_truenodeids"].AsBlob(),
                                                data->num_internal_nodes, sizeof(TreeIndexType)));
  TF_LITE_ENSURE(context, HasExpectedByteSize(m["nodes_falsenodeids"].AsBlob(),
                                                data->num_internal_nodes, sizeof(TreeIndexType)));
  TF_LITE_ENSURE(context, HasExpectedByteSize(m["nodes_weights"].AsBlob(),
                                                data->num_leaf_nodes, sizeof(float)));
  TF_LITE_ENSURE(context, HasExpectedByteSize(m["nodes_classids"].AsBlob(),
                                                data->num_leaf_nodes, sizeof(uint8_t)));
  TF_LITE_ENSURE(context, HasExpectedByteSize(m["tree_root_ids"].AsBlob(),
                                                data->num_trees, sizeof(TreeIndexType)));

  for (uint32_t i = 0; i < data->num_trees; i++) {
    TF_LITE_ENSURE(context,
                   ReadIndex(data->tree_root_ids, i) < num_nodes);
  }
  for (uint32_t i = 0; i < data->num_internal_nodes; i++) {
    TF_LITE_ENSURE(context,
                   ReadIndex(data->nodes_truenodeids, i) < num_nodes);
    TF_LITE_ENSURE(context,
                   ReadIndex(data->nodes_falsenodeids, i) < num_nodes);
  }

  TF_LITE_ENSURE_EQ(context, NumInputs(node), 1);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);
  TfLiteTensor* input = micro_context->AllocateTempInputTensor(node, 0);
  TF_LITE_ENSURE(context, input != nullptr);
  TF_LITE_ENSURE(context, NumDimensions(input) == 2);
  TfLiteTensor* output = micro_context->AllocateTempOutputTensor(node, 0);
  TF_LITE_ENSURE(context, output != nullptr);
  TF_LITE_ENSURE(context, NumDimensions(output) == 2);

  int input_width = SizeOfDimension(input, 1);
  int output_width = SizeOfDimension(output, 1);

  // Check that all indices into the input tensor are valid.
  for (uint32_t i = 0; i < data->num_internal_nodes; i++) {
    TF_LITE_ENSURE(context, ReadIndex(data->nodes_featureids, i) < input_width);
  }

  for (uint32_t i = 0; i < data->num_leaf_nodes; i++) {
    TF_LITE_ENSURE(context, data->nodes_classids[i] < output_width);
  }

  micro_context->DeallocateTempTfLiteTensor(input);
  micro_context->DeallocateTempTfLiteTensor(output);

  return kTfLiteOk;
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  // Recover only the common base first; its tags tell us which typed callback
  // can safely access the rest of this object.
  const OpData* data = static_cast<const OpData*>(node->user_data);
  TF_LITE_ENSURE(context, data != nullptr);
  const IndexTypes types = data->index_types;
  if (types.tree_index_uint32) {
    if (types.feature_id_uint32) {
      return PrepareTyped<uint32_t, uint32_t>(context, node);
    }
    return PrepareTyped<uint16_t, uint32_t>(context, node);
  }
  if (types.feature_id_uint32) {
    return PrepareTyped<uint32_t, uint16_t>(context, node);
  }
  return PrepareTyped<uint16_t, uint16_t>(context, node);
}

template <typename FeatureIdType, typename TreeIndexType>
static TfLiteStatus EvalTyped(TfLiteContext* context, TfLiteNode* node) {

  // Eval has selected the same specialization as Init, so this base-to-derived
  // cast recovers the existing state. The compiler already knows the array
  // element widths here; traversal needs no per-index uint16/uint32 dispatch.
  const auto* data = static_cast<const OpDataTree<FeatureIdType, TreeIndexType>*>(
      static_cast<const OpData*>(node->user_data));
  const TfLiteEvalTensor* input = tflite::micro::GetEvalInput(context, node, 0);
  TfLiteEvalTensor* output = tflite::micro::GetEvalOutput(context, node, 0);
  const float* input_data = tflite::micro::GetTensorData<float>(input);
  float* output_data = tflite::micro::GetTensorData<float>(output);
  memset(output_data, 0,
         tflite::micro::GetTensorShape(output).FlatSize() * sizeof(float));

  for (uint32_t i = 0; i < data->num_trees; i++) {
    uint32_t ix = ReadIndex(data->tree_root_ids, i);
    while (ix < data->num_internal_nodes) {
      if (input_data[ReadIndex(data->nodes_featureids, ix)] <= data->nodes_values[ix]) {
        ix = ReadIndex(data->nodes_truenodeids, ix);
      } else {
        ix = ReadIndex(data->nodes_falsenodeids, ix);
      }
    }
    ix -= data->num_internal_nodes;
    output_data[data->nodes_classids[ix]] += data->nodes_weights[ix];
  }

  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  // Read the cached tags through the base, then call the matching precompiled
  // EvalTyped. No state is reconstructed or model options reparsed per inference.
  const OpData* data = static_cast<const OpData*>(node->user_data);
  TF_LITE_ENSURE(context, data != nullptr);
  const IndexTypes types = data->index_types;
  if (types.tree_index_uint32) {
    if (types.feature_id_uint32) {
      return EvalTyped<uint32_t, uint32_t>(context, node);
    }
    return EvalTyped<uint16_t, uint32_t>(context, node);
  }
  if (types.feature_id_uint32) {
    return EvalTyped<uint32_t, uint16_t>(context, node);
  }
  return EvalTyped<uint16_t, uint16_t>(context, node);
}

}  // namespace tree_ensemble_classifier

TfLiteRegistration* Register_TREE_ENSEMBLE_CLASSIFIER() {
  static TfLiteRegistration r = {
          tree_ensemble_classifier::Init,
          nullptr,
          tree_ensemble_classifier::Prepare,
          tree_ensemble_classifier::Eval,
          /*profiling_string=*/nullptr,
          /*builtin_code=*/0,
          /*custom_name=*/nullptr,
          /*version=*/0};
  return &r;
}

TfLiteRegistration* Register_TFLITE_TREE_ENSEMBLE_CLASSIFIER() {
  return Register_TREE_ENSEMBLE_CLASSIFIER();
}

}  // namespace micro

// Keep the MicroMutableOpResolver API in the tflite namespace.
TfLiteRegistration* Register_TreeEnsembleClassifier() {
  return micro::Register_TREE_ENSEMBLE_CLASSIFIER();
}

const char* GetString_TreeEnsembleClassifier() {
  return "TreeEnsembleClassifier";
}

}  // namespace tflite
