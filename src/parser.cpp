#include "parser.hpp"
#include "ast.hpp"
#include "diagnostic.hpp"
#include "lexer.hpp"
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

token Parser::current_token() {
  return tokens[pos];
}

void Parser::advance_token() {
  pos++;
}

bool Parser::is_eof() {
  return (unsigned long)pos >= tokens.size();
}

token Parser::peek_token() {
  return (unsigned long)pos + 1 >= tokens.size() ? token{} : tokens[pos + 1];
}

ast::statements Parser::parseStatement() {
  if (current_token().tok_type == tokenType::INT || current_token().tok_type == tokenType::DOUBLE ||
      current_token().tok_type == tokenType::STRING || current_token().tok_type == tokenType::BOOL) {
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
  } else if (current_token().tok_type == tokenType::PRINT) {
    return parsePrintStatement();
  } else {
    throw CompileError("Parser", current_token().loc,
                       std::format("invalid start of statement '{}'", current_token().value));
  }
}

std::unique_ptr<ast::print_statement> Parser::parsePrintStatement() {
    advance_token();
  if (current_token().tok_type != tokenType::LEFT_PARENTHESES)
    throw CompileError("Parser", current_token().loc, "expected '(' after 'print'");
  advance_token();
  auto expression = parseExpression();
  if (current_token().tok_type != tokenType::RIGHT_PARENTHESES)
    throw CompileError("Parser", current_token().loc, "expected ')' after print expression");
  advance_token();
  return std::make_unique<ast::print_statement>(ast::print_statement{std::move(expression)});
}

bool Parser::isDataType(token tok) {
  return tok.tok_type == tokenType::INT || tok.tok_type == tokenType::DOUBLE || tok.tok_type == tokenType::STRING ||
         tok.tok_type == tokenType::BOOL;
}

bool Parser::isCompoundAssignmentOperator(token tok) {
  return tok.tok_type == tokenType::PLUS_EQUAL || tok.tok_type == tokenType::MINUS_EQUAL ||
         tok.tok_type == tokenType::STAR_EQUAL || tok.tok_type == tokenType::SLASH_EQUAL;
}

ast::declaration Parser::parseDeclarationExpression() {
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
      throw CompileError("Parser", current_token().loc,
                         std::format("expected identifier after type, got '{}'", current_token().value));
    }

    if (current_token().tok_type == tokenType::EQUAL) {
      advance_token();
      init = parseExpression();
    } else {
      throw CompileError("Parser", current_token().loc,
                         std::format("expected '=' in variable declaration, got '{}'", current_token().value));
    }

    return ast::createDeclarationNode(type, std::move(identifier), std::move(init));
  } else {
    throw CompileError("Parser", current_token().loc,
                       std::format("expected data type, got '{}'", current_token().value));
  }
}

ast::assignment Parser::parseAssignmentExpression() {
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
      throw CompileError("Parser", current_token().loc,
                         std::format("expected assignment operator, got '{}'", current_token().value));
    }

    return ast::createAssignmentNode(std::move(identifier), std::move(expr));
  } else {
    throw CompileError("Parser", current_token().loc,
                       std::format("expected identifier, got '{}'", current_token().value));
  }
}

