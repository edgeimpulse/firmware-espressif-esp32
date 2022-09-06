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
#include "ei_device_espressif_esp32.h"

#include <stdarg.h>
#include <stdio.h>
#include <cstdlib>
#include <cstdint>

#include "ei_config_types.h"
#include "ei_microphone.h"
#include "flash_memory.h"

#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/uart.h"
//#include "esp_wifi.h"

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

/* Constants --------------------------------------------------------------- */
#define EI_RED_LED_OFF      gpio_set_level(GPIO_NUM_21, 0);
#define EI_WHITE_LED_OFF    gpio_set_level(GPIO_NUM_22, 0);
#define EI_RED_LED_ON     gpio_set_level(GPIO_NUM_21, 1);
#define EI_WHITE_LED_ON    gpio_set_level(GPIO_NUM_22, 1);

/** Global objects */
TimerHandle_t fusion_timer;
void (*sample_cb_ptr)(void);

/* Private function declarations ------------------------------------------- */
void vTimerCallback(TimerHandle_t xTimer);

/* Public functions -------------------------------------------------------- */

EiDeviceESP32::EiDeviceESP32(EiDeviceMemory* mem)
{

    EiDeviceInfo::memory = mem;

    init_device_id();

    load_config();

    device_type = "ESPRESSIF_ESP32";

    cam = static_cast<EiCameraESP32*>(EiCameraESP32::get_camera());
    camera_present = cam->is_camera_present();

    // TODO
    //net = static_cast<EiWifiESP32*>(EiWifiESP32::get_network_device());
    // the absence of error on device init is success
    //network_present = !(net->init());

    // microphone is not handled by fusion system
    standalone_sensor_list[0].name = "Built-in microphone";
    standalone_sensor_list[0].start_sampling_cb = &ei_microphone_sample_start;
    standalone_sensor_list[0].max_sample_length_s = mem->get_available_sample_bytes() / (16000 * 2);
    standalone_sensor_list[0].frequencies[0] = 16000.0f;
    standalone_sensor_list[0].frequencies[1] = 8000.0f;
  
}


EiDeviceESP32::~EiDeviceESP32()
{

}

void EiDeviceESP32::init_device_id(void)
{
    // Setup device ID
    char temp[18];
	uint8_t baseMac[6];
	esp_read_mac(baseMac, ESP_MAC_WIFI_STA);

	sprintf(temp, 
            "%02X:%02X:%02X:%02X:%02X:%02X",
            baseMac[0],
            baseMac[1],
            baseMac[2],
            baseMac[3],
            baseMac[4],
            baseMac[5]);

    device_id = std::string(temp);
    mac_address = std::string(temp);    
}

EiDeviceInfo* EiDeviceInfo::get_device(void)
{
    static EiFlashMemory memory(sizeof(EiConfig));
    static EiDeviceESP32 dev(&memory);

    return &dev;
}

void EiDeviceESP32::clear_config(void)
{
    EiDeviceInfo::clear_config();

    init_device_id();
    save_config();
}

/**
 * @brief      Device specific delay ms implementation
 *
 * @param[in]  milliseconds  The milliseconds
 */
void EiDeviceESP32::delay_ms(uint32_t milliseconds)
{
    ei_sleep(milliseconds);
}

void EiDeviceESP32::set_state(EiState state)
{
    switch(state) {
    case eiStateErasingFlash:
        EI_WHITE_LED_ON;
        break;
    case eiStateSampling:
        EI_RED_LED_ON;
        delay_ms(100);
        EI_RED_LED_OFF;
        delay_ms(100);          
        break;
    case eiStateUploading:
        EI_RED_LED_ON;    
        EI_WHITE_LED_ON;
        break;
    case eiStateFinished:
        for (int i = 0; i < 4; i++) {    
            EI_RED_LED_ON;
            EI_WHITE_LED_ON;
            delay_ms(100);
            EI_RED_LED_OFF;
            EI_WHITE_LED_OFF;
            delay_ms(100);            
        }                                
        break;                
    default:
        EI_RED_LED_OFF;
        EI_WHITE_LED_OFF;
    }
}

/**
 * @brief      No Wifi available for device.
 *
 * @return     Always return false
 */
bool EiDeviceESP32::scan_networks(void)
{
    return false;
}

/**
 * @brief      No Wifi available for device.
 *
 * @return     Always return false
 */
bool EiDeviceESP32::get_wifi_connection_status(void)
{
    return false;
}

/**
 * @brief      No Wifi available for device.
 *
 * @return     Always return false
 */
bool EiDeviceESP32::get_wifi_present_status(void)
{
    return network_present;
}

