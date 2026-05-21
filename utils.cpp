#include "utils.hpp"

#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <cctype>
#include <algorithm>

using namespace std;

// ------------------------------------------------------------------
// Global state
// ------------------------------------------------------------------
map<string, SymInfo> symTable;
string currentFunction = "global";
vector<string> ir;
int tmpCount = 0;
int lblCount = 0;
vector<string> semanticLogs;
int semanticErrors = 0;
int semanticWarnings = 0;
vector<string> optimizeLogs;

extern int yylineno;

// ------------------------------------------------------------------
// AST helpers
// ------------------------------------------------------------------
Node* make(string t, string v)
{
   return new Node{t, v, {}};
}

void addChild(Node* p, Node* c)
{
   if (p && c) p->children.push_back(c);
}

// ------------------------------------------------------------------
// Symbol table / Scope helpers
// ------------------------------------------------------------------
string scopedKey(const string& n, const string& sc)
{
   return sc + "::" + n;
}

const SymInfo* symLookupInScope(const string& n, const string& sc)
{
   auto it = symTable.find(scopedKey(n, sc));
   if (it == symTable.end()) return nullptr;
   return &it->second;
}

const SymInfo* symLookup(const string& n)
{
   if (currentFunction != "global")
   {
      const SymInfo* local = symLookupInScope(n, currentFunction);
      if (local) return local;
   }
   return symLookupInScope(n, "global");
}

const SymInfo* symLookupGlobal(const string& n)
{
   return symLookupInScope(n, "global");
}

bool symExistsCurrentScope(const string& n)
{
   return symLookupInScope(n, currentFunction) != nullptr;
}

void symInsert(string n, string t, int ln, string sc)
{
   string scope = sc.empty() ? currentFunction : sc;
   symTable[scopedKey(n, scope)] = SymInfo{n, t, ln, scope};
}

// ------------------------------------------------------------------
// Intermediate Representation: Three-address code generation
// ------------------------------------------------------------------
string newTmp()
{
   return "t" + to_string(++tmpCount);
}

string newLbl()
{
   return "L" + to_string(++lblCount);
}

void emit(string s)
{
   ir.push_back(s);
}

// ------------------------------------------------------------------
// Printing helpers
// ------------------------------------------------------------------
void printAST(Node* n, int d)
{
   if (!n) return;

   string suffix = n->value.empty() ? string("") : string(": ") + n->value;
   printf("%*s[%s%s]\n", d * 3, "", n->type.c_str(), suffix.c_str());

   for (auto c : n->children)
      printAST(c, d + 1);
}

void printSymTable()
{
   printf("\n3. Symbol table");
   printf("\n----------------------------------------\n");
   printf("%-15s %-12s %-6s %-12s\n", "Name", "Type", "Line", "Scope");
   printf("\n----------------------------------------\n");
   for (auto &e : symTable)
      printf("%-15s %-12s %-6d %-12s\n", e.second.name.c_str(), e.second.type.c_str(), e.second.line, e.second.scope.c_str());
}

// ------------------------------------------------------------------
// Semantic helpers
// ------------------------------------------------------------------
void semanticWarn(const string& msg)
{
   semanticWarnings++;
   semanticLogs.push_back(string("[Warning] line ") + to_string(yylineno) + ": " + msg);
}

void semanticError(const string& msg)
{
   semanticErrors++;
   semanticLogs.push_back(string("[Error] line ") + to_string(yylineno) + ": " + msg);
}

string exprType(Node* n)
{
   if (!n) return "unknown";
   if (n->type == "NUM")
      return n->value.find('.') != string::npos ? "double" : "int";
   if (n->type == "CHAR") return "char";
   if (n->type == "STR") return "string";
   if (n->type == "ID")
   {
      const SymInfo* s = symLookup(n->value);
      if (s) return s->type;
      return "unknown";
   }
   if (n->type == "NEG") return exprType(n->children[0]);
   if (n->type == "ADD" || n->type == "SUB" || n->type == "MUL" || n->type == "DIV")
   {
      string lt = exprType(n->children[0]);
      string rt = exprType(n->children[1]);
      if (lt == "double" || rt == "double") return "double";
      if (lt == "char" && rt == "char") return "char";
      return "int";
   }
   if (n->type == "GT") return "bool";
   if (n->type == "CALL") return "int";
   return "unknown";
}

