#include <unordered_map>
#include <vector>

enum class Type { Int, Boolean, String, Double };

struct symbol {
  Type type;
};

typedef std::unordered_map<std::string, symbol> table;

class symbolTable {
  std::vector<table> scopes;

public:
  void enterScope();
  void exitScope();
  bool declare(const std::string& name, const symbol& sym);
  symbol* lookUp(const std::string);
};
