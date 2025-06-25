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

#ifndef _EDGE_IMPULSE_RUN_CLASSIFIER_TYPES_H_
#define _EDGE_IMPULSE_RUN_CLASSIFIER_TYPES_H_

#include <stdint.h>
// needed for standalone C example
#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/dsp/numpy_types.h"

#ifndef EI_CLASSIFIER_MAX_OBJECT_DETECTION_COUNT
#define EI_CLASSIFIER_MAX_OBJECT_DETECTION_COUNT 10
#endif

/**
 * @defgroup ei_structs Structs
 *
 * Public-facing structs for Edge Impulse C++ SDK.
 *
 * @addtogroup ei_structs
 * @{
 */

/**
 * @brief Holds the output of inference, anomaly results, and timing information.
 *
 * `ei_impulse_result_t` holds the output of `run_classifier()`. If object detection is
 * enabled, then the output results is a
 * pointer to an array of bounding boxes of size `bounding_boxes_count`, as given by
 * [ei_impulse_result_bounding_box_t](https://docs.edgeimpulse.com/reference/ei_impulse_result_bounding_box_t).
 * Otherwise, results are stored as an array of classification scores, as given by
 * [ei_impulse_result_classification_t](https://docs.edgeimpulse.com/reference/ei_impulse_result_classification_t).
 *
 * If anomaly detection is enabled (e.g. `EI_CLASSIFIER_HAS_ANOMALY == 1`), then the
 * anomaly score will be stored as a floating point value in `anomaly`.
 *
 * Timing information is stored in an
 * [ei_impulse_result_timing_t](https://docs.edgeimpulse.com/reference/ei_impulse_result_timing_t)
 * struct.
 *
 * **Source**: [classifier/ei_classifier_types.h](https://github.com/edgeimpulse/inferencing-sdk-cpp/blob/master/classifier/ei_classifier_types.h)
 *
 * **Example**: [standalone inferencing main.cpp](https://github.com/edgeimpulse/example-standalone-inferencing/blob/master/source/main.cpp)
 */
typedef struct {
    /**
     * Label of the detected object
     */
    const char *label;

    /**
     * Value of the detected object
     */
    float value;
} ei_impulse_result_classification_t;

/**
 * @brief Holds the output of visual anomaly detection (FOMO-AD)
 *
 * If visual anomaly detection is enabled (e.g. `EI_CLASSIFIER_HAS_VISUAL_ANOMALY ==
 * 1`), then the output results will be a pointer to an array of grid cells of size
 * `visual_ad_count`, as given by
 * [ei_impulse_result_bounding_box_t](https://docs.edgeimpulse.com/reference/ei_impulse_result_bounding_box_t).
 *
 * The visual anomaly detection result is stored in `visual_ad_result`, which contains the mean and max values of the grid cells.
 *
 * **Source**: [classifier/ei_classifier_types.h](https://github.com/edgeimpulse/inferencing-sdk-cpp/blob/master/classifier/ei_classifier_types.h)
 *
 * **Example**: [standalone inferencing main.cpp](https://github.com/edgeimpulse/example-standalone-inferencing/blob/master/source/main.cpp)
*/
typedef struct {
    /**
     * Mean value of the grid cells
     */
    float mean_value;

    /**
     * Max value of the grid cells
     */
    float max_value;
} ei_impulse_visual_ad_result_t;

/**
 * @brief Holds information for a single bounding box.
 *
 * If object detection is enabled (i.e. `EI_CLASSIFIER_OBJECT_DETECTION == 1`), then
 * inference results will be one or more bounding boxes. The bounding boxes with the
 * highest confidence scores (assuming those scores are equal to or greater than
 * `EI_CLASSIFIER_OBJECT_DETECTION_THRESHOLD`), given by the `value` member, are
 * returned from inference. The total number of bounding boxes returned will be at
 * least `EI_CLASSIFIER_OBJECT_DETECTION_COUNT`. The exact number of bounding boxes
 * is stored in `bounding_boxes_count` field of [ei_impulse_result_t]/C++ Inference
 * SDK Library/structs/ei_impulse_result_t.md).
 *
 * A bounding box is a rectangle that ideally surrounds the identified object. The
 * (`x`, `y`) coordinates in the struct identify the top-left corner of the box.
 * `label` is the predicted class with the highest confidence score. `value` is the
 * confidence score between [0.0..1.0] of the given `label`.
 *
 * **Source**: [classifier/ei_classifier_types.h](https://github.com/edgeimpulse/inferencing-sdk-cpp/blob/master/classifier/ei_classifier_types.h)
 *
 * **Example**: [standalone inferencing main.cpp](https://github.com/edgeimpulse/example-standalone-inferencing/blob/master/source/main.cpp)
*/
typedef struct {
    /**
     * Pointer to a character array describing the associated class of the given
     * bounding box. Taken from one of the elements of
     * `ei_classifier_inferencing_categories[]`.
     */
    const char *label;

    /**
     * x coordinate of the top-left corner of the bounding box
     */
    uint32_t x;

    /**
     * y coordinate of the top-left corner of the bounding box
     */
    uint32_t y;

    /**
     * Width of the bounding box
     */
    uint32_t width;

    /**
     * Height of the bounding box
     */
    uint32_t height;

    /**
     * Confidence score of the label describing the bounding box
     */
    float value;
} ei_impulse_result_bounding_box_t;

/**
 * @brief Holds timing information about the processing (DSP) and inference blocks.
 *
 * Records timing information during the execution of the preprocessing (DSP) and
 * inference blocks. Can be used to determine if inference will meet timing requirements
 * on your particular platform.
 *
 * **Source**: [classifier/ei_classifier_types.h](https://github.com/edgeimpulse/inferencing-sdk-cpp/blob/master/classifier/ei_classifier_types.h)
 *
 * **Example**: [standalone inferencing main.cpp](https://github.com/edgeimpulse/example-standalone-inferencing/blob/master/source/main.cpp)
 */
