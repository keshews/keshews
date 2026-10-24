#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/TargetSelect.h>

int run_JIT(std::unique_ptr<llvm::Module> mod, std::unique_ptr<llvm::LLVMContext> ctx) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  auto jit = llvm::cantFail(llvm::orc::LLJITBuilder().create());

  llvm::cantFail(jit->addIRModule(
      llvm::orc::ThreadSafeModule(std::move(mod), std::move(ctx))));

  auto mainSym = llvm::cantFail(jit->lookup("main"));
  int (*mainFn)() = mainSym.toPtr<int(*)()>();
  return mainFn();
}
