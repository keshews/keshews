#include <cctype>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

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
  LEFT_PARANTHESES,
  RIGHT_PARANTHESES,
  LEFT_SQUARE_BRACKETS,
  RIGHT_SQUARE_BRACKETS,
  LEFT_CURLY_BRACES,
  RIGHT_CURLY_BRACES,

  IDENTIFIER,
  INT_VAL,
  DOUBLE_VAL,
  STRING_VAL,
};

typedef struct {
  tokenType tok_type;
  std::string value;
  int line;
} token;

class Lexer {
private:
  int line;
  int pos;
  int s_pos;
  std::string input;
  std::vector<token> tokens;

  std::unordered_map<std::string, tokenType> keywords = {
      {"if", tokenType::IF},         {"else", tokenType::ELSE},
      {"elif", tokenType::ELIF},     {"for", tokenType::FOR},
      {"while", tokenType::WHILE},   {"int", tokenType::INT},
      {"double", tokenType::DOUBLE}, {"string", tokenType::STRING},
      {"bool", tokenType::BOOL},     {"true", tokenType::TRUE},
      {"false", tokenType::FALSE},
  };

  char peek() {
    if (pos + 1 >= input.length()) {
      return EOF;
    } else {
      return input[pos + 1];
    }
  }

  char current() { return input[pos]; }

  char advance() { return input[pos++]; }

  bool is_eof() { return pos >= input.length(); }

  void add_token(tokenType tok_type) {
    token tok;
    tok.tok_type = tok_type;
    tok.line = line;
    tokens.push_back(tok);
  }

  void add_token(tokenType tok_type, std::string val) {
    token tok;
    tok.tok_type = tok_type;
    tok.line = line;
    tok.value = val;
    tokens.push_back(tok);
  }

  void alpha_handler() {
    std::string str;
    while (!is_eof() &&
           (current() == '_' || isdigit(current()) || isalpha(current()))) {
      str += advance();
    }
    auto it = keywords.find(str);
    if (it == keywords.end()) {
      add_token(tokenType::IDENTIFIER, str);
    } else {
      add_token(it->second, it->first);
    }
  }

  void digit_handler() {
    bool isDouble = false;
    std::string str;
    while (!is_eof() && isdigit(current()))
      str += advance();

    if (!is_eof() && current() == '.') {
      str += advance();

      if (is_eof() || !isdigit(current())) {
        print_error("invalid numeric");
        return;
      }

      while (!is_eof() && isdigit(current()))
        str += advance();

      add_token(tokenType::DOUBLE_VAL, str);
    } else {
      add_token(tokenType::INT_VAL, str);
    }
  }

  void print_error(std::string msg) {
    std::cout << "Error: " << msg << " at line " << line << " and column "
              << pos - s_pos + 1 << std::endl;
    return;
  }

  void lex_file() {
    line = 0;
    pos = 0;
    s_pos = 0;
    while (!is_eof()) {
      switch (input[pos]) {
      case ' ':
        break;
      case '\t':
        break;
      case '\n':
        line++;
        s_pos = pos + 1;
        break;

        // delimiters
      case '\r':
        break;

        // delimiters
      case '(':
        add_token(tokenType::LEFT_PARANTHESES);
        break;
      case ')':
        add_token(tokenType::RIGHT_PARANTHESES);
        break;
      case '[':
        add_token(tokenType::LEFT_SQUARE_BRACKETS);
        break;
      case ']':
        add_token(tokenType::RIGHT_SQUARE_BRACKETS);
        break;
      case '{':
        add_token(tokenType::LEFT_CURLY_BRACES);
        break;
      case '}':
        add_token(tokenType::RIGHT_CURLY_BRACES);
        break;

        // operators
      case '+':
        if (peek() == '=') {
          advance();
          add_token(tokenType::PLUS_EQUAL);
        } else {
          add_token(tokenType::PLUS);
        }
        break;
      case '-':
        if (peek() == '=') {
          advance();
          add_token(tokenType::MINUS_EQUAL);
        } else {
          add_token(tokenType::MINUS);
        }
        break;
      case '*':
        if (peek() == '=') {
          advance();
          add_token(tokenType::STAR_EQUAL);
        } else {
          add_token(tokenType::STAR);
        }
        break;
      case '/':
        if (peek() == '=') {
          advance();
          add_token(tokenType::SLASH_EQUAL);
        } else if (peek() == '/') {
          while (!is_eof() && current() != '\n') {
            advance();
          }
          if (!is_eof()) {
            line++;
            s_pos = pos + 1;
          }

        } else {
          add_token(tokenType::SLASH);
        }
        break;
      case '|':
        if (peek() == '|') {
          advance();
          add_token(tokenType::OR);
        } else {
          add_token(tokenType::BIT_OR);
        }
        break;
      case '&':
        if (peek() == '&') {
          advance();
          add_token(tokenType::AND);
        } else {
          add_token(tokenType::BIT_AND);
        }
        break;
      case '^':
        add_token(tokenType::BIT_XOR);
        break;
      case '~':
        add_token(tokenType::COMPLEMENT);
        break;
      case '=':
        if (peek() == '=') {
          advance();
          add_token(tokenType::EQUAL_EQUAL);
        } else {
          add_token(tokenType::EQUAL);
        }
        break;
      case '>':
        if (peek() == '=') {
          advance();
          add_token(tokenType::GREATER_EQUAL);
        } else {
          add_token(tokenType::GREATER);
        }
        break;
      case '<':
        if (peek() == '=') {
          advance();
          add_token(tokenType::LESSER_EQUAL);
        } else {
          add_token(tokenType::LESSER);
        }
        break;
      case '!':
        if (peek() == '=') {
          advance();
          add_token(tokenType::BANG_EQUAL);
        } else {
          add_token(tokenType::BANG);
        }
        break;
      case '"': {
        std::string str;
        advance();
        while (!is_eof() && current() != '"') {
          str += advance();
        }
        if (is_eof()) {
          print_error("invalid string literal");
        } else {
          add_token(tokenType::STRING_VAL, str);
        }
        break;
      }

      default: {
        if (isalpha(input[pos]) || current() == '_') {
          alpha_handler();
        } else if (isdigit(input[pos])) {
          digit_handler();
        } else {
          print_error("invalid syntax");
        }
      }
      }
      advance();
    }
  }

public:
  Lexer(std::string input) { this->input = input; }

  std::vector<token> get_tokens() {
    lex_file();
    return tokens;
  }
};
