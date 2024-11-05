%skeleton "lalr1.cc" // -*- C++ -*-
%require "3.8.0"
%language "c++"
%header

%define api.token.raw
%define api.token.constructor
%define api.value.type variant
%define parse.assert

%code requires {
    #include <jsonrpcpp/jsonrpcpp.hpp>
    class CLIParsingDriver;
}

%param { CLIParsingDriver& cli_drv }

//%locations

%define parse.trace
%define parse.error verbose
%define parse.lac full

%code {
	#include "cli_parsing_driver.h"
}

%define api.token.prefix {TOK_}

%token END  0           "end of file"
%token LF_SEMICOLON

%token T_CLI            "cli"
%token T_CLEAR_CACHE    "clear_cache"

%token T_H9D            "h9d"
%token T_TCPCLIENT      "tcpclient"
%token T_NODES          "nodes"
%token T_DEVS           "devs"

%token T_DISCOVER       "discover"
%token T_LIST           "list"
%token T_RELOAD_DESCRIPTION          "reload_description"
%token T_NODE           "node"
%token T_RESET          "reset"
%token T_INFO           "info"
%token T_REG            "reg"
%token T_SET            "set"
%token T_GET            "get"
%token T_SETBIT         "setbit"
%token T_CLEARBIT       "clearbit"
%token T_TOGGLEBIT      "togglebit"

%token T_DEV            "dev"
%token T_STATUS         "status"
%token T_METHOD         "method"

%token MINUS            "-"
%token PLUS             "+"
%token STAR             "*"
%token SLASH            "/"
%token LPAREN           "("
%token RPAREN           ")"
%token ASSIGN           "="
%token COLON            ":"

%token <std::string> QUOTED_STRING "<string>"
%token <std::string> STRING "<string>"
%token <int> NUMBER "<number>"

%nterm <std::string> str_exp
%nterm <int> num_exp

%nterm <int> cli_command

%nterm <nlohmann::json> node_exp node_reg_exp dev_exp
%nterm <jsonrpcpp::Request> h9d_command nodes_command devs_command node_command node_reg_command dev_command

%printer { yyo << $$.c_str(); } STRING QUOTED_STRING
%printer { yyo << $$.dump().c_str(); } node_exp dev_exp

//%destructor { printf ("Discarding node_exp symbol.\n"); delete $$; } node_exp
//%destructor { printf ("Discarding node_reg_exp symbol.\n"); delete $$; } node_reg_exp
//%destructor { printf ("Discarding NodeRegExp symbol.\n"); } <NodeRegExp*>
//%destructor { printf ("Discarding AbstractCommand symbol.\n"); } <AbstractCommand*>

//%printer { yyo << $$; } <*>;

%%

%start unit;

%left "+" "-";
%left "*" "/";

unit:
    %empty                              { }
    | cli_command                       {
                                            cli_drv.set_funnction_to_call($1);
                                        }
    | h9d_command                       {
                                            cli_drv.set_jsonrpc_request($1);
                                            cli_drv.set_funnction_to_call(CLIParsingDriver::JSONRPC);
                                        }
    | node_command                      {
                                            cli_drv.set_jsonrpc_request($1);
                                            cli_drv.set_funnction_to_call(CLIParsingDriver::JSONRPC);
                                        }
    | node_reg_command                  {
                                            cli_drv.set_jsonrpc_request($1);
                                            cli_drv.set_funnction_to_call(CLIParsingDriver::JSONRPC);
                                        }
    | dev_command                       {
                                            cli_drv.set_jsonrpc_request($1);
                                            cli_drv.set_funnction_to_call(CLIParsingDriver::JSONRPC);
                                        }
    | num_exp                           { printf("%d\n", $1); }

cli_command:
    "cli" "clear_cache"                 { $$ = CLIParsingDriver::CLEAR_CACHE; }

h9d_command:
    "h9d" "tcpclient" "list"            {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "get_tcp_clients");
                                        }
    | "h9d" "nodes" nodes_command       {
                                            $$ = $3;
                                        }
    | "h9d" "devs" devs_command         {
                                            $$ = $3;
                                        }

nodes_command:
     "discover"                         {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "discover_nodes");
                                        }
     | "reload_description"             {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "reload_nodes_description");
                                        }
     | "list"                           {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "get_nodes_list");
                                        }

devs_command:
     "list"                             {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "get_devs_list");
                                        }

node_exp:
    "node" num_exp			            {
	                                        cli_drv.set_last_parsed_node_id($2);
	                                        $$ = nlohmann::json({{"node_id", $2}});
	                                    }
    | "node" str_exp                    {
                                            std::uint16_t id = cli_drv.cache.get_node_id_by_name($2);
                                            cli_drv.set_last_parsed_node_id(id);
                                            $$ = nlohmann::json({{"node_id", id}});
                                        }