bool typeCompatible(const string& lhs, const string& rhs)
{
   if (lhs == rhs) return true;
   if (lhs == "string" || rhs == "string") return false;
   if (lhs == "int" && (rhs == "char" || rhs == "double")) return true;
   if (lhs == "double" && (rhs == "int" || rhs == "char")) return true;
   if (lhs == "char" && rhs == "int") return true;
   return false;
}

// ------------------------------------------------------------------
// Intermediate Representation: Three-address code generation
// ------------------------------------------------------------------
string genIR(Node* n)
{
   if (!n) return "";

   if (n->type == "NUM") return n->value;
   if (n->type == "CHAR") return n->value;
   if (n->type == "STR") return n->value;
   if (n->type == "ID") return n->value;
   if (n->type == "PARAM" || n->type == "DECL") return "";

   if (n->type == "ASSIGN")
   {
      string r = genIR(n->children[1]);
      emit(n->children[0]->value + " = " + r);
      return n->children[0]->value;
   }

   if (n->type == "ADD" || n->type == "SUB" || n->type == "MUL" || n->type == "DIV" || n->type == "GT")
   {
      string ops[] = {"+", "-", "*", "/", ">"};
      string syms[] = {"ADD", "SUB", "MUL", "DIV", "GT"};
      string op;

      for (int i = 0; i < 5; i++)
         if (n->type == syms[i]) op = ops[i];

      string l = genIR(n->children[0]);
      string r = genIR(n->children[1]);
      string t = newTmp();

      emit(t + " = " + l + " " + op + " " + r);
      return t;
   }

   if (n->type == "NEG")
   {
      string r = genIR(n->children[0]);
      string t = newTmp();
      emit(t + " = 0 - " + r);
      return t;
   }

   if (n->type == "RETURN")
   {
      emit("return " + genIR(n->children[0]));
      return "";
   }

   if (n->type == "CALL")
   {
      for (auto c : n->children)
         emit("param " + genIR(c));

      string t = newTmp();
      emit(t + " = call " + n->value);
      return t;
   }

   if (n->type == "IF")
   {
      string cond = genIR(n->children[0]);
      string L1 = newLbl(), L2 = newLbl();

      emit("iffalse " + cond + " goto " + L1);
      genIR(n->children[1]);
      emit("goto " + L2);
      emit(L1 + ":");

      if (n->children.size() > 2)
         genIR(n->children[2]);

      emit(L2 + ":");
      return "";
   }

   if (n->type == "WHILE")
   {
      string S = newLbl(), E = newLbl();

      emit(S + ":");
      string cond = genIR(n->children[0]);
      emit("iffalse " + cond + " goto " + E);

      genIR(n->children[1]);
      emit("goto " + S);
      emit(E + ":");
      return "";
   }

   if (n->type == "FUNC")
   {
      emit("func " + n->value + ":");
      for (auto c : n->children)
         genIR(c);
      emit("endfunc");
      return "";
   }

   for (auto c : n->children)
      genIR(c);

   return "";
}

// ------------------------------------------------------------------
// Optimizer helpers
// ------------------------------------------------------------------
static bool isNumberLiteral(const string& s)
{
   if (s.empty()) return false;
   size_t i = 0;
   if (s[0] == '-') i = 1;
   bool hasDigit = false;
   bool hasDot = false;
   for (; i < s.size(); i++)
   {
      if (isdigit((unsigned char)s[i]))
      {
         hasDigit = true;
         continue;
      }
      if (s[i] == '.' && !hasDot)
      {
         hasDot = true;
         continue;
      }
      return false;
   }
   return hasDigit;
}

