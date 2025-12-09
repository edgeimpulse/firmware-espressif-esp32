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

#ifndef EI_POSTPROCESSING_AI_HUB_H
#define EI_POSTPROCESSING_AI_HUB_H

#if EI_HAS_QC_FACE_DET_LITE
#include "edge-impulse-sdk/classifier/ei_nms.h"
#include <numeric>
#include <array>
#include <limits>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

struct Shape4 {
    size_t N, C, H, W;
};

inline size_t idx4(size_t n, size_t c, size_t h, size_t w, const Shape4& s) {
    return ((n * s.C + c) * s.H + h) * s.W + w; // NCHW row-major
}

struct BBox {
    std::array<float,4> xyrb;                     // [x1, y1, x2, y2]
    float score;
    std::array<std::pair<float,float>,5> landmark;  // five (x,y) pairs
};

// Returns (output_data, output_shape). Mirrors Python version:
// input and output in NCHW
// - Pads with -inf
// - out_h = (H + 2*padding - kernel_size) // stride + 1   (floor)
// - out_w = (W + 2*padding - kernel_size) // stride + 1
template <typename T>
std::pair<std::vector<T>, Shape4>
max_pool2d_nchw(const T* input,
                const Shape4& in_shape,
                int kernel_size,
                int stride,
                int padding)
{
    if (kernel_size <= 0 || stride <= 0 || padding < 0) {
        EI_LOGE("kernel_size/stride must be >0 and padding >=0\n");
        EI_LOGE("kernel_size =%d, stride=%d, padding=%d\n", kernel_size, stride, padding);
        return {{}, {}};
    }

    // Padded shape
    Shape4 pad_shape{in_shape.N, in_shape.C,
                     in_shape.H + 2 * static_cast<size_t>(padding),
                     in_shape.W + 2 * static_cast<size_t>(padding)};

    // Output shape (floor behavior)
    if (pad_shape.H < static_cast<size_t>(kernel_size) ||
        pad_shape.W < static_cast<size_t>(kernel_size)) {
        EI_LOGE("Kernel larger than padded input.\n");
        EI_LOGE("Padded H: %d, W: %d, kernel_size: %d\n",
                static_cast<int>(pad_shape.H),
                static_cast<int>(pad_shape.W),
                kernel_size);
        return {{}, {}};
    }

    // Build padded tensor (fill with -inf)
    const T NEG_INF = std::numeric_limits<T>::lowest();
    std::vector<T> padded(pad_shape.N * pad_shape.C * pad_shape.H * pad_shape.W, NEG_INF);

    // Copy input into padded at offset (padding, padding)
    for (size_t n = 0; n < in_shape.N; ++n) {
        for (size_t c = 0; c < in_shape.C; ++c) {
            for (size_t h = 0; h < in_shape.H; ++h) {
                for (size_t w = 0; w < in_shape.W; ++w) {
                    size_t src = idx4(n, c, h, w, in_shape);
                    size_t dst = idx4(n, c, h + padding, w + padding, pad_shape);
                    padded[dst] = input[src];
                }
            }
        }
    }

    // N, C, H, W = padded.shape
    // out_h = (H - kernel_size) // stride + 1
    // out_w = (W - kernel_size) // stride + 1
    size_t out_h = (pad_shape.H - static_cast<size_t>(kernel_size)) / static_cast<size_t>(stride) + 1;
    size_t out_w = (pad_shape.W - static_cast<size_t>(kernel_size)) / static_cast<size_t>(stride) + 1;
    Shape4 out_shape{in_shape.N, in_shape.C, out_h, out_w};

    // Allocate output
    // output = np.empty((N, C, out_h, out_w), dtype=input.dtype)
    std::vector<T> output(out_shape.N * out_shape.C * out_shape.H * out_shape.W);

    // Max pooling
    for (size_t n = 0; n < pad_shape.N; ++n) {
        for (size_t c = 0; c < pad_shape.C; ++c) {
            for (size_t i = 0; i < out_shape.H; ++i) {
                size_t h_start = i * static_cast<size_t>(stride);
                size_t h_end   = h_start + static_cast<size_t>(kernel_size);

                for (size_t j = 0; j < out_shape.W; ++j) {
                    size_t w_start = j * static_cast<size_t>(stride);
                    size_t w_end   = w_start + static_cast<size_t>(kernel_size);

                    T m = NEG_INF;
                    for (size_t hh = h_start; hh < h_end; ++hh) {
                        for (size_t ww = w_start; ww < w_end; ++ww) {
                            T v = padded[idx4(n, c, hh, ww, pad_shape)];
                            if (v > m) m = v;
                        }
                    }
                    output[idx4(n, c, i, j, out_shape)] = m;
                }
            }
        }
    }

    return {std::move(output), out_shape};
}

