#include "tac.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>

#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

class CodeGen {
  llvm::LLVMContext ctx;
  llvm::Module mod{"my_lang", ctx};
  llvm::IRBuilder<> b{ctx};

  std::vector<std::unordered_map<std::string, llvm::AllocaInst *>> scopes;
  llvm::Function *mainFn = nullptr;

  [[noreturn]] void fail(const std::string &msg, int line = 0) {
    throw std::runtime_error("codegen error" + (line ? " (line " + std::to_string(line) + ")" : "") + ": " + msg);
  }

  llvm::Type *ptrTy() { return llvm::PointerType::getUnqual(ctx); }

  llvm::Type *toLLVMType(tokenType t) {
    switch (t) {
    case tokenType::INT:    return b.getInt32Ty();
    case tokenType::DOUBLE: return b.getDoubleTy();
    case tokenType::BOOL:   return b.getInt1Ty();
    case tokenType::STRING: return ptrTy();
    default: fail("unsupported type in declaration");
    }
  }

  llvm::Value *toBool(llvm::Value *v) {
    llvm::Type *t = v->getType();
    if (t->isIntegerTy(1)) return v;
    if (t->isDoubleTy()) return b.CreateFCmpONE(v, llvm::ConstantFP::get(t, 0.0), "tobool");
    if (t->isIntegerTy()) return b.CreateICmpNE(v, llvm::ConstantInt::get(t, 0), "tobool");
    fail("value cannot be used as a condition");
  }

  llvm::Value *coerce(llvm::Value *v, llvm::Type *to) {
    llvm::Type *from = v->getType();
    if (from == to) return v;

    if (to->isDoubleTy()) {
      if (from->isIntegerTy(1)) return b.CreateUIToFP(v, to, "conv");
      if (from->isIntegerTy()) return b.CreateSIToFP(v, to, "conv");
    } else if (to->isIntegerTy(1)) {
      return toBool(v);
    } else if (to->isIntegerTy()) {
      if (from->isDoubleTy()) return b.CreateFPToSI(v, to, "conv");
      if (from->isIntegerTy(1)) return b.CreateZExt(v, to, "conv");
    }
    fail("cannot convert between these types");
  }

  void unify(llvm::Value *&l, llvm::Value *&r, int line) {
    auto isNum = [](llvm::Type *t) { return t->isIntegerTy() || t->isDoubleTy(); };
    if (!isNum(l->getType()) || !isNum(r->getType())) fail("operator needs numeric operands", line);

    if (l->getType()->isDoubleTy() || r->getType()->isDoubleTy()) {
      l = coerce(l, b.getDoubleTy());
      r = coerce(r, b.getDoubleTy());
    } else {
      if (l->getType()->isIntegerTy(1)) l = b.CreateZExt(l, b.getInt32Ty(), "conv");
      if (r->getType()->isIntegerTy(1)) r = b.CreateZExt(r, b.getInt32Ty(), "conv");
    }
  }

  void pushScope() { scopes.emplace_back(); }
  void popScope() { scopes.pop_back(); }

