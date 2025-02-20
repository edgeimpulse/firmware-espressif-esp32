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

/* Include ----------------------------------------------------------------- */
#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "ei_at_command_set.h"
#include <vector>
#include <string>

bool at_info(void)
{
    std::vector<std::string> sensors = {"unknown", "microphone", "accelerometer", "camera", "9DoF", "environmental", "fusion"};

    ei_printf("*************************\n");
    ei_printf("* Edge Impulse firmware *\n");
    ei_printf("*************************\n");
    ei_printf("Firmware build date  : " __DATE__ "\n");
    ei_printf("Firmware build time  : " __TIME__ "\n");
    ei_printf("ML model author      : " EI_CLASSIFIER_PROJECT_OWNER "\n");
    ei_printf("ML model name        : " EI_CLASSIFIER_PROJECT_NAME "\n");
    ei_printf("ML model ID          : %d\n", EI_CLASSIFIER_PROJECT_ID);
    ei_printf("Model deploy version : %d\n", EI_CLASSIFIER_PROJECT_DEPLOY_VERSION);
    ei_printf("Edge Impulse version : v%d.%d.%d\n", EI_STUDIO_VERSION_MAJOR, EI_STUDIO_VERSION_MINOR, EI_STUDIO_VERSION_PATCH);
    ei_printf("Used sensor          : %s\n", sensors[EI_CLASSIFIER_SENSOR].c_str());

    return true;
}
