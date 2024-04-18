/*
 * Created by crowx on 21/10/2023.
 *
 */

#include "cli_parsing_driver.h"

#include <cstdlib>
#include <nlohmann/json.hpp>
#include <readline/readline.h>
#include <spdlog/spdlog.h>
#include <utility>

CLIParsingDriver* CLIParsingDriver::instance;

char* cli_main_completion_generator(const char* text, int state) {
    static const char* completion_list[] = {"cli", "h9d", "node", nullptr};
    static int list_index, len;
    const char* name;

    if (!state) {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = completion_list[list_index++])) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    return nullptr;
}

char** cli_completion_wrapper(const char* text, int start, int end) {
    return CLIParsingDriver::get().cli_completion(text, start, end);
}

static char* cli_grammar_completion_wrapper(const char* text, int state) {
    return CLIParsingDriver::get().cli_grammar_completion(text, state);
}

static char* cli_node_completion_wrapper(const char* text, int state) {
    return CLIParsingDriver::get().cli_node_completion(text, state);
}

static char* cli_register_completion_wrapper(const char* text, int state) {
    return CLIParsingDriver::get().cli_register_completion(text, state);
}

static char* cli_bit_completion_wrapper(const char* text, int state) {
    return CLIParsingDriver::get().cli_bit_completion(text, state);
}

std::string HackParser::yysyntax_error_(const context& yyctx) const {
    symbol_kind_type yyarg[YYNTOKENS];
    int a = yyctx.expected_tokens(yyarg, YYNTOKENS);
    const_cast<HackParser*>(this)->alt.clear();

    for (int i = 0; i < a; ++i) {
        switch (yyarg[i]) {

        case yy::parser::symbol_kind::S_MINUS:
        case yy::parser::symbol_kind::S_PLUS:
        case yy::parser::symbol_kind::S_STAR:
        case yy::parser::symbol_kind::S_SLASH:
        case yy::parser::symbol_kind::S_LPAREN:
        case yy::parser::symbol_kind::S_RPAREN:
        case yy::parser::symbol_kind::S_ASSIGN:
        case yy::parser::symbol_kind::S_COLON:
            break;
        default:
            const_cast<HackParser*>(this)->alt.push_back(yy::parser::symbol_name(yyarg[i]));
            break;
        }
    }
    return {""};
}

void HackParser::error(const std::string& m) {
    return;
}

char* CLIParsingDriver::escape(const char* str) {
    size_t str_len = strlen(str);

    char* escaped;
    if ((escaped = static_cast<char*>(malloc(2 * str_len + 1))) == nullptr) {
        SPDLOG_ERROR("malloc error");
        return nullptr;
    }

    int i, j;
    for (i = 0, j = 0; i < str_len; ++i, ++j) {
        if (str[i] == ' ') {
            escaped[j++] = '\\';
        }
        escaped[j] = str[i];
    }
    escaped[j] = '\0';

    char* resized_escaped;
    if ((resized_escaped = static_cast<char*>(realloc(escaped, j))) == nullptr) {
        free(escaped);
        resized_escaped = nullptr;
        SPDLOG_ERROR("realloc error");
    }

    return resized_escaped;
}

char* CLIParsingDriver::unescape(const char* str) {
    size_t str_len = strlen(str);

    char* unescaped;
    if ((unescaped = static_cast<char*>(malloc(str_len + 1))) == nullptr) {
        SPDLOG_ERROR("malloc error");
        return nullptr;
    }

    int i, j;
    for (i = 0, j = 0; i < str_len; ++i, ++j) {
        if (str[i] == '\\' && str[i + 1] == ' ') {
            ++i;
        }
        unescaped[j] = str[i];
    }
    unescaped[j] = '\0';

    char* resized_unescaped;
    if ((resized_unescaped = static_cast<char*>(realloc(unescaped, j+1))) == nullptr) {
        free(unescaped);
        resized_unescaped = nullptr;
        SPDLOG_ERROR("realloc error");
    }
    return resized_unescaped;
}

std::string CLIParsingDriver::unescape(const std::string str) {
    char* tmp = unescape(str.c_str());
    std::string ret = {tmp};
    free(tmp);
    return std::move(ret);
}

CLIParsingDriver::CLIParsingDriver(H9Connector* h9d_conn):
    h9d(h9d_conn),
    cache(h9d_conn),
    completion_parser(*this),
    cmd_executor(h9d_conn) {
}

