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

#include "ei_at_parser.h"

using namespace std;

void ATParser::init_result(void)
{
    last_result.type = AT_UNKNOWN;
    last_result.command.clear();
    last_result.arguments.clear();
    last_result.max_arg_len = 0;
}

const ATParseResult_t &ATParser::parse(string input)
{
    string tmp;
    size_t pos = string::npos;
    size_t delim1 = string::npos;
    size_t delim2 = string::npos;

    this->init_result();

    if (input.size() == 0) {
        last_result.type = AT_UNKNOWN;
        return last_result;
    }

    // trim leading whitespaces
    input = input.substr(input.find_first_not_of(" \t"));

    if (input.rfind("AT+", 0) != 0) {
        last_result.type = AT_UNKNOWN;
        return last_result;
    }

    //remove "AT+"
    input = input.substr(3);

    // trim spaces, newline and CR at the end
    input = input.erase(input.find_last_not_of(" \r\n") + 1);

    // extract command itself
    pos = input.find_first_of("?=");
    last_result.command = input.substr(0, pos);

    if (pos == string::npos) {
        last_result.type = AT_RUN;
    }
    else if (input.at(pos) == '=') {
        last_result.type = AT_WRITE;
    }
    else if (input.at(pos) == '?') {
        last_result.type = AT_READ;
    }
    else {
        last_result.type = AT_UNKNOWN;
    }

    // check if command has arguments and extract them
    if (pos != string::npos && input.at(pos) == '=') {
        delim1 = pos;
        delim2 = input.find_first_of(",", pos);
        // we have arguments, let's extract them
        while (delim2 != string::npos) {
            //TODO: support args in a quote
            tmp = string(input, delim1 + 1, delim2 - delim1 - 1);
            last_result.arguments.push_back(tmp);
            if (tmp.size() > last_result.max_arg_len) {
                last_result.max_arg_len = tmp.size();
            }
            delim1 = delim2;
            delim2 = input.find_first_of(",", delim1 + 1);
        }
        //there is one more argument, behind last comma so, get it until end of command
        tmp = string(input, delim1 + 1);
        last_result.arguments.push_back(tmp);
        if (tmp.size() > last_result.max_arg_len) {
            last_result.max_arg_len = tmp.size();
        }
    }

    return last_result;
}
