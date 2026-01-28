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

#ifndef _EDGE_IMPULSE_CLASSIFIER_PRINT_RESULTS_H_
#define _EDGE_IMPULSE_CLASSIFIER_PRINT_RESULTS_H_

#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/ei_classifier_types.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "edge-impulse-sdk/porting/ei_logging.h"

static void ei_print_timing(ei_impulse_result_t *result_ptr);

/**
 * @brief      Print results for an impulse. This looks at impulse_handle->impulse->results_type
 *             and print the relevant results from the result struct. Use this function to construct your
 *             own print function.
 * @param      impulse_handle  Pointer to impulse handle (e.g. &ei_default_impulse)
 * @param      result_ptr  Pointer to result struct.
 */
void ei_print_results(ei_impulse_handle_t *impulse_handle, ei_impulse_result_t *result_ptr) {
    ei_print_timing(result_ptr);

    const ei_impulse_t *impulse = impulse_handle->impulse;
    ei_impulse_result_t result = *result_ptr;

    if (impulse->results_type == EI_CLASSIFIER_TYPE_CLASSIFICATION) {
        ei_printf("#Classification predictions:\n");
        for (uint16_t i = 0; i < impulse->label_count; i++) {
            ei_printf("  %s: ", impulse->categories[i]);
            ei_printf_float(result.classification[i].value);
            ei_printf("\n");
        }

        if (impulse->has_anomaly != EI_ANOMALY_TYPE_UNKNOWN) {
            ei_printf("Anomaly prediction: ");
            ei_printf_float(result.anomaly);
            ei_printf("\n");
        }
    }
    else if (impulse->results_type == EI_CLASSIFIER_TYPE_REGRESSION) {
        ei_printf("#Regression prediction: ");
        ei_printf_float(result.classification[0].value);
        ei_printf("\n");

        if (impulse->has_anomaly != EI_ANOMALY_TYPE_UNKNOWN) {
            ei_printf("Anomaly prediction: ");
            ei_printf_float(result.anomaly);
            ei_printf("\n");
        }
    }
    else if (impulse->results_type == EI_CLASSIFIER_TYPE_OBJECT_DETECTION) {
        ei_printf("#Object detection bounding boxes:\n");
        for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
            ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
            if (bb.value == 0) {
                continue;
            }
            ei_printf("  %s (", bb.label);
            ei_printf_float(bb.value);
            ei_printf(") [ x: %u, y: %u, width: %u, height: %u ]\n",
                    (unsigned int)bb.x,
                    (unsigned int)bb.y,
                    (unsigned int)bb.width,
                    (unsigned int)bb.height);
        }
    }
#if EI_CLASSIFIER_OBJECT_TRACKING_ENABLED == 1
    else if (impulse->results_type == EI_CLASSIFIER_TYPE_OBJECT_TRACKING) {
        ei_printf("#Object tracking results:\n");
        for (uint32_t ix = 0; ix < result.postprocessed_output.object_tracking_output.open_traces_count; ix++) {
            ei_object_tracking_trace_t trace = result.postprocessed_output.object_tracking_output.open_traces[ix];
            ei_printf("  %s (ID %d) [ x: %u, y: %u, width: %u, height: %u ]\n", trace.label, (int)trace.id, trace.x, trace.y, trace.width, trace.height);
        }

        if (result.postprocessed_output.object_tracking_output.open_traces_count == 0) {
            ei_printf("    No objects found\n");
        }
    }
#endif // EI_CLASSIFIER_OBJECT_TRACKING_ENABLED == 1
#if EI_CLASSIFIER_FREEFORM_OUTPUT
    else if (impulse->results_type == EI_CLASSIFIER_TYPE_FREEFORM) {

        for (size_t ix = 0; ix < impulse_handle->impulse->freeform_outputs_size; ix++) {
            ei_printf("#Freeform output index=%d\n", (int)ix);
            ei_printf("  ");
            const ei::matrix_t& freeform_output = impulse_handle->freeform_outputs[ix];
            for (size_t jx = 0; jx < freeform_output.rows * freeform_output.cols; jx++) {
                ei_printf_float(freeform_output.buffer[jx]);
                ei_printf(" ");
            }
            ei_printf("\n");
        }
    }
#endif // EI_CLASSIFIER_FREEFORM_OUTPUT
#if EI_CLASSIFIER_HAS_VISUAL_ANOMALY
    else if (impulse->has_anomaly != EI_ANOMALY_TYPE_UNKNOWN) {
        ei_printf("#Visual anomalies:\n");
        for (uint32_t i = 0; i < result.visual_ad_count; i++) {
            ei_impulse_result_bounding_box_t bb = result.visual_ad_grid_cells[i];
            if (bb.value == 0.f) {
                continue;
            }
            ei_printf("  %s (", bb.label);
            ei_printf_float(bb.value);
            ei_printf(") [ x: %u, y: %u, width: %u, height: %u ]\n",
                    bb.x,
                    bb.y,
                    bb.width,
                    bb.height);
        }
        ei_printf("Visual anomaly mean: ");
        ei_printf_float(result.visual_ad_result.mean_value);
        ei_printf(", max: ");
        ei_printf_float(result.visual_ad_result.max_value);
        ei_printf("\n");
    }
#endif // #if EI_CLASSIFIER_HAS_VISUAL_ANOMALY
    else if (impulse->has_anomaly != EI_ANOMALY_TYPE_UNKNOWN) {
        ei_printf("Anomaly prediction: ");
        ei_printf_float(result.anomaly);
        ei_printf("\n");
    }
    else {
        ei_printf("Could not determine how to print the results for this impulse\n");
    }
}

/**
 * @brief      Print the time it took for DSP/Classification/Anomaly blocks to run.
 *             this prints data in ms., unless <1ms. then it prints data in us.
 * @param      result_ptr  Pointer to result struct.
 */
static void ei_print_timing(ei_impulse_result_t *result_ptr) {
    ei_impulse_result_t result = *result_ptr;

    ei_printf("Timing: ");
    if (result.timing.dsp_us != 0 && result.timing.dsp_us < 1000) {
        ei_printf("DSP %ld us", (long int)result.timing.dsp_us);
    }
    else {
        ei_printf("DSP %d ms", result.timing.dsp);
    }
    if (result.timing.classification_us != 0 && result.timing.classification_us < 1000) {
        ei_printf(", inference %ld us", (long int)result.timing.classification_us);
    }
    else {
        ei_printf(", inference %d ms", result.timing.classification);
    }
    if (result.timing.anomaly_us != 0 && result.timing.anomaly_us < 1000) {
        ei_printf(", anomaly %ld us", (long int)result.timing.anomaly_us);
    }
    else {
        ei_printf(", anomaly %d ms", result.timing.anomaly);
    }
    if (result.timing.postprocessing_us != 0) {
        if (result.timing.postprocessing_us < 1000) {
            ei_printf(", postprocessing %ld us", (long int)result.timing.postprocessing_us);
        }
        else {
            ei_printf(", postprocessing %d ms", result.timing.postprocessing);
        }
    }
    ei_printf("\n");
}

#endif // _EDGE_IMPULSE_CLASSIFIER_PRINT_RESULTS_H_