// Clamp-sorted IoU on [x1,y1,x2,y2]
inline float box_iou_xyxy(const std::array<float,4>& a, const std::array<float,4>& b) {
    float ax1 = std::min(a[0], a[2]);
    float ax2 = std::max(a[0], a[2]);
    float bx1 = std::min(b[0], b[2]);
    float bx2 = std::max(b[0], b[2]);

    float ay1 = std::min(a[1], a[3]);
    float ay2 = std::max(a[1], a[3]);
    float by1 = std::min(b[1], b[3]);
    float by2 = std::max(b[1], b[3]);

    float ix1 = std::max(ax1, bx1);
    float iy1 = std::max(ay1, by1);
    float ix2 = std::min(ax2, bx2);
    float iy2 = std::min(ay2, by2);

    float iw = std::max(0.f, ix2 - ix1);
    float ih = std::max(0.f, iy2 - iy1);
    float inter = iw * ih;

    float areaA = std::max(0.f, ax2 - ax1) * std::max(0.f, ay2 - ay1);
    float areaB = std::max(0.f, bx2 - bx1) * std::max(0.f, by2 - by1);
    float uni = areaA + areaB - inter;
    return (uni > 0.f) ? (inter / uni) : 0.f;
}

// ------------------------------- NMS ----------------------------------------
inline std::vector<BBox> nms(std::vector<BBox> objs, float iou_thresh = 0.5f) {
    if (objs.size() <= 1) return objs;

    // Sort by score desc
    std::sort(objs.begin(), objs.end(),
              [](const BBox& a, const BBox& b){ return a.score > b.score; });

    std::vector<BBox> keep;
    keep.reserve(objs.size());

    std::vector<char> suppressed(objs.size(), 0);
    for (std::size_t i = 0; i < objs.size(); ++i) {
        if (suppressed[i]) continue;
        keep.push_back(objs[i]);
        for (std::size_t j = i + 1; j < objs.size(); ++j) {
            if (suppressed[j]) continue;
            if (box_iou_xyxy(objs[i].xyrb, objs[j].xyrb) > iou_thresh) {
                suppressed[j] = 1;
            }
        }
    }
    return keep;
}

