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

#ifndef EI_DEVICE_INFO_LIB
#define EI_DEVICE_INFO_LIB

/* Include ----------------------------------------------------------------- */
#include "ei_camera_interface.h"
#include "ei_config_types.h"
#include "ei_device_memory.h"
#include "ei_fusion.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Available sensors to sample from on this board
typedef struct {
    // Name (e.g. 'Built-in accelerometer')
    const char *name;
    // Frequency list
    float frequencies[5];
    // Max. sample length in seconds (could be depending on the size of the flash chip)
    uint16_t max_sample_length_s;
    // Start sampling, this function should be blocking and is called when sampling commences
    // #ifdef __MBED__
    //     Callback<bool()> start_sampling_cb;
    // #else
    bool (*start_sampling_cb)();
    //#endif
} ei_device_sensor_t;

typedef struct {
    char str[32];
    int val;
} ei_device_data_output_baudrate_t;

typedef struct {
    bool has_snapshot;
    bool support_stream;
    std::string color_depth; /* allowed values: Grayscale, RGB */
    uint8_t resolutions_num;
    ei_device_snapshot_resolutions_t* resolutions;
} EiSnapshotProperties;

typedef ei_config_security_t EiWiFiSecurity;

typedef struct {
    char wifi_ssid[128];
    char wifi_password[128];
    EiWiFiSecurity wifi_security;
    float sample_interval_ms;
    uint32_t sample_length_ms;
    char sensor_label[64];
    char sample_label[128];
    char sample_hmac_key[33];
    char upload_host[128];
    char upload_path[128];
    char upload_api_key[128];
    char mgmt_url[128];
    uint32_t long_recording_length_ms;
    uint32_t long_recording_interval_ms;
    uint32_t magic;
} EiConfig;

typedef enum
{
    eiStateIdle = 0,
    eiStateErasingFlash,
    eiStateSampling,
    eiStateUploading,
    eiStateFinished

} EiState;

class EiDeviceInfo {
protected:
    // Wi-Fi should be board specific
    std::string wifi_ssid = "";
    std::string wifi_password = "";
    EiWiFiSecurity wifi_security = EI_SECURITY_NONE;

    std::string device_type = "Default type";
    std::string device_id = "01:02:03:04:05:06";
    std::string management_url = "path";

    std::string sample_hmac_key = "please-set-me";
    std::string sensor_label = "sensor";
    std::string sample_label = "test";
    float sample_interval_ms;
    uint32_t sample_length_ms;

    uint32_t long_recording_length_ms;
    uint32_t long_recording_interval_ms;

    std::string upload_host = "host";
    std::string upload_path = "path";
    std::string upload_api_key = "0123456789abcdef";

#if MULTI_FREQ_ENABLED == 1
    uint8_t fusioning;
    uint32_t sample_interval;
#endif

    EiDeviceMemory *memory;

public:
    EiDeviceInfo(void) {};
    ~EiDeviceInfo(void) {};
    static EiDeviceInfo *get_device(void);

    virtual bool save_config(void)
    {
        EiConfig *buf = (EiConfig *)ei_malloc(sizeof(EiConfig));
        if(buf == NULL) {
            return false;
        }

        memset(buf, 0, sizeof(EiConfig));

        strncpy(buf->wifi_ssid, wifi_ssid.c_str(), 128);
        strncpy(buf->wifi_password, wifi_password.c_str(), 128);
        buf->wifi_security = wifi_security;
        buf->sample_interval_ms = sample_interval_ms;
        buf->sample_length_ms = sample_length_ms;
        buf->long_recording_interval_ms = long_recording_interval_ms;
        buf->long_recording_length_ms = long_recording_length_ms;
        strncpy(buf->sample_label, sample_label.c_str(), 128);
        strncpy(buf->sample_hmac_key, sample_hmac_key.c_str(), 33);
        strncpy(buf->sensor_label, sensor_label.c_str(), 64);
        strncpy(buf->upload_host, upload_host.c_str(), 128);
        strncpy(buf->upload_path, upload_path.c_str(), 128);
        strncpy(buf->upload_api_key, upload_api_key.c_str(), 128);
        strncpy(buf->mgmt_url, management_url.c_str(), 128);
        buf->magic = 0xdeadbeef;

        bool ret = memory->save_config((uint8_t *)buf, sizeof(EiConfig));

        ei_free((void *)buf);

        return ret;
    }

    virtual void load_config(void)
    {
        EiConfig *buf = (EiConfig *)ei_malloc(sizeof(EiConfig));
        if(buf == NULL) {
            return;
        }

        memset(buf, 0, sizeof(EiConfig));
        memory->load_config((uint8_t *)buf, sizeof(EiConfig));

        if (buf->magic == 0xdeadbeef) {
            wifi_ssid = std::string(buf->wifi_ssid, 128);
            wifi_password = std::string(buf->wifi_password, 128);
            wifi_security = buf->wifi_security;
            sample_interval_ms = buf->sample_interval_ms;
            sample_length_ms = buf->sample_length_ms;
            sample_label = std::string(buf->sample_label, 128);
            sample_hmac_key = std::string(buf->sample_hmac_key, 33);
            upload_host = std::string(buf->upload_host, 128);
            upload_path = std::string(buf->upload_path, 128);
            upload_api_key = std::string(buf->upload_api_key, 128);
            management_url = std::string(buf->mgmt_url, 128);
            sensor_label = std::string(buf->sensor_label, 64);
            long_recording_interval_ms = buf->long_recording_interval_ms;
            long_recording_length_ms = buf->long_recording_length_ms;
        }

        ei_free((void *)buf);
    }

