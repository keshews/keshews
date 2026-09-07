#include <string>

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
  LEFT_PARANTHESES,
  RIGHT_PARANTHESES,
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
  tokenType tok_type;
  std::string value;
  int line;
} token;
