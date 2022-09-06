/* Edge Impulse inferencing library
 * Copyright (c) 2021 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _EDGE_IMPULSE_RUN_CLASSIFIER_H_
#define _EDGE_IMPULSE_RUN_CLASSIFIER_H_

#include "model-parameters/model_metadata.h"
#if EI_CLASSIFIER_HAS_MODEL_VARIABLES == 1
#include "model-parameters/model_variables.h"
#endif

#if EI_CLASSIFIER_HAS_ANOMALY == 1
#include "model-parameters/anomaly_clusters.h"
#endif
#include "ei_run_dsp.h"
#include "ei_classifier_types.h"
#include "ei_classifier_smooth.h"
#include "ei_signal_with_axes.h"
#if defined(EI_CLASSIFIER_HAS_SAMPLER) && EI_CLASSIFIER_HAS_SAMPLER == 1
#include "ei_sampler.h"
#endif
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "model-parameters/dsp_blocks.h"
#include "ei_performance_calibration.h"
#include "edge-impulse-sdk/classifier/ei_fill_result_struct.h"

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_COMPILED != 1)

#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_micro.h"

#elif EI_CLASSIFIER_COMPILED == 1

#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_eon.h"

#elif EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL

#include "edge-impulse-sdk/classifier/inferencing_engines/tflite_full.h"

#elif (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSORRT)
#include <stdlib.h>
#include "tflite-model/onnx-trained.h"
#include "tflite/linux-jetson-nano/libeitrt.h"
EiTrt* ei_trt_handle = NULL;

#elif EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_NONE
// noop
#elif EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW
#include "mcu.h"
extern "C" void infer(uint32_t* time, uint32_t* cycles);
int8_t *processed_features;
int8_t infer_result[EI_CLASSIFIER_LABEL_COUNT];

#elif EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_DRPAI
#include "edge-impulse-sdk/classifier/inferencing_engines/drpai.h"
#else
#error "Unknown inferencing engine"
#endif

#if ECM3532
void*   __dso_handle = (void*) &__dso_handle;
#endif

#ifdef __cplusplus
namespace {
#endif // __cplusplus

#define EI_CLASSIFIER_OBJDET_HAS_SCORE_TENSOR   (EI_CLASSIFIER_OBJECT_DETECTION && !(EI_CLASSIFIER_OBJECT_DETECTION_CONSTRAINED))

/* Function prototypes ----------------------------------------------------- */
extern "C" EI_IMPULSE_ERROR run_inference(ei::matrix_t *fmatrix, ei_impulse_result_t *result, bool debug);
extern "C" EI_IMPULSE_ERROR run_classifier_image_quantized(signal_t *signal, ei_impulse_result_t *result, bool debug);
static EI_IMPULSE_ERROR can_run_classifier_image_quantized();
static void calc_cepstral_mean_and_var_normalization_mfcc(ei_matrix *matrix, void *config_ptr);
static void calc_cepstral_mean_and_var_normalization_mfe(ei_matrix *matrix, void *config_ptr);
static void calc_cepstral_mean_and_var_normalization_spectrogram(ei_matrix *matrix, void *config_ptr);

/* Private variables ------------------------------------------------------- */

static uint64_t classifier_continuous_features_written = 0;

#if (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_MICROPHONE)
static RecognizeEvents *avg_scores = NULL;
#endif

/* Private functions ------------------------------------------------------- */

/**
 * @brief      Init static vars
 */
extern "C" void run_classifier_init(void)
{
    classifier_continuous_features_written = 0;
    ei_dsp_clear_continuous_audio_state();

#if (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_MICROPHONE)
    const ei_model_performance_calibration_t *calibration = &ei_calibration;

    if(calibration != NULL) {
        avg_scores = new RecognizeEvents(calibration,
            EI_CLASSIFIER_LABEL_COUNT, EI_CLASSIFIER_SLICE_SIZE, EI_CLASSIFIER_INTERVAL_MS);
    }
#endif
}

extern "C" void run_classifier_deinit(void)
{
#if (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_MICROPHONE)
    if((void *)avg_scores != NULL) {
        delete avg_scores;
    }
#endif
}

