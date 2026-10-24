#include "tac.hpp"
#include "ast.hpp"
#include "diagnostic.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic_analyzer.hpp"

#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>

struct VarSlot {
  llvm::Value* ptr;
  llvm::Type* ty;
};

class CodeGen {
  std::unique_ptr<llvm::LLVMContext> ctx;
  std::unique_ptr<llvm::Module> mod;
  std::unique_ptr<llvm::IRBuilder<>> b;

  std::vector<std::unordered_map<std::string, VarSlot>> scopes;
  llvm::Function* mainFn = nullptr;
  bool isRepl = false;
  std::unordered_map<std::string, llvm::Type*> replGlobals;

  [[noreturn]] void fail(const std::string& msg, int line = 0) {
    throw std::runtime_error("codegen error" + (line ? " (line " + std::to_string(line) + ")" : "") + ": " + msg);
  }

  llvm::Type* ptrTy() {
    return llvm::PointerType::getUnqual(*ctx);
  }

  llvm::Type* toLLVMType(tokenType t) {
    switch (t) {
      case tokenType::INT:
        return (*b).getInt32Ty();
      case tokenType::DOUBLE:
        return (*b).getDoubleTy();
      case tokenType::BOOL:
        return (*b).getInt1Ty();
      case tokenType::STRING:
        return ptrTy();
      default:
        fail("unsupported type in declaration");
    }
  }

  llvm::Value* toBool(llvm::Value* v) {
    llvm::Type* t = v->getType();
    if (t->isIntegerTy(1))
      return v;
    if (t->isDoubleTy())
      return (*b).CreateFCmpONE(v, llvm::ConstantFP::get(t, 0.0), "tobool");
    if (t->isIntegerTy())
      return (*b).CreateICmpNE(v, llvm::ConstantInt::get(t, 0), "tobool");
    fail("value cannot be used as a condition");
  }

  llvm::Value* coerce(llvm::Value* v, llvm::Type* to) {
    llvm::Type* from = v->getType();
    if (from == to)
      return v;

    if (to->isDoubleTy()) {
      if (from->isIntegerTy(1))
        return (*b).CreateUIToFP(v, to, "conv");
      if (from->isIntegerTy())
        return (*b).CreateSIToFP(v, to, "conv");
    } else if (to->isIntegerTy(1)) {
      return toBool(v);
    } else if (to->isIntegerTy()) {
      if (from->isDoubleTy())
        return (*b).CreateFPToSI(v, to, "conv");
      if (from->isIntegerTy(1))
        return (*b).CreateZExt(v, to, "conv");
    }
    fail("cannot convert between these types");
  }

  void unify(llvm::Value*& l, llvm::Value*& r, int line) {
    auto isNum = [](llvm::Type* t) { return t->isIntegerTy() || t->isDoubleTy(); };
    if (!isNum(l->getType()) || !isNum(r->getType()))
      fail("operator needs numeric operands", line);

    if (l->getType()->isDoubleTy() || r->getType()->isDoubleTy()) {
      l = coerce(l, (*b).getDoubleTy());
      r = coerce(r, (*b).getDoubleTy());
    } else {
      if (l->getType()->isIntegerTy(1))
        l = (*b).CreateZExt(l, (*b).getInt32Ty(), "conv");
      if (r->getType()->isIntegerTy(1))
        r = (*b).CreateZExt(r, (*b).getInt32Ty(), "conv");
    }
  }

  void pushScope() {
    scopes.emplace_back();
  }
  void popScope() {
    scopes.pop_back();
  }

