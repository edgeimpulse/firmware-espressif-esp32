/*
 * Copyright (c) 2024 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EI_OBJECT_TRACKING_H
#define EI_OBJECT_TRACKING_H

#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/porting/ei_logging.h"
#include "edge-impulse-sdk/classifier/postprocessing/ei_postprocessing_common.h"
#include "model-parameters/model_metadata.h"

extern ei_impulse_handle_t & ei_default_impulse;

#include <vector>
#include "tinyEKF/tinyekf.hpp"
#include "alignment/ei_alignment.hpp"

float clip(float num, float min_val = -3.4028235e+38, float max_val = 3.4028235e+38) {
    return std::fmax(min_val, std::fmin(num, max_val));
}

#if EI_CLASSIFIER_OBJECT_TRACKING_ENABLED == 1

typedef struct {
    float keep_grace;
} ei_obj_tracking_params_t;

class ExponentialMovingAverage {
public:
    ExponentialMovingAverage(int n, float gain = 2) : gain(gain / (n + 1)), ema_value(-255.0) {
    }

    void update(float value) {
        if (ema_value == -255.0) {
            ema_value = value;
        } else {
            ema_value = (value * gain) + (ema_value * (1 - gain));
        }
    }

    float smoothed_value() {
        return ema_value;
    }

private:
    float gain;
    float ema_value;
};

class Trace {
public:
    Trace(int id, int t, const ei_impulse_result_bounding_box_t& initial_bbox, uint32_t max_observations = 5)
        : id(id), last_ground_truth_update_t(t), last_prediction(initial_bbox), max_observations(max_observations) {
        if (max_observations < 2) {
            EI_LOGE("%s", "max_observations needs to be at least 2 for counting");
        }

        trace_label = initial_bbox.label;
        observations.push_back(initial_bbox);
        float initial_centroid[2] = { initial_bbox.x + static_cast<float>(initial_bbox.width) / 2,
                                      initial_bbox.y + static_cast<float>(initial_bbox.height) / 2 };

        float initial_width_height[2] = { static_cast<float>(initial_bbox.width),
                                          static_cast<float>(initial_bbox.height) };

        centroid_filter = new TinyEKF(initial_centroid, 8, 2);
        width_height_filter = new TinyEKF(initial_width_height, 8, 2);

        xyxy_emas[0] = new ExponentialMovingAverage(this->max_observations);
        xyxy_emas[1] = new ExponentialMovingAverage(this->max_observations);
        xyxy_emas[2] = new ExponentialMovingAverage(this->max_observations);
        xyxy_emas[3] = new ExponentialMovingAverage(this->max_observations);
    }

    ~Trace() {
        delete centroid_filter;
        delete width_height_filter;
        delete xyxy_emas[0];
        delete xyxy_emas[1];
        delete xyxy_emas[2];
        delete xyxy_emas[3];
    }

    ei_impulse_result_bounding_box_t predict() {

        fx_centroid[0] = centroid_filter->x[0];
        fx_centroid[1] = centroid_filter->x[1];
        fx_width_height[0] = width_height_filter->x[0];
        fx_width_height[1] = width_height_filter->x[1];

        centroid_filter->predict(fx_centroid);
        width_height_filter->predict(fx_width_height);

        ei_impulse_result_bounding_box_t p_bbox = {"", 0, 0, 0, 0, 0.0};
        p_bbox.label = trace_label;
        p_bbox.x = clip((centroid_filter->x[0] - width_height_filter->x[0] / 2), 0);
        p_bbox.y = clip(centroid_filter->x[1] - width_height_filter->x[1] / 2, 0);
        p_bbox.width = clip(width_height_filter->x[0], 0);
        p_bbox.height = clip(width_height_filter->x[1], 0);
        p_bbox.value = 0.0;
        last_prediction = p_bbox;
        EI_LOGD("predict %d %d %d %d %f\n", last_prediction.x, last_prediction.y, last_prediction.width, last_prediction.height, last_prediction.value);
        return last_prediction;
    }

    void update(int t, const ei_impulse_result_bounding_box_t* bbox) {
        if (bbox == nullptr) {
            EI_LOGD("update (last prediction) %d %d %d %d %f\n", last_prediction.x, last_prediction.y, last_prediction.width, last_prediction.height, last_prediction.value);
            bbox = &last_prediction;
        } else {
            EI_LOGD("update (ground truth prediction) %d %d %d %d %f\n", bbox->x, bbox->y, bbox->width, bbox->height, bbox->value);
            last_ground_truth_update_t = t;
        }

        hx_centroid[0] = centroid_filter->x[0];
        hx_centroid[1] = centroid_filter->x[1];
        hx_width_height[0] = width_height_filter->x[0];
        hx_width_height[1] = width_height_filter->x[1];

        float centroid[2] = { bbox->x + static_cast<float>(bbox->width) / 2,
                              bbox->y + static_cast<float>(bbox->height) / 2 };
        centroid_filter->update(centroid , hx_centroid);

        float width_height[2] = { static_cast<float>(bbox->width),
                                  static_cast<float>(bbox->height) };
        width_height_filter->update(width_height, hx_width_height);

        observations.push_back(*bbox);
        while (observations.size() > max_observations) {
            observations.erase(observations.begin());
        }

        xyxy_emas[0]->update(bbox->x);
        xyxy_emas[1]->update(bbox->y);
        xyxy_emas[2]->update(bbox->width);
        xyxy_emas[3]->update(bbox->height);

    }

    std::tuple<int, int, int, int> last_centroid_segment() const {
        if (observations.size() < 2) {
            return {};
        }
        auto obs_t_minus1 = observations[observations.size() - 2];
        auto obs_t_0 = observations.back();

        return {obs_t_minus1.x + static_cast<float>(obs_t_minus1.width) / 2,
                obs_t_minus1.y + static_cast<float>(obs_t_minus1.height) / 2,
                obs_t_0.x + static_cast<float>(obs_t_0.width) / 2,
                obs_t_0.y + static_cast<float>(obs_t_0.height) / 2};
    }

    const ei_impulse_result_bounding_box_t* last_observation() const {
        if (observations.empty()) {
            return nullptr;
        }
        return &observations.back();
    }

    ei_impulse_result_bounding_box_t smoothed_last_observation() const {
        ei_impulse_result_bounding_box_t bbox = {"", 0, 0, 0, 0, 0.0};
        if (observations.empty()) {
            return bbox;
        }

        bbox.x = round(xyxy_emas[0]->smoothed_value());
        bbox.y = round(xyxy_emas[1]->smoothed_value());
        bbox.width = round(xyxy_emas[2]->smoothed_value());
        bbox.height = round(xyxy_emas[3]->smoothed_value());

        return bbox;
    }

    void debug_output() const {
#if EI_LOG_LEVEL == EI_LOG_LEVEL_DEBUG
        // output debug info, C-style
        ei_printf("Trace %d:\n", id);
        ei_printf("  Last ground truth update: %d\n", last_ground_truth_update_t);
        ei_printf("  Last prediction: %d %d %d %d %f\n", last_prediction.x, last_prediction.y, last_prediction.width, last_prediction.height, last_prediction.value);
        ei_printf("  Observations:\n");
        for (const auto& obs : observations) {
            ei_printf("%d %d %d %d %f\n", obs.x, obs.y, obs.width, obs.height, obs.value);
        }
#endif
    }

    uint32_t id;
    uint32_t last_ground_truth_update_t;
    ei_impulse_result_bounding_box_t last_prediction;

private:
    std::vector<ei_impulse_result_bounding_box_t> observations;
    TinyEKF* centroid_filter;
    TinyEKF* width_height_filter;
    uint32_t max_observations;
    float fx_centroid[2];
    float fx_width_height[2];
    float hx_centroid[2];
    float hx_width_height[2];
    const char* trace_label;
    ExponentialMovingAverage *xyxy_emas[4];
};

class Tracker {
public:
    Tracker (uint32_t keep_grace = 5, uint16_t max_observations = 5, float threshold = 0.5, bool use_iou = true)
            : keep_grace(keep_grace),
              max_observations(max_observations),
              alignment(threshold, use_iou) {
        trace_seq_id = 0;
        t = 0;
    }

    ~Tracker() {
        for (auto trace : open_traces) {
            delete trace;
        }
        for (auto trace : closed_traces) {
            delete trace;
        }
    }

    std::vector<Trace*>open_traces;
    std::vector<Trace*>closed_traces;
    std::vector<ei_object_tracking_trace_t> object_tracking_output;

    void process_new_detections(std::vector<ei_impulse_result_bounding_box_t> detections) {
        // firstly try an alignment with last observations...
        std::vector<ei_impulse_result_bounding_box_t> last_obs_bboxes;
        for (auto trace : open_traces) {
            last_obs_bboxes.push_back(*trace->last_observation());
        }

        std::vector<std::tuple<int, int, float>> last_obs_matches = alignment.align(last_obs_bboxes, detections);

        float last_obs_cost = 0;
        for (auto last_obs_match : last_obs_matches) {
            EI_LOGD("last_obs_match %d %d %f\n", std::get<0>(last_obs_match), std::get<1>(last_obs_match), std::get<2>(last_obs_match));
            last_obs_cost += std::get<2>(last_obs_match);
        }
        EI_LOGD("last_obs_cost %f\n", last_obs_cost);

        // ... then with the kalman filter predictions
        std::vector<ei_impulse_result_bounding_box_t> predicted_bboxes;
        for (auto trace : open_traces) {
            predicted_bboxes.push_back(trace->predict());
            EI_LOGD("predicted %d %d %d %d %f\n", trace->last_prediction.x, trace->last_prediction.y, trace->last_prediction.width, trace->last_prediction.height, trace->last_prediction.value);
        }

        std::vector<std::tuple<int, int, float>> predicted_matches = alignment.align(predicted_bboxes, detections);
        float predicted_cost = 0;
        for (auto predicted_match : predicted_matches) {
            EI_LOGD("predicted_match %d %d %f\n", std::get<0>(predicted_match), std::get<1>(predicted_match), std::get<2>(predicted_match));
            predicted_cost += std::get<2>(predicted_match);
        }
        EI_LOGD("predicted_cost %f\n", predicted_cost);

        // and use whichever matching set is better
        std::vector<std::tuple<int, int, float>> matches;

        if (last_obs_cost > predicted_cost) {
            EI_LOGD("using last_obs_matches matches\n");
            matches = last_obs_matches;
        }
        else {
            EI_LOGD("using predicted_matches matches\n");
            matches = predicted_matches;
        }

        // assume all detections are unassigned and will becomes new tracks
        // until we see otherwise ( i.e. they match an existing track )∂        //
        std::set<uint16_t>unassigned_detection_idxs;
        for (size_t i = 0; i < detections.size(); i++) {
            unassigned_detection_idxs.insert(i);
        }

        // keep track of open traces idxs that haven't been updated
        std::set<uint16_t>open_traces_idxs_to_be_updated;
        for (size_t i = 0; i < open_traces.size(); i++) {
            open_traces_idxs_to_be_updated.insert(i);
        }

        // update existing traces with any matches
        for (size_t i = 0; i < matches.size(); i++) {
            uint32_t trace_idx = std::get<0>(matches[i]);
            uint32_t detection_idx = std::get<1>(matches[i]);
            EI_LOGD("t_idx=%u d_idx=%u iou=%.6f\n", trace_idx, detection_idx, std::get<2>(matches[i]));

            Trace *trace = open_traces[trace_idx];
            open_traces_idxs_to_be_updated.erase(trace_idx);
            trace->update(t, &detections[detection_idx]);
            unassigned_detection_idxs.erase(detection_idx);
        }

        for (auto detection_idx : unassigned_detection_idxs ) {
            EI_LOGD("unassigned detection %d %d %d %d %d %f => starting new trace\n", detection_idx, detections[detection_idx].x, detections[detection_idx].y, detections[detection_idx].width, detections[detection_idx].height, detections[detection_idx].value);
            open_traces.push_back(new Trace(trace_seq_id, t, detections[detection_idx], max_observations));
            trace_seq_id += 1;
        }

        std::vector<Trace*>traces_tmp;

        for (auto trace : open_traces) {
            EI_LOGD("grace checking trace %d at t=%d (trace.last_ground_truth_update_t=%d)\n", trace->id, t, trace->last_ground_truth_update_t);
            uint32_t time_since_last_update = t - trace->last_ground_truth_update_t;
            if (time_since_last_update > keep_grace) {
                // been too long since last update, close it
                EI_LOGD("closing trace %d\n", trace->id);
                closed_traces.push_back(trace);
            }
            else {
                if (trace->last_ground_truth_update_t != t) {
                    // wasn't match this step, so do rollout of filters
                    EI_LOGD("self rollout of trace %d\n", trace->id);
                    trace->update(t, nullptr);
                }
                EI_LOGD("trace %d still alive\n", trace->id);
                traces_tmp.push_back(trace);
            }
        }

        open_traces = traces_tmp;
        object_tracking_output.clear();

        for (auto trace : open_traces) {
            ei_object_tracking_trace_t trace_result = { 0 };
            trace_result.id = trace->id;
            trace_result.last_ground_truth_update_t = trace->last_ground_truth_update_t;
            trace_result.label = trace->last_prediction.label;
            trace_result.x = trace->last_prediction.x;
            trace_result.y = trace->last_prediction.y;
            trace_result.width = trace->last_prediction.width;
            trace_result.height = trace->last_prediction.height;
            trace_result.last_centroid_segment = trace->last_centroid_segment();

            object_tracking_output.push_back(trace_result);
        }
        t += 1;
    }

    void set_threshold(float threshold) {
        alignment.threshold = threshold;
    }

    float get_threshold() {
        return alignment.threshold;
    }

    uint32_t keep_grace;
    uint16_t max_observations;
private:
    uint32_t trace_seq_id;
    uint32_t t;
    GreedyAlignment alignment;
};

EI_IMPULSE_ERROR init_object_tracking(ei_impulse_handle_t *handle, void** state, void *config)
{
    //const ei_impulse_t *impulse = handle->impulse;
    const ei_object_tracking_config_t *ei_object_tracking_config = (ei_object_tracking_config_t*)config;

    // Allocate the object counter
    Tracker *object_tracker = new Tracker(ei_object_tracking_config->keep_grace,
                                          ei_object_tracking_config->max_observations,
                                          ei_object_tracking_config->threshold,
                                          ei_object_tracking_config->use_iou);
    if (!object_tracker) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }

    // Store the object counter state
    *state = (void*)object_tracker;

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR deinit_object_tracking(void* state, void *config)
{
    Tracker *object_tracker = (Tracker *)state;

    if (object_tracker) {
        delete object_tracker;
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR process_object_tracking(ei_impulse_handle_t *handle,
                                         ei_impulse_result_t *result,
                                         void *config,
                                         void *state)
{
    const ei_impulse_t *impulse = handle->impulse;
    Tracker *object_tracker = (Tracker *)state;

    if (impulse->sensor == EI_CLASSIFIER_SENSOR_CAMERA) {
        if((void *)object_tracker != NULL) {
            ei_impulse_result_bounding_box_t *bbs = result->bounding_boxes;
            uint32_t bbs_num = result->bounding_boxes_count;
            std::vector<ei_impulse_result_bounding_box_t> detections(bbs, bbs + bbs_num);

            object_tracker->process_new_detections(detections);

            result->postprocessed_output.object_tracking_output.open_traces = object_tracker->object_tracking_output.data();
            result->postprocessed_output.object_tracking_output.open_traces_count = object_tracker->object_tracking_output.size();
        }
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR display_object_tracking(ei_impulse_result_t *result,
                                         void *config)
{
    // print the open traces
    ei_printf("Open traces:\r\n");
    for (uint32_t i = 0; i < result->postprocessed_output.object_tracking_output.open_traces_count; i++) {
        ei_object_tracking_trace_t trace = result->postprocessed_output.object_tracking_output.open_traces[i];
        ei_printf("  Trace %d: %s [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                trace.id,
                trace.label,
                trace.x,
                trace.y,
                trace.width,
                trace.height);
    }

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR set_post_process_params(ei_impulse_handle_t* handle, ei_object_tracking_config_t* params) {
    int16_t block_number = get_block_number(handle, (void*)init_object_tracking);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    Tracker *object_tracker = (Tracker*)handle->post_processing_state[block_number];

    object_tracker->keep_grace = params->keep_grace;
    object_tracker->max_observations = params->max_observations;
    object_tracker->set_threshold(params->threshold);
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_impulse_handle_t* handle, ei_object_tracking_config_t* params) {
    int16_t block_number = get_block_number(handle, (void*)init_object_tracking);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    Tracker *object_tracker = (Tracker*)handle->post_processing_state[block_number];

    params->keep_grace = object_tracker->keep_grace;
    params->max_observations = object_tracker->max_observations;
    params->threshold = object_tracker->get_threshold();
    return EI_IMPULSE_OK;
}

// versions that operate on the default impulse
EI_IMPULSE_ERROR set_post_process_params(ei_object_tracking_config_t *params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        set_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_object_tracking_config_t *params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        get_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

#endif // EI_CLASSIFIER_OBJECT_TRACKING_ENABLED
#endif // EI_OBJECT_TRACKING_H