#pragma once

#include "ast.hpp"
#include <string>

std::string generateTAC(const ast::container& program, bool ssa = true);