/**
 * @brief      Fill the complete matrix with sample slices. From there, run inference
 *             on the matrix.
 *
 * @param      signal  Sample data
 * @param      result  Classification output
 * @param[in]  debug   Debug output enable boot
 *
 * @return     The ei impulse error.
 */
extern "C" EI_IMPULSE_ERROR run_classifier_continuous(signal_t *signal, ei_impulse_result_t *result,
                                                      bool debug = false, bool enable_maf = true)
{
    static ei::matrix_t static_features_matrix(1, EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);
    if (!static_features_matrix.buffer) {
        return EI_IMPULSE_ALLOC_FAILED;
    }

    EI_IMPULSE_ERROR ei_impulse_error = EI_IMPULSE_OK;

    uint64_t dsp_start_us = ei_read_timer_us();

    size_t out_features_index = 0;
    bool is_mfcc = false;
    bool is_mfe = false;
    bool is_spectrogram = false;

    for (size_t ix = 0; ix < ei_dsp_blocks_size; ix++) {
        ei_model_dsp_t block = ei_dsp_blocks[ix];

        if (out_features_index + block.n_output_features > EI_CLASSIFIER_NN_INPUT_FRAME_SIZE) {
            ei_printf("ERR: Would write outside feature buffer\n");
            return EI_IMPULSE_DSP_ERROR;
        }

        ei::matrix_t fm(1, block.n_output_features,
                        static_features_matrix.buffer + out_features_index);

        int (*extract_fn_slice)(ei::signal_t *signal, ei::matrix_t *output_matrix, void *config, const float frequency, matrix_size_t *out_matrix_size);

        /* Switch to the slice version of the mfcc feature extract function */
        if (block.extract_fn == extract_mfcc_features) {
            extract_fn_slice = &extract_mfcc_per_slice_features;
            is_mfcc = true;
        }
        else if (block.extract_fn == extract_spectrogram_features) {
            extract_fn_slice = &extract_spectrogram_per_slice_features;
            is_spectrogram = true;
        }
        else if (block.extract_fn == extract_mfe_features) {
            extract_fn_slice = &extract_mfe_per_slice_features;
            is_mfe = true;
        }
        else {
            ei_printf("ERR: Unknown extract function, only MFCC, MFE and spectrogram supported\n");
            return EI_IMPULSE_DSP_ERROR;
        }

        matrix_size_t features_written;

#if EIDSP_SIGNAL_C_FN_POINTER
        if (block.axes_size != EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
            ei_printf("ERR: EIDSP_SIGNAL_C_FN_POINTER can only be used when all axes are selected for DSP blocks\n");
            return EI_IMPULSE_DSP_ERROR;
        }
        int ret = extract_fn_slice(signal, &fm, block.config, EI_CLASSIFIER_FREQUENCY, &features_written);
#else
        SignalWithAxes swa(signal, block.axes, block.axes_size);
        int ret = extract_fn_slice(swa.get_signal(), &fm, block.config, EI_CLASSIFIER_FREQUENCY, &features_written);
#endif

        if (ret != EIDSP_OK) {
            ei_printf("ERR: Failed to run DSP process (%d)\n", ret);
            return EI_IMPULSE_DSP_ERROR;
        }

        if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
            return EI_IMPULSE_CANCELED;
        }

        classifier_continuous_features_written += (features_written.rows * features_written.cols);

        out_features_index += block.n_output_features;
    }

    result->timing.dsp_us = ei_read_timer_us() - dsp_start_us;
    result->timing.dsp = (int)(result->timing.dsp_us / 1000);

    if (debug) {
        ei_printf("\r\nFeatures (%d ms.): ", result->timing.dsp);
        for (size_t ix = 0; ix < static_features_matrix.cols; ix++) {
            ei_printf_float(static_features_matrix.buffer[ix]);
            ei_printf(" ");
        }
        ei_printf("\n");
    }

    if (classifier_continuous_features_written >= EI_CLASSIFIER_NN_INPUT_FRAME_SIZE) {
        dsp_start_us = ei_read_timer_us();
        ei::matrix_t classify_matrix(1, EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);

        /* Create a copy of the matrix for normalization */
        for (size_t m_ix = 0; m_ix < EI_CLASSIFIER_NN_INPUT_FRAME_SIZE; m_ix++) {
            classify_matrix.buffer[m_ix] = static_features_matrix.buffer[m_ix];
        }

        if (is_mfcc) {
            calc_cepstral_mean_and_var_normalization_mfcc(&classify_matrix, ei_dsp_blocks[0].config);
        }
        else if (is_spectrogram) {
            calc_cepstral_mean_and_var_normalization_spectrogram(&classify_matrix, ei_dsp_blocks[0].config);
        }
        else if (is_mfe) {
            calc_cepstral_mean_and_var_normalization_mfe(&classify_matrix, ei_dsp_blocks[0].config);
        }
        result->timing.dsp_us += ei_read_timer_us() - dsp_start_us;
        result->timing.dsp = (int)(result->timing.dsp_us / 1000);

#if EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_NONE
        if (debug) {
            ei_printf("Running neural network...\n");
        }
#endif
        ei_impulse_error = run_inference(&classify_matrix, result, debug);

#if (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_MICROPHONE)
        if((void *)avg_scores  != NULL && enable_maf == true) {
            result->label_detected = avg_scores->trigger(result->classification);
        }
#endif
    }
    return ei_impulse_error;
}

