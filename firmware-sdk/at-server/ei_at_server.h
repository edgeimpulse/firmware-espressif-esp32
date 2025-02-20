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

#ifndef AT_SERVER_H
#define AT_SERVER_H
#include "ei_at_history.h"
#include "ei_at_parser.h"
#include "ei_line_buffer.h"
#include <functional>
#include <string>
#include <vector>

typedef std::function<bool(void)> ATRunHandler_t;
typedef std::function<bool(void)> ATReadHandler_t;
typedef std::function<bool(const char **, const int)> ATWriteHandler_t;

const size_t default_history_size = 10;

typedef struct {
    std::string command;
    std::string help_text;
    ATRunHandler_t run_handler;
    ATReadHandler_t read_handler;
    ATWriteHandler_t write_handler;
    std::string write_handler_args_list;
} ATCommand_t;

class ATServer {
private:
    ATHistory history;
    std::vector<ATCommand_t> registered_commands;
    LineBuffer buffer;
    ATParser parser;
    void register_default_commands(void);

protected:
    ATServer();
    ATServer(ATCommand_t *commands, size_t length, size_t max_history_size = default_history_size);
    ~ATServer();
    bool print_help(void);
    bool execute(std::string &command);

public:
    ATServer(ATServer &other) = delete;
    void operator=(const ATServer &) = delete;

    /* Definition of get_instance methods are in a separate file (ATServerSingleton.cpp)
     * See comment over there.
     */
    static ATServer *get_instance();
    static ATServer *get_instance(
        ATCommand_t *commands,
        size_t length,
        size_t max_history_size = default_history_size);

    void handle(char c);
    void print_prompt(void);

    bool register_command(ATCommand_t &command);
    bool register_command(
        const char *cmd,
        const char *help_text,
        bool (*run_handler)(void),
        bool (*read_handler)(void),
        bool (*write_handler)(const char **, const int),
        const char *write_handler_args_list);
    bool register_handlers(
        const char *cmd,
        bool (*run_handler)(void),
        bool (*read_handler)(void),
        bool (*write_handler)(const char **, const int),
        const char *write_handler_args_list);
};

#endif /* AT_SERVER_H */