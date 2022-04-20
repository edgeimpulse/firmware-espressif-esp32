#include "ei_at_server.h"
#include "ei_at_command_set.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

using namespace std;

// fake handler (will never be called) just to make it
// possible to register HELP command
static bool print_help_handler(void)
{
    return true;
}

ATServer::ATServer()
    : history(default_history_size)
{
    register_help_command();
}

ATServer::ATServer(ATCommand_t *commands, size_t length, size_t max_history_size)
    : history(max_history_size)
{
    if (length == 0 || commands == nullptr) {
        register_help_command();
        return;
    }

    for (unsigned int i = 0; i < length; i++) {
        this->register_command(commands[i]);
    }

    // we have to overwrite any HELP handler added by user
    register_help_command();
}

ATServer::~ATServer()
{
    // nothing to do?
}

void ATServer::register_help_command(void)
{
    ATCommand_t tmp;
    tmp.command = AT_HELP;
    tmp.help_text = AT_HELP_HELP_TEXT;
    tmp.run_handler = print_help_handler;
    tmp.read_handler = nullptr;
    tmp.write_handler = nullptr;
    tmp.write_handler_args_list = string("");

    this->registered_commands.push_back(tmp);
}

/**
 * @brief Register a new command. If the same command already exists
 * (by comparing \ref ATCommand_t.command field) then overwrite it.
 * 
 * @param command 
 * @return true if the command has been registered
 * @return false if some sanity checks failed
 */
bool ATServer::register_command(ATCommand_t &command)
{
    // we can't register user version of the AT+HELP command
    if (command.command == AT_HELP) {
        return false;
    }

    // check if command exists
    for (auto it = this->registered_commands.begin(); it != this->registered_commands.end(); ++it) {
        if (it->command == command.command) {
            // remove command that is already exist
            this->registered_commands.erase(it);
            // there shouldn't be another handler for same command
            break;
        }
    }

    this->registered_commands.push_back(command);

    return true;
}

bool ATServer::register_command(
    const char *cmd,
    const char *help_text,
    bool (*run_handler)(void),
    bool (*read_handler)(void),
    bool (*write_handler)(const char **, const int),
    const char *write_handler_args_list)
{
    ATCommand_t temp_cmd;

    temp_cmd.command = cmd;
    temp_cmd.help_text = help_text;
    temp_cmd.run_handler = run_handler;
    temp_cmd.read_handler = read_handler;
    temp_cmd.write_handler = write_handler;
    if (write_handler_args_list != nullptr) {
        temp_cmd.write_handler_args_list = string(write_handler_args_list);
    }

    return this->register_command(temp_cmd);
}

bool ATServer::register_handlers(
    const char *cmd,
    bool (*run_handler)(void),
    bool (*read_handler)(void),
    bool (*write_handler)(const char **, const int),
    const char *write_handler_args_list)
{
    for (auto it = registered_commands.begin(); it != registered_commands.end(); ++it) {
        if (it->command.compare(cmd) == 0) {
            //TODO: add sanity checks?
            it->run_handler = run_handler;
            it->read_handler = read_handler;
            it->write_handler = write_handler;
            //TODO: parse write_handler_args_list and update write_handler_arg_count
            if (write_handler_args_list != nullptr) {
                it->write_handler_args_list = string(write_handler_args_list);
            }
            return true;
        }
    }

    return false;
}

bool ATServer::print_help(void)
{
    bool new_line_required = false;

    /* 
     * print list of commands in the following style
     * AT+COMMAND
     * AT+COMMAND?
     * AT+COMMAND=arg1,arg2
     *      Help text for the command
     * 
     */
    ei_printf("AT Server\nCommand set version: " AT_COMMAND_VERSION "\n");
    ei_printf("Arguments in square brackets are optional, eg.:\nAT+CMD=arg1,[arg2]\n\n");
    for (auto it = this->registered_commands.begin(); it != this->registered_commands.end(); ++it) {
        if (!it->run_handler && !it->read_handler && !it->write_handler) {
            continue;
        }
        /* print main command () */
        if (it->run_handler) {
            ei_printf("AT+%s\n", it->command.c_str());
            new_line_required = true;
        }

        if (it->read_handler) {
            ei_printf("AT+%s?\n", it->command.c_str());
            new_line_required = true;
        }

        if (it->write_handler && it->write_handler_args_list != "") {
            ei_printf("AT+%s=%s\n", it->command.c_str(), it->write_handler_args_list.c_str());
            new_line_required = true;
        }

        if (new_line_required) {
            // if new_line_required is true, it means at least one handler is active
            if (!it->help_text.empty()) {
                ei_printf("\t%s\n\n", it->help_text.c_str());
            }
        }
    }

    return true;
}

