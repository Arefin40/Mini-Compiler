%{
   #include "utils.hpp"
   #include <stdio.h>
   #include <string.h>
   using namespace std;
   extern int yylineno;
   extern FILE* yyin;
   int yylex();
   bool parseError = false;
   int parseErrors = 0;
   void yyerror(const char* s)
   {
      printf("[Syntax Error] line %d: %s\n", yylineno, s);
      parseError = true;
      parseErrors++;
   }
   Node* root = nullptr;
%}

%union {
   char* sval;
   struct Node* nval;
}

%token INT CHAR STRING RETURN IF ELSE WHILE
%token <sval> NUM
%token <sval> CHARLIT
%token <sval> STR
%token <sval> ID

%type <nval>
   program funcs func params paramlist
   block stmts stmt expr args arglist
%type <sval> type

%right '='
%left '>'
%left '+' '-'
%left '*' '/'

%%

program
   : funcs
      { root = $1; }
   ;

funcs
   : funcs func
      { addChild($1, $2); $$ = $1; }
   | func
      { $$ = make("PROGRAM"); addChild($$, $1); }
   ;

func
   : type ID
      { currentFunction = string($2); }
     '(' params ')' block
      {
         $$ = make("FUNC", $2);
         addChild($$, $5);
         addChild($$, $7);
         if (symLookupGlobal($2)) semanticWarn(string("redeclaration of function: ") + $2);
         symInsert($2, string("func(") + $1 + ")", yylineno, "global");
         currentFunction = "global";
      }
   ;

type
   : INT             { $$ = strdup("int"); }
   | CHAR            { $$ = strdup("char"); }
   | STRING          { $$ = strdup("string"); }
   ;

params
   : paramlist       { $$ = $1; }
   | /* empty */     { $$ = make("PARAMS"); }
   ;

paramlist
   : type ID
      {
         $$ = make("PARAMS");
         Node* p = make("PARAM", $2);
         addChild($$, p);
         if (symExistsCurrentScope($2))
            semanticWarn(string("redeclaration of parameter: ") + $2);
         symInsert($2, $1, yylineno);
      }
   | paramlist ',' type ID
      {
         Node* p = make("PARAM", $4);
         addChild($1, p);
         if (symExistsCurrentScope($4)) semanticWarn(string("redeclaration of parameter: ") + $4);
         symInsert($4, $3, yylineno);
         $$ = $1;
      }
   ;

block
   : '{' stmts '}'
      {
         $$ = make("BODY");
         for (auto c : $2->children) addChild($$, c);
      }
   ;

stmts
   : stmts stmt   { addChild($1, $2); $$ = $1; }
   | /* empty */  { $$ = make("STMTS"); }
   ;

stmt
   : type ID ';'
      {
         if (symExistsCurrentScope($2))
            semanticWarn(string("redeclaration: ") + $2);
         symInsert($2, $1, yylineno);
         $$ = make("DECL", $2);
      }
   | ID '=' expr ';'
      {
         const SymInfo* lhsInfo = symLookup($1);
         if (!lhsInfo)
            semanticError(string("undeclared variable: ") + $1);

         if (lhsInfo)
         {
            string lhsType = lhsInfo->type;
            string rhsType = exprType($3);
            if (!typeCompatible(lhsType, rhsType))
               semanticError(string("type mismatch in assignment to '") + $1 + "' (" + lhsType + " <- " + rhsType + ")");
         }

         $$ = make("ASSIGN");
         addChild($$, make("ID", $1));
         addChild($$, $3);
      }
   | RETURN expr ';'
      {
         $$ = make("RETURN");
         addChild($$, $2);
      }
   | IF '(' expr ')' block
      {
         $$ = make("IF");
         addChild($$, $3);
         addChild($$, $5);
      }
   | IF '(' expr ')' block ELSE block
      {
         $$ = make("IF");
         addChild($$, $3);
         addChild($$, $5);
         addChild($$, $7);
      }
   | WHILE '(' expr ')' block
      {
         $$ = make("WHILE");
         addChild($$, $3);
         addChild($$, $5);
      }
   ;

expr
   : expr '+' expr
      { $$ = make("ADD"); addChild($$, $1); addChild($$, $3); }
   | expr '-' expr
      { $$ = make("SUB"); addChild($$, $1); addChild($$, $3); }
   | '-' expr %prec '-'
      { $$ = make("NEG"); addChild($$, $2); }
   | expr '*' expr
      { $$ = make("MUL"); addChild($$, $1); addChild($$, $3); }
   | expr '/' expr
      { $$ = make("DIV"); addChild($$, $1); addChild($$, $3); }
   | expr '>' expr
      { $$ = make("GT"); addChild($$, $1); addChild($$, $3); }
   | ID '(' args ')' {
      const SymInfo* fn = symLookupGlobal($1);
      
      if (!fn)
         semanticError(string("call to undeclared function: ") + $1);
      else if (fn->type.find("func(") != 0)
         semanticError(string("identifier is not a function: ") + $1);

      $$ = make("CALL", $1);
      for (auto c : $3->children) addChild($$, c);
   }
   | ID {
      if (!symLookup($1)) semanticError(string("use of undeclared variable: ") + $1);
      $$ = make("ID", $1);
   }
   | NUM                { $$ = make("NUM", string($1)); }
   | CHARLIT            { $$ = make("CHAR", string($1)); }
   | STR                { $$ = make("STR", string($1)); }
   | '(' expr ')'       { $$ = $2; }
   ;

args
   : arglist            { $$ = $1; }
   | /* empty */        { $$ = make("ARGS"); }
   ;

arglist
   : expr               { $$ = make("ARGS"); addChild($$, $1); }
   | arglist ',' expr   { addChild($1, $3); $$ = $1; }
   ;

%%

int main()
{
   printf("\n1. Lexical Analysis");
   printf("\n----------------------------------------\n");
   printf("Line  Type            Token\n");
   printf("----------------------------------------\n");

   yyparse();

   printf("\n2. Syntax Analysis");
   printf("\n----------------------------------------\n");
   if (!parseError) printf("Success | 0 errors\n"); else printf("Failure | %d errors\n", parseErrors);

   printSymTable();

   printf("\nSyntax Tree:");
   printf("\n----------------------------------------\n");
   printAST(root);

   printf("\n3. Semantic Analysis");
   printf("\n----------------------------------------\n");
   printf("Errors: %d | Warnings: %d\n", semanticErrors, semanticWarnings);
   if (semanticLogs.empty())
      printf("No semantic issues.\n");
   else
      for (auto &l : semanticLogs)
         printf("%s\n", l.c_str());

   optimizeLogs.clear();
   root = optimizeAST(root);

   ir.clear();
   genIR(root);
   printf("\n4. IR (Three Address Code)");
   printf("\n----------------------------------------\n");
   for (auto &s : ir)
      printf("%s\n", s.c_str());

   optimize();
   printf("\n5. Code Optimization & Dead Code removal");
   printf("\n----------------------------------------\n");
   if (optimizeLogs.empty())
      printf("No optimization logs.\n");
   else
      for (auto &l : optimizeLogs)
         printf("%s\n", l.c_str());

   printf("\n6. Assembly Code");
   printf("\n----------------------------------------\n");
   codegen();

   return 0;
}