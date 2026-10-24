#include "ast.hpp"
#include "symbol_table.hpp"
#include <memory>

class Semantic_Analyzer {

  symbolTable table;

  void analyzeStatement(ast::statements&);
  Type analyze(std::unique_ptr<ast::expr>& node);
  Type analyzeBinaryOperator(Type left, const token& oper, Type right);
  void analyze(std::unique_ptr<ast::if_statement>& node);
  void analyze(std::unique_ptr<ast::else_statement>& node);
  void analyze(std::unique_ptr<ast::for_statement>& node);
  void analyze(std::unique_ptr<ast::while_statement>& node);
  void analyze(ast::assignment& node);
  void analyze(ast::declaration& node);
  void analyze(std::unique_ptr<ast::print_statement>& node);

public:
  Semantic_Analyzer();
  void analyze(ast::container& node);
};