/**
 * @brief      Do inferencing over the processed feature matrix
 *
 * @param      fmatrix  Processed matrix
 * @param      result   Output classifier results
 * @param[in]  debug    Debug output enable
 *
 * @return     The ei impulse error.
 */
extern "C" EI_IMPULSE_ERROR run_inference(
    ei::matrix_t *fmatrix,
    ei_impulse_result_t *result,
    bool debug = false)
{
#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE || EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE_FULL)

    EI_IMPULSE_ERROR run_res = run_nn_inference(fmatrix, result, debug);
    if (run_res != EI_IMPULSE_OK) {
        return run_res;
    }

#elif (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW)
    {
        uint64_t ctx_start_us = ei_read_timer_us();
        uint32_t time, cycles;

        /* Run tensaiflow inference */
        infer(&time, &cycles);

        // Inference results returned by post_process() and copied into infer_results

        result->timing.classification_us = ei_read_timer_us() - ctx_start_us;
        result->timing.classification = (int)(result->timing.classification_us / 1000);

        for (uint32_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            float value;
            // Dequantize the output if it is int8
            value = static_cast<float>(infer_result[ix] - EI_CLASSIFIER_TFLITE_OUTPUT_ZEROPOINT) *
                EI_CLASSIFIER_TFLITE_OUTPUT_SCALE;

            if (debug) {
                ei_printf("%s:\t", ei_classifier_inferencing_categories[ix]);
                ei_printf_float(value);
                ei_printf("\n");
            }
            result->classification[ix].label = ei_classifier_inferencing_categories[ix];
            result->classification[ix].value = value;
        }
    }

#elif (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSORRT)
    {
        #if EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1
        #error "TensorRT requires an unquantized network"
        #endif

        static bool first_run = true;
        static char model_file_name[128];

        if (first_run) {
            snprintf(model_file_name, 128, "/tmp/ei-%s", trained_onnx_hash);

            FILE *file = fopen(model_file_name, "w");
            if (!file) {
                ei_printf("ERR: TensorRT init failed to open '%s'\n", model_file_name);
                return EI_IMPULSE_TENSORRT_INIT_FAILED;
            }

            if (fwrite(trained_onnx, trained_onnx_len, 1, file) != 1) {
                ei_printf("ERR: TensorRT init fwrite failed\n");
                return EI_IMPULSE_TENSORRT_INIT_FAILED;
            }

            if (fclose(file) != 0) {
                ei_printf("ERR: TensorRT init fclose failed\n");
                return EI_IMPULSE_TENSORRT_INIT_FAILED;
            }
        }

        float tensorrt_output[EI_CLASSIFIER_LABEL_COUNT];

        // lazy initialize tensorRT context
        if( ei_trt_handle == nullptr ) {
            ei_trt_handle = libeitrt::create_EiTrt(model_file_name, debug);
        }

        uint64_t ctx_start_us = ei_read_timer_us();

        libeitrt::infer(ei_trt_handle, fmatrix->buffer, tensorrt_output, EI_CLASSIFIER_LABEL_COUNT);

        uint64_t ctx_end_us = ei_read_timer_us();

        result->timing.classification_us = ctx_end_us - ctx_start_us;
        result->timing.classification = (int)(result->timing.classification_us / 1000);

        for( int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
            result->classification[i].label = ei_classifier_inferencing_categories[i];
            result->classification[i].value = tensorrt_output[i];
        }
    }
#endif // EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE

#if EI_CLASSIFIER_HAS_ANOMALY == 1

    // Anomaly detection
    {
        uint64_t anomaly_start_us = ei_read_timer_us();

        float input[EI_CLASSIFIER_ANOM_AXIS_SIZE];
        for (size_t ix = 0; ix < EI_CLASSIFIER_ANOM_AXIS_SIZE; ix++) {
            input[ix] = fmatrix->buffer[EI_CLASSIFIER_ANOM_AXIS[ix]];
        }
        standard_scaler(input, ei_classifier_anom_scale, ei_classifier_anom_mean, EI_CLASSIFIER_ANOM_AXIS_SIZE);
        float anomaly = get_min_distance_to_cluster(
            input, EI_CLASSIFIER_ANOM_AXIS_SIZE, ei_classifier_anom_clusters, EI_CLASSIFIER_ANOM_CLUSTER_COUNT);

        uint64_t anomaly_end_us = ei_read_timer_us();

        result->timing.anomaly_us = anomaly_end_us - anomaly_start_us;
        result->timing.anomaly = (int)(result->timing.anomaly_us / 1000);
        result->anomaly = anomaly;

        if (debug) {
            ei_printf("Anomaly score (time: %d ms.): ", result->timing.anomaly);
            ei_printf_float(anomaly);
            ei_printf("\n");
        }
    }

#endif

    if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
        return EI_IMPULSE_CANCELED;
    }

    return EI_IMPULSE_OK;
}

