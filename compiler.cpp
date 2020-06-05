#include <iostream>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetMachine.h"

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
      lhs = TheBuilder.CreateFCmpULT(lhs, rhs, "lttmp");
      return TheBuilder.CreateUIToFP(
        lhs,
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

  cond = TheBuilder.CreateFCmpONE(
    cond,
    numericConstant(0),
    "ifcond"
  );
  TheBuilder.CreateCondBr(cond, ifBlock, elseBlock);

  // If block
  TheBuilder.SetInsertPoint(ifBlock);
  llvm::Value* aTimesB = binaryOperation(
    variableValue("a"),
    variableValue("b"),
    '*'
  );
  llvm::Value* ifAssignment = assignmentStatement("c", aTimesB);
  TheBuilder.CreateBr(continueBlock);

  // Else block
  currFn->getBasicBlockList().push_back(elseBlock);
  TheBuilder.SetInsertPoint(elseBlock);
  llvm::Value* aPlusB = binaryOperation(
    variableValue("a"),
    variableValue("b"),
    '+'
  );
  llvm::Value* elseAssignment = assignmentStatement("c", aPlusB);
  TheBuilder.CreateBr(continueBlock);

  currFn->getBasicBlockList().push_back(continueBlock);
  TheBuilder.SetInsertPoint(continueBlock);

  return continueBlock;
}


void generateMachineCode(std::string filename) {
  std::string targetTriple = llvm::sys::getDefaultTargetTriple();

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  std::string error;
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(
    targetTriple,
    error
  );
  if (!target) {
    std::cerr << error << std::endl;
    return;
  }

  std::string cpu = "generic";
  std::string features = "";
  llvm::TargetOptions options;
  llvm::TargetMachine* targetMachine = target->createTargetMachine(
    targetTriple,
    cpu,
    features,
    options,
    llvm::Optional<llvm::Reloc::Model>()
  );

  TheModule->setDataLayout(targetMachine->createDataLayout());
  TheModule->setTargetTriple(targetTriple);

  std::error_code ec;
  llvm::raw_fd_ostream file(filename, ec, llvm::sys::fs::F_None);
  if (ec) {
    std::cerr << "Could not open output file: " << ec.message() << std::endl;
    return;
  }

  llvm::legacy::PassManager pm;
  llvm::TargetMachine::CodeGenFileType filetype =
    llvm::TargetMachine::CGFT_ObjectFile;

  if (targetMachine->addPassesToEmitFile(pm, file, NULL, filetype)) {
    std::cerr << "Can't emit object file for target machine" << std::endl;
  }
  pm.run(*TheModule);
  file.close();
}


int main(int argc, char const *argv[]) {
  TheModule = new llvm::Module("LLVM_Demo", TheContext);

  llvm::FunctionType* fooFnType = llvm::FunctionType::get(
    llvm::Type::getFloatTy(TheContext), false
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

  llvm::Value* ifElse = ifElseStatement();

  TheBuilder.CreateRet(variableValue("c"));
  // TheBuilder.CreateRetVoid();

  llvm::verifyFunction(*fooFn);
  TheModule->print(llvm::outs(), NULL);

  generateMachineCode("target.o");

  delete TheModule;
  return 0;
}
