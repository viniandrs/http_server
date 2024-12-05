%{
#include "../include/lists.h"
#include "../include/methods.h"
#include "parser.tab.h"
#include <stdio.h>
int once = 0;

extern FILE *yyin;
%}

%START  ValueList

http_methods (GET|HEAD|TRACE|OPTIONS|DELETE|PUT)
%%

^{http_methods}                                                 ; { yylval.str = strdup(yytext); return HTTP_METHOD; }
\/([a-zA-Z0-9_]+\/)*(([a-zA-Z0-9_]+\.[a-z]+)|[a-zA-Z0-9_]+)*    ; { yylval.str = strdup(yytext); return FILEPATH; }
HTTP\/.*                                                        ; { yylval.str = strdup(yytext); return HTTP_VERSION; }
#.*                                                             ; /* ignore comments */
^[a-zA-Z][^: \t\n]*                                             ; { yylval.str = strdup(yytext); return FIELD_NAME; }
":"                                                             ; { BEGIN ValueList; return COLON; }
","                                                             ; return COMMA;
<ValueList>[^,\t\n]+                                            ; { yylval.str = strdup(yytext); return VALUE; }
[ \t]+                                                          ; /* ignore whitespaces */
\n                                                              ; BEGIN 0; return EOL;
<<EOF>>                                                         ; return once++ ? 0 : EOL;

%%