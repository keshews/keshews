#include "semantic_analyzer.hpp"
#include "ast.hpp"
#include "diagnostic.hpp"
#include "lexer.hpp"
#include <cstddef>
#include <format>
#include <string>
#include <type_traits>
#include <variant>

std::string returnTypeString(Type t) {
  switch (t) {
    case Type::String:
      return "string";
    case Type::Boolean:
      return "boolean";
    case Type::Double:
      return "double";
    case Type::Int:
      return "int";
    default:
      return "unknown_type";
  }
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
      return "unknown_type";
  }
}

bool doesTypeMatch(tokenType Ttype, Type type) {
  return ((Ttype == tokenType::INT && type == Type::Int) || (Ttype == tokenType::STRING && type == Type::String) ||
          (Ttype == tokenType::DOUBLE && (type == Type::Double || type == Type::Int)) ||
          (Ttype == tokenType::BOOL && type == Type::Boolean));
}

void Semantic_Analyzer::analyzeStatement(ast::statements& stmt) {
  std::visit([this](auto& node) { analyze(node); }, stmt);
}

void Semantic_Analyzer::analyze(ast::container& node) {
  for (auto& statement : node.stmts) {
    analyzeStatement(statement);
  }
}

void Semantic_Analyzer::analyze(std::unique_ptr<ast::if_statement>& node) {
  Type condType = analyze(node->condition);
  if (condType != Type::Boolean) {
    throw CompileError(
        "Semantic", std::format("condition in 'if' statement must be a boolean, got '{}'", returnTypeString(condType)));
  }

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

  Type condType = analyze(node->condition);
  if (condType != Type::Boolean) {
    throw CompileError("Semantic",
                       std::format("condition in 'for' loop must be a boolean, got '{}'", returnTypeString(condType)));
  }

  analyze(node->increment);
  analyze(*node->body);
  table.exitScope();
}

void Semantic_Analyzer::analyze(std::unique_ptr<ast::print_statement>& node) {
  analyze(node->expression); // Ensures variables inside print(a) actually exist!
}

void Semantic_Analyzer::analyze(std::unique_ptr<ast::while_statement>& node) {
  Type condType = analyze(node->condition);
  if (condType != Type::Boolean) {
    throw CompileError(
        "Semantic", std::format("condition in 'while' loop must be a boolean, got '{}'", returnTypeString(condType)));
  }

  table.enterScope();
  analyze(*node->body);
  table.exitScope();
}

Type Semantic_Analyzer::analyze(std::unique_ptr<ast::expr>& node) {
  return std::visit(
      [this](auto& value) -> Type {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, ast::int_val>) {
          return Type::Int;
        } else if constexpr (std::is_same_v<T, ast::double_val>) {
          return Type::Double;
        } else if constexpr (std::is_same_v<T, ast::string_val>) {
          return Type::String;
        } else if constexpr (std::is_same_v<T, ast::bool_val>) {
          return Type::Boolean;
        } else if constexpr (std::is_same_v<T, ast::identifier_val>) {
          symbol* sym = table.lookUp(value.name);
          if (sym == nullptr) {
            throw CompileError("Semantic", std::format("undefined identifier '{}'", value.name));
          }
          return sym->type;
        } else if constexpr (std::is_same_v<T, ast::group_expr>) {
          return analyze(value.expression);
        } else if constexpr (std::is_same_v<T, ast::binary_expression>) {
          Type leftType = analyze(value.left_val);
          Type rightType = analyze(value.right_val);
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
      if (left == Type::String && right == Type::String)
        return Type::String;

      throw CompileError("Semantic", oper.loc,
                         std::format("operator '+' cannot be applied to types '{}' and '{}'", returnTypeString(left),
                                     returnTypeString(right)));

    case tokenType::STAR:
    case tokenType::MINUS:
    case tokenType::SLASH:
      if (left == Type::Int && right == Type::Int)
        return Type::Int;
      if ((left == Type::Int || left == Type::Double) && (right == Type::Int || right == Type::Double))
        return Type::Double;

      throw CompileError("Semantic", oper.loc,
                         std::format("operator '{}' cannot be applied to types '{}' and '{}'", oper.value,
                                     returnTypeString(left), returnTypeString(right)));

    // Bitwise
    case tokenType::BIT_AND:
    case tokenType::BIT_OR:
    case tokenType::BIT_XOR:
      if (left == Type::Int && right == Type::Int)
        return Type::Int;

      throw CompileError("Semantic", oper.loc,
                         std::format("bitwise operator '{}' requires integer operands, got '{}' and '{}'", oper.value,
                                     returnTypeString(left), returnTypeString(right)));

    // Relational comparisons
    case tokenType::LESSER:
    case tokenType::GREATER:
    case tokenType::LESSER_EQUAL:
    case tokenType::GREATER_EQUAL:
      if ((left == Type::Int || left == Type::Double) && (right == Type::Int || right == Type::Double))
        return Type::Boolean;

      throw CompileError("Semantic", oper.loc,
                         std::format("relational operator '{}' requires numeric operands, got '{}' and '{}'",
                                     oper.value, returnTypeString(left), returnTypeString(right)));

    // Equality comparisons
    case tokenType::EQUAL_EQUAL:
    case tokenType::BANG_EQUAL:
      if (left == right ||
          ((left == Type::Int || left == Type::Double) && (right == Type::Int || right == Type::Double)))
        return Type::Boolean;

      throw CompileError("Semantic", oper.loc,
                         std::format("cannot compare incompatible types '{}' and '{}' with '{}'",
                                     returnTypeString(left), returnTypeString(right), oper.value));

    // Logical operations
    case tokenType::AND:
    case tokenType::OR:
      if (left == Type::Boolean && right == Type::Boolean)
        return Type::Boolean;

      throw CompileError("Semantic", oper.loc,
                         std::format("logical operator '{}' requires boolean operands, got '{}' and '{}'", oper.value,
                                     returnTypeString(left), returnTypeString(right)));

    default:
      throw CompileError("Semantic", oper.loc, std::format("unsupported binary operator '{}'", oper.value));
  }
}

void Semantic_Analyzer::analyze(ast::assignment& node) {
  symbol* identifier = Semantic_Analyzer::table.lookUp(node.identifier->name);
  if (identifier == nullptr) {
    throw CompileError("Semantic", std::format("cannot assign to undeclared variable '{}'", node.identifier->name));
  }

  Type type = analyze(node.expression);
  if (type != identifier->type && !(type == Type::Int && identifier->type == Type::Double)) {
    throw CompileError("Semantic",
                       std::format("type mismatch: cannot assign value of type '{}' to variable '{}' of type '{}'",
                                   returnTypeString(type), node.identifier->name, returnTypeString(identifier->type)));
  }
}

void Semantic_Analyzer::analyze(ast::declaration& node) {
  Type type = analyze(node.init);
  if (!doesTypeMatch(node.type, type)) {
    throw CompileError("Semantic",
                       std::format("type mismatch in declaration of '{}': declared as '{}', initialized with '{}'",
                                   node.identifier->name, returnTokenTypeString(node.type), returnTypeString(type)));
  }

  if (!table.declare(node.identifier->name,
                     {node.type == tokenType::DOUBLE && type == Type::Int ? Type::Double : type})) {
    throw CompileError("Semantic",
                       std::format("redeclaration of variable '{}' in the same scope", node.identifier->name));
  }
}

Semantic_Analyzer::Semantic_Analyzer() {
  table.enterScope();
}
