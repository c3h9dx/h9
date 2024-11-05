/*
 * Created by crowx on 21/10/2023.
 *
 */

#pragma once

#include "cli_cache.h"
#include "cli_parser.h"
#include "command_executor.h"
#include "h9connector.h"

#define YY_DECL yy::parser::symbol_type yylex(CLIParsingDriver& cli_drv)
YY_DECL;

char** cli_completion_wrapper(const char* text, int start, int end);

class HackParser: public yy::parser {
  public:
    std::vector<std::string> alt;

    HackParser(CLIParsingDriver& ref):
        parser(ref) {}

    virtual std::string yysyntax_error_(const context& yyctx) const override;
    virtual void error(const std::string& m) override;
};

class CLIParsingDriver {
  public:
    static constexpr int NOP = 0;
    static constexpr int JSONRPC = 1;
    static constexpr int CLEAR_CACHE = 2;

  private:
    static CLIParsingDriver* instance;
    H9Connector* h9d;

    CLIParsingDriver(H9Connector* h9d_conn);

    CommandExecutor cmd_executor;
    HackParser completion_parser;

    yy::parser::symbol_kind::symbol_kind_type last_token;
    std::uint16_t last_node_id;
    std::uint8_t last_reg_number;
    std::string last_dev;

    int _fun;
    jsonrpcpp::Request _req;

    void scan_string(const char* str);
    char* escape(const char* str);
    char* unescape(const char* str);

  public:
    CliCache cache;
    std::string unescape(const std::string str);

    static CLIParsingDriver& create(H9Connector* h9d_conn) {
        instance = new CLIParsingDriver(h9d_conn);
        return *instance;
    }

    static CLIParsingDriver& get() {
        return *instance;
    }

    char** cli_completion(const char* text, int start, int end);
    char* cli_grammar_completion(const char* text, int state);
    char* cli_node_completion(const char* text, int state);
    char* cli_register_completion(const char* text, int state);
    char* cli_bit_completion(const char* text, int state);
    char* cli_dev_completion(const char* text, int state);
    char* cli_dev_method_completion(const char* text, int state);

    yy::parser::symbol_type tee_token(yy::parser::symbol_type symbol);
    void set_last_parsed_node_id(std::uint16_t id);
    void set_last_parsed_reg_number(std::uint16_t num);
    void set_last_parsed_dev_name(std::string name);

    int parse(const char* str);
    void set_funnction_to_call(int fun);
    void set_jsonrpc_request(jsonrpcpp::Request req);

    void execute();
};
