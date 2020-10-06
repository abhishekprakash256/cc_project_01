%{
#include "y.tab.h"
int yylex();
#include<string.h>
%}
%%
[ \t\n] {}
[-]?[1-9][0-9]* { yylval.str = strdup(yytext); return CONST;}
"not" {return NOT;}
"=" {return EQ;}
"+" {return PLUS;}
"-" {return MINUS;}
"*" {return MUL;}
">" {return GT;}
"<" {return LT;}
">=" {return LTEQ;}
"<=" {return GTEQ;}
"div" {return DIV;}
"mod" {return MOD;}
"define-fun" return DEFFUN;
"get-int" return GETINT;
"true" return TRUE;
"false" return FALSE;
"and" return AND;
"print" return PRINT;
[a-zA-Z][a-zA-Z0-9]* { yylval.str = strdup(yytext); return ID;}        //changed id to ID
[(] {return LPAR;}
[)] {return RPAR;}
. {printf("Lex error: %s\n", yytext); return ERR;}
%%
int yywrap(void) {return 1;}