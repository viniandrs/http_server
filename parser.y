%{
#include "../include/lists.h"
#include "../include/utils.h"
#include "../include/methods.h"
#include "../include/auth.h"
#include "../include/post.h"

#include "parser.tab.h"

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

extern int yylex(void);
void yyerror(char *request, char *request_body, char ***body_addr, char ***header_addr, const char *s);

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
%parse-param { char *request } { char *request_body } {char ***body_addr } { char ***header_addr }

%%

input               : input http_request
                    | EOL input
                    | http_request
                    | EOL
                    ;

http_request        : HTTP_METHOD FILEPATH HTTP_VERSION EOL field_list {
    // printing request details on the screen
    printf("\nReceived request:\n%s\nwith request body:\n%s\n\n", request, request_body);

    // formating path strings
    char *resource = format_resource_path($2);
    char *resource_abs_path = format_and_resolve_resource_path($2);

    // content type from filepath
    const char content_type[32] = "text/html; charset=utf-8";//get_content_type(resource_abs_path);

    // get credentials from field_list
    ValueNode* credentials = get_credentials($5);
                        
    struct stat *st = (struct stat *)malloc(sizeof(struct stat));

    int status;
    char *header;
    if (strcmp($1, "POST") == 0){
        printf("Parsing request body...\n");
        char **values = parse_request_body(request_body);

        char *htaccess_path = htaccess_from_form(resource_abs_path);

        status = 201;
        if(!update_passwords(htaccess_path, values)) {
            // password not changed
            printf("Error while changing password\n");
            status = 200;
        } else {
            printf("Password changed successfully!\n");
            header = strcmp($1, "OPTIONS") == 0 ? options(st, status) : head(st, status, content_type);
        }
    } else {
        status = fetchr(resource, st, credentials);
        header = strcmp($1, "OPTIONS") == 0 ? options(st, status) : head(st, status, content_type);
    }
    
    // getting response body
    char *body;
    if (status == 200){
        if (strcmp($1, "HEAD")==0) {
            body = "";
        } else if (strcmp($1, "OPTIONS")==0) {
            body = "";
        } else if (strcmp($1, "GET")==0) {
            body = get(resource_abs_path, st);
        } else if (strcmp($1, "TRACE")==0) {
            size_t body_len = snprintf(NULL, 0, "%s", request);
            body = (char *)calloc(body_len + 1, sizeof(char));
            strcpy(body, request);
        } else if (strcmp($1, "POST")==0) {
            size_t wrong_passwd_abs_path_len = snprintf(NULL, 0, "%s/%s", webspace_path, "wrong_passwd.html");
            char *wrong_passwd_abs_path = (char *) calloc(wrong_passwd_abs_path_len, sizeof(char));
            sprintf(wrong_passwd_abs_path, "%s/%s", webspace_path, "wrong_passwd.html");
            printf("wrong passwd abs path: %s\n", wrong_passwd_abs_path);
            stat(wrong_passwd_abs_path, st);
            body = get(wrong_passwd_abs_path, st);
            header = head(st, status, content_type);
        } else {
            header = head(st, 501, content_type);
            body = error_handler(501); // Not implemented
        }
    }
    else if (status == 401) {
        header = header_401_unauthorized(resource, credentials);
        body = "";
    } else if (status == 201) {
        body = "NEW PASSWORD CONFIRMED";
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
                    | COLON value_list EOL { yyerror(NULL, NULL, NULL, NULL, "Invalid/empty field name."); }
                    | EOL { $$ = NULL; /* empty line or comment*/ }
                    | VALUE COLON VALUE EOL { yyerror(NULL, NULL, NULL, NULL, "Invalid/empty field name."); }
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

void yyerror(char *request, char *request_body, char ***body_addr, char ***header_addr, const char *s) {
  fprintf(stderr, "%s\n", s);
}

#ifdef YYDEBUG
    int yydebug=1;
#endif
