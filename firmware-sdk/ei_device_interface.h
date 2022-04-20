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
void ei_putc(char cChar);
//TODO: move to a one header with all method requied by FW SDK
char ei_getchar();


#endif /* EI_DEVICE_INTERFACE_H */
