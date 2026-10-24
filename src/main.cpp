#include "lexer.cpp"
#include "parser.cpp"
#include "semantic_analyzer.hpp"
#include "tac.hpp"
#include "diagnostic.hpp"
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filepath>\n";
    return 1;
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

    std::cout << generateTAC(*program);
  } catch (const CompileError& e) {
    Diagnostic::print(sourceCode, argv[1], e);
    return 1;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
  return 0;
}