typedef struct {
    /**
     * If using `run_impulse()` to perform sampling and inference, it is the amount of
     * time (in milliseconds) it took to fetch raw samples. Not used for
     * `run_classifier()`.
     */
    int sampling;

    /**
     * Amount of time (in milliseconds) it took to run the preprocessing (DSP) block
     */
    int dsp;

    /**
     * Amount of time (in milliseconds) it took to run the inference block
     */
    int classification;

    /**
     * Amount of time (in milliseconds) it took to run anomaly detection. Valid only if
     * `EI_CLASSIFIER_HAS_ANOMALY == 1`.
     */
    int anomaly;

    /**
     * Amount of time (in microseconds) it took to run the post-processing block
     */
    int64_t dsp_us;

    /**
     * Amount of time (in microseconds) it took to run the inference block
     */
    int64_t classification_us;

    /**
     * Amount of time (in microseconds) it took to run anomaly detection. Valid only if
     * `EI_CLASSIFIER_HAS_ANOMALY == 1`.
     */
    int64_t anomaly_us;
} ei_impulse_result_timing_t;

/**
 * @brief Holds intermediate results of hr / hrv block
 *
*/
typedef struct {
    float heart_rate;
} ei_impulse_result_hr_t;

/**
 * @brief Holds the output of inference, anomaly results, and timing information.
 *
 * `ei_impulse_result_t` holds the output of `run_classifier()`. If object detection is
 * enabled (e.g. `EI_CLASSIFIER_OBJECT_DETECTION == 1`), then the output results is a
 * pointer to an array of bounding boxes of size `bounding_boxes_count`, as given by
 * [ei_impulse_result_bounding_box_t](https://docs.edgeimpulse.com/reference/ei_impulse_result_bounding_box_t).
 * Otherwise, results are stored as an array of classification scores, as given by
 * [ei_impulse_result_classification_t](https://docs.edgeimpulse.com/reference/ei_impulse_result_classification_t).
 *
 * If anomaly detection is enabled (e.g. `EI_CLASSIFIER_HAS_ANOMALY == 1`), then the
 * anomaly score will be stored as a floating point value in `anomaly`.
 *
 * Timing information is stored in an
 * [ei_impulse_result_timing_t](https://docs.edgeimpulse.com/reference/ei_impulse_result_timing_t)
 * struct.
 *
 * **Source**: [classifier/ei_classifier_types.h](https://github.com/edgeimpulse/inferencing-sdk-cpp/blob/master/classifier/ei_classifier_types.h)
 *
 * **Example**: [standalone inferencing main.cpp](https://github.com/edgeimpulse/example-standalone-inferencing/blob/master/source/main.cpp)
 */
typedef struct {
    /**
     * Array of bounding boxes of the detected objects, if object detection is enabled.
     */
    ei_impulse_result_bounding_box_t *bounding_boxes;

    /**
     * Number of bounding boxes detected. If object detection is not enabled, this will
     * be 0.
     */
    uint32_t bounding_boxes_count;

    /**
     * Array of classification results. If object detection is enabled, this will be
     * empty.
     */
#ifdef EI_DSP_RESULT_OVERRIDE
    // For CI only.  We will create the array to hold results
    ei_impulse_result_classification_t* classification;
#else
#if EI_CLASSIFIER_LABEL_COUNT == 0
    // EI_CLASSIFIER_LABEL_COUNT can be 0 for anomaly only models
    // to prevent compiler warnings/errors, we need to have at least one element
    ei_impulse_result_classification_t classification[1];
#else
    ei_impulse_result_classification_t classification[EI_CLASSIFIER_LABEL_COUNT];
#endif // EI_CLASSIFIER_LABEL_COUNT == 0
#endif // EI_DSP_RESULT_OVERRIDE else

    /**
     * Anomaly score. If anomaly detection is not enabled, this will be 0. A higher
     * anomaly score indicates greater likelihood of an anomalous sample (e.g. it is
     * farther away from its cluster).
     */
    float anomaly;

    /**
     * Timing information for the processing (DSP) and inference blocks.
     */
    ei_impulse_result_timing_t timing;
#ifdef __cplusplus
    /**
     * Raw outputs from the neural network. The number of elements in this array is
     * equal to the number of learning blocks in the model.
     * INTERNAL
     * EXPERIMENTAL
     */
    ei_feature_t* _raw_outputs;
#else
    /** padding for C bindings to make sure the struct is the same size
     * INTERNAL
     * EXPERIMENTAL
     */
    void* _padding;
#endif
    bool copy_output;
#if EI_CLASSIFIER_HAS_VISUAL_ANOMALY || __DOXYGEN__
    /**
     * Array of grid cells of the detected visual anomalies, if visual anomaly detection
     * is enabled.
     */
    ei_impulse_result_bounding_box_t *visual_ad_grid_cells;

    /**
     * Number of grid cells detected as visual anomalies, if visual anomaly detection is
     * enabled.
     */
    uint32_t visual_ad_count;

    /**
     * Visual anomaly detection result, if visual anomaly detection is enabled.
     */
    ei_impulse_visual_ad_result_t visual_ad_result;
#endif // EI_CLASSIFIER_HAS_VISUAL_ANOMALY
    ei_post_processing_output_t postprocessed_output;

#if EI_CLASSIFIER_HR_ENABLED == 1
    ei_impulse_result_hr_t hr_calcs;
#endif
} ei_impulse_result_t;

/** @} */

#endif // _EDGE_IMPULSE_RUN_CLASSIFIER_TYPES_H_
