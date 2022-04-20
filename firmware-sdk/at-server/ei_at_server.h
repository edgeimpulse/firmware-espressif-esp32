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
    void register_help_command(void);

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