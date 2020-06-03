#include <iostream>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> TheBuilder(TheContext);
static llvm::Module* TheModule;
static std::map<std::string, llvm::Value*> TheSymbolTable;

llvm::Value* numericConstant(float val) {
  return llvm::ConstantFP::get(TheContext, llvm::APFloat(val));
}

llvm::Value* binaryOperation(llvm::Value* lhs, llvm::Value* rhs, char op) {
  if (!lhs || !rhs) {
    return NULL;
  }
  switch (op) {
    case '+':
      return TheBuilder.CreateFAdd(lhs, rhs, "addtmp");
    case '-':
      return TheBuilder.CreateFSub(lhs, rhs, "subtmp");
    case '*':
      return TheBuilder.CreateFMul(lhs, rhs, "multmp");
    case '/':
      return TheBuilder.CreateFDiv(lhs, rhs, "divtmp");
    case '<':
      llvm::Value* cond = TheBuilder.CreateFCmpULT(lhs, rhs, "lttmp");
      return TheBuilder.CreateUIToFP(
        cond,
        llvm::Type::getFloatTy(TheContext),
        "booltmp"
      );
    default:
      std::cerr << "Invalid operator: " << op << std::endl;
      return NULL;
  }
}

llvm::AllocaInst* generateEntryBlockAlloca(const std::string& name) {
  llvm::Function* currFn = TheBuilder.GetInsertBlock()->getParent();
  llvm::IRBuilder<> tmpBuilder(
    &currFn->getEntryBlock(),
    currFn->getEntryBlock().begin()
  );
  return tmpBuilder.CreateAlloca(
    llvm::Type::getFloatTy(TheContext),
    0,
    name.c_str()
  );
}

llvm::Value* assignmentStatement(const std::string& lhs, llvm::Value* rhs) {
  if (!rhs) {
    return NULL;
  }

  if (!TheSymbolTable.count(lhs)) {
    // Allocate space for lhs.
    TheSymbolTable[lhs] = generateEntryBlockAlloca(lhs);
  }
  // Store rhs into lhs space.
  return TheBuilder.CreateStore(rhs, TheSymbolTable[lhs]);
}


llvm::Value* variableValue(const std::string& name) {
  llvm::Value* allocaInst = TheSymbolTable[name];
  if (!allocaInst) {
    std::cerr << "Unknown variable: " << name << std::endl;
    return NULL;
  }
  return TheBuilder.CreateLoad(allocaInst, name.c_str());
}


llvm::Value* ifElseStatement() {
  llvm::Value* cond = binaryOperation(
    variableValue("b"),
    numericConstant(8),
    '<'
  );
  if (!cond) {
    return NULL;
  }


  llvm::Function* currFn =
    TheBuilder.GetInsertBlock()->getParent();
  llvm::BasicBlock* ifBlock =
    llvm::BasicBlock::Create(TheContext, "ifBlock", currFn);
  llvm::BasicBlock* elseBlock =
    llvm::BasicBlock::Create(TheContext, "elseBlock");
  llvm::BasicBlock* continueBlock =
    llvm::BasicBlock::Create(TheContext, "continueBlock");
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
  llvm::Value* expr1 = binaryOperation(
    numericConstant(4.0),
    numericConstant(2.0),
    '*'
  );
  llvm::Value* expr2 = binaryOperation(
    numericConstant(8.0),
    expr1,
    '+'
  );
  llvm::Value* assignment1 = assignmentStatement(
    "a",
    expr2
  );

  llvm::Value* expr3 = binaryOperation(
    variableValue("a"),
    numericConstant(4),
    '/'
  );
  llvm::Value* assignment2 = assignmentStatement(
    "b",
    expr3
  );

  TheBuilder.CreateRetVoid();

  llvm::verifyFunction(*fooFn);
  TheModule->print(llvm::outs(), NULL);

  delete TheModule;
  return 0;
}