static bool isCharLiteral(const string& s)
{
   return s.size() >= 3 && s.front() == '\'' && s.back() == '\'';
}

static bool isStringLiteral(const string& s)
{
   return s.size() >= 2 && s.front() == '"' && s.back() == '"';
}

static bool isLiteralValue(const string& s)
{
   return isNumberLiteral(s) || isCharLiteral(s) || isStringLiteral(s);
}

static bool toDoubleValue(const string& s, double& out)
{
   if (!isNumberLiteral(s)) return false;
   try
   {
      out = stod(s);
      return true;
   }
   catch (...) { return false; }
}

static string toNumberString(double val)
{
   string s = to_string(val);
   while (!s.empty() && s.back() == '0') s.pop_back();
   if (!s.empty() && s.back() == '.') s.pop_back();
   if (s.empty() || s == "-0") s = "0";
   return s;
}

static bool isIdentifierOrTemp(const string& tok)
{
   if (tok.empty()) return false;
   if (isdigit((unsigned char)tok[0])) return false;
   if (tok == "call" || tok == "goto" || tok == "iffalse" || tok == "func" || tok == "return" || tok == "param" || tok == "endfunc") return false;
   if (tok.back() == ':') return false;
   for (char c : tok)
      if (!(isalnum((unsigned char)c) || c == '_')) return false;
   return true;
}

static vector<string> extractUsedVars(const string& expr)
{
   vector<string> vars;
   string cur;
   auto flush = [&]() {
      if (!cur.empty() && isIdentifierOrTemp(cur)) vars.push_back(cur);
      cur.clear();
   };
   for (char ch : expr)
   {
      if (isalnum((unsigned char)ch) || ch == '_') cur.push_back(ch);
      else flush();
   }
   flush();
   return vars;
}

Node* optimizeAST(Node* n)
{
   if (!n) return n;

   for (size_t i = 0; i < n->children.size(); i++)
      n->children[i] = optimizeAST(n->children[i]);

   if (n->type == "NEG" && n->children.size() == 1 && n->children[0] && n->children[0]->type == "NUM")
   {
      double val;
      if (toDoubleValue(n->children[0]->value, val))
      {
         string folded = toNumberString(-val);
         optimizeLogs.push_back("Constant folded: -" + n->children[0]->value + " -> " + folded);
         return make("NUM", folded);
      }
   }

   if ((n->type == "ADD" || n->type == "SUB" || n->type == "MUL" || n->type == "DIV" || n->type == "GT") && n->children.size() == 2)
   {
      Node* left = n->children[0];
      Node* right = n->children[1];
      if (left && right && left->type == "NUM" && right->type == "NUM")
      {
         double lv, rv;
         if (toDoubleValue(left->value, lv) && toDoubleValue(right->value, rv))
         {
            if (n->type == "DIV" && rv == 0.0) return n;
            double result = 0;
            if (n->type == "ADD") result = lv + rv;
            else if (n->type == "SUB") result = lv - rv;
            else if (n->type == "MUL") result = lv * rv;
            else if (n->type == "DIV") result = lv / rv;
            else if (n->type == "GT") result = lv > rv ? 1 : 0;

            string folded = toNumberString(result);
            optimizeLogs.push_back("Constant folded: " + left->value + " " +
               (n->type == "ADD" ? "+" : n->type == "SUB" ? "-" : n->type == "MUL" ? "*" : n->type == "DIV" ? "/" : ">") +
               " " + right->value + " -> " + folded);
            return make("NUM", folded);
         }
      }

      if (n->type == "ADD")
      {
         if (right && right->type == "NUM" && right->value == "0")
         {
            optimizeLogs.push_back("Removed redundant operation: x + 0 -> x");
            return left;
         }
         if (left && left->type == "NUM" && left->value == "0")
         {
            optimizeLogs.push_back("Removed redundant operation: 0 + x -> x");
            return right;
         }
      }
      if (n->type == "SUB" && right && right->type == "NUM" && right->value == "0")
      {
         optimizeLogs.push_back("Removed redundant operation: x - 0 -> x");
         return left;
      }
      if (n->type == "MUL")
      {
         if ((right && right->type == "NUM" && right->value == "1") || (left && left->type == "NUM" && left->value == "1"))
         {
            optimizeLogs.push_back("Removed redundant operation: x * 1 -> x");
            return right && right->type == "NUM" && right->value == "1" ? left : right;
         }
         if ((right && right->type == "NUM" && right->value == "0") || (left && left->type == "NUM" && left->value == "0"))
         {
            optimizeLogs.push_back("Removed redundant operation: x * 0 -> 0");
            return make("NUM", "0");
         }
      }
      if (n->type == "DIV")
      {
         if (right && right->type == "NUM" && right->value == "1")
         {
            optimizeLogs.push_back("Removed redundant operation: x / 1 -> x");
            return left;
         }
         if (left && left->type == "NUM" && left->value == "0")
         {
            optimizeLogs.push_back("Removed redundant operation: 0 / x -> 0");
            return make("NUM", "0");
         }
      }
   }

   if (n->type == "IF" && n->children.size() >= 2)
   {
      Node* cond = n->children[0];
      if (cond && cond->type == "NUM")
      {
         double val;
         if (toDoubleValue(cond->value, val))
         {
            if (val != 0)
            {
               optimizeLogs.push_back("Removed dead branch: if(true) kept then-block");
               return n->children[1];
            }
            optimizeLogs.push_back("Removed dead branch: if(false) kept else-block");
            if (n->children.size() > 2) return n->children[2];
            return make("STMTS");
         }
      }
   }

   if (n->type == "WHILE" && n->children.size() == 2)
   {
      Node* cond = n->children[0];
      if (cond && cond->type == "NUM")
      {
         double val;
         if (toDoubleValue(cond->value, val) && val == 0)
         {
            optimizeLogs.push_back("Removed dead loop: while(false)");
            return make("STMTS");
         }
      }
   }

   return n;
}