  VarSlot lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto f = it->find(name);
      if (f != it->end())
        return f->second;
    }
    // Check if it exists in current module globals
    if (auto* gv = mod->getNamedGlobal(name)) {
      return {gv, gv->getValueType()};
    }
    // Check if it was declared in a previous REPL chunk
    if (replGlobals.count(name)) {
      llvm::Type* ty = replGlobals[name];
      auto* gv = new llvm::GlobalVariable(
          *mod, ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, name);
      return {gv, ty};
    }
    fail("undeclared variable '" + name + "'");
  }

  llvm::Value* gen(const ast::expr& e) {
    return std::visit([&](const auto& n) -> llvm::Value* { return genNode(n); }, e.val);
  }

  llvm::Value* genNode(const ast::int_val& n) {
    return (*b).getInt32(static_cast<uint32_t>(std::stoll(n.val)));
  }

  llvm::Value* genNode(const ast::double_val& n) {
    return llvm::ConstantFP::get((*b).getDoubleTy(), std::stod(n.val));
  }

  llvm::Value* genNode(const ast::string_val& n) {
    return (*b).CreateGlobalString(n.val, "str");
  }

  llvm::Value* genNode(const ast::bool_val& n) {
    return (*b).getInt1(n.val);
  }

  llvm::Value* genNode(const ast::group_expr& n) {
    return gen(*n.expression);
  }

  llvm::Value* genNode(const ast::identifier_val& n) {
    VarSlot slot = lookup(n.name);
    return (*b).CreateLoad(slot.ty, slot.ptr, n.name);
  }

  llvm::Value* genLogical(const ast::binary_expression& n, bool isAnd) {
    llvm::Function* fn = (*b).GetInsertBlock()->getParent();

    llvm::Value* l = toBool(gen(*n.left_val));
    llvm::BasicBlock* lhsBB = (*b).GetInsertBlock();
    auto* rhsBB = llvm::BasicBlock::Create(*ctx, isAnd ? "and.rhs" : "or.rhs", fn);
    auto* endBB = llvm::BasicBlock::Create(*ctx, isAnd ? "and.end" : "or.end", fn);

    if (isAnd)
      (*b).CreateCondBr(l, rhsBB, endBB);
    else
      (*b).CreateCondBr(l, endBB, rhsBB);

    (*b).SetInsertPoint(rhsBB);
    llvm::Value* r = toBool(gen(*n.right_val));
    llvm::BasicBlock* rhsEnd = (*b).GetInsertBlock();
    (*b).CreateBr(endBB);

    (*b).SetInsertPoint(endBB);
    llvm::PHINode* phi = (*b).CreatePHI((*b).getInt1Ty(), 2, isAnd ? "andtmp" : "ortmp");
    phi->addIncoming((*b).getInt1(!isAnd), lhsBB);
    phi->addIncoming(r, rhsEnd);
    return phi;
  }

  llvm::Value* genNode(const ast::binary_expression& n) {
    const tokenType op = n.oper.tok_type;
    const int line = n.oper.loc.line;

    if (op == tokenType::AND)
      return genLogical(n, true);
    if (op == tokenType::OR)
      return genLogical(n, false);

    llvm::Value* l = gen(*n.left_val);
    llvm::Value* r = gen(*n.right_val);
    unify(l, r, line);
    const bool fp = l->getType()->isDoubleTy();

    switch (op) {
      case tokenType::PLUS:
        return fp ? (*b).CreateFAdd(l, r, "addtmp") : (*b).CreateAdd(l, r, "addtmp");
      case tokenType::MINUS:
        return fp ? (*b).CreateFSub(l, r, "subtmp") : (*b).CreateSub(l, r, "subtmp");
      case tokenType::STAR:
        return fp ? (*b).CreateFMul(l, r, "multmp") : (*b).CreateMul(l, r, "multmp");
      case tokenType::SLASH:
        return fp ? (*b).CreateFDiv(l, r, "divtmp") : (*b).CreateSDiv(l, r, "divtmp");

      case tokenType::EQUAL_EQUAL:
        return fp ? (*b).CreateFCmpOEQ(l, r, "eqtmp") : (*b).CreateICmpEQ(l, r, "eqtmp");
      case tokenType::BANG_EQUAL:
        return fp ? (*b).CreateFCmpUNE(l, r, "netmp") : (*b).CreateICmpNE(l, r, "netmp");
      case tokenType::GREATER:
        return fp ? (*b).CreateFCmpOGT(l, r, "gttmp") : (*b).CreateICmpSGT(l, r, "gttmp");
      case tokenType::GREATER_EQUAL:
        return fp ? (*b).CreateFCmpOGE(l, r, "getmp") : (*b).CreateICmpSGE(l, r, "getmp");
      case tokenType::LESSER:
        return fp ? (*b).CreateFCmpOLT(l, r, "lttmp") : (*b).CreateICmpSLT(l, r, "lttmp");
      case tokenType::LESSER_EQUAL:
        return fp ? (*b).CreateFCmpOLE(l, r, "letmp") : (*b).CreateICmpSLE(l, r, "letmp");

      case tokenType::BIT_AND:
      case tokenType::BIT_OR:
      case tokenType::BIT_XOR:
        if (fp)
          fail("bitwise operators need integer operands", line);
        if (op == tokenType::BIT_AND)
          return (*b).CreateAnd(l, r, "andtmp");
        if (op == tokenType::BIT_OR)
          return (*b).CreateOr(l, r, "ortmp");
        return (*b).CreateXor(l, r, "xortmp");

      default:
        fail("unknown binary operator", line);
    }
  }

  void genDecl(const ast::declaration& d) {
    llvm::Type* ty = toLLVMType(d.type);
    const std::string& name = d.identifier->name;

    if (scopes.back().count(name))
      fail("redeclaration of '" + name + "'");

    llvm::Value* init = d.init ? coerce(gen(*d.init), ty) : nullptr;

    // Top-level variables in REPL mode: allocate as GlobalVariable!
    if (isRepl && scopes.size() == 1) {
      auto* gv = new llvm::GlobalVariable(
          *mod, ty, false, llvm::GlobalValue::ExternalLinkage,
          llvm::Constant::getNullValue(ty), name);
      replGlobals[name] = ty;
      scopes.back()[name] = {gv, ty};
      if (init)
        (*b).CreateStore(init, gv);
      return;
    }

    llvm::BasicBlock& entry = mainFn->getEntryBlock();
    llvm::IRBuilder<> eb(&entry, entry.begin());
    llvm::AllocaInst* slot = eb.CreateAlloca(ty, nullptr, name);

    scopes.back()[name] = {slot, ty};
    (*b).CreateStore(init ? init : llvm::Constant::getNullValue(ty), slot);
  }

  void genAssign(const ast::assignment& a) {
    VarSlot slot = lookup(a.identifier->name);
    llvm::Value* v = coerce(gen(*a.expression), slot.ty);
    (*b).CreateStore(v, slot.ptr);
  }

  void genIf(const ast::if_statement& s) {
    llvm::Function* fn = (*b).GetInsertBlock()->getParent();
    auto* thenBB = llvm::BasicBlock::Create(*ctx, "then", fn);
    auto* elseBB = llvm::BasicBlock::Create(*ctx, "else", fn);
    auto* mergeBB = llvm::BasicBlock::Create(*ctx, "ifcont", fn);

    (*b).CreateCondBr(toBool(gen(*s.condition)), thenBB, elseBB);

    (*b).SetInsertPoint(thenBB);
    genContainer(*s.then);
    if (!(*b).GetInsertBlock()->hasTerminator())
      (*b).CreateBr(mergeBB);

    (*b).SetInsertPoint(elseBB);
    if (auto* elif = std::get_if<std::unique_ptr<ast::if_statement>>(&s.nxt_branch))
      genIf(**elif);
    else if (auto* els = std::get_if<std::unique_ptr<ast::else_statement>>(&s.nxt_branch))
      genContainer(*(*els)->then);
    if (!(*b).GetInsertBlock()->hasTerminator())
      (*b).CreateBr(mergeBB);

    (*b).SetInsertPoint(mergeBB);
  }

  void genWhile(const ast::while_statement& s) {
    llvm::Function* fn = (*b).GetInsertBlock()->getParent();
    auto* condBB = llvm::BasicBlock::Create(*ctx, "while.cond", fn);
    auto* bodyBB = llvm::BasicBlock::Create(*ctx, "while.body", fn);
    auto* endBB = llvm::BasicBlock::Create(*ctx, "while.end", fn);

    (*b).CreateBr(condBB);
    (*b).SetInsertPoint(condBB);
    (*b).CreateCondBr(toBool(gen(*s.condition)), bodyBB, endBB);

    (*b).SetInsertPoint(bodyBB);
    genContainer(*s.body);
    if (!(*b).GetInsertBlock()->hasTerminator())
      (*b).CreateBr(condBB);

    (*b).SetInsertPoint(endBB);
  }

  void genFor(const ast::for_statement& s) {
    llvm::Function* fn = (*b).GetInsertBlock()->getParent();

    pushScope();
    if (auto* a = std::get_if<ast::assignment>(&s.init))
      genAssign(*a);
    if (auto* d = std::get_if<ast::declaration>(&s.init))
      genDecl(*d);

    auto* condBB = llvm::BasicBlock::Create(*ctx, "for.cond", fn);
    auto* bodyBB = llvm::BasicBlock::Create(*ctx, "for.body", fn);
    auto* endBB = llvm::BasicBlock::Create(*ctx, "for.end", fn);

    (*b).CreateBr(condBB);
    (*b).SetInsertPoint(condBB);
    if (s.condition)
      (*b).CreateCondBr(toBool(gen(*s.condition)), bodyBB, endBB);
    else
      (*b).CreateBr(bodyBB);

    (*b).SetInsertPoint(bodyBB);
    genContainer(*s.body);
    genAssign(s.increment);
    if (!(*b).GetInsertBlock()->hasTerminator())
      (*b).CreateBr(condBB);

    (*b).SetInsertPoint(endBB);
    popScope();
  }

  void genContainer(const ast::container& c) {
    pushScope();
    for (const auto& st : c.stmts) {
      std::visit(
          [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<ast::expr>>)
              gen(*s);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ast::if_statement>>)
              genIf(*s);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ast::for_statement>>)
              genFor(*s);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ast::while_statement>>)
              genWhile(*s);
            else if constexpr (std::is_same_v<T, ast::assignment>)
              genAssign(s);
            else if constexpr (std::is_same_v<T, ast::declaration>)
              genDecl(s);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ast::print_statement>>) {
              genPrint(*s);
            }
          },
          st);
    }
    popScope();
  }

  void genPrint(const ast::print_statement& p) {
    llvm::Value* val = gen(*p.expression);
    llvm::Type* ty = val->getType();

    auto* printfTy = llvm::FunctionType::get((*b).getInt32Ty(), llvm::PointerType::getUnqual(*ctx), true);
    auto printfFn = mod->getOrInsertFunction("printf", printfTy);

    if (ty->isIntegerTy(1)) {
      llvm::Value* trueStr = (*b).CreateGlobalString("true", "str_true");
      llvm::Value* falseStr = (*b).CreateGlobalString("false", "str_false");
      llvm::Value* boolStr = (*b).CreateSelect(val, trueStr, falseStr);
      llvm::Value* fmtVal = (*b).CreateGlobalString("%s\n", "printf_fmt");
      (*b).CreateCall(printfFn, {fmtVal, boolStr});
      return;
    }

    const char* fmt = "%d\n";
    if (ty->isDoubleTy())
      fmt = "%f\n";
    else if (ty->isPointerTy())
      fmt = "%s\n";

    llvm::Value* fmtVal = (*b).CreateGlobalString(fmt, "printf_fmt");
    (*b).CreateCall(printfFn, {fmtVal, val});
  }

