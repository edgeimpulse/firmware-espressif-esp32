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