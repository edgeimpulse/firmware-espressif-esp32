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

#ifndef EI_PERFORMANCE_CALIBRATION_H
#define EI_PERFORMANCE_CALIBRATION_H

#if EI_CLASSIFIER_CALIBRATION_ENABLED

/* Includes ---------------------------------------------------------------- */
#include "edge-impulse-sdk/dsp/numpy_types.h"
#include "edge-impulse-sdk/dsp/returntypes.hpp"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/postprocessing/ei_postprocessing_common.h"
#include "edge-impulse-sdk/porting/ei_logging.h"

/* Private const types ----------------------------------------------------- */
#define MEM_ERROR   "ERR: Failed to allocate memory for performance calibration\r\n"

#define EI_PC_RET_NO_EVENT_DETECTED    -1
#define EI_PC_RET_MEMORY_ERROR         -2

extern ei_impulse_handle_t & ei_default_impulse;

typedef struct {
    float detection_threshold;
} ei_perf_cal_params_t;

class PerfCal {
public:
    PerfCal(
        const ei_performance_calibration_config_t *config,
        uint32_t n_labels,
        uint32_t sample_length,
        float sample_interval_ms)
    {
        this->_score_array = nullptr;
        this->_running_sum = nullptr;
        this->_detection_threshold = config->detection_threshold;
        this->_suppression_flags = config->suppression_flags;
        this->_should_boost = config->is_configured;
        this->_n_labels = n_labels;

        /* Determine sample length in ms */
        float sample_length_ms = (static_cast<float>(sample_length) * sample_interval_ms);

        /* Calculate number of inference runs needed for the duration window */
        this->_average_window_duration_samples =
            (config->average_window_duration_ms < static_cast<uint32_t>(sample_length_ms))
            ? 1
            : static_cast<uint32_t>(static_cast<float>(config->average_window_duration_ms) / sample_length_ms);

        /* Calculate number of inference runs for suppression */
        this->_suppression_samples = (config->suppression_ms < static_cast<uint32_t>(sample_length_ms))
            ? 0
            : static_cast<uint32_t>(static_cast<float>(config->suppression_ms) / sample_length_ms);

        /* Detection threshold should be high enough to only classify 1 possible output */
        if (this->_detection_threshold <= (1.f / this->_n_labels)) {
            EI_LOGE("Classifier detection threshold too low\r\n");
            return;
        }

        /* Array to store scores for all labels */
        this->_score_array = (float *)ei_malloc(
            this->_average_window_duration_samples * this->_n_labels * sizeof(float));

        if (this->_score_array == NULL) {
            ei_printf(MEM_ERROR);
            return;
        }

        for (uint32_t i = 0; i < this->_average_window_duration_samples * this->_n_labels; i++) {
            this->_score_array[i] = 0.f;
        }
        this->_score_idx = 0;

        /* Running sum for all labels */
        this->_running_sum = (float *)ei_malloc(this->_n_labels * sizeof(float));

        if (this->_running_sum != NULL) {
            for (uint32_t i = 0; i < this->_n_labels; i++) {
                this->_running_sum[i] = 0.f;
            }
        }
        else {
            ei_printf(MEM_ERROR);
            return;
        }

        this->_suppression_count = this->_suppression_samples;
        this->_n_scores_in_array = 0;
    }

    ~PerfCal()
    {
        if (this->_score_array) {
            ei_free((void *)this->_score_array);
        }
        if (this->_running_sum) {
            ei_free((void *)this->_running_sum);
        }
    }

    bool should_boost()
    {
        return this->_should_boost;
    }

    void set_detection_threshold(float detection_threshold)
    {
        this->_detection_threshold = detection_threshold;
    }

    float get_detection_threshold()
    {
        return this->_detection_threshold;
    }

    int32_t trigger(ei_impulse_result_classification_t *scores)
    {
        int32_t recognized_event = EI_PC_RET_NO_EVENT_DETECTED;
        float current_top_score = 0.f;
        uint32_t current_top_index = 0;

        /* Check pointers */
        if (this->_score_array == NULL || this->_running_sum == NULL) {
            return EI_PC_RET_MEMORY_ERROR;
        }

        /* Update the score array and running sum */
        for (uint32_t i = 0; i < this->_n_labels; i++) {
            this->_running_sum[i] -= this->_score_array[(this->_score_idx * this->_n_labels) + i];
            this->_running_sum[i] += scores[i].value;
            this->_score_array[(this->_score_idx * this->_n_labels) + i] = scores[i].value;
        }

        if (++this->_score_idx >= this->_average_window_duration_samples) {
            this->_score_idx = 0;
        }

        /* Number of samples to average, increases until the buffer is full */
        if (this->_n_scores_in_array < this->_average_window_duration_samples) {
            this->_n_scores_in_array++;
        }

        /* Average data and place in scores & determine top score */
        for (uint32_t i = 0; i < this->_n_labels; i++) {
            scores[i].value = this->_running_sum[i] / this->_n_scores_in_array;

            if (scores[i].value > current_top_score) {
                if(this->_suppression_flags == 0) {
                    current_top_score = scores[i].value;
                    current_top_index = i;
                }
                else if(this->_suppression_flags & (1 << i)) {
                    current_top_score = scores[i].value;
                    current_top_index = i;
                }
            }
        }

        /* Check threshold, suppression */
        if (this->_suppression_samples && this->_suppression_count < this->_suppression_samples) {
            this->_suppression_count++;
        }
        else {
            if (current_top_score >= this->_detection_threshold) {
                recognized_event = current_top_index;

                if (this->_suppression_flags & (1 << current_top_index)) {
                    this->_suppression_count = 0;
                }
            }
        }

        return recognized_event;
    };