public:
  CodeGen(bool isRepl = false, std::unordered_map<std::string, llvm::Type*> replGlobals = {})
      : ctx(std::make_unique<llvm::LLVMContext>()),
        mod(std::make_unique<llvm::Module>("keshews", *ctx)),
        b(std::make_unique<llvm::IRBuilder<>>(*ctx)),
        isRepl(isRepl),
        replGlobals(std::move(replGlobals)) {}

  void generate(const ast::container& program) {
    auto* fnTy = llvm::FunctionType::get((*b).getInt32Ty(), false);
    mainFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "main", *mod);
    (*b).SetInsertPoint(llvm::BasicBlock::Create(*ctx, "entry", mainFn));

    genContainer(program);

    if (!(*b).GetInsertBlock()->hasTerminator())
      (*b).CreateRet((*b).getInt32(0));

    if (llvm::verifyFunction(*mainFn, &llvm::errs()))
      fail("generated IR failed verification");
  }

  std::string gen_repl(const ast::container& chunk, int cnt) {
    std::string fnName = std::format("repl_{:03d}", cnt);
    auto* fnTy = llvm::FunctionType::get((*b).getInt32Ty(), false);
    mainFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *mod);
    (*b).SetInsertPoint(llvm::BasicBlock::Create(*ctx, "entry", mainFn));

    genContainer(chunk);

    if (!(*b).GetInsertBlock()->hasTerminator())
      (*b).CreateRet((*b).getInt32(0));

    if (llvm::verifyFunction(*mainFn, &llvm::errs()))
      fail("generated IR failed verification");

    optimize();
    return fnName;
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
    (*mod).print(os, nullptr);
    return os.str();
  }

  std::unique_ptr<llvm::LLVMContext> getContext() {
    return std::move(ctx);
  }

  std::unique_ptr<llvm::Module> getModule() {
    return std::move(mod);
  }

  std::string generateTAC(const ast::container& program, bool ssa = true) {
    generate(program);
    if (ssa)
      optimize();
    return ir();
  }

  const std::unordered_map<std::string, llvm::Type*>& getReplGlobals() const {
    return replGlobals;
  }
};