    /**
     * @brief This method should init device_id field
     * to any unique ID available on the MCU.
     * It may be MAC address, CPU ID or similar value.
     */
    virtual void init_device_id(void) = 0;

    EiDeviceMemory *get_memory(void)
    {
        return memory;
    }

    virtual const std::string& get_device_type(void)
    {
        return device_type;
    }

    virtual const std::string& get_device_id(void)
    {
        return device_id;
    }

    virtual void set_device_id(std::string id, bool save = true)
    {
        device_id = id;

        if(save) {
            save_config();
        }
    }

    virtual const std::string& get_management_url(void)
    {
        return management_url;
    }

    virtual void set_management_url(std::string mgmt_url, bool save = true)
    {
        management_url = mgmt_url;

        if(save) {
            save_config();
        }
    }

    virtual const std::string& get_sample_hmac_key(void)
    {
        return sample_hmac_key;
    }

    virtual void set_sample_hmac_key(std::string hmac_key, bool save = true)
    {
        sample_hmac_key = hmac_key;

        if(save) {
            save_config();
        }
    }

    virtual const std::string& get_sensor_label(void)
    {
        return sensor_label;
    }

    virtual void set_sensor_label(std::string label, bool save = true)
    {
        sensor_label = label;

        if(save) {
            save_config();
        }
    }

    virtual const std::string& get_sample_label(void)
    {
        return sample_label;
    }

    virtual void set_sample_label(std::string label, bool save = true)
    {
        sample_label = label;

        if(save) {
            save_config();
        }
    }

    virtual float get_sample_interval_ms(void)
    {
        return sample_interval_ms;
    }

    virtual void set_sample_interval_ms(float interval_ms, bool save = true)
    {
        sample_interval_ms = interval_ms;

        if(save) {
            save_config();
        }
    }

    virtual uint32_t get_sample_length_ms(void)
    {
        return sample_length_ms;
    }

    virtual void set_sample_length_ms(uint32_t length_ms, bool save = true)
    {
        sample_length_ms = length_ms;

        if(save) {
            save_config();
        }
    }

    virtual uint32_t get_long_recording_length_ms(void)
    {
        return long_recording_length_ms;
    }

    virtual void set_long_recording_length_ms(uint32_t length_ms, bool save = true)
    {
        long_recording_length_ms = length_ms;

        if(save) {
            save_config();
        }
    }

    virtual uint32_t get_long_recording_interval_ms(void)
    {
        return long_recording_interval_ms;
    }

    virtual void set_long_recording_interval_ms(uint32_t interval_ms, bool save = true)
    {
        long_recording_interval_ms = interval_ms;

        if(save) {
            save_config();
        }
    }

    virtual const std::string& get_upload_host(void)
    {
        return upload_host;
    }

    virtual void set_upload_host(std::string host, bool save = true)
    {
        upload_host = host;

        if(save) {
            save_config();
        }
    }

    virtual const std::string& get_upload_path(void)
    {
        return upload_path;
    }

    virtual void set_upload_path(std::string path, bool save = true)
    {
        upload_path = path;

        if(save) {
            save_config();
        }
    }

    virtual const std::string& get_upload_api_key(void)
    {
        return upload_api_key;
    }

    virtual void set_upload_api_key(std::string upload_api_key, bool save = true)
    {
        this->upload_api_key = upload_api_key;

        if(save) {
            save_config();
        }
    }

    virtual bool get_wifi_connection_status(void)
    {
        return false;
    }

    virtual void set_wifi_config(std::string ssid, std::string password, EiWiFiSecurity security, bool save = true)
    {
        wifi_ssid = ssid;
        wifi_password = password;
        wifi_security = security;

        if(save) {
            save_config();
        }
    }

    virtual void get_wifi_config(std::string& ssid, std::string& password, EiWiFiSecurity* security)
    {
        ssid = wifi_ssid;
        password = wifi_password;
        *security = wifi_security;
    }

    virtual void clear_config(void)
    {
        wifi_ssid = "";
        wifi_password = "";
        wifi_security = EI_SECURITY_NONE;
        device_id = "11:22:33:44:55:66";
        management_url = "";
        sample_hmac_key = "";
        sensor_label = "";
        sample_label = "";
        sample_interval_ms = 0;
        sample_length_ms = 0;
        upload_host = "";
        upload_path = "";
        upload_api_key = "";
        long_recording_length_ms = 0;
        long_recording_interval_ms = 0;

        this->init_device_id();
    }

