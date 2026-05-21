%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.hpp"

extern int yylineno;

static void token(const char *type) {
	printf("%-5d %-15s %s\n", yylineno, type, yytext);
}

void error(const char *msg){
	fprintf(stderr, "[Lexical Error] line %d: %s\n", yylineno, msg);
	exit(EXIT_FAILURE);
}
%}

%option yylineno
%option noyywrap

ID      [a-zA-Z_][a-zA-Z0-9_]*
NUM     [0-9]+(\.[0-9]+)?
CHARLIT  \'([^\\\'\n]|\\.)\'
STRLIT  \"(\\.|[^\"\\])*\"

%%

"int"           { token("KEYWORD"); return INT; }
"char"          { token("KEYWORD"); return CHAR; }
"string"        { token("KEYWORD"); return STRING; }
"return"        { token("KEYWORD"); return RETURN; }
"if"            { token("KEYWORD"); return IF; }
"else"          { token("KEYWORD"); return ELSE; }
"while"         { token("KEYWORD"); return WHILE; }

{ID}             { token("ID"); yylval.sval = strdup(yytext); return ID; }
{NUM}            { token("NUM"); yylval.sval = strdup(yytext); return NUM; }
{CHARLIT}        { token("CHAR"); yylval.sval = strdup(yytext); return CHARLIT; }
{STRLIT}         { token("STR"); yylval.sval = strdup(yytext); return STR; }

"+"             { token("OPERATOR"); return '+'; }
"-"             { token("OPERATOR"); return '-'; }
"*"             { token("OPERATOR"); return '*'; }
"/"             { token("OPERATOR"); return '/'; }
">"             { token("OPERATOR"); return '>'; }
"="             { token("OPERATOR"); return '='; }

[(){};,]        { token("PUNCT"); return yytext[0]; }


[ \t\r\n]+ ;
"//".* ;
"/*"([^*]|\*+[^*/])*\*+"/" ;

. { error("invalid character"); }

%%