std::string generateTAC(const ast::container& program, bool ssa) {
  CodeGen cg;
  return cg.generateTAC(program, ssa);
}

int run_JIT(const ast::container& program, bool ssa) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  auto jit = llvm::cantFail(llvm::orc::LLJITBuilder().create());
  jit->getMainJITDylib().addGenerator(
      llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          jit->getDataLayout().getGlobalPrefix())));

  CodeGen cg;
  cg.generate(program);
  if (ssa)
    cg.optimize();

  auto mod = cg.getModule();
  auto ctx = cg.getContext();

  llvm::cantFail(jit->addIRModule(llvm::orc::ThreadSafeModule(std::move(mod), std::move(ctx))));

  auto mainSym = llvm::cantFail(jit->lookup("main"));
  int (*mainFn)() = mainSym.toPtr<int (*)()>();
  return mainFn();
}

void run_repl() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  auto jit = llvm::cantFail(llvm::orc::LLJITBuilder().create());
  jit->getMainJITDylib().addGenerator(
      llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          jit->getDataLayout().getGlobalPrefix())));

  Semantic_Analyzer analyzer;
  std::unordered_map<std::string, llvm::Type*> replGlobals;
  std::string buffer;
  int depth = 0;
  int counter = 1;

  std::cout << "keshews (REPL)\nType 'exit' or 'quit' to quit.\n";

  while (true) {
    std::cout << (depth == 0 ? "kesh> " : "... ");
    std::string line;
    if (!std::getline(std::cin, line))
      break;

    if (depth == 0 && (line == "exit" || line == "quit"))
      break;

    buffer += line + "\n";
    for (char c : line) {
      if (c == '{')
        depth++;
      else if (c == '}')
        depth--;
    }

    if (depth <= 0) {
      depth = 0;
      try {
        Lexer lex(buffer);
        Parser parser(lex.get_tokens());
        auto program = parser.parse();
        analyzer.analyze(*program);

        CodeGen cg(true, replGlobals);
        std::string fnName = cg.gen_repl(*program, counter++);
        replGlobals = cg.getReplGlobals();

        auto mod = cg.getModule();
        auto ctx = cg.getContext();

        llvm::cantFail(jit->addIRModule(
            llvm::orc::ThreadSafeModule(std::move(mod), std::move(ctx))));

        auto sym = llvm::cantFail(jit->lookup(fnName));
        int (*fn)() = sym.toPtr<int (*)()>();
        fn();

      } catch (const CompileError& e) {
        Diagnostic::print(buffer, "<repl>", e);
      } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
      }
      buffer.clear();
    }
  }
}
