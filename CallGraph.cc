//===-- CallGraph.cc - Build global call-graph------------------===//
// 
// This pass builds a global call-graph. The targets of an indirect
// call are identified based on type-analysis, i.e., matching the
// number and type of function parameters.
//
//===-----------------------------------------------------------===//

#include "headers/CallGraph.h"

#include <llvm/IR/DebugInfo.h>
#include <llvm/Pass.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Analysis/CallGraph.h>


using namespace llvm;


// Find targets of indirect calls based on type analysis: as long as
// the number and type of parameters of a function matches with the
// ones of the callsite, we say the function is a possible target of
// this call.
void CallGraphPass::findCalleesByType(CallInst *CI, FuncSet &S) {

  if (CI->isInlineAsm())
    return;

  CallSite CS(CI);
  for (Function *F : globalContext->AddressTakenFuncs) {

    // VarArg
    if (F->getFunctionType()->isVarArg()) {
      // Compare only known args in VarArg.
    }
    // otherwise, the numbers of args should be equal.
    else if (F->arg_size() != CS.arg_size()) {
      continue;
    }

    if (F->isIntrinsic()) {
      continue;
    }

    // Type matching on args.
    bool Matched = true;
    CallSite::arg_iterator AI = CS.arg_begin();
    for (Function::arg_iterator FI = F->arg_begin(), 
        FE = F->arg_end();
        FI != FE; ++FI, ++AI) {
      // Check type mis-matches.
      // Get defined type on callee side.
      Type *DefinedTy = FI->getType();
      // Get actual type on caller side.
      Type *ActualTy = (*AI)->getType();

      if (DefinedTy == ActualTy)
        continue;
      // Make the type analysis conservative: assume universal
      // pointers, i.e., "void *" and "char *", are equivalent to 
      // any pointer type and integer type.
      else if (
          (DefinedTy == Int8PtrTy &&
           (ActualTy->isPointerTy() || ActualTy == IntPtrTy)) 
          ||
          (ActualTy == Int8PtrTy &&
           (DefinedTy->isPointerTy() || DefinedTy == IntPtrTy))
          )
        continue;
      else {
        Matched = false;
        break;
      }
    }

    if (Matched)
      S.insert(F);
  }
}

bool CallGraphPass::doInitialization(Module *M) {
  return false;
}

bool CallGraphPass::doFinalization(Module *M) {

  // Do nothing here.
  return false;
}

bool CallGraphPass::doModulePass(Module *M) {

  // Do nothing here.
  return false;
}
