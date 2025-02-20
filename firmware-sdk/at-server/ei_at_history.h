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

#ifndef AT_HISTORY_H
#define AT_HISTORY_H
#include <string>
#include <vector>

class ATHistory {
private:
    std::vector<std::string> history;
    const size_t history_max_size;
    size_t history_position;

public:
    ATHistory(size_t max_size = 10)
        : history_max_size(max_size)
        , history_position(0) {};

    std::string go_back(void)
    {
        if (!is_at_begin()) {
            history_position--;
        }

        if (history.size() == 0) {
            return std::string("");
        }
        else {
            return history[history_position];
        }
    }

    std::string go_next(void)
    {
        if (++history_position >= history.size()) {
            history_position = history.size();
            return std::string("");
        }

        return history[history_position];
    }

    bool is_at_end(void)
    {
        return history_position == history.size();
    }

    bool is_at_begin(void)
    {
        return history_position == 0;
    }

    void add(std::string &entry)
    {
        // don't add empty entries
        if (entry == "") {
            return;
        }

        history.push_back(entry);

        if (history.size() > history_max_size) {
            history.erase(history.begin());
        }

        history_position = history.size();
    }
};

#endif /* AT_HISTORY_H */