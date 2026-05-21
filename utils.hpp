#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <map>

struct Node {
   std::string type;
   std::string value;
   std::vector<Node*> children;
};

struct SymInfo {
   std::string name;
   std::string type;
   int line;
   std::string scope;
};

extern std::map<std::string, SymInfo> symTable;
extern std::string currentFunction;
extern std::vector<std::string> ir;
extern int tmpCount;
extern int lblCount;
extern std::vector<std::string> semanticLogs;
extern int semanticErrors;
extern int semanticWarnings;
extern std::vector<std::string> optimizeLogs;
// --- AST helpers ---
Node* make(std::string t, std::string v = "");
void addChild(Node* p, Node* c);

// --- Symbol table / Scope helpers ---
std::string scopedKey(const std::string& n, const std::string& sc);
const SymInfo* symLookupInScope(const std::string& n, const std::string& sc);
const SymInfo* symLookup(const std::string& n);
const SymInfo* symLookupGlobal(const std::string& n);
bool symExistsCurrentScope(const std::string& n);
void symInsert(std::string n, std::string t, int ln, std::string sc = "");

// --- IR generation helpers ---
std::string newTmp();
std::string newLbl();
void emit(std::string s);
std::string genIR(Node* n);

// --- Semantic helpers ---
std::string exprType(Node* n);
bool typeCompatible(const std::string& lhs, const std::string& rhs);
void semanticWarn(const std::string& msg);
void semanticError(const std::string& msg);

// --- Printing / Debug ---
void printAST(Node* n, int d = 0);
void printSymTable();

// --- Optimization / Codegen ---
void optimize();
void codegen();
Node* optimizeAST(Node* n);

#endif