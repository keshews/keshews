#include "ast.hpp"
#include "lexer.hpp"
#include <memory>
#include <vector>
class Parser {
private:
  std::vector<token> tokens;
  int pos = 0;
  token current_token();
  void advance_token();
  bool is_eof();
  token peek_token();
  ast::statements parseStatement();
  bool isDataType(token tok);
  bool isCompoundAssignmentOperator(token tok);
  ast::assignment parseAssignmentExpression();
  bool isIdentifier(token tok);
  bool isLiteral(token tok);
  bool isOperator(token tok);
  std::unique_ptr<ast::expr> createOperandNode(token tok);
  ast::declaration parseDeclarationExpression();

  std::unordered_map<tokenType, int> binding_power = {
      {tokenType::STAR, 10},
      {tokenType::SLASH, 10},
      // {tokenType::MODULUS, 10},
      {tokenType::PLUS, 9},
      {tokenType::MINUS, 9},
      {tokenType::GREATER, 8},
      {tokenType::GREATER_EQUAL, 8},
      {tokenType::LESSER_EQUAL, 8},
      {tokenType::LESSER, 8},
      {tokenType::EQUAL_EQUAL, 7},
      {tokenType::BANG_EQUAL, 7},
      {tokenType::BIT_AND, 6},
      {tokenType::BIT_XOR, 5},
      {tokenType::BIT_OR, 4},
      {tokenType::AND, 3},
      {tokenType::OR, 2},
  };

  std::unique_ptr<ast::expr> parseExpression(int threshold = 0);
  std::unique_ptr<ast::if_statement> parseConditionalStatement();
  std::unique_ptr<ast::for_statement> parseForStatement();
  std::unique_ptr<ast::while_statement> parseWhileLoop();
  std::vector<ast::statements> parseBlock();

public:
  Parser(std::vector<token> tokens) {
    this->tokens = tokens;
  };
  std::unique_ptr<ast::container> parse();
};
