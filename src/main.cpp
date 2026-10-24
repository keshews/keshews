#include "diagnostic.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic_analyzer.hpp"
#include "tac.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    run_repl();
    return 0;
  }

  std::ifstream file(argv[1]);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file " << argv[1] << "\n";
    return 1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string sourceCode = buffer.str();

  try {
    Lexer lex(sourceCode);
    Parser parser(lex.get_tokens());
    auto program = parser.parse();

    Semantic_Analyzer analyzer;
    analyzer.analyze(*program);

    return run_JIT(*program, true);

  } catch (const CompileError& e) {
    Diagnostic::print(sourceCode, argv[1], e);
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
