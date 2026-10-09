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

#ifndef EI_OBJECT_TRACKING_SORT_H
#define EI_OBJECT_TRACKING_SORT_H

#include "edge-impulse-sdk/classifier/ei_classifier_types.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/postprocessing/alignment/rectangular_lsap.hpp"
#include "edge-impulse-sdk/classifier/postprocessing/ei_postprocessing_common.h"
#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/porting/ei_logging.h"
#include "model-parameters/model_metadata.h"

extern ei_impulse_handle_t &ei_default_impulse;

#if EI_CLASSIFIER_OBJECT_TRACKING_SORT_ENABLED == 1

// This is a C++ implementation of the SORT algorithm (https://arxiv.org/abs/1602.00763) for object tracking.
// Some constans and design choices are based on the authors' reference python implementation
// https://github.com/abewley/sort

#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

static inline bool labels_match(const char *a, const char *b)
{
    // If labels are not provided, don't constrain matching.
    if (a == nullptr || b == nullptr) {
        return true;
    }
    return std::strcmp(a, b) == 0;
}

struct BBox {
    float x1, y1, x2, y2;
    float score; // optional; SORT core doesn't require it
    const char *label { nullptr };
};

static inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(v, hi));
}

static inline float iou(const BBox &a, const BBox &b)
{
    const float xx1 = std::max(a.x1, b.x1);
    const float yy1 = std::max(a.y1, b.y1);
    const float xx2 = std::min(a.x2, b.x2);
    const float yy2 = std::min(a.y2, b.y2);
    const float w = std::max(0.0f, xx2 - xx1);
    const float h = std::max(0.0f, yy2 - yy1);
    const float inter = w * h;
    const float areaA = (a.x2 - a.x1) * (a.y2 - a.y1);
    const float areaB = (b.x2 - b.x1) * (b.y2 - b.y1);
    const float uni = areaA + areaB - inter;
    if (uni <= 0.0f) {
        return 0.0f;
    }
    return inter / uni;
}

// Convert [x1,y1,x2,y2] to z=[x,y,s,r] with x,y center, s area, r aspect (w/h)
static inline std::array<double, 4> bbox_to_z(const BBox &bb)
{
    const double w = std::max(0.0f, bb.x2 - bb.x1);
    const double h = std::max(0.0f, bb.y2 - bb.y1);
    const double x = bb.x1 + w / 2.0;
    const double y = bb.y1 + h / 2.0;
    const double s = w * h;
    const double r = (h > 1e-12) ? (w / h) : 0.0;
    return { x, y, s, r };
}

// Convert x=[x,y,s,r,...] to bbox [x1,y1,x2,y2] using w=sqrt(s*r), h=s/w
static inline BBox
x_to_bbox(const std::array<double, 7> &x, const char *label = nullptr, float score = 1.0f)
{
    const double cx = x[0], cy = x[1], s = x[2], r = x[3];
    double w = 0.0, h = 0.0;
    if (s > 0.0 && r > 0.0) {
        w = std::sqrt(s * r);
        h = (w > 1e-12) ? (s / w) : 0.0;
    }
    BBox bb;
    bb.x1 = static_cast<float>(cx - w / 2.0);
    bb.y1 = static_cast<float>(cy - h / 2.0);
    bb.x2 = static_cast<float>(cx + w / 2.0);
    bb.y2 = static_cast<float>(cy + h / 2.0);
    bb.score = score;
    bb.label = label;
    return bb;
}

// Invert 4x4 matrix using Gauss-Jordan with partial pivoting.
// Returns false if singular/ill-conditioned.
static bool invert4x4(const double A[4][4], double invA[4][4])
{
    double aug[4][8];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            aug[i][j] = A[i][j];
        }
        for (int j = 0; j < 4; ++j) {
            aug[i][4 + j] = (i == j) ? 1.0 : 0.0;
        }
    }

    for (int col = 0; col < 4; ++col) {
        int piv = col;
        double best = std::fabs(aug[col][col]);
        for (int r = col + 1; r < 4; ++r) {
            double v = std::fabs(aug[r][col]);
            if (v > best) {
                best = v;
                piv = r;
            }
        }
        if (best < 1e-12) {
            return false;
        }
        if (piv != col) {
            for (int c = 0; c < 8; ++c) {
                std::swap(aug[piv][c], aug[col][c]);
            }
        }

        const double diag = aug[col][col];
        for (int c = 0; c < 8; ++c) {
            aug[col][c] /= diag;
        }

        for (int r = 0; r < 4; ++r) {
            if (r == col) {
                continue;
            }
            const double f = aug[r][col];
            if (std::fabs(f) < 1e-18) {
                continue;
            }
            for (int c = 0; c < 8; ++c) {
                aug[r][c] -= f * aug[col][c];
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            invA[i][j] = aug[i][4 + j];
        }
    }
    return true;
}

