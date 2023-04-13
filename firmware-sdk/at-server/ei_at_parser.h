/*
 * Copyright (c) 2022 Edge Impulse Inc.
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

#ifndef AT_PARSER_H
#define AT_PARSER_H
#include <string>
#include <vector>

enum ATCommandType_t
{
    AT_RUN,
    AT_READ,
    AT_WRITE,
    AT_UNKNOWN
};

typedef struct {
    ATCommandType_t type;
    std::string command;
    std::vector<std::string> arguments;
    unsigned int max_arg_len;
} ATParseResult_t;

class ATParser {
private:
    ATParseResult_t last_result;
    void init_result(void);

public:
    ATParser() {};
    ~ATParser() {};
    const ATParseResult_t &parse(std::string command);
};

#endif /* AT_PARSER_H */