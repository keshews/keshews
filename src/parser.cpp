#include "../include/ast.hpp"
#include "lexer.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class Parser {

private:
  std::vector<token> tokens;
  int pos = 0;

  // helper functions

  token current_token() {
    return tokens[pos];
  }

  void advance_token() {
    pos++;
  }

  bool isEof() {
    return pos >= tokens.size();
  }

  token peek_token() {
    return pos + 1 > tokens.size() ? token{} : tokens[pos + 1];
  }

  void printError(token tok, tokenType type) {}

  void printError(token tok) {}

  void printError(token tok, std::string msg) {
    std::cout << "expected " << msg << " got " << tok.value << std::endl;
    exit(1);
  }

  void printError(std::string msg) {}

  ast::statements parseStatement() {
    if (current_token().tok_type == tokenType::INT or current_token().tok_type == tokenType::DOUBLE or
        current_token().tok_type == tokenType::STRING) {
      return parseDeclarationExpression();
    } else if (current_token().tok_type == tokenType::IDENTIFIER) {
      return parseAssignmentExpression();
    } else if (current_token().tok_type == tokenType::IF) {
      return parseConditionalStatement();
    } else if (current_token().tok_type == tokenType::FOR) {
      advance_token();
      return parseForStatement();
    } else if (current_token().tok_type == tokenType::WHILE) {
      return parseWhileLoop();
    } else {
      printError(current_token());
      throw std::runtime_error("invalid start token");
    }
  }

  bool isDataType(token tok) {
    return tok.tok_type == tokenType::INT || tok.tok_type == tokenType::DOUBLE || tok.tok_type == tokenType::STRING;
  }

  bool isCompoundAssignmentOperator(token tok) {
    return tok.tok_type == tokenType::PLUS_EQUAL || tok.tok_type == tokenType::MINUS_EQUAL ||
           tok.tok_type == tokenType::SLASH_EQUAL || tok.tok_type == tokenType::SLASH_EQUAL;
  }

  ast::declaration parseDeclarationExpression() {
    // <type> <identifier> = <expr>
    if (isDataType(current_token())) {
      tokenType type = current_token().tok_type;
      std::unique_ptr<ast::identifier_val> identifier;
      std::unique_ptr<ast::expr> init;
      advance_token();

      if (current_token().tok_type == tokenType::IDENTIFIER) {
        identifier = std::make_unique<ast::identifier_val>(ast::createIdentifierNode(current_token().value));
        advance_token();
      } else {
        printError(current_token(), "IDENTIFIER");
      }

      if (current_token().tok_type == tokenType::EQUAL) {
        advance_token();
        init = parseExpression();
      } else {
        printError(current_token(), "EQUAL");
      }

      return ast::createDeclarationNode(type, std::move(identifier), std::move(init));
    } else {
      printError(current_token(), "INT, STRING or DOUBLE");
      throw std::runtime_error("invalid data type");
    }
  }

  ast::assignment parseAssignmentExpression() {
    // <identifier> < =| += | -= | *= | /=> <expr>
    if (current_token().tok_type == tokenType::IDENTIFIER) {
      std::unique_ptr<ast::identifier_val> identifier;
      std::unique_ptr<ast::expr> expr;
      identifier = std::make_unique<ast::identifier_val>(ast::createIdentifierNode(current_token().value));
      advance_token();

      if (current_token().tok_type == tokenType::EQUAL || isCompoundAssignmentOperator(current_token())) {
        advance_token();
        expr = parseExpression();
      } else {
        printError(current_token(), "ASSIGNMENT or COMPOUND_ASSIGNMENT");
      }

      return ast::createAssignmentNode(std::move(identifier), std::move(expr));
    }

    else {
      printError(current_token(), "IDENTIFIER");
      throw std::runtime_error("should be a identifier");
    }
  }

  bool isOperator(token tok) {
    switch (tok.tok_type) {
      case tokenType::OR:
      case tokenType::AND:
      case tokenType::BIT_OR:
      case tokenType::BIT_XOR:
      case tokenType::BIT_AND:

      case tokenType::EQUAL_EQUAL:
      case tokenType::BANG_EQUAL:
      case tokenType::GREATER:
      case tokenType::GREATER_EQUAL:
      case tokenType::LESSER:
      case tokenType::LESSER_EQUAL:

      case tokenType::PLUS:
      case tokenType::MINUS:
      case tokenType::STAR:
      case tokenType::SLASH:
        return true;

      default:
        return false;
    }
  }

  bool isLiteral(token tok) {
    return tok.tok_type == tokenType::INT_VAL || tok.tok_type == tokenType::DOUBLE_VAL ||
           tok.tok_type == tokenType::STRING_VAL;
  }

  bool isIdentifier(token tok) {
    return tok.tok_type == tokenType::IDENTIFIER;
  }

  std::unordered_map<tokenType, int> binding_power = {
      {tokenType::OR, 1},         {tokenType::AND, 2},          {tokenType::BIT_OR, 3},
      {tokenType::BIT_XOR, 4},    {tokenType::BIT_AND, 5},      {tokenType::EQUAL_EQUAL, 6},
      {tokenType::BANG_EQUAL, 6}, {tokenType::GREATER, 6},      {tokenType::GREATER_EQUAL, 6},
      {tokenType::LESSER, 6},     {tokenType::LESSER_EQUAL, 6}, {tokenType::PLUS, 7},
      {tokenType::MINUS, 7},      {tokenType::STAR, 8},         {tokenType::SLASH, 8},
  };

  std::unique_ptr<ast::expr> createOperandNode(token tok) {
    switch (tok.tok_type) {
      case tokenType::IDENTIFIER:
        return ast::createExprNode(ast::createIdentifierNode(tok.value));
      case tokenType::STRING_VAL:
        return ast::createExprNode(ast::createStringNode(tok.value));
      case tokenType::INT_VAL:
        return ast::createExprNode(ast::createIntNode(tok.value));
      case tokenType::DOUBLE_VAL:
        return ast::createExprNode(ast::createDoubleNode(tok.value));
      default:
        printError(tok, "IDENTIFIER or LITERAL");
        throw std::runtime_error("invalid operand");
    }
  }

  std::unique_ptr<ast::expr> parseExpression(int threshold = 0) {
    std::unique_ptr<ast::expr> lhs;
    if (isIdentifier(current_token()) || isLiteral(current_token())) {
      lhs = createOperandNode(current_token());
      advance_token();
    }

    else if (current_token().tok_type == tokenType::LEFT_PARANTHESES) {
      advance_token();
      auto lhs = parseExpression(0);
      if (current_token().tok_type != tokenType::RIGHT_PARANTHESES) {
        printError(current_token(), tokenType::RIGHT_PARANTHESES);
        throw std::runtime_error("expected ')'");
      }
      advance_token();
    }

    else {
      // TODO: change error message to suit the exisiting style later;
      printError("operator and operator can't exist");
      throw std::runtime_error("invalid operand");
    }

    while (!isEof() && isOperator(current_token())) {
      token oper = current_token();
      int op_bp = binding_power[current_token().tok_type];
      if (op_bp <= threshold) {
        break;
      }
      advance_token();
      auto rhs = parseExpression(op_bp);
      lhs = ast::createBinaryExprNode(std::move(lhs), oper, std::move(rhs));
    }
    return lhs;
  }

  std::unique_ptr<ast::if_statement> parseConditionalStatement() {
    if (current_token().tok_type != tokenType::IF && current_token().tok_type != tokenType::ELIF) {
      printError(current_token(), "IF or ELIF");
    }

    advance_token();
    auto condition = parseExpression();
    std::cout << current_token().value << pos << tokens.size() << std::endl;
    // advance_token();
    auto then = ast::createContainerNode(parseBlock());
    ast::next_branch_type nxt_branch;

    if (current_token().tok_type == tokenType::ELIF) {
      nxt_branch = parseConditionalStatement();
    } else if (current_token().tok_type == tokenType::ELSE) {
      advance_token();
      auto else_then = ast::createContainerNode(parseBlock());
      nxt_branch = ast::createElseStatementNode(std::move(else_then));
    } else {
      nxt_branch = std::monostate{};
    }

    return ast::createIfStatementNode(std::move(condition), std::move(then), std::move(nxt_branch));
  }

  std::unique_ptr<ast::for_statement> parseForStatement() {
    if (current_token().tok_type != tokenType::LEFT_PARANTHESES) {
      printError(current_token(), "LEFT_PARANTHESES");
    }

    advance_token();
    ast::init_type init;

    if (current_token().tok_type != tokenType::SEMI_COLON) {
      if (isDataType(current_token())) {
        init = parseDeclarationExpression();
      } else {
        init = parseAssignmentExpression();
      }
    }

    std::unique_ptr<ast::expr> condition;

    if (current_token().tok_type == tokenType::SEMI_COLON) {
      advance_token();
      condition = parseExpression();
    } else {
      printError(current_token(), "SEMI_COLON");
    }

    ast::assignment increment;

    if (current_token().tok_type == tokenType::SEMI_COLON) {
      advance_token();
      increment = parseAssignmentExpression();
    } else {
      printError(current_token(), "SEMI_COLON");
    }

    if (current_token().tok_type != tokenType::RIGHT_PARANTHESES) {
      printError(current_token(), tokenType::RIGHT_PARANTHESES);
    }

    advance_token();
    std::unique_ptr<ast::container> stmts = ast::createContainerNode(parseBlock());
    return ast::createForStatementNode(std::move(init), std::move(condition), std::move(increment), std::move(stmts));
  }

  std::unique_ptr<ast::while_statement> parseWhileLoop() {
    std::unique_ptr<ast::expr> condition = parseExpression();
    std::unique_ptr<ast::container> body = ast::createContainerNode(parseBlock());
    return ast::createWhileStatementNode(std::move(condition), std::move(body));
  }

  std::vector<ast::statements> parseBlock() {
    std::vector<ast::statements> stmts;
    if (current_token().tok_type != tokenType::LEFT_CURLY_BRACES) {
      printError(current_token(), "tokenType::LEFT_CURLY_BRACES");
      throw std::runtime_error("expected {");
    }
    advance_token();
    while (!isEof() && current_token().tok_type != tokenType::RIGHT_CURLY_BRACES) {
      stmts.push_back(parseStatement());
    }
    if (isEof()) {
      printError("expected } got EOF instead");
      throw std::runtime_error("expected } got EOF instead");
    }
    advance_token();
    return stmts;
  }

public:
  Parser(std::vector<token> tokens) {
    this->tokens = tokens;
  }
  std::vector<ast::statements> stmts;

  std::unique_ptr<ast::container> parse() {
    while (!isEof()) {
      stmts.push_back(parseStatement());
    }
    return ast::createContainerNode(std::move(stmts));
  }
};