bool Parser::isOperator(token tok) {
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

bool Parser::isLiteral(token tok) {
  return tok.tok_type == tokenType::INT_VAL || tok.tok_type == tokenType::DOUBLE_VAL ||
         tok.tok_type == tokenType::STRING_VAL || tok.tok_type == tokenType::TRUE || tok.tok_type == tokenType::FALSE;
}

bool Parser::isIdentifier(token tok) {
  return tok.tok_type == tokenType::IDENTIFIER;
}

std::unique_ptr<ast::expr> Parser::createOperandNode(token tok) {
  switch (tok.tok_type) {
    case tokenType::IDENTIFIER:
      return ast::createExprNode(ast::createIdentifierNode(tok.value));
    case tokenType::STRING_VAL:
      return ast::createExprNode(ast::createStringNode(tok.value));
    case tokenType::INT_VAL:
      return ast::createExprNode(ast::createIntNode(tok.value));
    case tokenType::DOUBLE_VAL:
      return ast::createExprNode(ast::createDoubleNode(tok.value));
    case tokenType::TRUE:
      return ast::createExprNode(ast::createBoolNode(true));
    case tokenType::FALSE:
      return ast::createExprNode(ast::createBoolNode(false));
    default:
      throw CompileError("Parser", tok.loc,
                         std::format("expected operand (identifier or literal), got '{}'", tok.value));
  }
}

std::unique_ptr<ast::expr> Parser::parseExpression(int threshold) {
  std::unique_ptr<ast::expr> lhs;
  if (isIdentifier(current_token()) || isLiteral(current_token())) {
    lhs = createOperandNode(current_token());
    advance_token();
  } else if (current_token().tok_type == tokenType::LEFT_PARENTHESES) {
    advance_token();
    lhs = parseExpression(0);
    if (current_token().tok_type != tokenType::RIGHT_PARENTHESES) {
      throw CompileError("Parser", current_token().loc, std::format("expected ')', got '{}'", current_token().value));
    }
    advance_token();
  } else {
    throw CompileError("Parser", current_token().loc,
                       std::format("unexpected token in expression '{}'", current_token().value));
  }

  while (!is_eof() && isOperator(current_token())) {
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

std::unique_ptr<ast::if_statement> Parser::parseConditionalStatement() {
  if (current_token().tok_type != tokenType::IF && current_token().tok_type != tokenType::ELIF) {
    throw CompileError("Parser", current_token().loc,
                       std::format("expected 'if' or 'elif', got '{}'", current_token().value));
  }

  advance_token();
  auto condition = parseExpression();
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

std::unique_ptr<ast::for_statement> Parser::parseForStatement() {
  if (current_token().tok_type != tokenType::LEFT_PARENTHESES) {
    throw CompileError("Parser", current_token().loc,
                       std::format("expected '(' after 'for', got '{}'", current_token().value));
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
    throw CompileError("Parser", current_token().loc,
                       std::format("expected ';' after for-init, got '{}'", current_token().value));
  }

  ast::assignment increment;

  if (current_token().tok_type == tokenType::SEMI_COLON) {
    advance_token();
    increment = parseAssignmentExpression();
  } else {
    throw CompileError("Parser", current_token().loc,
                       std::format("expected ';' after for-condition, got '{}'", current_token().value));
  }

  if (current_token().tok_type != tokenType::RIGHT_PARENTHESES) {
    throw CompileError("Parser", current_token().loc,
                       std::format("expected ')' after for-increment, got '{}'", current_token().value));
  }

  advance_token();
  std::unique_ptr<ast::container> stmts = ast::createContainerNode(parseBlock());
  return ast::createForStatementNode(std::move(init), std::move(condition), std::move(increment), std::move(stmts));
}

std::unique_ptr<ast::while_statement> Parser::parseWhileLoop() {
  advance_token();
  std::unique_ptr<ast::expr> condition = parseExpression();
  std::unique_ptr<ast::container> body = ast::createContainerNode(parseBlock());
  return ast::createWhileStatementNode(std::move(condition), std::move(body));
}

std::vector<ast::statements> Parser::parseBlock() {
  std::vector<ast::statements> stmts;
  if (current_token().tok_type != tokenType::LEFT_CURLY_BRACES) {
    throw CompileError("Parser", current_token().loc, std::format("expected '{{', got '{}'", current_token().value));
  }
  advance_token();
  while (!is_eof() && current_token().tok_type != tokenType::RIGHT_CURLY_BRACES) {
    stmts.push_back(parseStatement());
  }
  if (is_eof()) {
    throw CompileError("Parser", pos > 0 ? tokens[pos - 1].loc : sourceLocation{1, 1},
                       "expected '}' before end of file");
  }
  advance_token();
  return stmts;
}

std::unique_ptr<ast::container> Parser::parse() {
  std::vector<ast::statements> stmts;
  while (!is_eof()) {
    stmts.push_back(parseStatement());
  }
  return ast::createContainerNode(std::move(stmts));
}