    virtual bool get_wifi_present_status(void)
    {
        return false;
    }

    /**
	 * @brief      Get pointer to the list of available sensors, and the number of sensors
	 *             used
	 * @param      sensor_list       Place pointer to sensor list here
	 * @param      sensor_list_size  Fill in the number of sensors in the list
	 *
	 * @return     The sensor list.
	 */
    virtual bool get_sensor_list(const ei_device_sensor_t **sensor_list, size_t *sensor_list_size)
    {
        *sensor_list = NULL;
        *sensor_list_size = 0;
        return true;
    }

    /**
	 * @brief      Create resolution list for snapshot setting
	 *             The studio and daemon require this list
	 *
	 * @return     EiSnapshotProperties
	 */
    virtual EiSnapshotProperties get_snapshot_list()
    {
        EiSnapshotProperties props;
        return props;
    }

    virtual uint32_t get_data_output_baudrate(void)
    {
        return 115200;
    }

    virtual void set_default_data_output_baudrate(void)
    {
    }

    virtual void set_max_data_output_baudrate(void)
    {
    }

    virtual bool start_sample_thread(void (*sample_read_cb)(void), float sample_interval_ms)
    {
        return false;
    }

    virtual bool stop_sample_thread(void)
    {
        return false;
    }

#if MULTI_FREQ_ENABLED == 1
	uint32_t actual_timer;
    std::vector<float> multi_sample_interval;
    void (*sample_multi_read_callback)(uint8_t);

    virtual bool start_multi_sample_thread(void (*sample_multi_read_cb)(uint8_t), float* fusion_sample_interval_ms, uint8_t num_fusioned)
    {
        uint8_t i;
        uint8_t flag = 0;

        this->sample_multi_read_callback = sample_multi_read_cb;
        this->fusioning = num_fusioned;
        this->multi_sample_interval.clear();

        for (i = 0; i < num_fusioned; i++){
            this->multi_sample_interval.push_back(fusion_sample_interval_ms[i]);
        }

        /* to improve, we consider just a 2 sensors case for now */
        this->sample_interval = ei_fusion_calc_multi_gcd(this->multi_sample_interval.data(), this->fusioning);

        /* force first reading */
        for (i = 0; i < this->fusioning; i++){
                flag |= (1<<i);
        }
        this->sample_multi_read_callback(flag);

        this->actual_timer = 0;
        /*
        * TODO
        * start timer/thread
        */

        return false;
    }

    virtual uint8_t get_fusioning(void)
    {
        return fusioning;
    }

    virtual uint32_t get_sample_interval(void)
    {
        return sample_interval;
    }

#endif

    virtual void set_state(EiState)
    {
    }

    // ******* DEPRECATED BELOW HERE *********
    /**
     * @brief      Get byte size of memory block
     *
     * @return     uint32_t size in bytes
     */
    virtual uint32_t filesys_get_block_size(void)
    {
        return 0;
    }

    /**
     * @brief      Get number of available blocks
     *
     * @return     uint32_t
     */
    virtual uint32_t filesys_get_n_available_sample_blocks(void)
    {
        return 0;
    }

    static constexpr int STR_SIZE = 32;
    /**
	 * @brief      Gets the device ID string
     * Deprecated.  C strings are unsafe.
     * Get a copy of string from std::string get_id(), and call str() on that.
	 *
	 * @param      out_buffer  Destination buffer for ID
	 * @param      out_size    Length of ID in bytes
	 *
	 * @return     Zero if ok, non-zero to signal an error
	 */
    virtual int get_id(uint8_t out_buffer[STR_SIZE], size_t *out_size)
    {
        *out_size = device_id.copy((char *)out_buffer, STR_SIZE - 1);
        out_buffer[*out_size] = 0; // Null terminate
        return 0;
    };

    /**
	 * @brief      Get pointer to zero terminatied id string
     * Deprecated.  C strings are unsafe.
     * Get a copy of string from get_id, and call str() on that.
	 *
	 * @return     The id pointer.
	 */
    virtual const char *get_id_pointer(void)
    {
        return device_id.c_str();
    }

    /**
	 * @brief      Gets the device type string
     * Deprecated.  C strings are unsafe.
     * Get a copy of string from std::string get_id(), and call str() on that.
	 * @param      out_buffer  Destination buffer for type
	 * @param      out_size    Length of type string in bytes
	 *
	 * @return     Zero if ok, non-zero to signal an error
	 */
    virtual int get_type(uint8_t out_buffer[STR_SIZE], size_t *out_size)
    {
        *out_size = device_type.copy((char*)out_buffer, STR_SIZE - 1);
        out_buffer[*out_size] = 0; // Null terminate
        return 0;
    }

    /**
	 * @brief      Get pointer to zero terminatied type string
     * Deprecated.  C strings are unsafe.
     * Get a copy of string from std::string get_id(), and call str() on that.
	 * @return     The type pointer.
	 */
    virtual const char *get_type_pointer(void)
    {
        return device_type.c_str();
    }
};

#endif /* EI_DEVICE_INFO_LIB */
