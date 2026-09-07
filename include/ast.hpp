#include "lexer.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

#pragma once

namespace ast {

// expresisons

struct expr;

typedef struct {
  std::string val;
} int_val;

typedef struct {
  std::string val;
} double_val;

typedef struct {
  std::string val;
} string_val;

typedef struct {
  std::string name;
} identifier_val;

typedef struct {
  std::unique_ptr<expr> left_val;
  token oper;
  std::unique_ptr<expr> right_val;
} binary_expression;

typedef struct {
  token left_par;
  std::unique_ptr<expr> expression;
  token right_par;
} group_expr;

using ExprType = std::variant<int_val, double_val, string_val, binary_expression, group_expr, identifier_val>;

struct expr {
  ExprType val;
};

// <id> = <expr>
struct assignment {
  std::unique_ptr<identifier_val> identifier;
  std::unique_ptr<expr> expression;
};

// <type> <id> = <expr>
struct declaration {
  tokenType type;
  std::unique_ptr<identifier_val> identifier;
  std::unique_ptr<expr> init;
};

identifier_val createIdentifierNode(std::string val);
int_val createIntNode(std::string val);
double_val createDoubleNode(std::string val);
string_val createStringNode(std::string val);
group_expr createGroupExprNode(token left_par, std::unique_ptr<expr> expression, token right_par);
std::unique_ptr<expr> createExprNode(ExprType val);
std::unique_ptr<expr> createBinaryExprNode(std::unique_ptr<expr> left_val, token oper, std::unique_ptr<expr> right_val);
declaration createDeclarationNode(tokenType type, std::unique_ptr<identifier_val> identifier,
                                  std::unique_ptr<expr> init);
assignment createAssignmentNode(std::unique_ptr<identifier_val> identifier, std::unique_ptr<expr> expression);

// container
// could be wither {...} or ...
// that is either code enclosed in curly braces or just normal code

struct container;

// conditional statements

struct if_statement;
struct else_statement;

using next_branch_type = std::variant<std::monostate, std::unique_ptr<if_statement>, std::unique_ptr<else_statement>>;

struct else_statement {
  std::unique_ptr<container> then;
};

struct if_statement {
  std::unique_ptr<expr> condition;
  std::unique_ptr<container> then;
  next_branch_type nxt_branch;
};

std::unique_ptr<if_statement> createIfStatementNode(std::unique_ptr<expr> condition, std::unique_ptr<container> then,
                                                    next_branch_type next_branch);

std::unique_ptr<else_statement> createElseStatementNode(std::unique_ptr<container> then);

// loop statements

struct for_statement;
struct while_statement;

using init_type = std::variant<std::monostate, assignment, declaration>;

struct for_statement {
  init_type init;
  std::unique_ptr<expr> condition;
  assignment increment;
  std::unique_ptr<container> body;
};

struct while_statement {
  std::unique_ptr<expr> condition;
  std::unique_ptr<container> body;
};

std::unique_ptr<for_statement> createForStatementNode(init_type type, std::unique_ptr<expr> condition,
                                                      assignment increment, std::unique_ptr<container> body);

std::unique_ptr<while_statement> createWhileStatementNode(std::unique_ptr<expr> condition,
                                                          std::unique_ptr<container> body);

using statements = std::variant<std::unique_ptr<expr>, std::unique_ptr<if_statement>, std::unique_ptr<for_statement>,
                                std::unique_ptr<while_statement>, assignment, declaration>;

struct container {
  std::vector<statements> stmts;
};

std::unique_ptr<container> createContainerNode(std::vector<statements>);

}; // namespace ast
