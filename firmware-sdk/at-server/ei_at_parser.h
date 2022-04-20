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