  llvm::AllocaInst *lookup(const std::string &name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto f = it->find(name);
      if (f != it->end()) return f->second;
    }
    fail("undeclared variable '" + name + "'");
  }

  llvm::Value *gen(const ast::expr &e) {
    return std::visit([&](const auto &n) -> llvm::Value * { return genNode(n); }, e.val);
  }

  llvm::Value *genNode(const ast::int_val &n) {
    return b.getInt32(static_cast<uint32_t>(std::stoll(n.val)));
  }

  llvm::Value *genNode(const ast::double_val &n) {
    return llvm::ConstantFP::get(b.getDoubleTy(), std::stod(n.val));
  }


  llvm::Value *genNode(const ast::string_val &n) { return b.CreateGlobalString(n.val, "str"); }

  llvm::Value *genNode(const ast::group_expr &n) { return gen(*n.expression); }

  llvm::Value *genNode(const ast::identifier_val &n) {
    llvm::AllocaInst *slot = lookup(n.name);
    return b.CreateLoad(slot->getAllocatedType(), slot, n.name);
  }


  llvm::Value *genLogical(const ast::binary_expression &n, bool isAnd) {
    llvm::Function *fn = b.GetInsertBlock()->getParent();

    llvm::Value *l = toBool(gen(*n.left_val));
    llvm::BasicBlock *lhsBB = b.GetInsertBlock();
    auto *rhsBB = llvm::BasicBlock::Create(ctx, isAnd ? "and.rhs" : "or.rhs", fn);
    auto *endBB = llvm::BasicBlock::Create(ctx, isAnd ? "and.end" : "or.end", fn);

    if (isAnd) b.CreateCondBr(l, rhsBB, endBB);
    else       b.CreateCondBr(l, endBB, rhsBB);

    b.SetInsertPoint(rhsBB);
    llvm::Value *r = toBool(gen(*n.right_val));
    llvm::BasicBlock *rhsEnd = b.GetInsertBlock();
    b.CreateBr(endBB);

    b.SetInsertPoint(endBB);
    llvm::PHINode *phi = b.CreatePHI(b.getInt1Ty(), 2, isAnd ? "andtmp" : "ortmp");
    phi->addIncoming(b.getInt1(!isAnd), lhsBB);
    phi->addIncoming(r, rhsEnd);
    return phi;
  }

  llvm::Value *genNode(const ast::binary_expression &n) {
    const tokenType op = n.oper.tok_type;
    const int line = n.oper.loc.line;

    if (op == tokenType::AND) return genLogical(n, true);
    if (op == tokenType::OR)  return genLogical(n, false);

    llvm::Value *l = gen(*n.left_val);
    llvm::Value *r = gen(*n.right_val);
    unify(l, r, line);
    const bool fp = l->getType()->isDoubleTy();

    switch (op) {
    case tokenType::PLUS:  return fp ? b.CreateFAdd(l, r, "addtmp") : b.CreateAdd(l, r, "addtmp");
    case tokenType::MINUS: return fp ? b.CreateFSub(l, r, "subtmp") : b.CreateSub(l, r, "subtmp");
    case tokenType::STAR:  return fp ? b.CreateFMul(l, r, "multmp") : b.CreateMul(l, r, "multmp");
    case tokenType::SLASH: return fp ? b.CreateFDiv(l, r, "divtmp") : b.CreateSDiv(l, r, "divtmp");

    case tokenType::EQUAL_EQUAL:   return fp ? b.CreateFCmpOEQ(l, r, "eqtmp")  : b.CreateICmpEQ(l, r, "eqtmp");
    case tokenType::BANG_EQUAL:    return fp ? b.CreateFCmpUNE(l, r, "netmp")  : b.CreateICmpNE(l, r, "netmp");
    case tokenType::GREATER:       return fp ? b.CreateFCmpOGT(l, r, "gttmp")  : b.CreateICmpSGT(l, r, "gttmp");
    case tokenType::GREATER_EQUAL: return fp ? b.CreateFCmpOGE(l, r, "getmp")  : b.CreateICmpSGE(l, r, "getmp");
    case tokenType::LESSER:        return fp ? b.CreateFCmpOLT(l, r, "lttmp")  : b.CreateICmpSLT(l, r, "lttmp");
    case tokenType::LESSER_EQUAL:  return fp ? b.CreateFCmpOLE(l, r, "letmp")  : b.CreateICmpSLE(l, r, "letmp");

    case tokenType::BIT_AND:
    case tokenType::BIT_OR:
    case tokenType::BIT_XOR:
      if (fp) fail("bitwise operators need integer operands", line);
      if (op == tokenType::BIT_AND) return b.CreateAnd(l, r, "andtmp");
      if (op == tokenType::BIT_OR)  return b.CreateOr(l, r, "ortmp");
      return b.CreateXor(l, r, "xortmp");

    default:
      fail("unknown binary operator", line);
    }
  }

  void genDecl(const ast::declaration &d) {
    llvm::Type *ty = toLLVMType(d.type);
    const std::string &name = d.identifier->name;

    if (scopes.back().count(name)) fail("redeclaration of '" + name + "'");


    llvm::Value *init = d.init ? coerce(gen(*d.init), ty) : nullptr;


    llvm::BasicBlock &entry = mainFn->getEntryBlock();
    llvm::IRBuilder<> eb(&entry, entry.begin());
    llvm::AllocaInst *slot = eb.CreateAlloca(ty, nullptr, name);

    scopes.back()[name] = slot;
    b.CreateStore(init ? init : llvm::Constant::getNullValue(ty), slot);
  }

  void genAssign(const ast::assignment &a) {
    llvm::AllocaInst *slot = lookup(a.identifier->name);
    llvm::Value *v = coerce(gen(*a.expression), slot->getAllocatedType());
    b.CreateStore(v, slot);
  }

  void genIf(const ast::if_statement &s) {
    llvm::Function *fn = b.GetInsertBlock()->getParent();
    auto *thenBB = llvm::BasicBlock::Create(ctx, "then", fn);
    auto *elseBB = llvm::BasicBlock::Create(ctx, "else", fn);
    auto *mergeBB = llvm::BasicBlock::Create(ctx, "ifcont", fn);

    b.CreateCondBr(toBool(gen(*s.condition)), thenBB, elseBB);

    b.SetInsertPoint(thenBB);
    genContainer(*s.then);
    if (!b.GetInsertBlock()->getTerminator()) b.CreateBr(mergeBB);

    b.SetInsertPoint(elseBB);
    if (auto *elif = std::get_if<std::unique_ptr<ast::if_statement>>(&s.nxt_branch))
      genIf(**elif);
    else if (auto *els = std::get_if<std::unique_ptr<ast::else_statement>>(&s.nxt_branch))
      genContainer(*(*els)->then);
    if (!b.GetInsertBlock()->getTerminator()) b.CreateBr(mergeBB);

    b.SetInsertPoint(mergeBB);
  }

  void genWhile(const ast::while_statement &s) {
    llvm::Function *fn = b.GetInsertBlock()->getParent();
    auto *condBB = llvm::BasicBlock::Create(ctx, "while.cond", fn);
    auto *bodyBB = llvm::BasicBlock::Create(ctx, "while.body", fn);
    auto *endBB = llvm::BasicBlock::Create(ctx, "while.end", fn);

    b.CreateBr(condBB);
    b.SetInsertPoint(condBB);
    b.CreateCondBr(toBool(gen(*s.condition)), bodyBB, endBB);

    b.SetInsertPoint(bodyBB);
    genContainer(*s.body);
    if (!b.GetInsertBlock()->getTerminator()) b.CreateBr(condBB);

    b.SetInsertPoint(endBB);
  }

  void genFor(const ast::for_statement &s) {
    llvm::Function *fn = b.GetInsertBlock()->getParent();

    pushScope();
    if (auto *a = std::get_if<ast::assignment>(&s.init))  genAssign(*a);
    if (auto *d = std::get_if<ast::declaration>(&s.init)) genDecl(*d);

    auto *condBB = llvm::BasicBlock::Create(ctx, "for.cond", fn);
    auto *bodyBB = llvm::BasicBlock::Create(ctx, "for.body", fn);
    auto *endBB = llvm::BasicBlock::Create(ctx, "for.end", fn);

    b.CreateBr(condBB);
    b.SetInsertPoint(condBB);
    if (s.condition) b.CreateCondBr(toBool(gen(*s.condition)), bodyBB, endBB);
    else             b.CreateBr(bodyBB);

    b.SetInsertPoint(bodyBB);
    genContainer(*s.body);
    genAssign(s.increment);
    if (!b.GetInsertBlock()->getTerminator()) b.CreateBr(condBB);

    b.SetInsertPoint(endBB);
    popScope();
  }

  void genContainer(const ast::container &c) {
    pushScope();
    for (const auto &st : c.stmts) {
      std::visit(
          [&](const auto &s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<ast::expr>>)                  gen(*s);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ast::if_statement>>)     genIf(*s);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ast::for_statement>>)    genFor(*s);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ast::while_statement>>)  genWhile(*s);
            else if constexpr (std::is_same_v<T, ast::assignment>)                        genAssign(s);
            else if constexpr (std::is_same_v<T, ast::declaration>)                       genDecl(s);
          },
          st);
    }
    popScope();
  }

public:

  void generate(const ast::container &program) {
    auto *fnTy = llvm::FunctionType::get(b.getInt32Ty(), false);
    mainFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "main", mod);
    b.SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", mainFn));

    genContainer(program);

    b.CreateRet(b.getInt32(0));

    if (llvm::verifyFunction(*mainFn, &llvm::errs())) fail("generated IR failed verification");
  }


  void optimize() {
    llvm::PassBuilder pb;
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);

    llvm::FunctionPassManager fpm;
    fpm.addPass(llvm::PromotePass());
    fpm.run(*mainFn, fam);
  }


  std::string ir() {
    std::string s;
    llvm::raw_string_ostream os(s);
    mod.print(os, nullptr);
    return os.str();
  }
};

}

std::string generateTAC(const ast::container& program, bool ssa) {
  CodeGen cg;
  cg.generate(program);
  if (ssa) cg.optimize();
  return cg.ir();
}