    void *operator new(size_t size)
    {
        void *p = ei_malloc(size);
        return p;
    }

    void operator delete(void *p)
    {
        ei_free(p);
    }

private:
    uint32_t _average_window_duration_samples;
    float _detection_threshold;
    bool _should_boost;
    uint32_t _suppression_samples;
    uint32_t _suppression_count;
    uint32_t _suppression_flags;
    uint32_t _n_labels;
    float *_score_array;
    uint32_t _score_idx;
    float *_running_sum;
    uint32_t _n_scores_in_array;
};

EI_IMPULSE_ERROR init_perfcal(ei_impulse_handle_t *handle, void **state, void *config)
{
    const ei_impulse_t *impulse = handle->impulse;
    const ei_performance_calibration_config_t *calibration = (ei_performance_calibration_config_t*)config;

    if(calibration != NULL) {
        PerfCal *perf_cal = new PerfCal(calibration, impulse->label_count, impulse->slice_size,
                                            impulse->interval_ms);
        *state = (void *)perf_cal;

    }
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR deinit_perfcal(void *state, void *config)
{
    PerfCal *perf_cal = (PerfCal*)state;

    if((void *)perf_cal != NULL) {
        delete perf_cal;
    }

    state = NULL;
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR process_perfcal(ei_impulse_handle_t *handle,
                                 uint32_t block_index,
                                 uint32_t input_block_id,
                                 ei_impulse_result_t *result,
                                 void *config_ptr,
                                 void *state)
{
    const ei_impulse_t *impulse = handle->impulse;
    PerfCal *perf_cal = (PerfCal*)state;

    if (impulse->sensor == EI_CLASSIFIER_SENSOR_MICROPHONE) {
        if((void *)perf_cal != NULL) {

            // perfcal is configured
            static bool has_printed_msg = false;
            result->postprocessed_output.perf_cal_output = *std::unique_ptr<ei_perf_cal_output_t>(new ei_perf_cal_output_t).get();

            if (!has_printed_msg) {
                ei_printf("\nPerformance calibration is configured for your project. If no event is detected, all values are 0.\r\n\n");
                has_printed_msg = true;
            }

            int label_detected = perf_cal->trigger(result->classification);

            if (perf_cal->should_boost()) {
                for (int i = 0; i < impulse->label_count; i++) {
                    if (i == label_detected) {
                        result->classification[i].value = 1.0f;
                        result->postprocessed_output.perf_cal_output.detected_label = (char*)result->classification[i].label;
                    }
                    else {
                        result->classification[i].value = 0.0f;
                    }
                }
            }
        }
    }
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR display_perfcal(ei_impulse_result_t *result,
                                         void *config)
{
    // print the detected label
    ei_printf("Detected label: %s\r\n", result->postprocessed_output.perf_cal_output.detected_label);

    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR set_post_process_params(ei_impulse_handle_t* handle, ei_perf_cal_params_t* params) {
    int16_t block_number = get_block_number(handle, (void*)init_perfcal);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    PerfCal *perf_cal = (PerfCal*)handle->post_processing_state[block_number];

    perf_cal->set_detection_threshold(params->detection_threshold);
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_impulse_handle_t* handle, ei_perf_cal_params_t* params) {
    int16_t block_number = get_block_number(handle, (void*)init_perfcal);
    if (block_number == -1) {
        return EI_IMPULSE_POSTPROCESSING_ERROR;
    }
    PerfCal *perf_cal = (PerfCal*)handle->post_processing_state[block_number];

    params->detection_threshold = perf_cal->get_detection_threshold();
    return EI_IMPULSE_OK;
}

// versions that operate on the default impulse
EI_IMPULSE_ERROR set_post_process_params(ei_perf_cal_params_t *params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        set_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR get_post_process_params(ei_perf_cal_params_t* params) {
    ei_impulse_handle_t* handle = &ei_default_impulse;

    if(handle->post_processing_state != NULL) {
        get_post_process_params(handle, params);
    }
    return EI_IMPULSE_OK;
}

#endif //EI_CLASSIFIER_CALIBRATION_ENABLED
#endif //EI_PERFORMANCE_CALIBRATION
