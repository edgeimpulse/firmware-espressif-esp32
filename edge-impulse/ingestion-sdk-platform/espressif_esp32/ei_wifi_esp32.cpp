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

/*
#include "firmware-sdk/ei_device_interface.h"

#include "ei_wifi_esp32.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_log.h"

#define DEFAULT_SCAN_LIST_SIZE 10

static const char *TAG = "NetworkDriver";

EiWifiESP32::EiWifiESP32()
{

}

int8_t EiWifiESP32::init() {

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);

    return ret;
}

int8_t EiWifiESP32::deinit() {

    esp_err_t ret = esp_wifi_deinit();

    return ret;
}

int8_t EiWifiESP32::scan_networks(ei_device_network_list_t **networks, uint16_t *network_num) {

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];

    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);

    ret = esp_wifi_start();
    ret = esp_wifi_scan_start(NULL, true);

    ret = esp_wifi_scan_get_ap_records(&number, ap_info);
    ret = esp_wifi_scan_get_ap_num(network_num);
    ESP_LOGI(TAG, "Total APs scanned = %u", *network_num);

    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < *network_num); i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        networks[i]->wifi_ssid = ap_info[i].ssid;
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        networks[i]->signal_strength = ap_info[i].rssi;

        ESP_LOGI(TAG, "Auth mode \t\t%d", ap_info[i].authmode);
        //networks[i]->wifi_security = ap_info[i].authmode;

        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }

    return ret;
}

int8_t EiWifiESP32::connect_network(ei_config_t *config) {

    return EI_CONFIG_OK;
}

int8_t EiWifiESP32::disconnect_network(void) {

    return EI_CONFIG_OK;
}

int8_t EiWifiESP32::send_sample(ei_config_t *config) {

    return EI_CONFIG_OK;
}


EiNetworkDevice *EiNetworkDevice::get_network_device()
{
    static EiWifiESP32 net;
    return &net;
}
*/