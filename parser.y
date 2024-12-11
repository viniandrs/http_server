%{
#include "../include/lists.h"

#include "parser.tab.h"

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

extern int yylex(void);
void yyerror(char **method, char **filepath, FieldNode **field_list_address, char *s);
%}

%union {
    char *str;
    ValueNode *value_list;
    FieldNode *field_list;
}

%token FIELD_NAME VALUE COLON COMMA EOL HTTP_METHOD FILEPATH HTTP_VERSION
%type <str> FIELD_NAME VALUE HTTP_METHOD FILEPATH HTTP_VERSION 
%type <value_list> value_list
%type <field_list> field field_list

%code requires { typedef void *yyscan_t; }
%parse-param { char **method } { char **filepath } { FieldNode **field_list_address }

%%

input               : http_request
                    | EOL
                    ;

http_request        : HTTP_METHOD FILEPATH HTTP_VERSION EOL field_list{
    *method = $1;
    *filepath = $2;
    *field_list_address = $5;

    free($3);    
}
                    ;

field_list          : field_list field { $$ = append_field_node($1, $2); }
                    | field { $$ = append_field_node(NULL, $1); }
                    ;                    

field               : FIELD_NAME COLON value_list EOL { 
                        $$ = create_field($1, $3); 
                        free($1);
                    }
                    | FIELD_NAME COLON EOL { 
                        $$ = create_field($1, NULL); 
                        free($1);
                    }
                    | COLON value_list EOL { yyerror(NULL, NULL, NULL, "Invalid/empty field name."); }
                    | EOL { $$ = NULL; /* empty line or comment*/ }
                    | VALUE COLON VALUE EOL { yyerror(NULL, NULL, NULL, "Invalid/empty field name."); }
                    ;

value_list          : value_list COMMA VALUE {
                        $$ = append_value($1, $3); 
                        free($3);
                    }
                    | VALUE {
                        $$ = append_value(NULL, $1); 
                        free($1);
                    }
                    ;

%%

void yyerror(char **method, char **filepath, FieldNode **field_list_address, char *s) {
  fprintf(stderr, "%s\n", s);
}

#ifdef YYDEBUG
    int yydebug=1;
#endif