void optimize()
{
   vector<string> propagated;
   map<string, string> constMap;

   for (auto &line : ir)
   {
      string s = line;

      if (s.empty()) continue;
      if (s.back() == ':' || s.find("goto ") == 0 || s.find("iffalse ") == 0 || s.find("func ") == 0 || s == "endfunc")
         constMap.clear();

      if (s.find(" = ") != string::npos)
      {
         size_t eq = s.find(" = ");
         string lhs = s.substr(0, eq);
         string rhs = s.substr(eq + 3);

         if (lhs == rhs)
         {
            optimizeLogs.push_back("Removed redundant copy: " + s);
            constMap.erase(lhs);
            continue;
         }

         if (rhs.find("call ") == string::npos)
         {
            vector<string> vars = extractUsedVars(rhs);
            for (auto &v : vars)
            {
               if (constMap.count(v))
               {
                  size_t pos = 0;
                  while ((pos = rhs.find(v, pos)) != string::npos)
                  {
                     bool leftOk = pos == 0 || !(isalnum((unsigned char)rhs[pos - 1]) || rhs[pos - 1] == '_');
                     bool rightOk = pos + v.size() >= rhs.size() || !(isalnum((unsigned char)rhs[pos + v.size()]) || rhs[pos + v.size()] == '_');
                     if (leftOk && rightOk)
                     {
                        rhs.replace(pos, v.size(), constMap[v]);
                        pos += constMap[v].size();
                     }
                     else pos += v.size();
                  }
               }
            }

            stringstream ss(rhs);
            string a, op, b;
            ss >> a >> op >> b;
            if (!a.empty() && !op.empty() && !b.empty() && isNumberLiteral(a) && isNumberLiteral(b))
            {
               double av, bv;
               if (toDoubleValue(a, av) && toDoubleValue(b, bv))
               {
                  bool foldable = true;
                  double rv = 0;
                  if (op == "+") rv = av + bv;
                  else if (op == "-") rv = av - bv;
                  else if (op == "*") rv = av * bv;
                  else if (op == "/")
                  {
                     if (bv == 0) foldable = false;
                     else rv = av / bv;
                  }
                  else if (op == ">") rv = av > bv ? 1 : 0;
                  else foldable = false;

                  if (foldable)
                  {
                     string folded = toNumberString(rv);
                     optimizeLogs.push_back("Constant folded IR: " + rhs + " -> " + folded);
                     rhs = folded;
                  }
               }
            }
         }

         s = lhs + " = " + rhs;
         if (isLiteralValue(rhs)) constMap[lhs] = rhs;
         else constMap.erase(lhs);
      }
      else if (s.find("return ") == 0)
      {
         string val = s.substr(7);
         if (constMap.count(val)) s = "return " + constMap[val];
      }

      propagated.push_back(s);
   }

   vector<string> cleaned;
   set<string> live;
   bool conservativeZone = false;
   for (int i = (int)propagated.size() - 1; i >= 0; i--)
   {
      string s = propagated[i];
      bool keep = true;

      bool isControlFlow = s.back() == ':' || s.find("goto ") == 0 || s.find("iffalse ") == 0;
      if (isControlFlow) conservativeZone = true;
      if (s.find("func ") == 0 || s == "endfunc")
      {
         conservativeZone = false;
         live.clear();
      }

      if (s.find("return ") == 0)
      {
         string expr = s.substr(7);
         for (auto &v : extractUsedVars(expr)) live.insert(v);
      }
      else if (s.find("param ") == 0)
      {
         for (auto &v : extractUsedVars(s.substr(6))) live.insert(v);
      }
      else if (s.find("iffalse ") == 0)
      {
         size_t g = s.find(" goto ");
         if (g != string::npos)
         {
            string cond = s.substr(8, g - 8);
            for (auto &v : extractUsedVars(cond)) live.insert(v);
         }
      }
      else if (s.find(" = ") != string::npos)
      {
         size_t eq = s.find(" = ");
         string lhs = s.substr(0, eq);
         string rhs = s.substr(eq + 3);
         bool hasSideEffect = rhs.find("call ") == 0;
         for (auto &v : extractUsedVars(rhs)) live.insert(v);

         if (!conservativeZone && !hasSideEffect && !live.count(lhs))
         {
            keep = false;
            optimizeLogs.push_back("Removed dead assignment: " + s);
         }
         live.erase(lhs);
      }

      if (keep) cleaned.push_back(s);
   }

   reverse(cleaned.begin(), cleaned.end());
   ir = cleaned;
}

