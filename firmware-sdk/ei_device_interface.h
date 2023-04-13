/*
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EI_DEVICE_INTERFACE_H
#define EI_DEVICE_INTERFACE_H

/* Function prototypes ----------------------------------------------------- */
//TODO: remove as it is device specific and wil be superseded by AT Server
void ei_command_line_handle(void);
//TODO: redeclared in ei_device_lib.h
bool ei_user_invoke_stop_lib(void);
//TODO: do we need it in the FW SDK?
void ei_serial_setup(void);

//TODO: remove as it is device specific
void ei_write_string(char *data, int length);

//TODO: move to a one header with all method requied by FW SDK
char ei_getchar();


#endif /* EI_DEVICE_INTERFACE_H */