template<typename T>
std::vector<BBox>
detect(const std::vector<T>& hm,
       const std::vector<T>& box,
       const std::vector<T>& landmark,
       const uint32_t grid_size_x,
       const uint32_t grid_size_y,
       float threshold, float nms_iou_val, int stride = 8)
{
    Shape4 hm_shape({1, 1, grid_size_y, grid_size_x});
    Shape4 box_shape({1, 4, grid_size_y, grid_size_x});
    Shape4 lm_shape({1, 10, grid_size_y, grid_size_x});

    const std::size_t H = hm_shape.H, W = hm_shape.W;
    const std::size_t plane = H * W;

    // 1) hm = sigmoid(hm)
    auto sigmoid = [](T x) { return 1 / (1 + std::exp(-static_cast<float>(x))); };
    std::vector<float> hm_sig;
    for (size_t i = 0; i < hm.size(); ++i) {
        hm_sig.push_back(sigmoid(hm[i]));
    }

    // 2) hm_pool = max_pool2d(hm, 3, 1, 1)
    std::pair<std::vector<float>, Shape4> ret_max_pool2d = max_pool2d_nchw(hm_sig.data(), hm_shape, 3, 1, 1);
    std::vector<float> hm_pool = ret_max_pool2d.first;

    const Shape4 hm_pool_shape = ret_max_pool2d.second;
    if (hm_pool_shape.N != hm_shape.N || hm_pool_shape.C != hm_shape.C ||
        hm_pool_shape.H != hm_shape.H || hm_pool_shape.W != hm_shape.W) {
        EI_LOGE("max_pool2d output shape mismatch\n");
        return {};
    }

    const std::size_t Ntot = hm_sig.size();

    // 3) flat_scores = ((hm == hm_pool).astype(float) * hm).reshape(-1)
    std::vector<float> flat_scores(Ntot);
    for (std::size_t i = 0; i < Ntot; ++i) {
        flat_scores[i] = (hm_sig[i] == hm_pool[i]) ? hm_sig[i] : 0.0f;
    }

    // k = min(total elements, 2000)
    std::size_t k = std::min<std::size_t>(Ntot, 2000);

    // 4) Top-k by value (descending), returning indices and scores
    std::vector<std::size_t> idx(Ntot);
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin() + k, idx.end(),
                      [&](std::size_t a, std::size_t b){
                          return flat_scores[a] > flat_scores[b];
                      });
    idx.resize(k);

    std::vector<float> scores(k);
    for (std::size_t i = 0; i < k; ++i) {
        scores[i] = flat_scores[idx[i]];
    }

    // 5) Convert flat indices -> (y, x) in the last 2 dims
    std::vector<int> xs(k), ys(k);
    for (std::size_t i = 0; i < k; ++i) {
        std::size_t hw_index = idx[i] % plane;  // since N=C=1
        ys[i] = static_cast<int>(hw_index / W);
        xs[i] = static_cast<int>(hw_index % W);
    }

    // 6) Build objects (mirrors your Python loop)
    std::vector<BBox> objs;
    objs.reserve(k);

    for (std::size_t i = 0; i < k; ++i) {
        float s = scores[i];
        if (s < threshold) break;          // sorted desc, so we can early-exit

        int cx = xs[i], cy = ys[i];
        if (cx < 0 || cy < 0 || cx >= static_cast<int>(W) || cy >= static_cast<int>(H))
            continue;

        // box[0, :, cy, cx] -> (x, y, r, b)
        float bx = static_cast<float>(box[idx4(0, 0, cy, cx, box_shape)]);
        float by = static_cast<float>(box[idx4(0, 1, cy, cx, box_shape)]);
        float br = static_cast<float>(box[idx4(0, 2, cy, cx, box_shape)]);
        float bb = static_cast<float>(box[idx4(0, 3, cy, cx, box_shape)]);

        // xyrb = ([cx, cy, cx, cy] + [-x, -y, r, b]) * stride
        std::array<float,4> xyrb = {
            (cx - bx) * stride,
            (cy - by) * stride,
            (cx + br) * stride,
            (cy + bb) * stride
        };

        // landmark[0, :, cy, cx] -> 10 values (x5 then y5),
        // then add [cx]*5 + [cy]*5 and scale by stride
        std::array<std::pair<float,float>,5> lm_pairs;
        for (int p = 0; p < 5; ++p) {
            float lx = static_cast<float>(landmark[idx4(0, p,     cy, cx, lm_shape)]);
            float ly = static_cast<float>(landmark[idx4(0, p + 5, cy, cx, lm_shape)]);
            float X = (lx + cx * 5) * stride;
            float Y = (ly + cy * 5) * stride;
            lm_pairs[p] = {X, Y};
        }

        objs.push_back(BBox{xyrb, s, lm_pairs});
    }

    // 7) NMS
    if (nms_iou_val != -1.0f) {
        objs = nms(std::move(objs), nms_iou_val);
    }

    return objs;
}

template<typename T>
void nhwc_to_nchw_inplace(std::vector<T>& data,
                          int N, int H, int W, int C) {
    if (data.size() != static_cast<size_t>(N * H * W * C)) {
        throw std::runtime_error("Input size does not match given dimensions");
    }

    std::vector<T> temp(data.size());

    for (int n = 0; n < N; ++n) {
        for (int h = 0; h < H; ++h) {
            for (int w = 0; w < W; ++w) {
                for (int c = 0; c < C; ++c) {
                    size_t nhwc_index = ((n * H + h) * W + w) * C + c;
                    size_t nchw_index = ((n * C + c) * H + h) * W + w;
                    temp[nchw_index] = data[nhwc_index];
                }
            }
        }
    }

    data.swap(temp);  // overwrite original with transposed data
}

