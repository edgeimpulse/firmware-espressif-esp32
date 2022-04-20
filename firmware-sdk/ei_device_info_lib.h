/* Edge Impulse firmware SDK
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

#ifndef EI_DEVICE_INFO_LIB
#define EI_DEVICE_INFO_LIB

/* Include ----------------------------------------------------------------- */
#include "ei_camera_interface.h"
#include "ei_config_types.h"
#include "ei_device_memory.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

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
    char sample_label[128];
    char sample_hmac_key[33];
    char upload_host[128];
    char upload_path[128];
    char upload_api_key[128];
    char mgmt_url[128];
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
    std::string sample_label = "test";
    float sample_interval_ms;
    uint32_t sample_length_ms;

    std::string upload_host = "host";
    std::string upload_path = "path";
    std::string upload_api_key = "0123456789abcdef";

    EiDeviceMemory *memory;

public:
    EiDeviceInfo(void) {};
    ~EiDeviceInfo(void) {};
    static EiDeviceInfo *get_device(void);

    virtual bool save_config(void)
    {
        EiConfig buf;

        memset(&buf, 0, sizeof(EiConfig));

        strncpy(buf.wifi_ssid, wifi_ssid.c_str(), 128);
        strncpy(buf.wifi_password, wifi_password.c_str(), 128);
        buf.wifi_security = wifi_security;
        buf.sample_interval_ms = sample_interval_ms;
        buf.sample_length_ms = sample_length_ms;
        strncpy(buf.sample_label, sample_label.c_str(), 128);
        strncpy(buf.sample_hmac_key, sample_hmac_key.c_str(), 33);
        strncpy(buf.upload_host, upload_host.c_str(), 128);
        strncpy(buf.upload_path, upload_path.c_str(), 128);
        strncpy(buf.upload_api_key, upload_api_key.c_str(), 128);
        strncpy(buf.mgmt_url, management_url.c_str(), 128);
        buf.magic = 0xdeadbeef;

        memory->save_config((uint8_t *)&buf, sizeof(EiConfig));

        return true;
    }

    virtual void load_config(void)
    {
        EiConfig buf;

        memset(&buf, 0, sizeof(EiConfig));
        memory->load_config((uint8_t *)&buf, sizeof(EiConfig));

        if (buf.magic == 0xdeadbeef) {
            wifi_ssid = std::string(buf.wifi_ssid, 128);
            wifi_password = std::string(buf.wifi_password, 128);
            wifi_security = buf.wifi_security;
            sample_interval_ms = buf.sample_interval_ms;
            sample_length_ms = buf.sample_length_ms;
            sample_label = std::string(buf.sample_label, 128);
            sample_hmac_key = std::string(buf.sample_hmac_key, 33);
            upload_host = std::string(buf.upload_host, 128);
            upload_path = std::string(buf.upload_path, 128);
            upload_api_key = std::string(buf.upload_api_key, 128);
            management_url = std::string(buf.mgmt_url, 128);
        }
    }

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

    virtual void set_device_id(std::string id)
    {
        device_id = id;

        save_config();
    }

    virtual const std::string& get_management_url(void)
    {
        return management_url;
    }

    virtual void set_management_url(std::string mgmt_url)
    {
        management_url = mgmt_url;

        save_config();
    }

    virtual const std::string& get_sample_hmac_key(void)
    {
        return sample_hmac_key;
    }

    virtual void set_sample_hmac_key(std::string hmac_key)
    {
        sample_hmac_key = hmac_key;

        save_config();
    }

    virtual const std::string& get_sample_label(void)
    {
        return sample_label;
    }

    virtual void set_sample_label(std::string label)
    {
        sample_label = label;

        save_config();
    }

    virtual float get_sample_interval_ms(void)
    {
        return sample_interval_ms;
    }

    virtual void set_sample_interval_ms(float interval_ms)
    {
        sample_interval_ms = interval_ms;

        save_config();
    }

    virtual uint32_t get_sample_length_ms(void)
    {
        return sample_length_ms;
    }

    virtual void set_sample_length_ms(uint32_t length_ms)
    {
        sample_length_ms = length_ms;

        save_config();
    }

    virtual const std::string& get_upload_host(void)
    {
        return upload_host;
    }

    virtual void set_upload_host(std::string host)
    {
        upload_host = host;

        save_config();
    }

    virtual const std::string& get_upload_path(void)
    {
        return upload_path;
    }

    virtual void set_upload_path(std::string path)
    {
        upload_path = path;

        save_config();
    }

    virtual const std::string& get_upload_api_key(void)
    {
        return upload_api_key;
    }

    virtual void set_upload_api_key(std::string upload_api_key)
    {
        this->upload_api_key = upload_api_key;

        save_config();
    }

    virtual bool get_wifi_connection_status(void)
    {
        return false;
    }

    virtual void clear_config(void)
    {
        device_id = "";
        management_url = "";
        sample_hmac_key = "";
        sample_label = "";
        sample_interval_ms = 0;
        sample_length_ms = 0;
        upload_host = "";
        upload_path = "";
        upload_api_key = "";
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
	 * @param      snapshot_list       Place pointer to resolution list
	 * @param      snapshot_list_size  Write number of resolutions here
	 *
	 * @return     False if all went ok
	 */
    virtual bool get_snapshot_list(
        const ei_device_snapshot_resolutions_t **snapshot_list,
        size_t *snapshot_list_size,
        const char **color_depth)
    {
        return true;
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

    virtual void set_state(EiState) {};

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