/**
 * @brief      Create sensor list with sensor specs
 *             The studio and daemon require this list
 * @param      sensor_list       Place pointer to sensor list
 * @param      sensor_list_size  Write number of sensors here
 *
 * @return     False if all went ok
 */
bool EiDeviceESP32::get_sensor_list(
    const ei_device_sensor_t **sensor_list,
    size_t *sensor_list_size)
{

    *sensor_list = this->standalone_sensor_list;
    *sensor_list_size = this->standalone_sensor_num;

    return false;
}

/**
 * @brief      Create resolution list for snapshot setting
 *             The studio and daemon require this list
 * @param      snapshot_list       Place pointer to resolution list
 * @param      snapshot_list_size  Write number of resolutions here
 *
 * @return     False if all went ok
 */

EiSnapshotProperties EiDeviceESP32::get_snapshot_list(void)
{
    ei_device_snapshot_resolutions_t *res = NULL;
    uint8_t res_num = 0;

    //TODO: move the getting of snapshot to camera device
    EiSnapshotProperties props = {
        .has_snapshot = false,
        .support_stream = false,
        .color_depth = "",
        .resolutions_num = 0,
        .resolutions = res
    };

    if(this->cam->is_camera_present() == true) {
        this->cam->get_resolutions(&res, &res_num);
        props.has_snapshot = true;
        props.support_stream = true;
        props.color_depth = "RGB";
        props.resolutions_num = res_num;
        props.resolutions = res;
    }

    return props;
}


uint32_t EiDeviceESP32::get_data_output_baudrate(void)
{
    return MAX_BAUD;
}

void EiDeviceESP32::set_default_data_output_baudrate(void)
{
    fflush(stdout);
    ei_sleep(10);
    esp_err_t ret = uart_set_baudrate(0, DEFAULT_BAUD);
}

void EiDeviceESP32::set_max_data_output_baudrate(void)
{
    fflush(stdout);
    ei_sleep(10);
    esp_err_t ret = uart_set_baudrate(0, MAX_BAUD);
}

/**
 * @brief Setup timer or thread with given interval and call cb function each period
 * @param sample_read_cb
 * @param sample_interval_ms
 * @return true
 */
bool EiDeviceESP32::start_sample_thread(void (*sample_read_cb)(void), float sample_interval_ms)
{
    sample_cb_ptr = sample_read_cb;   
    fusion_timer = xTimerCreate(
                        "Fusion sampler",
                        (uint32_t)sample_interval_ms / portTICK_RATE_MS,
                        pdTRUE,
                        (void *) 0,
                        vTimerCallback
                    );
    xTimerStart(fusion_timer, 0);

    return true;
}

/**
 * @brief Stop timer of thread
 * @return true
 */
bool EiDeviceESP32::stop_sample_thread(void)
{
    
    if (xTimerStop(fusion_timer, 0) != pdPASS)
    {
        ei_printf("ERR: timer has not been stopped \n");
    }

    return true;
}

// to preserve compatibility with no memory interface devices using firmware-sdk
// supposed to be deprecating

/**
 * @brief Get byte size of memory block
 *
 * @return uint32_t size in bytes
 */
uint32_t EiDeviceESP32::filesys_get_block_size(void)
{
    return this->memory->block_size;
}

/**
 * @brief Get number of available blocks
 *
 * @return uint32_t
 */
uint32_t EiDeviceESP32::filesys_get_n_available_sample_blocks(void)
{
    return this->memory->get_available_sample_blocks();
}

/**
 * @brief      Checks for presense of b character to stop the inference
 *
 * @return     Returns true if b character is found, false otherwise
 */

bool ei_user_invoke_stop(void)
{
    bool stop_found = false;
	char ch = getchar();

    if (ch == 'b') {
        stop_found = true;
    }

    return stop_found;
}

/**
 * @brief      Get next available byte
 *
 * @return     byte
 */
char ei_get_serial_byte(void)
{
	char ch = getchar();
    // for some reason ESP32 only gets 10 (\n) and AT server has 13 (\r) as terminator character...
    if (ch == '\n') {
        ch = '\r';
    }
    
    return ch;
}

/**
 * @brief      Write character to serial
 *
 * @param      cChar     Char addr to write
 */
void ei_putc(char cChar)
{
    putchar(cChar);
}


char ei_getchar()
{
	char ch = getchar();
    // for some reason ESP32 only gets 10 (\n)and AT server has 13 (\r) as terminator character...

    if (ch == 255) {
        ch = 0;
    }

    if (ch == '\n') {
        ch = '\r';
    }
    
    return ch;

}

/* Private functions ------------------------------------------------------- */

void vTimerCallback(TimerHandle_t xTimer)
{
    sample_cb_ptr();
}