// Minimal fixed-size Kalman filter for SORT.
// State: [u,v,s,r, u_dot,v_dot,s_dot]^T (7D), measurement: [u,v,s,r]^T (4D)
struct Kalman7x4 {
    std::array<double, 7> x {}; // state
    double P[7][7] {}; // covariance
    double F[7][7] {}; // state transition
    double H[4][7] {}; // measurement matrix
    double Q[7][7] {}; // process noise
    double R[4][4] {}; // measurement noise

    Kalman7x4()
    {
        // F as in authors' reference implementation: constant velocity, dt=1
        // [1 0 0 0 1 0 0]
        // [0 1 0 0 0 1 0]
        // [0 0 1 0 0 0 1]
        // [0 0 0 1 0 0 0]
        // [0 0 0 0 1 0 0]
        // [0 0 0 0 0 1 0]
        // [0 0 0 0 0 0 1]
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                F[i][j] = 0.0;
            }
        }
        F[0][0] = 1;
        F[0][4] = 1;
        F[1][1] = 1;
        F[1][5] = 1;
        F[2][2] = 1;
        F[2][6] = 1;
        F[3][3] = 1;
        F[4][4] = 1;
        F[5][5] = 1;
        F[6][6] = 1;

        // H maps state -> measurement [u,v,s,r]
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 7; ++j) {
                H[i][j] = 0.0;
            }
        }
        H[0][0] = 1;
        H[1][1] = 1;
        H[2][2] = 1;
        H[3][3] = 1;

        // Default R, Q, P initialized similarly to authors' implementation,
        // Start with identity-ish then scale.
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                R[i][j] = (i == j) ? 1.0 : 0.0;
            }
        }
        R[2][2] *= 10.0;
        R[3][3] *= 10.0;

        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                Q[i][j] = 0.0;
            }
        }
        for (int i = 0; i < 7; ++i) {
            Q[i][i] = 1.0;
        }
        Q[6][6] *= 0.01;
        for (int i = 4; i < 7; ++i) {
            Q[i][i] *= 0.01;
        }

        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                P[i][j] = 0.0;
            }
        }
        for (int i = 0; i < 7; ++i) {
            P[i][i] = 10.0;
        }
        for (int i = 4; i < 7; ++i) {
            P[i][i] *= 1000.0; // high uncertainty on initial velocities
        }
    }

    void init_from_bbox(const BBox &bb)
    {
        const auto z = bbox_to_z(bb);
        x = { z[0], z[1], z[2], z[3], 0.0, 0.0, 0.0 };
    }

    void predict()
    {
        // Prevent scale from going negative (matches python guard)
        if ((x[6] + x[2]) <= 0.0) {
            x[6] = 0.0;
        }

        // x = F x
        std::array<double, 7> xp {};
        for (int i = 0; i < 7; ++i) {
            double s = 0.0;
            for (int j = 0; j < 7; ++j) {
                s += F[i][j] * x[j];
            }
            xp[i] = s;
        }
        x = xp;

        // P = F P F^T + Q
        double FP[7][7];
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                double s = 0.0;
                for (int k = 0; k < 7; ++k) {
                    s += F[i][k] * P[k][j];
                }
                FP[i][j] = s;
            }
        }
        double FPFt[7][7];
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                double s = 0.0;
                for (int k = 0; k < 7; ++k) {
                    s += FP[i][k] * F[j][k]; // FPF^T
                }
                FPFt[i][j] = s + Q[i][j];
            }
        }
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                P[i][j] = FPFt[i][j];
            }
        }
    }

    void update(const BBox &bb)
    {
        const auto z = bbox_to_z(bb);

        // y = z - Hx
        double y[4];
        for (int i = 0; i < 4; ++i) {
            double hx = 0.0;
            for (int j = 0; j < 7; ++j) {
                hx += H[i][j] * x[j];
            }
            y[i] = z[i] - hx;
        }

        // S = HPH^T + R (4x4)
        double HP[4][7];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 7; ++j) {
                double s = 0.0;
                for (int k = 0; k < 7; ++k) {
                    s += H[i][k] * P[k][j];
                }
                HP[i][j] = s;
            }
        }
        double S[4][4];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                double s = 0.0;
                for (int k = 0; k < 7; ++k) {
                    s += HP[i][k] * H[j][k]; // HP H^T
                }
                S[i][j] = s + R[i][j];
            }
        }

        double invS[4][4];
        if (!invert4x4(S, invS)) {
            // If degenerate, skip update (rare in practice).
            return;
        }

        // K = P H^T invS  => (7x4)
        double PHt[7][4];
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 4; ++j) {
                double s = 0.0;
                for (int k = 0; k < 7; ++k) {
                    s += P[i][k] * H[j][k];
                }
                PHt[i][j] = s;
            }
        }
        double K[7][4];
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 4; ++j) {
                double s = 0.0;
                for (int k = 0; k < 4; ++k) {
                    s += PHt[i][k] * invS[k][j];
                }
                K[i][j] = s;
            }
        }

        // x = x + K y
        for (int i = 0; i < 7; ++i) {
            double s = 0.0;
            for (int j = 0; j < 4; ++j) {
                s += K[i][j] * y[j];
            }
            x[i] += s;
        }

        // P = (I - K H) P
        double KH[7][7];
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                double s = 0.0;
                for (int k = 0; k < 4; ++k) {
                    s += K[i][k] * H[k][j];
                }
                KH[i][j] = s;
            }
        }
        double IminusKH[7][7];
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                IminusKH[i][j] = (i == j ? 1.0 : 0.0) - KH[i][j];
            }
        }
        double newP[7][7];
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                double s = 0.0;
                for (int k = 0; k < 7; ++k) {
                    s += IminusKH[i][k] * P[k][j];
                }
                newP[i][j] = s;
            }
        }
        for (int i = 0; i < 7; ++i) {
            for (int j = 0; j < 7; ++j) {
                P[i][j] = newP[i][j];
            }
        }
    }
};

