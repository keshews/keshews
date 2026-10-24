#include <string>
#include <unordered_map>
#include <vector>

#pragma once

enum class tokenType {
  // keywords
  IF,
  ELSE,
  ELIF,

  FOR,
  WHILE,

  INT,
  DOUBLE,
  STRING,
  BOOL,

  TRUE,
  FALSE,

  // operators
  PLUS,
  MINUS,
  SLASH,
  STAR,

  EQUAL,

  BANG,
  EQUAL_EQUAL,
  BANG_EQUAL,

  GREATER,
  GREATER_EQUAL,

  LESSER,
  LESSER_EQUAL,

  PLUS_EQUAL,
  MINUS_EQUAL,
  SLASH_EQUAL,
  STAR_EQUAL,

  AND,
  OR,
  NOT,

  BIT_AND,
  BIT_OR,
  COMPLEMENT,
  BIT_XOR,

  // delimiters
  LEFT_PARENTHESES,
  RIGHT_PARENTHESES,
  LEFT_SQUARE_BRACKETS,
  RIGHT_SQUARE_BRACKETS,
  LEFT_CURLY_BRACES,
  RIGHT_CURLY_BRACES,

  SEMI_COLON,
  COLON,

  IDENTIFIER,
  INT_VAL,
  DOUBLE_VAL,
  STRING_VAL,
};

typedef struct {
    int line;
    int col;
} sourceLocation;

typedef struct {
  tokenType tok_type;
  std::string value;
  sourceLocation loc;
} token;

class Lexer {
private:
  int line = 1;
  int pos = 0;
  int s_pos = 0;
  std::string input;

  std::vector<token> tokens;

  std::unordered_map<std::string, tokenType> keywords = {
      {"if", tokenType::IF},         {"else", tokenType::ELSE},     {"elif", tokenType::ELIF},
      {"for", tokenType::FOR},       {"while", tokenType::WHILE},   {"int", tokenType::INT},
      {"double", tokenType::DOUBLE}, {"string", tokenType::STRING}, {"bool", tokenType::BOOL},
      {"true", tokenType::TRUE},     {"false", tokenType::FALSE},   {"not", tokenType::NOT}};

  char peek();
  char current();
  char advance();
  bool is_eof();
  void add_token(tokenType tok_type);
  void add_token(tokenType tok_type,int col);
  void add_token(tokenType tok_type, std::string lexeme, int col);
  void alpha_handler();
  void digit_handler();
  void lex_file();

public:
  Lexer(std::string input);
  std::vector<token> get_tokens();
};