template<typename T>
__attribute__((unused)) static EI_IMPULSE_ERROR process_qc_face_det_lite_common(const ei_impulse_t *impulse,
                                                                                ei_impulse_result_t *result,
                                                                                T *heatmap_buf,
                                                                                uint32_t heatmap_buf_size,
                                                                                T *bbox_buf,
                                                                                uint32_t bbox_buf_size,
                                                                                T *landmark_buf,
                                                                                uint32_t landmark_buf_size,
                                                                                float zero_point,
                                                                                float scale,
                                                                                float threshold,
                                                                                size_t object_detection_count,
                                                                                ei_object_detection_nms_config_t nms_config) {
    const int width = impulse->input_width;
    const int height = impulse->input_height;
    const uint32_t grid_size_x = width / 8;
    const uint32_t grid_size_y = height / 8;
    static std::vector<ei_impulse_result_bounding_box_t> results;

    results.clear();

    // raw_output_mtx has three matrixes:
    // heatmap:  1, grid_size_y, grid_size_x, 1
    // bbox:     1, grid_size_y, grid_size_x, 4
    // landmark: 1, grid_size_y, grid_size_x, 1

    std::vector<T> heatmap(heatmap_buf, heatmap_buf + heatmap_buf_size);
    std::vector<T> bbox(bbox_buf, bbox_buf + bbox_buf_size);
    std::vector<T> landmark(landmark_buf, landmark_buf + landmark_buf_size);

    nhwc_to_nchw_inplace(heatmap, 1, grid_size_y, grid_size_x, 1);
    nhwc_to_nchw_inplace(bbox, 1, grid_size_y, grid_size_x, 4);
    nhwc_to_nchw_inplace(landmark, 1, grid_size_y, grid_size_x, 10);

    auto dets = detect(heatmap, bbox, landmark, grid_size_x, grid_size_y, threshold, nms_config.iou_threshold);

    std::vector<float> boxes;
    std::vector<float> scores;
    std::vector<int> classes;

    for (auto& box: dets) {
        int32_t xmin = static_cast<int32_t>(std::min(box.xyrb[0], box.xyrb[2]));
        int32_t ymin = static_cast<int32_t>(std::min(box.xyrb[1], box.xyrb[3]));
        int32_t xmax = static_cast<int32_t>(std::max(box.xyrb[0], box.xyrb[2]));
        int32_t ymax = static_cast<int32_t>(std::max(box.xyrb[1], box.xyrb[3]));
        int32_t w    = static_cast<int32_t>(std::abs(box.xyrb[2] - box.xyrb[0]));
        int32_t h    = static_cast<int32_t>(std::abs(box.xyrb[3] - box.xyrb[1]));

        // Clip to image bounds
        if (xmin < 0) xmin = 0;
        if (ymin < 0) ymin = 0;
        if (xmax >= width) xmax = width - 1;
        if (ymax >= height) ymax = height - 1;

        // Enlarge bounding box by 10% (5% on each side), if it still fits
        int32_t b_Left   = xmin - static_cast<int32_t>(w * 0.05f);
        int32_t b_Top    = ymin - static_cast<int32_t>(h * 0.05f);
        int32_t b_Width  = static_cast<int32_t>(w * 1.1f);
        int32_t b_Height = static_cast<int32_t>(h * 1.1f);
        if (b_Left >= 0 && b_Top >= 0 &&
            (b_Width - 1 + b_Left) < width &&
            (b_Height - 1 + b_Top) < height)
        {
            xmin = b_Left;
            ymin = b_Top;
            w = b_Width;
            h = b_Height;
            xmax = w - 1 + xmin;
            ymax = h - 1 + ymin;
        }

        if (box.score >= threshold && box.score <= 1.0f) {
            boxes.push_back(ymin);
            boxes.push_back(xmin);
            boxes.push_back(ymax);
            boxes.push_back(xmax);
            scores.push_back(box.score);
            // this model always detects one class (face)
            classes.push_back(0);
        }
    }

    EI_IMPULSE_ERROR nms_res = ei_run_nms(impulse,
                                        &results,
                                        boxes.data(),
                                        scores.data(),
                                        classes.data(),
                                        scores.size(),
                                        true /*clip_boxes*/,
                                        &nms_config);

    if (nms_res != EI_IMPULSE_OK)
        return nms_res;

    prepare_nms_results_common(object_detection_count, result, &results);

    return EI_IMPULSE_OK;
}
#endif // EI_HAS_QC_FACE_DET_LITE