// ------------------------------------------------------------------
// Code generation
// ------------------------------------------------------------------
void codegen()
{
   for (auto &s : ir)
   {
      if (s.back() == ':') printf("  %s\n", s.c_str());
      else if (s.find("func ") == 0) printf("%s\n  push ebp\n  mov ebp, esp\n", s.c_str() + 5);
      else if (s == "endfunc") printf("  pop ebp\n  ret\n");
      else if (s.find("return ") == 0) printf("  mov eax, %s\n", s.c_str() + 7);
      else if (s.find("param ") == 0) printf("  push %s\n", s.c_str() + 6);
      else if (s.find(" = call ") != string::npos)
      {
         auto e = s.find(" = call ");
         string d = s.substr(0, e);
         string f = s.substr(e + 8);

         printf("  call %s\n  mov %s, eax\n", f.c_str(), d.c_str());
      }
      else if (s.find(" = ") != string::npos)
      {
         auto e = s.find(" = ");
         printf("  mov eax, %s\n  mov %s, eax\n", s.substr(e + 3).c_str(), s.substr(0, e).c_str());
      }
      else if (s.find("iffalse ") == 0)
      {
         auto g = s.find(" goto ");
         printf("  cmp %s, 0\n  je %s\n", s.substr(8, g - 8).c_str(), s.substr(g + 6).c_str());
      }
      else if (s.find("goto ") == 0) printf("  jmp %s\n", s.c_str() + 5);
   }
}
