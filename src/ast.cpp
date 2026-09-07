#include "ast.hpp"
#include <memory>
#include <string>

// expressions
ast::int_val ast::createIntNode(std::string val) {
  return int_val{val};
}

ast::double_val ast::createDoubleNode(std::string val) {
  return double_val{val};
}

ast::string_val ast::createStringNode(std::string val) {
  return string_val{val};
}

ast::identifier_val ast::createIdentifierNode(std::string val) {
  return identifier_val{val};
}

ast::group_expr ast::createGroupExprNode(token left_par, std::unique_ptr<expr> expression, token right_par) {
  return group_expr{left_par, std::move(expression), right_par};
}

std::unique_ptr<ast::expr> ast::createExprNode(ast::ExprType val) {
  auto node = std::make_unique<ast::expr>();
  node->val = std::move(val);
  return node;
}

std::unique_ptr<ast::expr> ast::createBinaryExprNode(std::unique_ptr<ast::expr> left_val, token oper,
                                                     std::unique_ptr<ast::expr> right_val) {
  return std::make_unique<ast::expr>(
      ast::expr{ast::binary_expression{std::move(left_val), oper, std::move(right_val)}});
}

ast::declaration ast::createDeclarationNode(tokenType type, std::unique_ptr<identifier_val> identifier,
                                            std::unique_ptr<expr> init) {

  return declaration(type, std::move(identifier), std::move(init));
}

ast::assignment ast::createAssignmentNode(std::unique_ptr<identifier_val> identifier,
                                          std::unique_ptr<expr> expression) {
  return assignment{std::move(identifier), std::move(expression)};
}

// conditional statements
std::unique_ptr<ast::if_statement> ast::createIfStatementNode(std::unique_ptr<expr> condition,
                                                              std::unique_ptr<container> then,
                                                              next_branch_type next_branch) {
  return std::make_unique<if_statement>(if_statement{std::move(condition), std::move(then), std::move(next_branch)});
}

std::unique_ptr<ast::else_statement> ast::createElseStatementNode(std::unique_ptr<container> then) {
  return std::make_unique<else_statement>(else_statement{std::move(then)});
}

// loop statements
std::unique_ptr<ast::for_statement> ast::createForStatementNode(init_type type, std::unique_ptr<expr> condition,
                                                                assignment increment, std::unique_ptr<container> body) {
  return std::make_unique<for_statement>(
      for_statement{std::move(type), std::move(condition), std::move(increment), std::move(body)});
}

std::unique_ptr<ast::while_statement> ast::createWhileStatementNode(std::unique_ptr<expr> condition,
                                                                    std::unique_ptr<container> body) {
  return std::make_unique<while_statement>(while_statement{std::move(condition), std::move(body)});
}

// container
std::unique_ptr<ast::container> ast::createContainerNode(std::vector<statements> stmts) {
  return std::make_unique<container>(container{std::move(stmts)});
}
