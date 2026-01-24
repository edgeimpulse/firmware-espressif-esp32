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

#ifndef EI_POSTPROCESSING_THRESHOLDS_H
#define EI_POSTPROCESSING_THRESHOLDS_H

#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/postprocessing/ei_postprocessing_types.h"
#include "edge-impulse-sdk/classifier/postprocessing/ei_postprocessing_ai_hub.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/ei_classifier_types.h"
#include <string>

#include "edge-impulse-sdk/classifier/postprocessing/ei_postprocessing_common.h"

#if EI_CLASSIFIER_OBJECT_TRACKING_ENABLED == 1
#include "edge-impulse-sdk/classifier/postprocessing/ei_object_tracking.h"
#endif // EI_CLASSIFIER_OBJECT_TRACKING_ENABLED == 1

/**
 * @brief Get all configurable thresholds for a postprocessing block
 * @param pp_block Pointer to a postprocessing block
 * @param out_thresholds Vector of thresholds
 * @return EI_IMPULSE_OK
 */
EI_IMPULSE_ERROR get_thresholds_postprocessing(const ei_postprocessing_block_t *pp_block, std::vector<ei_threshold_desc_t>& out_thresholds) {

    switch (pp_block->type) {
        case EI_CLASSIFIER_MODE_OBJECT_DETECTION: {

            if (pp_block->postprocess_fn == process_paddleocr_f32) {
                ei_fill_result_paddleocr_f32_config_t *config = (ei_fill_result_paddleocr_f32_config_t*)pp_block->config;

                out_thresholds.push_back({
                    "object_detection", /* type */
                    "min_score_pixel", /* name */
                    config->min_score_pixel, /* value */
                    [config](float v) {
                        config->min_score_pixel = v;
                    }
                });
                out_thresholds.push_back({
                    "object_detection", /* type */
                    "min_score_box", /* name */
                    config->min_score_box, /* value */
                    [config](float v) {
                        config->min_score_box = v;
                    }
                });
                out_thresholds.push_back({
                    "object_detection", /* type */
                    "unclip_ratio", /* name */
                    config->unclip_ratio, /* value */
                    [config](float v) {
                        config->unclip_ratio = v;
                    }
                });
            }
            else if (pp_block->postprocess_fn == process_paddleocr_i8) {
                ei_fill_result_paddleocr_i8_config_t *config = (ei_fill_result_paddleocr_i8_config_t*)pp_block->config;

                out_thresholds.push_back({
                    "object_detection", /* type */
                    "min_score_pixel", /* name */
                    config->min_score_pixel, /* value */
                    [config](float v) {
                        config->min_score_pixel = v;
                    }
                });
                out_thresholds.push_back({
                    "object_detection", /* type */
                    "min_score_box", /* name */
                    config->min_score_box, /* value */
                    [config](float v) {
                        config->min_score_box = v;
                    }
                });
                out_thresholds.push_back({
                    "object_detection", /* type */
                    "unclip_ratio", /* name */
                    config->unclip_ratio, /* value */
                    [config](float v) {
                        config->unclip_ratio = v;
                    }
                });
            }
            else {
                ei_fill_result_object_detection_threshold_config_t *config = (ei_fill_result_object_detection_threshold_config_t*)pp_block->config;

                out_thresholds.push_back({
                    "object_detection", /* type */
                    "min_score", /* name */
                    config->threshold, /* value */
                    [config](float v) {
                        config->threshold = v;
                    }
                });
            }
            break;
        }
        case EI_CLASSIFIER_MODE_VISUAL_ANOMALY: {
            ei_fill_result_visual_ad_f32_config_t *config = (ei_fill_result_visual_ad_f32_config_t*)pp_block->config;

            out_thresholds.push_back({
                "anomaly_gmm", /* type */
                "min_anomaly_score", /* name */
                config->threshold, /* value */
                [config](float v) {
                    config->threshold = v;
                }
            });
            break;
        }
    }

#if EI_CLASSIFIER_OBJECT_TRACKING_ENABLED == 1
    if (pp_block->init_fn == init_object_tracking) {
        ei_object_tracking_config_t *config = (ei_object_tracking_config_t*)pp_block->config;

        out_thresholds.push_back({
            "object_tracking", /* type */
            "keep_grace", /* name */
            static_cast<float>(config->keep_grace), /* value */
            [config](float v) {
                config->keep_grace = static_cast<uint32_t>(v);
            }
        });
        out_thresholds.push_back({
            "object_tracking", /* type */
            "max_observations", /* name */
            static_cast<float>(config->max_observations), /* value */
            [config](float v) {
                config->max_observations = static_cast<uint16_t>(v);
            }
        });
        out_thresholds.push_back({
            "object_tracking", /* type */
            "threshold", /* name */
            config->threshold, /* value */
            [config](float v) {
                config->threshold = v;
            }
        });
    }
#endif // EI_CLASSIFIER_OBJECT_TRACKING_ENABLED == 1

    return EI_IMPULSE_OK;
}

/**
 * @brief Set a threshold for a postprocessing block (view available thresholds via get_thresholds_postprocessing)
 * @param pp_block Pointer to a postprocessing block
 * @param name Name of the threshold
 * @param value New value of the threshold
 * @return EI_IMPULSE_OK if threshold was set; EI_IMPULSE_POSTPROCESSING_THRESHOLD_KEY_NOT_FOUND
 *         if threshold key was not found for this block.
 */
EI_IMPULSE_ERROR set_threshold_postprocessing(const ei_postprocessing_block_t *pp_block, std::string name, float value) {
    std::vector<ei_threshold_desc_t> thresholds;
    EI_IMPULSE_ERROR res = get_thresholds_postprocessing(pp_block, thresholds);
    if (res != EI_IMPULSE_OK) {
        return res;
    }

    bool found_threshold = false;
    for (auto threshold : thresholds) {
        if (threshold.name != name) continue;

        threshold.set_value(value);
        found_threshold = true;
        break;
    }

    if (!found_threshold) {
        return EI_IMPULSE_POSTPROCESSING_THRESHOLD_KEY_NOT_FOUND;
    }

    return EI_IMPULSE_OK;
}

#endif // EI_POSTPROCESSING_THRESHOLDS_H