/**
 * Run the classifier over a raw features array
 * @param raw_features Raw features array
 * @param raw_features_size Size of the features array
 * @param result Object to store the results in
 * @param debug Whether to show debug messages (default: false)
 */
extern "C" EI_IMPULSE_ERROR run_classifier(
    signal_t *signal,
    ei_impulse_result_t *result,
    bool debug = false)
{
#if (EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1 && (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE || EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW)) || EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_DRPAI

    // Shortcut for quantized image models
    if (can_run_classifier_image_quantized() == EI_IMPULSE_OK) {
        return run_classifier_image_quantized(signal, result, debug);
    }
#endif

    // if (debug) {
    // static float buf[1000];
    // printf("Raw data: ");
    // for (size_t ix = 0; ix < 16000; ix += 1000) {
    //     int r = signal->get_data(ix, 1000, buf);
    //     for (size_t jx = 0; jx < 1000; jx++) {
    //         printf("%.0f, ", buf[jx]);
    //     }
    // }
    // printf("\n");
    // }

    memset(result, 0, sizeof(ei_impulse_result_t));

    ei::matrix_t features_matrix(1, EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);

    uint64_t dsp_start_us = ei_read_timer_us();

    size_t out_features_index = 0;

    for (size_t ix = 0; ix < ei_dsp_blocks_size; ix++) {
        ei_model_dsp_t block = ei_dsp_blocks[ix];

        if (out_features_index + block.n_output_features > EI_CLASSIFIER_NN_INPUT_FRAME_SIZE) {
            ei_printf("ERR: Would write outside feature buffer\n");
            return EI_IMPULSE_DSP_ERROR;
        }

        ei::matrix_t fm(1, block.n_output_features, features_matrix.buffer + out_features_index);

#if EIDSP_SIGNAL_C_FN_POINTER
        if (block.axes_size != EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
            ei_printf("ERR: EIDSP_SIGNAL_C_FN_POINTER can only be used when all axes are selected for DSP blocks\n");
            return EI_IMPULSE_DSP_ERROR;
        }
        int ret = block.extract_fn(signal, &fm, block.config, EI_CLASSIFIER_FREQUENCY);
#else
        SignalWithAxes swa(signal, block.axes, block.axes_size);
        int ret = block.extract_fn(swa.get_signal(), &fm, block.config, EI_CLASSIFIER_FREQUENCY);
#endif

        if (ret != EIDSP_OK) {
            ei_printf("ERR: Failed to run DSP process (%d)\n", ret);
            return EI_IMPULSE_DSP_ERROR;
        }

        if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
            return EI_IMPULSE_CANCELED;
        }

        out_features_index += block.n_output_features;
    }

    result->timing.dsp_us = ei_read_timer_us() - dsp_start_us;
    result->timing.dsp = (int)(result->timing.dsp_us / 1000);

    if (debug) {
        ei_printf("Features (%d ms.): ", result->timing.dsp);
        for (size_t ix = 0; ix < features_matrix.cols; ix++) {
            ei_printf_float(features_matrix.buffer[ix]);
            ei_printf(" ");
        }
        ei_printf("\n");
    }

