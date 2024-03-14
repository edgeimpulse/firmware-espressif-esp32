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
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_idf_version.h"

#include <stdio.h>

#include "ei_device_espressif_esp32.h"

#include "ei_at_handlers.h"
#include "ei_classifier_porting.h"
#include "ei_run_impulse.h"

#include "ei_analogsensor.h"
#include "ei_inertial_sensor.h"

#define RED_LED_PIN GPIO_NUM_21
#define WHITE_LED_PIN GPIO_NUM_22

EiDeviceInfo *EiDevInfo = dynamic_cast<EiDeviceInfo *>(EiDeviceESP32::get_device());
static ATServer *at;

/* Private variables ------------------------------------------------------- */

/* Public functions -------------------------------------------------------- */

void setup_led() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_rom_gpio_pad_select_gpio(RED_LED_PIN);
    esp_rom_gpio_pad_select_gpio(WHITE_LED_PIN);
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    gpio_pad_select_gpio(RED_LED_PIN);
    gpio_pad_select_gpio(WHITE_LED_PIN);
#endif
    gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(WHITE_LED_PIN, GPIO_MODE_OUTPUT);
}

extern "C" int app_main()
{
    setup_led();

    /* Initialize Edge Impulse sensors and commands */

    EiDeviceESP32* dev = static_cast<EiDeviceESP32*>(EiDeviceESP32::get_device());

    ei_printf(
        "Hello from Edge Impulse Device SDK.\r\n"
        "Compiled on %s %s\r\n",
        __DATE__,
        __TIME__);

    /* Setup the inertial sensor */
    if (ei_inertial_init() == false) {
        ei_printf("Inertial sensor initialization failed\r\n");
    }

    /* Setup the analog sensor */
    if (ei_analog_sensor_init() == false) {
        ei_printf("ADC sensor initialization failed\r\n");
    }

    at = ei_at_init(dev);
    ei_printf("Type AT+HELP to see a list of commands.\r\n");
    at->print_prompt();

    dev->set_state(eiStateFinished);

    while(1){
        /* handle command comming from uart */
        char data = ei_get_serial_byte();

        while (data != 0xFF) {
            at->handle(data);
            data = ei_get_serial_byte();
        }
    }
}