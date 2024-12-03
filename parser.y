%{
#include "../include/lists.h"
#include "../include/utils.h"
#include "../include/methods.h"
#include "../include/auth.h"

#include "parser.tab.h"

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

extern int yylex(void);
void yyerror(char *request, char ***body_addr, char ***header_addr, const char *s);

extern char *webspace_path, *log_file;
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
%parse-param { char *request } {char ***body_addr } { char ***header_addr }

%%

input               : input http_request
                    | EOL input
                    | http_request
                    | EOL
                    ;

http_request        : HTTP_METHOD FILEPATH HTTP_VERSION EOL field_list {
                        // printing request details on the screen
                        printf("\nProcessing request:\n%s\n", request);

                        // formating path strings
                        char *resource = format_resource_path($2);

                        // get credentials from field_list
                        ValueNode* credentials = get_credentials($5);
                        
                        struct stat *st = (struct stat *)malloc(sizeof(struct stat));
                        int status = fetchr(webspace_path, resource, st, credentials);
                        
                        // getting header
                        char *header = strcmp($1, "OPTIONS") == 0 ? options(st, status) : head(st, status);

                        // getting response body
                        char *body;
                        if (status == 200){
                            if (strcmp($1, "HEAD")==0) {
                                body = "";
                            } else if (strcmp($1, "OPTIONS")==0) {
                                body = "";
                            } else if (strcmp($1, "GET")==0) {
                                size_t abs_path_len = snprintf(NULL, 0, "%s/%s", webspace_path, resource);
                                char *abs_path = calloc(abs_path_len + 1, sizeof(char));
                                sprintf(abs_path, "%s%s", webspace_path, resource);
                                body = get(abs_path, st);
                            } else if (strcmp($1, "TRACE")==0) {
                                size_t body_len = snprintf(NULL, 0, "%s", request);
                                body = (char *)calloc(body_len + 1, sizeof(char));
                                strcpy(body, request);
                            } else {
                                header = head(st, 501);
                                body = error_handler(501); // Not implemented
                            }
                        }
                        else if (status == 401) {
                            header = auth_header(webspace_path, resource, credentials);
                            body = "";
                        }
                        else { 
                            body = error_handler(status);
                        }               

                        *body_addr = &body;
                        *header_addr = &header;

                        // freeing pointers
                        free(st);
                        free($1);
                        free($3);
                        free(resource);
                        free_field_list($5);
                    }
                    ;

field_list          : field_list field {
                        $$ = add_node($1, $2);
                    }
                    | field {
                        $$ = add_node(NULL, $1);
                    }
                    ;                    

field               : FIELD_NAME COLON value_list EOL { 
                        $$ = create_node($1, $3); 
                        free($1);
                    }
                    | FIELD_NAME COLON EOL { 
                        $$ = create_node($1, NULL); 
                        free($1);
                    }
                    | COLON value_list EOL { yyerror(NULL, NULL, NULL, "Invalid/empty field name."); }
                    | EOL { $$ = NULL; /* empty line or comment*/ }
                    | VALUE COLON VALUE EOL { yyerror(NULL, NULL, NULL, "Invalid/empty field name."); }
                    ;

value_list          : value_list COMMA VALUE {
                        $$ = add_value($1, $3); 
                        free($3);
                    }
                    | VALUE {
                        $$ = add_value(NULL, $1); 
                        free($1);
                    }
                    ;

%%

void yyerror(char *request, char ***body_addr, char ***header_addr, const char *s) {
  fprintf(stderr, "%s\n", s);
}

#ifdef YYDEBUG
    int yydebug=1;
#endif