#if EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_NONE
    if (debug) {
        ei_printf("Running neural network...\n");
    }
#endif

    return run_inference(&features_matrix, result, debug);
}



/**
 * @brief      Calculates the cepstral mean and variable normalization.
 *
 * @param      matrix      Source and destination matrix
 * @param      config_ptr  ei_dsp_config_mfcc_t struct pointer
 */
static void calc_cepstral_mean_and_var_normalization_mfcc(ei_matrix *matrix, void *config_ptr)
{
    ei_dsp_config_mfcc_t *config = (ei_dsp_config_mfcc_t *)config_ptr;

    uint32_t original_matrix_size = matrix->rows * matrix->cols;

    /* Modify rows and colums ration for matrix normalization */
    matrix->rows = original_matrix_size / config->num_cepstral;
    matrix->cols = config->num_cepstral;

    // cepstral mean and variance normalization
    int ret = speechpy::processing::cmvnw(matrix, config->win_size, true, false);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: cmvnw failed (%d)\n", ret);
        return;
    }

    /* Reset rows and columns ratio */
    matrix->rows = 1;
    matrix->cols = original_matrix_size;
}

/**
 * @brief      Calculates the cepstral mean and variable normalization.
 *
 * @param      matrix      Source and destination matrix
 * @param      config_ptr  ei_dsp_config_mfe_t struct pointer
 */
static void calc_cepstral_mean_and_var_normalization_mfe(ei_matrix *matrix, void *config_ptr)
{
    ei_dsp_config_mfe_t *config = (ei_dsp_config_mfe_t *)config_ptr;

    uint32_t original_matrix_size = matrix->rows * matrix->cols;

    /* Modify rows and colums ration for matrix normalization */
    matrix->rows = (original_matrix_size) / config->num_filters;
    matrix->cols = config->num_filters;

    if (config->implementation_version < 3) {
        // cepstral mean and variance normalization
        int ret = speechpy::processing::cmvnw(matrix, config->win_size, false, true);
        if (ret != EIDSP_OK) {
            ei_printf("ERR: cmvnw failed (%d)\n", ret);
            return;
        }
    }
    else {
        // normalization
        int ret = speechpy::processing::mfe_normalization(matrix, config->noise_floor_db);
        if (ret != EIDSP_OK) {
            ei_printf("ERR: normalization failed (%d)\n", ret);
            return;
        }
    }

    /* Reset rows and columns ratio */
    matrix->rows = 1;
    matrix->cols = (original_matrix_size);
}

/**
 * @brief      Calculates the cepstral mean and variable normalization.
 *
 * @param      matrix      Source and destination matrix
 * @param      config_ptr  ei_dsp_config_spectrogram_t struct pointer
 */
static void calc_cepstral_mean_and_var_normalization_spectrogram(ei_matrix *matrix, void *config_ptr)
{
    ei_dsp_config_spectrogram_t *config = (ei_dsp_config_spectrogram_t *)config_ptr;

    uint32_t original_matrix_size = matrix->rows * matrix->cols;

    /* Modify rows and colums ration for matrix normalization */
    matrix->cols = config->fft_length / 2 + 1;
    matrix->rows = (original_matrix_size) / matrix->cols;

    if (config->implementation_version < 3) {
        int ret = numpy::normalize(matrix);
        if (ret != EIDSP_OK) {
            ei_printf("ERR: normalization failed (%d)\n", ret);
            return;
        }
    }
    else {
        // normalization
        int ret = speechpy::processing::spectrogram_normalization(matrix, config->noise_floor_db);
        if (ret != EIDSP_OK) {
            ei_printf("ERR: normalization failed (%d)\n", ret);
            return;
        }
    }

    /* Reset rows and columns ratio */
    matrix->rows = 1;
    matrix->cols = (original_matrix_size);
}

