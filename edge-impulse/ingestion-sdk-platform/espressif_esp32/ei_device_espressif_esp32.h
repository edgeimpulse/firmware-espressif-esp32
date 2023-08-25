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

#ifndef EI_DEVICE_ESP32
#define EI_DEVICE_ESP32

/* Include ----------------------------------------------------------------- */
#include "ei_classifier_porting.h"
#include "ei_device_info_lib.h"
#include "firmware-sdk/ei_device_memory.h"
#include "ei_camera.h"
#include "ei_wifi_esp32.h"

/** Baud rates */
#define DEFAULT_BAUD 115200
#define MAX_BAUD     1000000 //works well with 1000000, less stable at 2000000, setting to multiple of 115200 causes issues

/** Number of sensors used */
#define EI_DEVICE_N_SENSORS            1
#define EI_MAX_FREQUENCIES             5
#define EI_DEVICE_N_RESOLUTIONS        4
#define EI_DEVICE_N_RESIZE_RESOLUTIONS 2

/** C Callback types */
typedef int (*c_callback)(uint8_t out_buffer[32], size_t *out_size);
typedef bool (*c_callback_status)(void);
typedef bool (*c_callback_read_sample_buffer)(
    size_t begin,
    size_t length,
    void (*data_fn)(uint8_t *, size_t));

typedef struct {
    size_t width;
    size_t height;
} ei_device_resize_resolutions_t;

/**
 * @brief      Class description and implementation of device specific
 * 			   characteristics
 */
class EiDeviceESP32 : public EiDeviceInfo {
private:

    static const int standalone_sensor_num = 1;
    ei_device_sensor_t standalone_sensor_list[standalone_sensor_num];

    ei_device_snapshot_resolutions_t snapshot_resolutions[EI_DEVICE_N_RESOLUTIONS];

    bool camera_present;
    EiCameraESP32 *cam;

    bool network_present;
    bool network_connected;
    //EiNetworkDevice *net;

    EiDeviceESP32() = delete;
    std::string mac_address = "00:11:22:33:44:55:66";
    EiState state;

public:

    EiDeviceESP32(EiDeviceMemory* mem);
    ~EiDeviceESP32();

    void delay_ms(uint32_t milliseconds);

    bool scan_networks(void);
    bool get_wifi_connection_status(void);
    bool get_wifi_present_status();

    std::string get_mac_address(void);

    void (*sample_read_callback)(void);
    void init_device_id(void);
    void clear_config(void);
    bool is_camera_present(void);
    bool start_sample_thread(void (*sample_read_cb)(void), float sample_interval_ms) override;
    bool stop_sample_thread(void) override;
    void set_state(EiState state) override;
    EiState get_state(void);
    bool get_sensor_list(const ei_device_sensor_t **sensor_list, size_t *sensor_list_size) override;
    EiSnapshotProperties get_snapshot_list(void);
    uint32_t filesys_get_block_size(void);
    uint32_t filesys_get_n_available_sample_blocks(void);
    void set_default_data_output_baudrate(void) override;
    void set_max_data_output_baudrate(void) override;
    uint32_t get_data_output_baudrate(void) override;

};

/* Function prototypes ----------------------------------------------------- */
char ei_get_serial_byte(void);
bool ei_user_invoke_stop(void);

#endif