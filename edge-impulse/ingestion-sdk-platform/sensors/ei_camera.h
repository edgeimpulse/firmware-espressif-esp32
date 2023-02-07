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

#ifndef EI_CAMERA
#define EI_CAMERA

/* Include ----------------------------------------------------------------- */
#include "firmware-sdk/ei_camera_interface.h"

#define CAMERA_MODEL_TIMERCAM

/*
 *   Pin definitions for some common ESP-CAM modules
 *
 *   Defaults to ESP-EYE
 *
 */
#if defined(CAMERA_MODEL_AI_THINKER)
  //
  // AI Thinker
  // https://github.com/SeeedDocument/forum_doc/raw/master/reg/ESP32_CAM_V1.6.pdf
  //
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
  #define LED_PIN           33 // Status led
  #define LED_ON           LOW // - Pin is inverted.
  #define LED_OFF         HIGH //
  #define LAMP_PIN           4 // LED FloodLamp.

#elif defined(CAMERA_MODEL_ESP_EYE)
  // ESP-EYE
  // https://twitter.com/esp32net/status/1085488403460882437
  //
  #define PWDN_GPIO_NUM    -1
  #define RESET_GPIO_NUM   -1
  #define XCLK_GPIO_NUM     4
  #define SIOD_GPIO_NUM    18
  #define SIOC_GPIO_NUM    23
  #define Y9_GPIO_NUM      36
  #define Y8_GPIO_NUM      37
  #define Y7_GPIO_NUM      38
  #define Y6_GPIO_NUM      39
  #define Y5_GPIO_NUM      35
  #define Y4_GPIO_NUM      14
  #define Y3_GPIO_NUM      13
  #define Y2_GPIO_NUM      34
  #define VSYNC_GPIO_NUM    5
  #define HREF_GPIO_NUM    27
  #define PCLK_GPIO_NUM    25

  #define V_FLIP 1


#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
  //
  // ESP32 M5STACK
  //
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
  // M5 Stack status/illumination LED details unknown/unclear
  // #define LED_PIN            x // Status led
  // #define LED_ON          HIGH //
  // #define LED_OFF          LOW //
  // #define LAMP_PIN          x  // LED FloodLamp.

  #elif defined(CAMERA_MODEL_TIMERCAM)
  //
  // ESP32 M5STACK
  //
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
  // M5 Stack status/illumination LED details unknown/unclear
  // #define LED_PIN            x // Status led
  // #define LED_ON          HIGH //
  // #define LED_OFF          LOW //
  // #define LAMP_PIN          x  // LED FloodLamp.

#elif defined(CAMERA_MODEL_M5STACK_V2_PSRAM)
  //
  // ESP32 M5STACK V2
  //
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     22
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
  // M5 Stack status/illumination LED details unknown/unclear
  // #define LED_PIN            x // Status led
  // #define LED_ON          HIGH //
  // #define LED_OFF          LOW //
  // #define LAMP_PIN          x  // LED FloodLamp.

#elif defined(CAMERA_MODEL_M5STACK_WIDE)
  //
  // ESP32 M5STACK WIDE
  //
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     22
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
  // M5 Stack status/illumination LED details unknown/unclear
  // #define LED_PIN            x // Status led
  // #define LED_ON          HIGH //
  // #define LED_OFF          LOW //
  // #define LAMP_PIN          x  // LED FloodLamp.

#elif defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  //
  // Common M5 Stack without PSRAM
  //
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       17
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21
  // Note NO PSRAM,; so maximum working resolution is XGA 1024×768
  // M5 Stack status/illumination LED details unknown/unclear
  // #define LED_PIN            x // Status led
  // #define LED_ON          HIGH //
  // #define LED_OFF          LOW //
  // #define LAMP_PIN          x  // LED FloodLamp.

#else
  // Well.
  // that went badly...
  #error "Camera model not selected, did you forget to uncomment it in myconfig?"

#endif

class EiCameraESP32 : public EiCamera {
private:

    static ei_device_snapshot_resolutions_t resolutions[];

    uint32_t width;
    uint32_t height;
    uint32_t output_width;
    uint32_t output_height;

    bool camera_present;

public:
    EiCameraESP32();
    bool init(uint16_t width, uint16_t height);
    bool deinit();
    bool ei_camera_capture_jpeg(uint8_t **image, uint32_t *image_size);
    bool ei_camera_capture_rgb888_packed_big_endian(uint8_t *image, uint32_t image_size);
    bool ei_camera_jpeg_to_rgb888(uint8_t *jpeg_image, uint32_t jpeg_image_size,
                                  uint8_t *rgb88_image);
    bool set_resolution(const ei_device_snapshot_resolutions_t res);
    ei_device_snapshot_resolutions_t get_min_resolution(void);
    bool is_camera_present(void);
    void get_resolutions(ei_device_snapshot_resolutions_t **res, uint8_t *res_num);
};

#endif