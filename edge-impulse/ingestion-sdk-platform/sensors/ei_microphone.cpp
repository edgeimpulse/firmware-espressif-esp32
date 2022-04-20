/* Edge Impulse ingestion SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
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

/* Include ----------------------------------------------------------------- */
#include "ei_microphone.h"

#include "ei_device_espressif_esp32.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "ei_config_types.h"
#include "sensor_aq_mbedtls_hs256.h"
#include "sensor_aq_none.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"

typedef struct {
    int16_t *buffers[2];
    uint8_t buf_select;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

/* Extern declared --------------------------------------------------------- */
//extern EiDeviceESP32* dev;
//extern EiDeviceMemory* mem;

/* Dummy functions for sensor_aq_ctx type */
static size_t ei_write(const void*, size_t size, size_t count, EI_SENSOR_AQ_STREAM*)
{
    return count;
}

static int ei_seek(EI_SENSOR_AQ_STREAM*, long int offset, int origin)
{
    return 0;
}

/* Private variables ------------------------------------------------------- */
static signed short *sampleBuffer;
static bool is_uploaded = false;
static int record_status = 0;
static uint32_t headerOffset;
static uint32_t samples_required;
static uint32_t current_sample;
static uint32_t audio_sampling_frequency = 16000;

//static signed short *inference_buffers[2];
static inference_t inference;

static unsigned char ei_mic_ctx_buffer[1024];
static sensor_aq_signing_ctx_t ei_mic_signing_ctx;
static sensor_aq_mbedtls_hs256_ctx_t ei_mic_hs_ctx;
static sensor_aq_ctx ei_mic_ctx = {
    { ei_mic_ctx_buffer, 1024 },
    &ei_mic_signing_ctx,
    &ei_write,
    &ei_seek,
    NULL,
};


/* Audio thread setup */
#define AUDIO_THREAD_STACK_SIZE 4096

static const char* TAG = "AUDIO_PROVIDER";

/* Private functions ------------------------------------------------------- */

static void audio_write_callback(uint32_t n_bytes)
{
    EiDeviceESP32* dev = static_cast<EiDeviceESP32*>(EiDeviceESP32::get_device());
    EiDeviceMemory* mem = dev->get_memory();

    mem->write_sample_data((const uint8_t *)sampleBuffer, headerOffset + current_sample, n_bytes);

    ei_mic_ctx.signature_ctx->update(ei_mic_ctx.signature_ctx, (uint8_t*)sampleBuffer, n_bytes);

    current_sample += n_bytes;
    if(current_sample >= (samples_required << 1)) {
        i2s_deinit();
        record_status = false;
        free(sampleBuffer);
    }
}

static void audio_inference_callback(uint32_t n_bytes)
{
    for(int i = 0; i < n_bytes>>1; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

        if(inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

static void capture_samples(void* arg) {

  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read = i2s_bytes_to_read;

  while (record_status) {

    /* read data at once from i2s */
    i2s_read((i2s_port_t)1, (void*)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);

    if (bytes_read <= 0) {
      ESP_LOGE(TAG, "Error in I2S read : %d", bytes_read);
    } 
    else {
        if (bytes_read < i2s_bytes_to_read) {
        ESP_LOGW(TAG, "Partial I2S read");
        }

        // scale the data (otherwise the sound is too quiet)
        for (int x = 0; x < i2s_bytes_to_read/2; x++) {
            sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
        }
        
        // see if are recording samples for ingestion
        // or inference and send them their way
        if (record_status == 1) {
            audio_write_callback(i2s_bytes_to_read);
        }
        else if (record_status == 2) {
            audio_inference_callback(i2s_bytes_to_read);
        }
        else {
            break;
        }

    }
  }
  vTaskDelete(NULL);
}

static void finish_and_upload(char *filename, uint32_t sample_length_ms) {

    EiDeviceESP32* dev = static_cast<EiDeviceESP32*>(EiDeviceESP32::get_device());
    EiDeviceMemory* mem = dev->get_memory();
    
    ei_printf("Done sampling, total bytes collected: %u\n", current_sample*2);

    dev->set_state(eiStateUploading);

    ei_printf("[1/1] Uploading file to Edge Impulse...\n");

    ei_printf("Not uploading file, not connected to WiFi. Used buffer, from=%lu, to=%lu.\n", 0, current_sample + headerOffset);

    ei_printf("[1/1] Uploading file to Edge Impulse OK (took %d ms.)\n", 0);

    is_uploaded = true;

    dev->set_state(eiStateFinished);

    ei_printf("OK\n");
}

static int insert_ref(char *buffer, int hdrLength)
{
    #define EXTRA_BYTES(a)  ((a & 0x3) ? 4 - (a & 0x3) : (a & 0x03))
    const char *ref = "Ref-BINARY-i16";
    int addLength = 0;
    int padding = EXTRA_BYTES(hdrLength);

    buffer[addLength++] = 0x60 + 14 + padding;
    for(int i = 0; i < strlen(ref); i++) {
        buffer[addLength++] = *(ref + i);
    }
    for(int i = 0; i < padding; i++) {
        buffer[addLength++] = ' ';
    }

    buffer[addLength++] = 0xFF;

    return addLength;
}

static bool create_header(void)
{

    EiDeviceESP32* dev = static_cast<EiDeviceESP32*>(EiDeviceESP32::get_device());
    EiDeviceMemory* mem = dev->get_memory();
    sensor_aq_init_mbedtls_hs256_context(&ei_mic_signing_ctx, &ei_mic_hs_ctx, dev->get_sample_hmac_key().c_str());

    sensor_aq_payload_info payload = {
        dev->get_device_id().c_str(),
        dev->get_device_type().c_str(),
        dev->get_sample_interval_ms(),
        { { "audio", "wav" } }
    };

    int tr = sensor_aq_init(&ei_mic_ctx, &payload, NULL, true);

    if (tr != AQ_OK) {
        ei_printf("sensor_aq_init failed (%d)\n", tr);
        return false;
    }

    // then we're gonna find the last byte that is not 0x00 in the CBOR buffer.
    // That should give us the whole header
    size_t end_of_header_ix = 0;
    for (size_t ix = ei_mic_ctx.cbor_buffer.len - 1; ix >= 0; ix--) {
        if (((uint8_t*)ei_mic_ctx.cbor_buffer.ptr)[ix] != 0x0) {
            end_of_header_ix = ix;
            break;
        }
    }

    if (end_of_header_ix == 0) {
        ei_printf("Failed to find end of header\n");
        return false;
    }

    int ref_size = insert_ref(((char*)ei_mic_ctx.cbor_buffer.ptr + end_of_header_ix), end_of_header_ix);

    // and update the signature
    tr = ei_mic_ctx.signature_ctx->update(ei_mic_ctx.signature_ctx, (uint8_t*)(ei_mic_ctx.cbor_buffer.ptr + end_of_header_ix), ref_size);
    if (tr != 0) {
        ei_printf("Failed to update signature from header (%d)\n", tr);
        return false;
    }

    end_of_header_ix += ref_size;

    // Write to blockdevice
    int ret = mem->write_sample_data((uint8_t*)ei_mic_ctx.cbor_buffer.ptr, 0, end_of_header_ix);

    if (ret != end_of_header_ix) {
        ei_printf("Failed to write to header blockdevice (%d)\n", ret);
        return false;
    }

    headerOffset = end_of_header_ix;

    return true;
}


/* Public functions -------------------------------------------------------- */

bool ei_microphone_record(uint32_t sample_length_ms, uint32_t start_delay_ms, bool print_start_messages)
{
    EiDeviceESP32* dev = static_cast<EiDeviceESP32*>(EiDeviceESP32::get_device());
    EiDeviceMemory* mem = dev->get_memory();

    start_delay_ms = start_delay_ms < 2000 ? 2000 : start_delay_ms;

    if (print_start_messages) {
        ei_printf("Starting in %lu ms... (or until all flash was erased)\n", start_delay_ms);
    }

    if(mem->erase_sample_data(0, (samples_required << 1) + 4096) != ((samples_required << 1) + 4096)) {
        return false;
    }    

    vTaskDelay(start_delay_ms / portTICK_RATE_MS);

    create_header();

    record_status = 1;

    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)AUDIO_THREAD_STACK_SIZE, 10, NULL);

    if (print_start_messages) {
        ei_printf("Sampling...\n");
    }

    return true;
}

bool ei_microphone_inference_start(uint32_t n_samples, float interval_ms)
{

    inference.buffers[0] = (int16_t *)malloc(n_samples * sizeof(int16_t));

    if(inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (int16_t *)malloc(n_samples * sizeof(int16_t));

    if(inference.buffers[1] == NULL) {
        free(inference.buffers[0]);
        return false;
    }

    uint32_t sample_buffer_size = (n_samples / 100) * sizeof(int16_t);
    sampleBuffer = (int16_t *)malloc(sample_buffer_size);

    if(sampleBuffer == NULL) {
        free(inference.buffers[0]);
        free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count  = 0;
    inference.n_samples  = n_samples;
    inference.buf_ready  = 0;

    // Calculate sample rate from sample interval
    audio_sampling_frequency = (uint32_t)(1000.f / interval_ms);

    if (i2s_init(audio_sampling_frequency)) {
        ei_printf("Failed to start I2S!");
    }

    ei_sleep(100);

    record_status = 2;

    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL);

    return true;

}

/**
 * @brief      Wait for a full buffer
 *
 * @return     In case of an buffer overrun return false
 */
bool ei_microphone_inference_record(void)
{
    bool ret = true;

    if (inference.buf_ready == 1) {
        ei_printf(
            "Error sample buffer overrun. Decrease the number of slices per model window "
            "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
        ret = false;
    }

    while (inference.buf_ready == 0) {
        vTaskDelay(1 / portTICK_RATE_MS);
    }

    inference.buf_ready = 0;

    return ret;
}

bool ei_microphone_inference_is_recording(void)
{
    return inference.buf_ready == 0;
}

/**
 * @brief      Reset buffer counters for non-continuous inferencing
 */
void ei_microphone_inference_reset_buffers(void)
{
    inference.buf_ready = 0;
    inference.buf_count = 0;
}

/**
 * Get raw audio signal data
 */
int ei_microphone_inference_get_data(size_t offset, size_t length, float *out_ptr)
{
    return ei::numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
}


bool ei_microphone_inference_end(void)
{
    record_status = 0;
    ei_sleep(100);
    i2s_deinit();
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    free(sampleBuffer);
    return 0;
}

/**
 * Sample raw data
 */
bool ei_microphone_sample_start(void)
{
    EiDeviceESP32* dev = static_cast<EiDeviceESP32*>(EiDeviceESP32::get_device());
    EiDeviceMemory* mem = dev->get_memory();

    int sample_length_blocks;
    int ret;

    ei_printf("Sampling settings:\n");
    ei_printf("\tInterval: %.5f ms.\n", dev->get_sample_interval_ms());
    ei_printf("\tLength: %lu ms.\n", dev->get_sample_length_ms());
    ei_printf("\tName: %s\n", dev->get_sample_label().c_str());
    ei_printf("\tHMAC Key: %s\n", dev->get_sample_hmac_key().c_str());
    ei_printf("\tFile name: %s\n", dev->get_sample_label().c_str());

    samples_required = (uint32_t)((dev->get_sample_length_ms()) / dev->get_sample_interval_ms());

    // Round to even number of samples for word align flash write
    if(samples_required & 1) {
        samples_required++;
    }

    current_sample = 0;

    is_uploaded = false;

    sampleBuffer = (int16_t *)malloc(mem->block_size);

    if (sampleBuffer == NULL) {
        return false;
    }

    // Calculate sample rate from sample interval
    audio_sampling_frequency = (uint32_t)(1000.f / dev->get_sample_interval_ms());

    if (i2s_init(audio_sampling_frequency)) {
        ei_printf("Failed to start I2S!");
    }

    bool r = ei_microphone_record(dev->get_sample_length_ms(), (((samples_required << 1)/ mem->block_size) * mem->block_erase_time), true);
    if (!r) {
        return r;
    }

    while(record_status) {
        dev->set_state(eiStateSampling);
        //vTaskDelay(10 / portTICK_RATE_MS);
    };

    int ctx_err = ei_mic_ctx.signature_ctx->finish(ei_mic_ctx.signature_ctx, ei_mic_ctx.hash_buffer.buffer);
    if (ctx_err != 0) {
        ei_printf("Failed to finish signature (%d)\n", ctx_err);
        return false;
    }

    // load the first page in flash...
    uint8_t *page_buffer = (uint8_t*)malloc(mem->block_size);
    if (!page_buffer) {
        ei_printf("Failed to allocate a page buffer to write the hash\n");
        return false;
    }

    ret = mem->read_sample_data(page_buffer, 0, mem->block_size);
    if (ret != mem->block_size) {
        ei_printf("Failed to read first page (read %d, requested %d)\n", ret, mem->block_size);
        ei_free(page_buffer);
        return false;
    }

    // update the hash
    uint8_t *hash = ei_mic_ctx.hash_buffer.buffer;
    // we have allocated twice as much for this data (because we also want to be able to represent in hex)
    // thus only loop over the first half of the bytes as the signature_ctx has written to those
    for (size_t hash_ix = 0; hash_ix < ei_mic_ctx.hash_buffer.size / 2; hash_ix++) {
        // this might seem convoluted, but snprintf() with %02x is not always supported e.g. by newlib-nano
        // we encode as hex... first ASCII char encodes top 4 bytes
        uint8_t first = (hash[hash_ix] >> 4) & 0xf;
        // second encodes lower 4 bytes
        uint8_t second = hash[hash_ix] & 0xf;

        // if 0..9 -> '0' (48) + value, if >10, then use 'a' (97) - 10 + value
        char first_c = first >= 10 ? 87 + first : 48 + first;
        char second_c = second >= 10 ? 87 + second : 48 + second;

        page_buffer[ei_mic_ctx.signature_index + (hash_ix * 2) + 0] = first_c;
        page_buffer[ei_mic_ctx.signature_index + (hash_ix * 2) + 1] = second_c;
    }

    ret = mem->erase_sample_data(0, mem->block_size);
    if (ret != mem->block_size) {
        ei_printf("Failed to erase first page (read %d, requested %d)\n", ret, mem->block_size);
        ei_free(page_buffer);
        return false;
    }

    ret = mem->write_sample_data(page_buffer, 0, mem->block_size);
    ei_free(page_buffer);

    if (ret != mem->block_size) {
        ei_printf("Failed to write first page with updated hash (read %d, requested %d)\n", ret, mem->block_size);
        return false;
    }

    finish_and_upload((char*)dev->get_sample_label().c_str(), dev->get_sample_length_ms());

    return true;
}

static int i2s_init(uint32_t sampling_rate) {
  // Start listening for audio: MONO @ 8/16KHz
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
      .sample_rate = sampling_rate,
      .bits_per_sample = (i2s_bits_per_sample_t)16,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = -1,
  };
  i2s_pin_config_t pin_config = {
      .bck_io_num = 26,    // IIS_SCLK
      .ws_io_num = 32,     // IIS_LCLK
      .data_out_num = -1,  // IIS_DSIN
      .data_in_num = 33,   // IIS_DOUT
  };
  esp_err_t ret = 0;

  ret = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_driver_install");
  }

  ret = i2s_set_pin((i2s_port_t)1, &pin_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_set_pin");
  }

  ret = i2s_zero_dma_buffer((i2s_port_t)1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error in initializing dma buffer with 0");
  }

  return int(ret);
}

static int i2s_deinit(void) {
    i2s_driver_uninstall((i2s_port_t)1); //stop & destroy i2s driver
    return 0;
}