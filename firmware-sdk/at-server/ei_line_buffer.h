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

#ifndef LINEBUFFER_H
#define LINEBUFFER_H

#include <string>

class LineBuffer {
private:
    std::string buffer;
    size_t position;

public:
    LineBuffer()
        : buffer("")
        , position(0) {};

    void clear()
    {
        buffer.clear();
        position = 0;
    }

    void add(std::string &s)
    {
        if (position == buffer.size()) {
            buffer.append(s);
        }
        else {
            buffer.insert(position, s);
        }
        position += s.size();
    }

    void add(const char c)
    {
        if (position == buffer.size()) {
            buffer.append(&c, 1);
        }
        else {
            buffer.insert(position, &c, 1);
        }
        position++;
    }

    bool do_backspace(void)
    {
        if (is_empty() || is_at_begin()) {
            return false;
        }

        buffer.erase(position - 1, 1);
        position--;

        return true;
    }

    bool do_delete(void)
    {
        if (is_empty() || is_at_end()) {
            return false;
        }

        buffer.erase(position, 1);

        return true;
    }

    bool is_at_begin(void)
    {
        return position == 0;
    }

    bool is_at_end(void)
    {
        return position == buffer.size();
    }

    bool is_empty(void)
    {
        return buffer.size() == 0;
    }

    std::string get_string()
    {
        return buffer;
    }

    size_t get_position()
    {
        return position;
    }

    void set_position(int pos)
    {
        if (pos > (int)buffer.size()) {
            position = buffer.size();
        }
        else if (pos < 0) {
            position = 0;
        }
        else {
            position = pos;
        }
    }

    size_t size()
    {
        return buffer.size();
    }
};

#endif /* LINEBUFFER_H */