node_command:
    node_exp "reset"         	        {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "node_reset", $1);
                                        }
    | node_exp "info"      	            {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "get_node_info", $1);
                                        }

node_reg_exp:
    node_exp "reg" num_exp  	        {
                                            cli_drv.set_last_parsed_reg_number($3);
                                            $1["reg"] = $3;
                                            $$ = $1;
                                        }
    | node_exp "reg" str_exp  	        {
                                            std::uint8_t reg = cli_drv.cache.get_register_number_by_name($1["node_id"], $3);
                                            cli_drv.set_last_parsed_reg_number(reg);
                                            $1["reg"] = reg;
                                            $$ = $1;
                                        }

node_reg_command:
	node_reg_exp "get"      	        {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "get_register_value", $1);
                                        }
	| node_reg_exp "setbit" num_exp	    {
                                            jsonrpcpp::Id id(0);
                                            $1["bit_num"] = $3;
                                            $$ = jsonrpcpp::Request(id, "set_register_bit", $1);
                                        }
    | node_reg_exp "setbit" str_exp	    {
                                            std::uint8_t bit_num = cli_drv.cache.get_bit_number_by_name($1["node_id"], $1["reg"], $3);
                                            $1["bit_num"] = bit_num;
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "set_register_bit", $1);
                                        }
	| node_reg_exp "clearbit" num_exp	{
                                            jsonrpcpp::Id id(0);
                                            $1["bit_num"] = $3;
                                            $$ = jsonrpcpp::Request(id, "clear_register_bit", $1);
                                        }
	| node_reg_exp "clearbit" str_exp	{
                                            std::uint8_t bit_num = cli_drv.cache.get_bit_number_by_name($1["node_id"], $1["reg"], $3);
                                            $1["bit_num"] = bit_num;
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "clear_register_bit", $1);
                                        }
	| node_reg_exp "togglebit" num_exp	{
                                            jsonrpcpp::Id id(0);
                                            $1["bit_num"] = $3;
                                            $$ = jsonrpcpp::Request(id, "toggle_register_bit", $1);
                                        }
	| node_reg_exp "togglebit" str_exp	{
                                            std::uint8_t bit_num = cli_drv.cache.get_bit_number_by_name($1["node_id"], $1["reg"], $3);
                                            $1["bit_num"] = bit_num;
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "toggle_register_bit", $1);
                                        }
	| node_reg_exp "set" num_exp	    {
                                            jsonrpcpp::Id id(0);
                                            $1["value"] = $3;
                                            $$ = jsonrpcpp::Request(id, "set_register_value", $1);
                                        }
	| node_reg_exp "set" QUOTED_STRING  {
                                            jsonrpcpp::Id id(0);
                                            $1["value"] = $3;
                                            $$ = jsonrpcpp::Request(id, "set_register_value", $1);
                                        }

dev_exp:
    "dev" str_exp                       {
                                            cli_drv.set_last_parsed_dev_name($2);
                                            $$ = nlohmann::json({{"dev_name", $2}});
                                        }

dev_command:
    dev_exp "info"      	            {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "get_dev_description", $1);
                                        }
    | dev_exp "status"      	        {
                                            jsonrpcpp::Id id(0);
                                            $$ = jsonrpcpp::Request(id, "get_dev_status", $1);
                                        }
    | dev_exp "method" STRING           {
                                            jsonrpcpp::Id id(0);
                                            $1["method"] = $3;
                                            $$ = jsonrpcpp::Request(id, "dev_method_call", $1);
                                        }

str_exp:
    STRING                              { $$ = $1; }
    | QUOTED_STRING                     { $$ = $1; }

num_exp:
	NUMBER
	| num_exp "+" num_exp               { $$ = $1 + $3; }
	| num_exp "-" num_exp               { $$ = $1 - $3; }
	| num_exp "*" num_exp               { $$ = $1 * $3; }
	| num_exp "/" num_exp               { $$ = $1 / $3; }
	| "(" num_exp ")"   	            { $$ = $2; }
	| "-" num_exp                       { $$ = -$2; }

%%

void yy::parser::error (const std::string& m) {
    std::cerr << ": " << m << '\n';
    //if (cli_parser.print_parse_error)
 /*   	std::cerr << ": " << m << '\n';

        yy_stack_print_();
        printf("yytable[]: %c\n", yytable_[yylast_]);
        printf("term num: %d\n", yyfinal_);
        for (auto& a : yylac_stack_) {
            printf("%c ", a);
        }
        printf("\n");

            *yycdebug_ << "Stack3 now";
            for (stack_type::const_iterator
                   i = yystack_.begin (),
                   i_end = yystack_.end ();
                 i != i_end; ++i)
              *yycdebug_ << ' ' << i->name();
            *yycdebug_ << '\n';
*/
}
