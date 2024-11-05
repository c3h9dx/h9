%{
#include <jsonrpcpp/jsonrpcpp.hpp>
#include "cli_parsing_driver.h"
%}

%option noyywrap nounput noinput batch

/*%option debug*/

%%

#.*                     /*comment*/

[ \t\n]                 /*white space*/

;|\n                    return cli_drv.tee_token(yy::parser::make_LF_SEMICOLON());

"-"                     return cli_drv.tee_token(yy::parser::make_MINUS());
"+"                     return cli_drv.tee_token(yy::parser::make_PLUS());
"*"                     return cli_drv.tee_token(yy::parser::make_STAR());
"/"                     return cli_drv.tee_token(yy::parser::make_SLASH());
"("                     return cli_drv.tee_token(yy::parser::make_LPAREN());
")"                     return cli_drv.tee_token(yy::parser::make_RPAREN());

"="                     return cli_drv.tee_token(yy::parser::make_ASSIGN());
":"                     return cli_drv.tee_token(yy::parser::make_COLON());

cli                     return cli_drv.tee_token(yy::parser::make_T_CLI());
clear_cache             return cli_drv.tee_token(yy::parser::make_T_CLEAR_CACHE());

h9d                     return cli_drv.tee_token(yy::parser::make_T_H9D());
tcpclient               return cli_drv.tee_token(yy::parser::make_T_TCPCLIENT());
nodes                   return cli_drv.tee_token(yy::parser::make_T_NODES());
devs                    return cli_drv.tee_token(yy::parser::make_T_DEVS());
discover                return cli_drv.tee_token(yy::parser::make_T_DISCOVER());
list                    return cli_drv.tee_token(yy::parser::make_T_LIST());
reload_description      return cli_drv.tee_token(yy::parser::make_T_RELOAD_DESCRIPTION());
node                    return cli_drv.tee_token(yy::parser::make_T_NODE());

reset                   return cli_drv.tee_token(yy::parser::make_T_RESET());
info                    return cli_drv.tee_token(yy::parser::make_T_INFO());

reg                     return cli_drv.tee_token(yy::parser::make_T_REG());
set                     return cli_drv.tee_token(yy::parser::make_T_SET());
get                     return cli_drv.tee_token(yy::parser::make_T_GET());
setbit                  return cli_drv.tee_token(yy::parser::make_T_SETBIT());
clearbit                return cli_drv.tee_token(yy::parser::make_T_CLEARBIT());
togglebit               return cli_drv.tee_token(yy::parser::make_T_TOGGLEBIT());

dev                     return cli_drv.tee_token(yy::parser::make_T_DEV());
status                  return cli_drv.tee_token(yy::parser::make_T_STATUS());
method                  return cli_drv.tee_token(yy::parser::make_T_METHOD());

[0-9]+                  {
                            //printf("!- number token mach\n");
                            int tmp = std::stoi(yytext, nullptr, 10);
                            //std::cout << "int: " << tmp << std::endl;
                            return cli_drv.tee_token(yy::parser::make_NUMBER(tmp));
                        }

b[01]{1,32}             {
                            int tmp = std::stoul(&yytext[1], nullptr, 2);
                            //std::cout << "int: " << tmp << std::endl;
                            return cli_drv.tee_token(yy::parser::make_NUMBER(tmp));
                        }

0[xX][0-9a-fA-F]{1,8}   {
                            int tmp = std::stoul(yytext, nullptr, 16);
                            //std::cout << "int: " << tmp << std::endl;
                            return cli_drv.tee_token(yy::parser::make_NUMBER(tmp));
                        }

([0-9A-Za-z_]|(\\[ ]))+ {
                            return cli_drv.tee_token(yy::parser::make_STRING(cli_drv.unescape(std::string(yytext))));
                        }

\"[^\n\"]+\"            {
                            std::string val = {&yytext[1]};
                            //val.erase(0, 1);
                            val.pop_back();
                            //std::cout << "string: " << val << '\n';
                            return cli_drv.tee_token(yy::parser::make_QUOTED_STRING(val));
                        }

.|[^ \t\n]+             {
                            throw yy::parser::syntax_error(std::string("invalid input: ") + yytext);
                        }

%%

void CLIParsingDriver::scan_string(const char *str) {
    YY_BUFFER_STATE bp = yy_scan_string(str);
    yy_switch_to_buffer(bp);
}

/*
void CLIParser::scan_begin(std::string file) {
    //yy_flex_debug = trace_scanning;
    if (file.empty () || file == "-")
        yyin = stdin;
    else if (!(yyin = fopen (file.c_str (), "r"))) {
        std::cerr << "cannot open " << file << ": " << strerror(errno) << '\n';
        exit (EXIT_FAILURE);
    }
}

void CLIParser::scan_end() {
    fclose (yyin);
}
*/