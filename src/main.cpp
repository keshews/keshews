#include "lexer.cpp"
#include "parser.cpp"
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
  std::string input = buffer.str();

  // 4. Run the lexer and parser
  Lexer lex = Lexer(input);
  auto tokens = lex.get_tokens();
  Parser parser = Parser(tokens);
  parser.parse();

  return 0;
}