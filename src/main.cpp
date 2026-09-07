#include "lexer.cpp"
#include "parser.cpp"

int main() {
  std::string input = "while (i<n) {}";
  Lexer lex = Lexer(input);
  auto tokens = lex.get_tokens();
  Parser parser = Parser(tokens);
  parser.parse();
}