char** CLIParsingDriver::cli_completion(const char* text, int start, int end) {
    //    printf("\n%s\n", rl_line_buffer);
    //    printf("%*c %d\n", start + 1, '^', start);
    //    printf("%*c %d\n", end + 1, '^', end);
    //    printf("%s\n", text);

    rl_attempted_completion_over = 1;

    if (start == 0)
        return rl_completion_matches(text, cli_main_completion_generator);
    else {
        last_node_id = 0xffff;
        last_reg_number = 0;

        char* input = strndup(rl_line_buffer, start);
        completion_parser.alt.clear();

        scan_string(input);
        free(input);
        completion_parser.set_debug_level(0);
        completion_parser.parse();

        char** ret_tmp = nullptr;

        if (last_token == yy::parser::symbol_kind::symbol_kind_type::S_T_NODE) {
            char* unescape_text = unescape(text);

            ret_tmp = rl_completion_matches(unescape_text, cli_node_completion_wrapper);
            if (ret_tmp && ret_tmp[0] && rl_completion_quote_character == 0) {
                char* escape_substitution = escape(ret_tmp[0]);
                free(ret_tmp[0]);
                ret_tmp[0] = escape_substitution;
            }
            free(unescape_text);
        }
        else if (last_token == yy::parser::symbol_kind::symbol_kind_type::S_T_REG && last_node_id != 0xffff) {
            char* unescape_text = unescape(text);

            ret_tmp = rl_completion_matches(unescape_text, cli_register_completion_wrapper);
            if (ret_tmp && ret_tmp[0] && rl_completion_quote_character == 0) {
                char* escape_substitution = escape(ret_tmp[0]);
                free(ret_tmp[0]);
                ret_tmp[0] = escape_substitution;
            }
            free(unescape_text);
        }
        else if ((last_token == yy::parser::symbol_kind::symbol_kind_type::S_T_SETBIT || last_token == yy::parser::symbol_kind::symbol_kind_type::S_T_CLEARBIT || last_token == yy::parser::symbol_kind::symbol_kind_type::S_T_TOGGLEBIT) && last_node_id != 0xffff && last_reg_number != 0) {
            char* unescape_text = unescape(text);

            ret_tmp = rl_completion_matches(unescape_text, cli_bit_completion_wrapper);
            if (ret_tmp && ret_tmp[0] && rl_completion_quote_character == 0) {
                char* escape_substitution = escape(ret_tmp[0]);
                free(ret_tmp[0]);
                ret_tmp[0] = escape_substitution;
            }
            free(unescape_text);
        }
        else {
            ret_tmp = rl_completion_matches(text, cli_grammar_completion_wrapper);
        }

        if (ret_tmp && ret_tmp[0] && ret_tmp[0][0] == '<') { // DROP <* match e.g. <number> or <string>
            if (ret_tmp[1]) {
                char* m = ret_tmp[0];
                ret_tmp[0] = strdup("");
                free(m);
            }
            else {  //only one mach
                char* m = unescape(ret_tmp[0]);
                free(ret_tmp[0]);
                free(ret_tmp);
                ret_tmp = static_cast<char**>(malloc(sizeof(char*) * 3));
                ret_tmp[0] = strdup("");
                ret_tmp[1] = m;
                ret_tmp[2] = nullptr;
            }
        }

        return ret_tmp;
    }
    return nullptr;
}

char* CLIParsingDriver::cli_grammar_completion(const char* text, int state) {
    static int len;
    static decltype(completion_parser.alt)::iterator it;

    if (!state) {
        it = completion_parser.alt.begin();
        len = strlen(text);
    }

    while (it != completion_parser.alt.end()) {
        if ((*it).compare(0, len, text) == 0) {
            return strdup((*it++).c_str());
        }
        it++;
    }
    return nullptr;
}

char* CLIParsingDriver::cli_node_completion(const char* text, int state) {
    static std::vector<std::string>* completion_list = nullptr;
    static std::remove_pointer<decltype(completion_list)>::type::iterator it;
    static int len;

    // printf("node <%s>:%ds\n", text, state);

    if (!state) {
        len = strlen(text);

        completion_list = cache.get_nodes_list();

        it = completion_list->begin();
    }

    if (state == 0 && text[0] == '\0') {
        return strdup("<node ID>");
    }

    while (it != completion_list->end()) {
        if ((*it).compare(0, len, text) == 0) {
            return strdup((*it++).c_str());
        }
        it++;
    }
    return nullptr;
}

char* CLIParsingDriver::cli_register_completion(const char* text, int state) {
    static std::vector<std::string>* completion_list = nullptr;
    static std::remove_pointer<decltype(completion_list)>::type::iterator it;
    static int len;

    if (!state) {
        len = strlen(text);

        completion_list = cache.get_registers_list(last_node_id);

        it = completion_list->begin();
    }

    if (state == 0 && text[0] == '\0') {
        return strdup("<reg number>");
    }

    while (it != completion_list->end()) {
        if ((*it).compare(0, len, text) == 0) {
            return strdup((*it++).c_str());
        }
        it++;
    }
    return nullptr;
}

char* CLIParsingDriver::cli_bit_completion(const char* text, int state) {
    static std::vector<std::string>* completion_list = nullptr;
    static std::remove_pointer<decltype(completion_list)>::type::iterator it;
    static int len;

    if (!state) {
        len = strlen(text);

        completion_list = cache.get_bits_list(last_node_id, last_reg_number);

        it = completion_list->begin();
    }

    if (state == 0 && text[0] == '\0') {
        return strdup("<bit number>");
    }

    while (it != completion_list->end()) {
        if ((*it).compare(0, len, text) == 0) {
            return strdup((*it++).c_str());
        }
        it++;
    }
    return nullptr;
}

yy::parser::symbol_type CLIParsingDriver::tee_token(yy::parser::symbol_type symbol) {
    last_token = symbol.type_get();
    return std::move(symbol);
}

void CLIParsingDriver::set_last_parsed_node_id(std::uint16_t id) {
    last_node_id = id;
}

void CLIParsingDriver::set_last_parsed_reg_number(std::uint16_t num) {
    last_reg_number = num;
}

int CLIParsingDriver::parse(const char* str) {
    _fun = NOP;
    scan_string(str);
    yy::parser parser = {*this};
    // parser.set_debug_level(10);
    return parser.parse();
}

void CLIParsingDriver::set_funnction_to_call(int fun) {
    _fun = fun;
}

void CLIParsingDriver::set_jsonrpc_request(jsonrpcpp::Request req) {
    _req = std::move(req);
}

void CLIParsingDriver::execute() {
    if (_fun == CLEAR_CACHE) {
        cache.clear();
    }
    else if (_fun == JSONRPC) {
        cmd_executor.execute(_req);
    }
}
