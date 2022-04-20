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
