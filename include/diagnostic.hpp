#pragma once

#include "lexer.hpp"
#include <format>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

class CompileError : public std::runtime_error {
public:
  std::string stage;
  sourceLocation loc;

  CompileError(std::string stage, sourceLocation loc, const std::string& msg)
      : std::runtime_error(msg), stage(std::move(stage)), loc(loc) {}

  CompileError(std::string stage, int line, int col, const std::string& msg)
      : std::runtime_error(msg), stage(std::move(stage)), loc{line, col} {}

  CompileError(std::string stage, const std::string& msg)
      : std::runtime_error(msg), stage(std::move(stage)), loc{0, 0} {}
};

class Diagnostic {
public:
  // ANSI color codes
  static constexpr const char* RED = "\033[1;31m";
  static constexpr const char* BLUE = "\033[1;34m";
  static constexpr const char* BOLD = "\033[1m";
  static constexpr const char* RESET = "\033[0m";

  static void print(std::string_view source, std::string_view filepath, const CompileError& err) {
    if (err.loc.line <= 0) {
      std::cerr << std::format("{}{}: {}error [{}]: {}{}{}\n", BOLD, filepath, RED, err.stage, BOLD,
                               err.what(), RESET);
      return;
    }

    std::istringstream stream{std::string(source)};
    std::string lineText;
    int currentLine = 1;
    while (std::getline(stream, lineText)) {
      if (currentLine == err.loc.line)
        break;
      currentLine++;
    }

    std::cerr << std::format("{}{}:{}:{}: {}error [{}]: {}{}{}\n", BOLD, filepath, err.loc.line, err.loc.col, RED,
                             err.stage, BOLD, err.what(), RESET);

    std::cerr << std::format("{}{:>5} |{} {}\n", BLUE, err.loc.line, RESET, lineText);

    std::string underline = std::string(err.loc.col > 0 ? err.loc.col - 1 : 0, ' ') + "^";
    std::cerr << std::format("{}{:>5} |{} {}{}{}\n", BLUE, "", RESET, RED, underline, RESET);
  }
};
