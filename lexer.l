%{
#include "../include/lists.h"
#include "parser.tab.h"
#include <stdio.h>

int once = 0;

extern FILE *yyin;
%}

%START VALUE_LIST RESOURCE_PATH

http_methods (GET|HEAD|TRACE|OPTIONS|DELETE|POST)
%%

^{http_methods}                   ; { BEGIN FILEPATH; yylval.str = strdup(yytext); return HTTP_METHOD; }
<RESOURCE_PATH>[ ]\/[^: \t\n]*    ; { BEGIN 0; yylval.str = strdup(yytext); return FILEPATH; }
HTTP\/.*                          ; { yylval.str = strdup(yytext); return HTTP_VERSION; }
#.*                               ; /* ignore comments */
^[a-zA-Z][^: \t\n]*               ; { yylval.str = strdup(yytext); return FIELD_NAME; }
":"                               ; { BEGIN VALUE_LIST; return COLON; }
","                               ; return COMMA;
<VALUE_LIST>[^,\t\n]+             ; { yylval.str = strdup(yytext); return VALUE; }
[ \t]+                            ; /* ignore whitespaces */
\n                                ; { BEGIN 0; return EOL; } 
<<EOF>>                           ; return once++ ? 0 : EOL;

%%