#include "semantic_analyzer.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include <cstddef>
#include <cstdio>
#include <format>
#include <variant>

[[noreturn]] inline void semanticError(std::string message) {
  throw std::runtime_error("Semantic Error: " + std::move(message));
}

std::string returnTypeString(Type t) {
  if (t == Type::String)
    return "string";
  if (t == Type::Boolean)
    return "boolean";
  if (t == Type::Double)
    return "double";
  if (t == Type::Int)
    return "int";
  return "unknown_type";
}

void Semantic_Analyzer::analyzeStatement(ast::statements& stmt) {
  std::visit([this](auto& node) { analyze(node); }, stmt);
}

void Semantic_Analyzer::analyze(ast::container& node) {
  for (auto& statements : node.stmts) {
    analyzeStatement(statements);
  }
}

void Semantic_Analyzer::analyze(std::unique_ptr<ast::if_statement>& node) {
  if (analyze(node->condition) == Type::Boolean) {
    table.enterScope();
    analyze(*node->then);
    table.exitScope();
    std::visit(
        [this](auto& branch) {
          using T = std::decay_t<decltype(branch)>;

          if constexpr (!std::is_same_v<T, std::monostate>) {
            analyze(branch);
          }
        },
        node->nxt_branch);
    return;
  }
  semanticError("invalid if statement, the condition is expected to be a boolean");
}

void Semantic_Analyzer::analyze(std::unique_ptr<ast::else_statement>& node) {
  table.enterScope();
  analyze(*node->then);
  table.exitScope();
}

void Semantic_Analyzer::analyze(std::unique_ptr<ast::for_statement>& node) {
  table.enterScope();
  std::visit(
      [this](auto& init) {
        using T = std::decay_t<decltype(init)>;

        if constexpr (!std::is_same_v<T, std::monostate>) {
          analyze(init);
        }
      },
      node->init);

  if (analyze(node->condition) != Type::Boolean) {
    semanticError("For loop condition must be Boolean");
  }

  analyze(node->increment);
  analyze(*node->body);
  table.exitScope();
}

Type Semantic_Analyzer::analyze(std::unique_ptr<ast::expr>& node) {
  return std::visit(
      [this](auto& value) -> Type {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, ast::int_val>) {
          return Type::Int;
        }

        else if constexpr (std::is_same_v<T, ast::double_val>) {
          return Type::Double;
        }

        else if constexpr (std::is_same_v<T, ast::string_val>) {
          return Type::String;
        }

        else if constexpr (std::is_same_v<T, ast::identifier_val>) {
          symbol* symbol = table.lookUp(value.name);

          if (symbol == nullptr) {
            throw std::runtime_error("Undefined identifier: " + value.name);
          }

          return symbol->type;
        }

        else if constexpr (std::is_same_v<T, ast::group_expr>) {
          return analyze(value.expression);
        }

        else if constexpr (std::is_same_v<T, ast::binary_expression>) {
          Type leftType = analyze(value.left_val);
          Type rightType = analyze(value.right_val);

          // Check operator compatibility here.
          return analyzeBinaryOperator(leftType, value.oper, rightType);
        }
      },
      node->val);
}

Type Semantic_Analyzer::analyzeBinaryOperator(Type left, const token& oper, Type right) {
  switch (oper.tok_type) {

    // Arithmetic
    case tokenType::PLUS:
      if (left == Type::Int && right == Type::Int)
        return Type::Int;
      if ((left == Type::Int || left == Type::Double) && (right == Type::Int || right == Type::Double))
        return Type::Double;

      if (left == Type::String && right == Type::String) {
        return Type::String;
      }

      else {
        std::string msg = std::format("Invalid Type in expression: type {} and type {}", returnTypeString(left),
                                      returnTypeString(right));

        semanticError(msg);
      }
      break;
    case tokenType::STAR:
    case tokenType::MINUS:
    case tokenType::SLASH: 
      if (left == Type::Int && right == Type::Int)
        return Type::Int;

      if ((left == Type::Int || left == Type::Double) && (right == Type::Int || right == Type::Double))
        return Type::Double;


      else {
        std::string msg = std::format("Invalid Type in expression: type {} and type {}", returnTypeString(left),returnTypeString(right));

        semanticError(msg);
      }

      break;

    // Comparisons
    case tokenType::LESSER:
    case tokenType::GREATER:
    case tokenType::LESSER_EQUAL:
    case tokenType::GREATER_EQUAL:
      if ((left == Type::Int || left == Type::Double) && (right == Type::Int || right == Type::Double))
        return Type::Boolean;

      break;

    case tokenType::EQUAL:
    case tokenType::BANG_EQUAL:
      if (left == right)
        return Type::Boolean;

      break;

    // Logical
    case tokenType::AND:
    case tokenType::OR:
      if (left == Type::Boolean && right == Type::Boolean)
        return Type::Boolean;

      break;

    default:
      throw std::runtime_error("Invalid opertors for binary operator");
  }

  // Invalid operator/type combination
  throw std::runtime_error("Invalid operands for binary operator");
}

void Semantic_Analyzer::analyze(std::unique_ptr<ast::while_statement>& node) {
  if (analyze(node->condition) == Type::Boolean) {
    analyze(*node->body);
    return;
  }
  semanticError("while loop condition must be boolean");
}

void Semantic_Analyzer::analyze(ast::assignment& node) {
  symbol* identifier = Semantic_Analyzer::table.lookUp(node.identifier->name);
  if (identifier == nullptr) {
    semanticError(std::format("symbol not found {}",node.identifier->name));
  }
  Type type = analyze(node.expression);
  if (type != identifier->type) {
    semanticError(std::format("type mismatch {} {}",returnTypeString(type),returnTypeString(identifier->type)));
  }
  return;
}

bool doesTypeMatch(tokenType Ttype, Type type) {
  return (Ttype == tokenType::INT && type == Type::Int || Ttype == tokenType::STRING && type == Type::String ||
          Ttype == tokenType::DOUBLE && type == Type::Double);
}

std::string returnTokenTypeString(tokenType t) {
  switch (t) {
    case tokenType::DOUBLE:
    return "double";
    case tokenType::INT:
    return "int";
    case tokenType::STRING:
    return "string";
    case tokenType::BOOL:
    return "boolean";
    default:
    return "incorrect type for declaration";
  }
}


void Semantic_Analyzer::analyze(ast::declaration& node) {
  Type type = analyze(node.init);
  if (doesTypeMatch(node.type, type)) {
    table.declare(node.identifier->name, {type});
    return;
  }
  semanticError(std::format("{} not match {}",returnTokenTypeString(node.type),returnTypeString(type)));
}

Semantic_Analyzer::Semantic_Analyzer() {
  // global scope
  table.enterScope();
}