__attribute__((unused)) static EI_IMPULSE_ERROR process_qc_face_det_lite_f32(ei_impulse_handle_t *handle,
                                                                             uint32_t block_index,
                                                                             uint32_t input_block_id,
                                                                             ei_impulse_result_t *result,
                                                                             void *config_ptr,
                                                                             void* state) {
#if EI_HAS_QC_FACE_DET_LITE
    const ei_impulse_t *impulse = handle->impulse;
    const ei_fill_result_object_detection_f32_config_t *config = (ei_fill_result_object_detection_f32_config_t*)config_ptr;

    ei::matrix_t* heatmap_mtx = NULL;
    ei::matrix_t* bbox_mtx = NULL;
    ei::matrix_t* landmark_mtx = NULL;

    find_mtx_by_idx(result->_raw_outputs, &heatmap_mtx, input_block_id + 0, impulse->learning_blocks_size + 3);
    find_mtx_by_idx(result->_raw_outputs, &bbox_mtx, input_block_id + 1, impulse->learning_blocks_size + 3);
    find_mtx_by_idx(result->_raw_outputs, &landmark_mtx, input_block_id + 2, impulse->learning_blocks_size + 3);

    const uint32_t width = impulse->input_width;
    const uint32_t height = impulse->input_height;

    // by design, width and height must be multiples of 32
    if (width % 32 != 0 || height % 32 != 0) {
        EI_LOGE("Input width and height must be multiples of 32\n");
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }

    // however, the grid cell size is 8x8, not 32x32
    const uint32_t grid_size_x = width / 8;
    const uint32_t grid_size_y = height / 8;

    if (heatmap_mtx == NULL || bbox_mtx == NULL || landmark_mtx == NULL) {
        EI_LOGE("Could not find required matrices in raw outputs\n");
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }

    if(heatmap_mtx->cols * heatmap_mtx->rows != grid_size_x * grid_size_y) {
        EI_LOGE("Heat map size is incorrect %d != %d\n", heatmap_mtx->cols * heatmap_mtx->rows, grid_size_x * grid_size_y);
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }

    if(bbox_mtx->cols * bbox_mtx->rows != grid_size_x * grid_size_y * 4) {
        EI_LOGE("Bounding box size is incorrect %d != %d\n", bbox_mtx->cols * bbox_mtx->rows, grid_size_x * grid_size_y * 4);
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }

    if(landmark_mtx->cols * landmark_mtx->rows != grid_size_x * grid_size_y * 10) {
        EI_LOGE("Landmark size is incorrect %d != %d\n", landmark_mtx->cols * landmark_mtx->rows, grid_size_x * grid_size_y * 10);
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }

    return process_qc_face_det_lite_common(impulse,
                                           result,
                                           heatmap_mtx->buffer,
                                           heatmap_mtx->cols * heatmap_mtx->rows,
                                           bbox_mtx->buffer,
                                           bbox_mtx->cols * bbox_mtx->rows,
                                           landmark_mtx->buffer,
                                           landmark_mtx->cols * landmark_mtx->rows,
                                           0.0f,
                                           1.0f,
                                           config->threshold,
                                           config->object_detection_count,
                                           config->nms_config);
#else
    return EI_IMPULSE_LAST_LAYER_NOT_AVAILABLE;
#endif // #ifdef EI_HAS_QC_FACE_DET_LITE
}

#endif /* EI_POSTPROCESSING_AI_HUB_H */