/**
 * Check if the current impulse could be used by 'run_classifier_image_quantized'
 */
__attribute__((unused)) static EI_IMPULSE_ERROR can_run_classifier_image_quantized() {
#if (EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_TFLITE) && (EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_TENSAIFLOW) && (EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_DRPAI)
    return EI_IMPULSE_UNSUPPORTED_INFERENCING_ENGINE;
#endif

#if EI_CLASSIFIER_HAS_ANOMALY == 1
    return EI_IMPULSE_ONLY_SUPPORTED_FOR_IMAGES;
#endif

    // Check if we have a quantized NN Input layer (input is always quantized for DRP-AI)
#if EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED != 1
    return EI_IMPULSE_ONLY_SUPPORTED_FOR_IMAGES;
#endif

    // And if we have one DSP block which operates on images...
    if (ei_dsp_blocks_size != 1 || ei_dsp_blocks[0].extract_fn != extract_image_features) {
        return EI_IMPULSE_ONLY_SUPPORTED_FOR_IMAGES;
    }

    return EI_IMPULSE_OK;
}

#if EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1 && EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW

int8_t infer_result[EI_CLASSIFIER_LABEL_COUNT];

extern "C" void get_data(int8_t *in_buf_0, uint16_t in_buf_0_dim_0, uint16_t in_buf_0_dim_1, uint16_t in_buf_0_dim_2)
{
#if EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA

    if (ei_dsp_blocks_size == 1 || ei_dsp_blocks[0].extract_fn == extract_image_features) {
        memcpy(in_buf_0, processed_features, EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);
    }

#endif
}

extern "C" void post_process(int8_t *out_buf_0, int8_t *out_buf_1)
{
    memcpy(infer_result, out_buf_0, EI_CLASSIFIER_LABEL_COUNT);
}

#endif // #if EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1 && EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW

#if EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1 && (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE || EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW || EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_DRPAI)
/**
 * Special function to run the classifier on images, only works on TFLite models (either interpreter or EON or for tensaiflow)
 * that allocates a lot less memory by quantizing in place. This only works if 'can_run_classifier_image_quantized'
 * returns EI_IMPULSE_OK.
 */
extern "C" EI_IMPULSE_ERROR run_classifier_image_quantized(
    signal_t *signal,
    ei_impulse_result_t *result,
    bool debug = false)
{
    EI_IMPULSE_ERROR verify_res = can_run_classifier_image_quantized();
    if (verify_res != EI_IMPULSE_OK) {
        return verify_res;
    }

    memset(result, 0, sizeof(ei_impulse_result_t));

#if ((EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_TENSAIFLOW) && (EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_DRPAI))

#if (EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_TFLITE)
    return EI_IMPULSE_UNSUPPORTED_INFERENCING_ENGINE;
#else

    return run_nn_inference_image_quantized(signal, result, debug);

#endif // EI_CLASSIFIER_INFERENCING_ENGINE != EI_CLASSIFIER_TFLITE

#elif (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW)

    uint64_t ctx_start_us;
    uint64_t dsp_start_us = ei_read_timer_us();

    ei::matrix_i8_t features_matrix(1, EI_CLASSIFIER_NN_INPUT_FRAME_SIZE);
    processed_features = (int8_t *) features_matrix.buffer;

    // run DSP process and quantize automatically
    int ret = extract_image_features_quantized(signal, &features_matrix, ei_dsp_blocks[0].config, EI_CLASSIFIER_FREQUENCY);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to run DSP process (%d)\n", ret);
        return EI_IMPULSE_DSP_ERROR;
    }

    if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
        return EI_IMPULSE_CANCELED;
    }

    result->timing.dsp_us = ei_read_timer_us() - dsp_start_us;
    result->timing.dsp = (int)(result->timing.dsp_us / 1000);

    if (debug) {
        ei_printf("Features (%d ms.): ", result->timing.dsp);
        for (size_t ix = 0; ix < features_matrix.cols; ix++) {
            ei_printf_float((features_matrix.buffer[ix] - EI_CLASSIFIER_TFLITE_INPUT_ZEROPOINT) * EI_CLASSIFIER_TFLITE_INPUT_SCALE);
            ei_printf(" ");
        }
        ei_printf("\n");
    }

    uint32_t time, cycles;
    ctx_start_us = ei_read_timer_us();

    /* Run tensaiflow inference */
    infer(&time, &cycles);

    // Inference results returned by post_process() and copied into infer_results

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;
    result->timing.classification = (int)(result->timing.classification_us / 1000);

    for (uint32_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        float value;
        // Dequantize the output if it is int8
        value = static_cast<float>(infer_result[ix] - EI_CLASSIFIER_TFLITE_OUTPUT_ZEROPOINT) *
            EI_CLASSIFIER_TFLITE_OUTPUT_SCALE;

        if (debug) {
            ei_printf("%s:\t", ei_classifier_inferencing_categories[ix]);
            ei_printf_float(value);
            ei_printf("\n");
        }
        result->classification[ix].label = ei_classifier_inferencing_categories[ix];
        result->classification[ix].value = value;
    }

    return EI_IMPULSE_OK;