// Fast rectangular Hungarian (min-cost assignment) using potentials (O(n^2 m))
// Returns pairs (row, col) for assigned rows.
// If n==0 or m==0, returns empty.
static std::vector<std::pair<int, int>> hungarian_min(const std::vector<std::vector<double>> &cost)
{
    const int n = static_cast<int>(cost.size());
    const int m = n ? static_cast<int>(cost[0].size()) : 0;
    std::vector<std::pair<int, int>> result;
    if (n == 0 || m == 0) {
        return result;
    }

    // Ensure n <= m for this implementation; if not, transpose.
    if (n > m) {
        std::vector<std::vector<double>> ct(m, std::vector<double>(n));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < m; ++j) {
                ct[j][i] = cost[i][j];
            }
        }
        auto tr = hungarian_min(ct);
        result.reserve(tr.size());
        for (auto &p : tr) {
            result.emplace_back(p.second, p.first);
        }
        return result;
    }

    const double INF = 1e100;
    std::vector<double> u(n + 1, 0.0), v(m + 1, 0.0);
    std::vector<int> p(m + 1, 0), way(m + 1, 0);

    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<double> minv(m + 1, INF);
        std::vector<char> used(m + 1, false);
        do {
            used[j0] = true;
            int i0 = p[j0];
            double delta = INF;
            int j1 = 0;
            for (int j = 1; j <= m; ++j) {
                if (!used[j]) {
                    double cur = cost[i0 - 1][j - 1] - u[i0] - v[j];
                    if (cur < minv[j]) {
                        minv[j] = cur;
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            }
            for (int j = 0; j <= m; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                }
                else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);

        // Augmenting
        do {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }

    // p[j] = assigned row for column j
    std::vector<int> assign_row(n + 1, 0); // row -> col
    for (int j = 1; j <= m; ++j) {
        if (p[j] != 0) {
            assign_row[p[j]] = j;
        }
    }

    result.reserve(n);
    for (int i = 1; i <= n; ++i) {
        if (assign_row[i]) {
            result.emplace_back(i - 1, assign_row[i] - 1);
        }
    }
    return result;
}

struct KalmanBoxTracker {
    Kalman7x4 kf;
    int time_since_update = 0;
    int id = -1;
    int output_id = -1;
    int hits = 0;
    int hit_streak = 0;
    int age = 0;
    const char *label = nullptr;
    float score = 1.0f;

    static int next_id;
    static int next_output_id;

    explicit KalmanBoxTracker(const BBox &init_bb)
    {
        kf.init_from_bbox(init_bb);
        id = next_id++;
        label = init_bb.label;
        score = init_bb.score;
    }

    void ensure_output_id()
    {
        if (output_id < 0) {
            output_id = next_output_id++;
        }
    }

    void update(const BBox &bb)
    {
        time_since_update = 0;
        hits++;
        hit_streak++;
        // Persist the most recent class label for this track.
        label = bb.label;
        score = bb.score;
        kf.update(bb);
    }

    BBox predict()
    {
        kf.predict();
        age++;
        if (time_since_update > 0) {
            hit_streak = 0;
        }
        time_since_update++;
        return x_to_bbox(kf.x, label, score);
    }

    BBox get_state() const
    {
        return x_to_bbox(kf.x, label, score);
    }
};
int KalmanBoxTracker::next_id = 0;
int KalmanBoxTracker::next_output_id = 0;

// Association similar to SORT:
// - build IOU matrix between detections and predicted trackers
// - if unique one-to-one above threshold -> use directly else Hungarian on -IOU
// - reject matches with IOU < threshold
static void associate_detections_to_trackers(
    const std::vector<BBox> &detections,
    const std::vector<BBox> &trackers_pred,
    float iou_threshold,
    std::vector<std::pair<int, int>> &matches,
    std::vector<int> &unmatched_dets,
    std::vector<int> &unmatched_trks)
{
    matches.clear();
    unmatched_dets.clear();
    unmatched_trks.clear();

    const int D = static_cast<int>(detections.size());
    const int T = static_cast<int>(trackers_pred.size());
    // shortcut if no trackers exist, all detections are unmatched
    if (T == 0) {
        unmatched_dets.resize(D);
        std::iota(unmatched_dets.begin(), unmatched_dets.end(), 0);
        return;
    }

    // initial IOU matrix (Det x Trackers); zero if labels don't match
    std::vector<std::vector<double>> iou_mat(D, std::vector<double>(T, 0.0));
    for (int d = 0; d < D; ++d) {
        for (int t = 0; t < T; ++t) {
            iou_mat[d][t] = labels_match(detections[d].label, trackers_pred[t].label)
                ? static_cast<double>(iou(detections[d], trackers_pred[t]))
                : 0.0;
        }
    }

    // Check if each row/col has at most one candidate above threshold.
    bool trivial = true;
    if (D > 0 && T > 0) {
        for (int d = 0; d < D && trivial; ++d) {
            int c = 0;
            for (int t = 0; t < T; ++t) {
                if (iou_mat[d][t] > iou_threshold) {
                    ++c;
                }
            }
            if (c > 1) {
                trivial = false;
            }
        }
        for (int t = 0; t < T && trivial; ++t) {
            int c = 0;
            for (int d = 0; d < D; ++d) {
                if (iou_mat[d][t] > iou_threshold) {
                    ++c;
                }
            }
            if (c > 1) {
                trivial = false;
            }
        }
    }

    std::vector<std::pair<int, int>> cand;
    if (trivial) {
        for (int d = 0; d < D; ++d) {
            for (int t = 0; t < T; ++t) {
                if (iou_mat[d][t] > iou_threshold) {
                    cand.emplace_back(d, t);
                }
            }
        }
    }
    else {
        // Hungarian on cost = -IOU (maximize IOU)
        std::vector<std::vector<double>> cost(D, std::vector<double>(T, 0.0));
        for (int d = 0; d < D; ++d) {
            for (int t = 0; t < T; ++t) {
                cost[d][t] = -iou_mat[d][t];
            }
        }
        cand = hungarian_min(cost);
    }

    std::vector<char> det_assigned(D, false), trk_assigned(T, false);
    // Filter out low-IOU assignments
    for (auto &m : cand) {
        const int d = m.first, t = m.second;
        if (d < 0 || d >= D || t < 0 || t >= T) {
            continue;
        }
        if (iou_mat[d][t] < iou_threshold) {
            continue;
        }
        matches.emplace_back(d, t);
        det_assigned[d] = true;
        trk_assigned[t] = true;
    }

    for (int d = 0; d < D; ++d) {
        if (!det_assigned[d]) {
            unmatched_dets.push_back(d);
        }
    }
    for (int t = 0; t < T; ++t) {
        if (!trk_assigned[t]) {
            unmatched_trks.push_back(t);
        }
    }
}

struct TrackResult {
    BBox bbox;
    int id; // 1-based if you want MOTChallenge style; here we keep 0-based by default.
};

class SORTTracker {
public:
    // In the paper TLost=1 in experiments; min_hits is the "probationary" period.
    SORTTracker(
        int max_age_,
        int min_hits_,
        float iou_threshold_,
        const char *const *_classes_to_track,
        uint32_t _classes_to_track_count)
        : max_age(max_age_)
        , min_hits(min_hits_)
        , iou_threshold(iou_threshold_)
        , classes_to_track(_classes_to_track)
        , classes_to_track_count(_classes_to_track_count)
    {
    }

    void process_new_detections(const std::vector<ei_impulse_result_bounding_box_t> &detections_in)
    {
        frame_count_++;
        std::vector<BBox> detections;
        detections.reserve(detections_in.size());

        for (const auto &det : detections_in) {
            bool track_detection =
                (classes_to_track == nullptr) || (classes_to_track_count == 0);

            for (uint32_t cls = 0; cls < classes_to_track_count; cls++) {
                if (classes_to_track[cls] != nullptr && strcmp(det.label, classes_to_track[cls]) == 0) {
                    track_detection = true;
                    break;
                }
            }
            if (!track_detection) {
                continue;
            }

            BBox b;
            // input is top-left + width/height; tracker uses centers
            b.x1 = static_cast<float>(det.x);
            b.y1 = static_cast<float>(det.y);
            b.x2 = static_cast<float>(det.x + det.width);
            b.y2 = static_cast<float>(det.y + det.height);
            b.label = det.label;
            b.score = det.value;
            detections.push_back(b);
        }

        // 1) Predict existing trackers.
        std::vector<BBox> trks_pred;
        trks_pred.reserve(trackers_.size());
        for (auto &trk : trackers_) {
            trks_pred.push_back(trk.predict());
        }

        // 2) Associate detections to trackers.
        std::vector<std::pair<int, int>> matches;
        std::vector<int> unmatched_dets;
        std::vector<int> unmatched_trks; //not used at all in this implementation but could be useful for extensions
        associate_detections_to_trackers(
            detections,
            trks_pred,
            iou_threshold,
            matches,
            unmatched_dets,
            unmatched_trks);

        // 3) Update matched trackers with assigned detections.
        for (auto &mt : matches) {
            const int d = mt.first;
            const int t = mt.second;
            trackers_[t].update(detections[d]);
        }

        // 4) Create new trackers for unmatched detections.
        for (int idx : unmatched_dets) {
            trackers_.emplace_back(detections[idx]);
        }

        // 5) Prepare output + prune dead trackers.
        object_tracking_output.clear();
        object_tracking_output.reserve(trackers_.size());

        // Iterate backwards when erasing.
        for (int i = static_cast<int>(trackers_.size()) - 1; i >= 0; --i) {
            auto &trk = trackers_[i];

            // Output only “confirmed” tracks, as in reference SORT:
            // if time_since_update < 1 AND (hit_streak >= min_hits OR frame_count <= min_hits)
            if (trk.time_since_update < 1 &&
                (trk.hit_streak >= min_hits || frame_count_ <= min_hits)) {
                trk.ensure_output_id();
                ei_object_tracking_trace_t trace_result = { 0 };
                BBox bbox = trk.get_state();
                trace_result.x = static_cast<uint32_t>(clampf(bbox.x1, 0.0f, 65535.0f));
                trace_result.y = static_cast<uint32_t>(clampf(bbox.y1, 0.0f, 65535.0f));
                trace_result.width =
                    static_cast<uint32_t>(clampf(bbox.x2 - bbox.x1, 0.0f, 65535.0f));
                trace_result.height =
                    static_cast<uint32_t>(clampf(bbox.y2 - bbox.y1, 0.0f, 65535.0f));
                trace_result.label = bbox.label;
                trace_result.id = trk.output_id; // 0-based; sequential for emitted tracks
                trace_result.value = bbox.score;
                object_tracking_output.push_back(trace_result);
            }

            // Remove dead tracklets.
            if (trk.time_since_update > max_age) {
                trackers_.erase(trackers_.begin() + i);
            }
        }
    }

    void reset_ids()
    {
        KalmanBoxTracker::next_id = 0;
        KalmanBoxTracker::next_output_id = 0;
    }

private:
    int frame_count_ = 0;
    std::vector<KalmanBoxTracker> trackers_;

public:
    int max_age;
    int min_hits;
    float iou_threshold;
    const char *const *classes_to_track;
    uint32_t classes_to_track_count;
    std::vector<ei_object_tracking_trace_t> object_tracking_output;
};

EI_IMPULSE_ERROR init_object_tracking(ei_impulse_handle_t *handle, void **state, void *config)
{
    const ei_object_tracking_sort_config_t *ei_object_tracking_config =
        (ei_object_tracking_sort_config_t *)config;

    // Allocate the object counter
    SORTTracker *object_tracker = new SORTTracker(
        ei_object_tracking_config->max_age,
        ei_object_tracking_config->min_hits,
        ei_object_tracking_config->iou_threshold,
        ei_object_tracking_config->classes_to_track,
        ei_object_tracking_config->classes_to_track_count);
    if (!object_tracker) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }

    // Store the object counter state
    *state = (void *)object_tracker;

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR deinit_object_tracking(void *state, void *config)
{
    SORTTracker *object_tracker = (SORTTracker *)state;

    if (object_tracker) {
        delete object_tracker;
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR process_object_tracking(
    ei_impulse_handle_t *handle,
    uint32_t block_index,
    uint32_t input_block_id,
    ei_impulse_result_t *result,
    void *config_ptr,
    void *state)
{
    SORTTracker *object_tracker = (SORTTracker *)state;
    ei_object_tracking_sort_config_t *config = (ei_object_tracking_sort_config_t*)config_ptr;

    if ((void *)object_tracker != NULL) {
        ei_impulse_result_bounding_box_t *bbs = result->bounding_boxes;
        uint32_t bbs_num = result->bounding_boxes_count;
        std::vector<ei_impulse_result_bounding_box_t> detections(bbs, bbs + bbs_num);

        object_tracker->max_age = config->max_age;
        object_tracker->min_hits = config->min_hits;
        object_tracker->iou_threshold = config->iou_threshold;
        object_tracker->process_new_detections(detections);

        result->postprocessed_output.object_tracking_output.open_traces =
            object_tracker->object_tracking_output.data();
        result->postprocessed_output.object_tracking_output.open_traces_count =
            object_tracker->object_tracking_output.size();
    }
    else {
        EI_LOGW(
            "process_object_tracking: object_tracker is NULL, did you forget to call "
            "run_classifier_init()?\n");
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR display_object_tracking(ei_impulse_result_t *result, void *config)
{
    // print the open traces
    ei_printf("Open traces:\r\n");
    for (uint32_t i = 0; i < result->postprocessed_output.object_tracking_output.open_traces_count;
         i++) {
        ei_object_tracking_trace_t trace =
            result->postprocessed_output.object_tracking_output.open_traces[i];
        ei_printf(
            "  Trace %d: %s [ x: %u, y: %u, width: %u, height: %u ]\r\n",
            trace.id,
            (trace.label != NULL) ? trace.label : "(unknown)",
            trace.x,
            trace.y,
            trace.width,
            trace.height);
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR
set_post_process_params(ei_impulse_handle_t *handle, ei_object_tracking_sort_config_t *params)
{
    int16_t block_number = get_block_number(handle, (void *)init_object_tracking);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    SORTTracker *object_tracker = (SORTTracker *)handle->post_processing_state[block_number];

    object_tracker->max_age = params->max_age;
    object_tracker->min_hits = params->min_hits;
    object_tracker->iou_threshold = params->iou_threshold;
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR
get_post_process_params(ei_impulse_handle_t *handle, ei_object_tracking_sort_config_t *params)
{
    int16_t block_number = get_block_number(handle, (void *)init_object_tracking);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    SORTTracker *object_tracker = (SORTTracker *)handle->post_processing_state[block_number];

    params->max_age = object_tracker->max_age;
    params->min_hits = object_tracker->min_hits;
    params->iou_threshold = object_tracker->iou_threshold;
    return EI_IMPULSE_OK;
}

// versions that operate on the default impulse
EI_IMPULSE_ERROR set_post_process_params(ei_object_tracking_sort_config_t *params)
{
    ei_impulse_handle_t *handle = &ei_default_impulse;

    if (handle->post_processing_state != NULL) {
        set_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_object_tracking_sort_config_t *params)
{
    ei_impulse_handle_t *handle = &ei_default_impulse;

    if (handle->post_processing_state != NULL) {
        get_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

#endif // EI_CLASSIFIER_OBJECT_TRACKING_SORT_ENABLED
#endif // EI_OBJECT_TRACKING_SORT_H
