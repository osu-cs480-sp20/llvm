#include <iostream>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> TheBuilder(TheContext);
static llvm::Module* TheModule;

llvm::Value* numericConstant(float val) {
  return llvm::ConstantFP::get(TheContext, llvm::APFloat(val));
}

llvm::Value* binaryOperation(llvm::Value* lhs, llvm::Value* rhs, char op) {
  if (!lhs || !rhs) {
    return NULL;
  }
  return TheBuilder.CreateFMul(lhs, rhs, "multmp");
}

int main(int argc, char const *argv[]) {
  TheModule = new llvm::Module("LLVM_Demo", TheContext);

  llvm::FunctionType* fooFnType = llvm::FunctionType::get(
    llvm::Type::getVoidTy(TheContext), false
  );
  llvm::Function* fooFn = llvm::Function::Create(
    fooFnType,
    llvm::GlobalValue::ExternalLinkage,
    "foo",
    TheModule
  );
  llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(
    TheContext,
    "entry",
    fooFn
  );
  TheBuilder.SetInsertPoint(entryBlock);

  /* Insert more instructions. */

  TheBuilder.CreateRetVoid();

  llvm::verifyFunction(*fooFn);
  TheModule->print(llvm::outs(), NULL);

  delete TheModule;
  return 0;
}
