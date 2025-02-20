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