#elif (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_DRPAI)
    static bool first_run = true;
    uint64_t ctx_start_us;
    uint64_t dsp_start_us = ei_read_timer_us();

    if (first_run) {
        // map memory regions to the DRP-AI UDMA. This is required for passing data
        // to and from DRP-AI
        int t = drpai_init_mem();
        if (t != 0) {
            return EI_IMPULSE_DRPAI_INIT_FAILED;
        }

        EI_IMPULSE_ERROR ret = drpai_init_classifier(debug);
        if (ret != EI_IMPULSE_OK) {
            drpai_close(debug);
            return EI_IMPULSE_DRPAI_INIT_FAILED;
        }
    }

    if (debug) {
        ei_printf("starting DSP...\n");
    }

    // Creates a features matrix mapped to the DRP-AI UDMA input region
    ei::matrix_i8_t features_matrix(1, proc[DRPAI_INDEX_INPUT].size, (int8_t *)drpai_input_buf);

    // Grabs the raw image buffer from the signal, DRP-AI will automatically
    // extract features
    int ret = extract_drpai_features_quantized(
        signal,
        &features_matrix,
        ei_dsp_blocks[0].config,
        EI_CLASSIFIER_FREQUENCY);
    if (ret != EIDSP_OK) {
        ei_printf("ERR: Failed to run DSP process (%d)\n", ret);
        return EI_IMPULSE_DSP_ERROR;
    }

    if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
        return EI_IMPULSE_CANCELED;
    }

    result->timing.dsp_us = ei_read_timer_us() - dsp_start_us;
    result->timing.dsp = (int)(result->timing.dsp_us / 1000);

    //if (debug) {
    //    ei_printf("DSP was assigned address: 0x%lx\n", drpai_input_buf);
    //    ei_printf("Features (%d ms.): ", result->timing.dsp);
    //    for (size_t ix = 0; ix < EI_CLASSIFIER_NN_INPUT_FRAME_SIZE; ix++) {
    //        ei_printf("0x%hhx, ", drpai_input_buf[ix]);
    //    }
    //    ei_printf("\n");
    //}

    ctx_start_us = ei_read_timer_us();

    // Run DRP-AI inference, a static buffer is used to store the raw output
    // results
    ret = drpai_run_classifier_image_quantized(debug);

    // close driver to reset memory, file pointer
    if (ret != EI_IMPULSE_OK) {
        drpai_close(debug);
        first_run = true;
    }
    else {
        // drpai_reset();
        first_run = false;
    }

