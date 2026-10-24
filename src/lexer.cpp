#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
#include <format>

#include "lexer.hpp"
#include "diagnostic.hpp"

char Lexer::peek() {
  if ((unsigned long)pos + 1 >= input.length()) {
    return EOF;
  } else {
    return input[pos + 1];
  }
}

char Lexer::current() {
  return input[pos];
}

char Lexer::advance() {
  return input[pos++];
}

bool Lexer::is_eof() {
  return (unsigned long)pos >= input.length();
}

void Lexer::add_token(tokenType tok_type) {
  token tok;
  tok.tok_type = tok_type;
  tok.loc.line = line;
  tok.loc.col = pos - s_pos + 1;
  tokens.push_back(tok);
}
void Lexer::add_token(tokenType tok_type, int col) {
  token tok;
  tok.tok_type = tok_type;
  tok.loc.line = line;
  tok.loc.col = col;
  tokens.push_back(tok);
}

void Lexer::add_token(tokenType tok_type, std::string val, int col) {
  token tok;
  tok.tok_type = tok_type;
  tok.loc.line = line;
  tok.loc.col = col;
  tok.value = val;
  tokens.push_back(tok);
}

void Lexer::alpha_handler() {
  int col = pos - s_pos + 1;
  std::string str;
  while (!is_eof() && (current() == '_' || isdigit(current()) || isalpha(current()))) {
    str += advance();
  }
  auto it = keywords.find(str);
  if (it == keywords.end()) {
    add_token(tokenType::IDENTIFIER, str, col);
  } else {
    add_token(it->second, it->first, col);
  }
}

void Lexer::digit_handler() {
  int col = pos - s_pos + 1;
  std::string str;
  while (!is_eof() && isdigit(current()))
    str += advance();

  if (!is_eof() && current() == '.') {
    str += advance();

    if (is_eof() || !isdigit(current())) {
      throw CompileError("Lexer", line, pos - s_pos + 1, std::format("invalid numeric '{}'", input[pos]));
      return;
    }

    while (!is_eof() && isdigit(current()))
      str += advance();

    add_token(tokenType::DOUBLE_VAL, str, col);
  } else {
    add_token(tokenType::INT_VAL, str, col);
  }
}

void Lexer::lex_file() {
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
      case '\r':
        break;

        // delimiters
      case '(':
        add_token(tokenType::LEFT_PARENTHESES);
        break;
      case ')':
        add_token(tokenType::RIGHT_PARENTHESES);
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
      case ';':
        add_token(tokenType::SEMI_COLON);
        break;
      case ':':
        add_token(tokenType::COLON);
        break;

        // operators
      case '+':
        if (peek() == '=') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::PLUS_EQUAL, col);
        } else {
          add_token(tokenType::PLUS);
        }
        break;
      case '-':
        if (peek() == '=') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::MINUS_EQUAL, col);
        } else {
          add_token(tokenType::MINUS);
        }
        break;
      case '*':
        if (peek() == '=') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::STAR_EQUAL, col);
        } else {
          add_token(tokenType::STAR);
        }
        break;
      case '/':
        if (peek() == '=') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::SLASH_EQUAL, col);
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
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::OR, col);
        } else {
          add_token(tokenType::BIT_OR);
        }
        break;
      case '&':
        if (peek() == '&') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::AND, col);
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
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::EQUAL_EQUAL, col);
        } else {
          add_token(tokenType::EQUAL);
        }
        break;
      case '>':
        if (peek() == '=') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::GREATER_EQUAL, col);
        } else {
          add_token(tokenType::GREATER);
        }
        break;
      case '<':
        if (peek() == '=') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::LESSER_EQUAL, col);
        } else {
          add_token(tokenType::LESSER);
        }
        break;
      case '!':
        if (peek() == '=') {
          int col = pos - s_pos + 1;
          advance();
          add_token(tokenType::BANG_EQUAL, col);
        } else {
          add_token(tokenType::BANG);
        }
        break;
      case '"': {
        int col = pos - s_pos + 1;
        std::string str;
        advance();
        while (!is_eof() && current() != '"') {
          str += advance();
        }
        if (is_eof()) {
          throw CompileError("Lexer", line, pos - s_pos + 1, std::format("invalid string literal '{}'", input[pos]));
        } else {
          add_token(tokenType::STRING_VAL, str, col);
        }
        break;
      }

      default: {
        if (isalpha(input[pos]) || current() == '_') {
          alpha_handler();
          continue;
        } else if (isdigit(input[pos])) {
          digit_handler();
          continue;
        } else {
          throw CompileError("Lexer", line, pos - s_pos + 1, std::format("unidentified character '{}'", input[pos]));
        }
      }
    }
    advance();
  }
}

Lexer::Lexer(std::string input) {
  this->input = input;
}

std::vector<token> Lexer::get_tokens() {
  lex_file();
  return tokens;
}
