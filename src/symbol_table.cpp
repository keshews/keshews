#include "symbol_table.hpp"
#include <cstddef>

void symbolTable::enterScope() {
  symbolTable::scopes.push_back({});
}

void symbolTable::exitScope() {
  if (!scopes.empty())
    scopes.pop_back();
}

// shadowing is allowed
bool symbolTable::declare(const std::string& name, const symbol& sym) {
  auto& current = scopes.back();
  if (current.contains(name))
    return false;

  current.insert({name, sym});
  return true;
}

symbol* symbolTable::lookUp(const std::string name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); it++) {
    if (it->find(name) != it->end()) {
      return &(it->find(name)->second);
    }
  }
  return nullptr;
}