#if EI_CLASSIFIER_OBJECT_DETECTION_CONSTRAINED == 1
    if (debug) {
        ei_printf("DEBUG: raw drpai output");
        ei_printf("\n[");
        for (uint32_t i = 0; i < 12 * 12 * 2; i++) {
            ei_printf_float(drpai_output_buf[i]);
            ei_printf(" ");
        }
        ei_printf("]\n");
    }

    fill_result_struct_f32(
        result,
        drpai_output_buf,
        EI_CLASSIFIER_INPUT_WIDTH / 8,
        EI_CLASSIFIER_INPUT_HEIGHT / 8);
#elif EI_CLASSIFIER_OBJECT_DETECTION == 1
    if (debug) {
        ei_printf("ERR: Output tensor formatting not yet implemented for DRP-AI");
    }
    return EI_IMPULSE_UNSUPPORTED_INFERENCING_ENGINE;
#else
    fill_result_struct_f32(result, drpai_output_buf, debug);
#endif

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;
    result->timing.classification = (int)(result->timing.classification_us / 1000);
    return EI_IMPULSE_OK;
#endif // (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_DRPAI)
}

#endif // #if EI_CLASSIFIER_TFLITE_INPUT_QUANTIZED == 1 && (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TFLITE || EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_TENSAIFLOW || EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_DRPAI)

#if EIDSP_SIGNAL_C_FN_POINTER == 0

/**
 * Run the impulse, if you provide an instance of sampler it will also persist the data for you
 * @param sampler Instance to an **initialized** sampler
 * @param result Object to store the results in
 * @param data_fn Function to retrieve data from sensors
 * @param debug Whether to log debug messages (default false)
 */
__attribute__((unused)) EI_IMPULSE_ERROR run_impulse(
#if defined(EI_CLASSIFIER_HAS_SAMPLER) && EI_CLASSIFIER_HAS_SAMPLER == 1
        EdgeSampler *sampler,
#endif
        ei_impulse_result_t *result,
#ifdef __MBED__
        mbed::Callback<void(float*, size_t)> data_fn,
#else
        std::function<void(float*, size_t)> data_fn,
#endif
        bool debug = false) {

    float *x = (float*)calloc(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, sizeof(float));
    if (!x) {
        return EI_IMPULSE_OUT_OF_MEMORY;
    }

    uint64_t next_tick = 0;

    uint64_t sampling_us_start = ei_read_timer_us();

    // grab some data
    for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
        uint64_t curr_us = ei_read_timer_us() - sampling_us_start;

        next_tick = curr_us + (EI_CLASSIFIER_INTERVAL_MS * 1000);

        data_fn(x + i, EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME);
#if defined(EI_CLASSIFIER_HAS_SAMPLER) && EI_CLASSIFIER_HAS_SAMPLER == 1
        if (sampler != NULL) {
            sampler->write_sensor_data(x + i, EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME);
        }
#endif

        if (ei_run_impulse_check_canceled() == EI_IMPULSE_CANCELED) {
            free(x);
            return EI_IMPULSE_CANCELED;
        }

        while (next_tick > ei_read_timer_us() - sampling_us_start);
    }

    result->timing.sampling = (ei_read_timer_us() - sampling_us_start) / 1000;

    signal_t signal;
    int err = numpy::signal_from_buffer(x, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0) {
        free(x);
        ei_printf("ERR: signal_from_buffer failed (%d)\n", err);
        return EI_IMPULSE_DSP_ERROR;
    }

    EI_IMPULSE_ERROR r = run_classifier(&signal, result, debug);
    free(x);
    return r;
}

#if defined(EI_CLASSIFIER_HAS_SAMPLER) && EI_CLASSIFIER_HAS_SAMPLER == 1
/**
 * Run the impulse, does not persist data
 * @param result Object to store the results in
 * @param data_fn Function to retrieve data from sensors
 * @param debug Whether to log debug messages (default false)
 */
__attribute__((unused)) EI_IMPULSE_ERROR run_impulse(
        ei_impulse_result_t *result,
#ifdef __MBED__
        mbed::Callback<void(float*, size_t)> data_fn,
#else
        std::function<void(float*, size_t)> data_fn,
#endif
        bool debug = false) {
    return run_impulse(NULL, result, data_fn, debug);
}
#endif

#endif // #if EIDSP_SIGNAL_C_FN_POINTER == 0

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _EDGE_IMPULSE_RUN_CLASSIFIER_H_