void ATServer::print_prompt(void)
{
    ei_printf("> ");
}

void ATServer::handle(char c)
{
    string tmp;
    bool print_new_prompt = true;
    static bool in_ctrl_char = false;
    static vector<char> control_sequence;

    // control characters start with 0x1b and end with a-zA-Z
    // typically \x1b[<LETTER> eg. \x1b[A
    if (in_ctrl_char) {
        control_sequence.push_back(c);
        // if a-zA-Z then it's the last one in the control char...
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == 0x7e)) {
            in_ctrl_char = false;
            // up: \x1b[A
            if (control_sequence.size() == 2 && control_sequence.at(0) == 0x5b &&
                control_sequence.at(1) == 0x41) {

                ei_printf("\x1b[u"); // restore current position
                tmp = history.go_back();
                // ei_printf("\r\x1b[K> %s", tmp.c_str());
                ei_printf("\x1b[2K\r> %s", tmp.c_str());
                buffer.clear();
                buffer.add(tmp);
            }
            // down: \x1b[B
            else if (
                control_sequence.size() == 2 && control_sequence.at(0) == 0x5b &&
                control_sequence.at(1) == 0x42) {

                ei_printf("\x1b[u"); // restore current position
                tmp = history.go_next();
                // reset cursor to 0, do \r, then write the new command...
                // ei_printf("\r\x1b[K> %s", tmp.c_str());
                ei_printf("\x1b[2K\r> %s", tmp.c_str());
                buffer.clear();
                buffer.add(tmp);
            }
            // left: \x1b[D
            else if (
                control_sequence.size() == 2 && control_sequence.at(0) == 0x5b &&
                control_sequence.at(1) == 0x44) {

                size_t curr = buffer.get_position();

                // at pos0? prevent moving to the left
                if (curr == 0) {
                    ei_printf("\x1b[u"); // restore current position
                }
                // otherwise it's OK, move the cursor back
                else {
                    buffer.set_position(curr - 1);
                    ei_putchar('\x1b');
                    for (size_t ix = 0; ix < control_sequence.size(); ix++) {
                        ei_putchar(control_sequence[ix]);
                    }
                }
            }
            // right: \x1b[C
            else if (
                control_sequence.size() == 2 && control_sequence.at(0) == 0x5b &&
                control_sequence.at(1) == 0x43) {

                size_t curr = buffer.get_position();

                // already at the end?
                if (curr == buffer.size()) {
                    ei_printf("\x1b[u"); // restore current position
                }
                else {
                    buffer.set_position(curr + 1);
                    ei_putchar('\x1b');
                    for (size_t ix = 0; ix < control_sequence.size(); ix++) {
                        ei_putchar(control_sequence[ix]);
                    }
                }
            }
            // HOME key: \x1b[H
            else if (
                control_sequence.size() == 2 && control_sequence.at(0) == 0x5b &&
                control_sequence.at(1) == 0x48) {
                // move to begining of the buffer...
                buffer.set_position(0);
                // ...and the line
                ei_printf(
                    "\r\x1b[K> %s\x1b[%uG",
                    buffer.get_string().c_str(),
                    buffer.get_position() + 3);
            }
            // END key: \x1b[F
            else if (
                control_sequence.size() == 2 && control_sequence.at(0) == 0x5b &&
                control_sequence.at(1) == 0x46) {
                // move to end of the buffer...
                buffer.set_position(buffer.size());
                // ...and the line
                ei_printf(
                    "\r\x1b[K> %s\x1b[%uG",
                    buffer.get_string().c_str(),
                    buffer.get_position() + 3);
            }
            // DELETE key: \x1b[3\x7e
            else if (
                control_sequence.size() == 3 && control_sequence.at(0) == 0x5b &&
                control_sequence.at(1) == 0x33 && control_sequence.at(2) == 0x7e) {
                if (buffer.do_delete()) {
                    ei_printf(
                        "\r\x1b[K> %s\x1b[%uG",
                        buffer.get_string().c_str(),
                        buffer.get_position() + 3);
                }
            }
            else {
                // not up/down? execute original control sequence
                ei_putchar('\x1b');
                for (size_t ix = 0; ix < control_sequence.size(); ix++) {
                    ei_putchar(control_sequence[ix]);
                }
            }

            control_sequence.clear();
        }
        return;
    }

    switch (c) {
    case '\n': /* Ignore newline as input */
        break;
    case '\r': /* want to run the buffer */
        ei_putchar(c);
        ei_putchar('\n');
        tmp = buffer.get_string();

        history.add(tmp);

        print_new_prompt = execute(tmp);

        buffer.clear();

        if (print_new_prompt) {
            print_prompt();
        }
        break;
    case 0x08: /* backspace */
    case 0x7f: /* also backspace on some terminals */
        if (buffer.do_backspace() == false) {
            break;
        }
        ei_printf("\r\x1b[K> %s\x1b[%uG", buffer.get_string().c_str(), buffer.get_position() + 3);
        break;
    case 0x1b: /* control character */
        // start processing characters as they are control sequence
        in_ctrl_char = true;
        ei_printf("\x1b[s"); // save current position
        break;
    default:
        if (c >= 0x20 && c <= 0x7e) {
            buffer.add(c);
            if (buffer.is_at_end()) {
                ei_putchar(c);
            }
            else {
                ei_printf("\r> %s\x1b[%uG", buffer.get_string().c_str(), buffer.get_position() + 3);
            }
        }
        break;
    }
}

