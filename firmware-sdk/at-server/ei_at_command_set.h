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

#ifndef AT_COMMAND_SET_H
#define AT_COMMAND_SET_H

/* Edge Impulse CLI is supporting AT Command set version up to currently locked major/minor version.
 * If you are upgrading the major/minor here without upgrading CLI tools, they won't work with the changed firmware.
 *
 * If you are adding or modifying REQUIRED commands,
 * upgrade the minor or major version AND update the CLI tools.
 *
 * If you are adding or modifying OPTIONAL commands,
 * just upgrade the release version.
 */
#define AT_COMMAND_VERSION "1.8.0"

/*************************************************************************************************/
/* Required commands by Edge Impulse CLI Tools        */
/* Any changes here requires upgrade of the CLI tools */
#define AT_CLEARCONFIG               "CLEARCONFIG"
#define AT_CLEARCONFIG_HELP_TEXT     "Clears complete config and resets system"
#define AT_CONFIG                    "CONFIG"
#define AT_CONFIG_HELP_TEXT          "Lists complete config"
#define AT_DEVICEID                  "DEVICEID"
#define AT_DEVICEID_ARGS             "DEVICEID"
#define AT_DEVICEID_HELP_TEXT        "Sets the device ID"
#define AT_SAMPLESETTINGS            "SAMPLESETTINGS"
#define AT_SAMPLESETTINGS_ARGS       "LABEL,INTERVAL_MS,LENGTH_MS,[HMAC_KEY]"
#define AT_SAMPLESETTINGS_HELP_TEXT  "Lists or sets current sampling settings"
#define AT_UPLOADSETTINGS            "UPLOADSETTINGS"
#define AT_UPLOADSETTINGS_ARGS       "APIKEY,PATH"
#define AT_UPLOADSETTINGS_HELP_TEXT  "Lists or sets current upload settings"
#define AT_UPLOADHOST                "UPLOADHOST"
#define AT_UPLOADHOST_ARGS           "HOST"
#define AT_UPLOADHOST_HELP_TEXT      "Sets upload host"
#define AT_MGMTSETTINGS              "MGMTSETTINGS"
#define AT_MGMTSETTINGS_ARGS         "URL"
#define AT_MGMTSETTINGS_HELP_TEXT    "Lists or sets current management settings"
#define AT_READFILE                  "READFILE"
#define AT_READFILE_ARGS             "FILENAME,[USEMAXRATE]"
#define AT_READFILE_HELP_TEXT        "Read a specific file (as base64)"
#define AT_READBUFFER                "READBUFFER"
#define AT_READBUFFER_ARGS           "START,LENGTH,[USEMAXRATE]"
#define AT_READBUFFER_HELP_TEXT      "Read from the temporary buffer (as base64)"
#define AT_UNLINKFILE                "UNLINKFILE"
#define AT_UNLINKFILE_ARGS           "FILE"
#define AT_UNLINKFILE_HELP_TEXT      "Unlink a specific file"
#define AT_SAMPLESTART               "SAMPLESTART"
#define AT_SAMPLESTART_ARGS          "SENSOR_NAME"
#define AT_SAMPLESTART_HELP_TEXT     "Start sampling"
#define AT_RUNIMPULSE                "RUNIMPULSE"
#define AT_RUNIMPULSE_HELP_TEXT      "Run the impulse"
#define AT_RUNIMPULSEDEBUG           "RUNIMPULSEDEBUG"
#define AT_RUNIMPULSEDEBUG_ARGS      "USEMAXRATE"
#define AT_RUNIMPULSEDEBUG_HELP_TEXT "Run the impulse with additional debug output or live preview"
#define AT_RUNIMPULSECONT            "RUNIMPULSECONT"
#define AT_RUNIMPULSECONT_HELP_TEXT  "Run the impulse continuously"
#define AT_RUNIMPULSESTATIC          "RUNIMPULSESTATIC"
#define AT_RUNIMPULSESTATIC_ARGS     "DEBUG,LENGTH"
#define AT_RUNIMPULSESTATIC_HELP_TEXT "Run the impulse on static data (base64 encoded)"
#define AT_INGESTIONCYCLESETTINGS            "INGESTIONCYCLESETTINGS"
#define AT_INGESTIONCYCLESETTINGS_ARGS       "SENSOR_LABEL,TOTAL_INGESTION_TIME_MS,INTERVAL_TIME_MS"
#define AT_INGESTIONCYCLESETTINGS_HELP_TEXT  "Set ingestion cycle settings"
#define AT_STARTINGESTIONCYCLE               "STARTINGESTIONCYCLE"
#define AT_STARTINGESTIONCYCLE_HELP_TEXT     "Start ingestion cycle and stores sample on sd"

/*************************************************************************************************/
/* Optional commands (not required by Edge Impulse CLI Tools) */
#define AT_WIFI                     "WIFI"
#define AT_WIFI_ARGS                "SSID,PASSWORD,SECURITY"
#define AT_WIFI_HELP_TEXT           "Lists or sets WiFi credentials"
#define AT_SCANWIFI                 "SCANWIFI"
#define AT_SCANWIFI_HELP_TEXT       "Scans for WiFi networks"
#define AT_SNAPSHOT                 "SNAPSHOT"
#define AT_SNAPSHOT_ARGS            "WIDTH,HEIGHT,[USEMAXRATE]"
#define AT_SNAPSHOT_HELP_TEXT       "Take a snapshot"
#define AT_SNAPSHOTSTREAM           "SNAPSHOTSTREAM"
#define AT_SNAPSHOTSTREAM_ARGS      "WIDTH,HEIGHT,[USEMAXRATE]"
#define AT_SNAPSHOTSTREAM_HELP_TEXT "Take a stream of snapshot stream"
#define AT_CLEARFILES               "CLEARFILES"
#define AT_CLEARFILES_HELP_TEXT     "Clears all files from the file system, this does not clear config"
#define AT_DEVICEINFO               "DEVICEINFO"
#define AT_DEVICEINFO_HELP_TEXT     "Lists device information"
#define AT_SENSORS                  "SENSORS"
#define AT_SENSORS_HELP_TEXT        "Lists sensors"
#define AT_RESET                    "RESET"
#define AT_RESET_HELP_TEXT          "Reset the system"
#define AT_LISTFILES                "LISTFILES"
#define AT_LISTFILES_HELP_TEXT      "Lists all files on the device"
#define AT_READRAW                  "READRAW"
#define AT_READRAW_ARS              "START,LENGTH"
#define AT_READRAW_HELP_TEXT        "Read raw from flash"
#define AT_BOOTMODE                 "BOOTMODE"
#define AT_BOOTMODE_HELP_TEXT       "Jump to bootloader"
#define AT_INFO                     "INFO"
#define AT_INFO_HELP_TEXT           "Prints details about compiled firmware and ML model"

/*************************************************************************************************/
/* HELP is not necessary as it is built-in into ATServer and
   any custom implementation is ignored. For documentation purposes only */
#define AT_HELP           "HELP"
#define AT_HELP_HELP_TEXT "Lists all commands"

/*************************************************************************************************/
/* Common AT commands handlers */

bool at_info(void);

#endif /* AT_COMMAND_SET_H */
