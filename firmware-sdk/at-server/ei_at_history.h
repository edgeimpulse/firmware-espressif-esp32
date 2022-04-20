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