bool ATServer::execute(string &input)
{
    bool new_prompt_required = false;
    char **args = nullptr;
    ATParseResult_t res;

    res = parser.parse(input);
    if (res.type == AT_UNKNOWN) {
        ei_printf("Not a valid AT command (%s)\n", input.c_str());
        return true;
    }

    // exception for HELP command which is built-in
    if (res.command == AT_HELP && res.type == AT_RUN) {
        return this->print_help();
    }

    // find a command to execute
    for (auto it = this->registered_commands.begin(); it != this->registered_commands.end(); ++it) {
        if (it->command == res.command) {
            // we've got a hit!
            if (res.type == AT_RUN && it->run_handler) {
                // simple command like AT+HELP
                new_prompt_required = it->run_handler();
            }
            else if (res.type == AT_READ && it->read_handler) {
                // read command like AT+CONFIG?
                new_prompt_required = it->read_handler();
            }
            else if (res.type == AT_WRITE && it->write_handler) {
                // write command like AT+DEVICEID=abcde
                args = (char **)ei_malloc(res.arguments.size() * sizeof(char *));
                for (unsigned int i = 0; i < res.arguments.size(); i++) {
                    // one more byte for null terminator (in case of zero-length = empty argument eg. AT+RUN=123,,5)
                    args[i] = (char *)ei_malloc(res.max_arg_len + 1);
                    strncpy(args[i], res.arguments.at(i).c_str(), res.max_arg_len + 1);
                }

                new_prompt_required =
                    it->write_handler((const char **)args, (int)res.arguments.size());

                // now we are freeing the memory
                for (unsigned int i = 0; i < res.arguments.size(); i++) {
                    ei_free(args[i]);
                }
                ei_free(args);
            }
            else {
                ei_printf("No handler for command! (%s)\n", input.c_str());
                return true;
            }
            return new_prompt_required;
        }
    }

    // we shouldn't be here!
    ei_printf("Command not found! (AT+%s)\n", res.command.c_str());
    return true;
}
