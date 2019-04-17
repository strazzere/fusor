//
// Created by neil on 3/19/19.
//

#include "util_funcs.hpp"
#include "utils.hpp"

using namespace std;
using namespace llvm;

pair<Function*, Value*> rand_16807(Module *module, uint32_t default_seed) {
  auto *ftype = FunctionType::get(Int32, false);

  auto *F = Function::Create(ftype, GlobalValue::InternalLinkage, "rand_16807", module);
  auto *entry = BasicBlock::Create(getGlobalContext(), "entry", F);
  IRBuilder<> irbuilder(entry);

  auto *Seed = new GlobalVariable(*module, Int32, False, GlobalVariable::InternalLinkage, ConstantInt::get(Int32, default_seed),
                                  "rand_16807.seed");
  auto *loaded_seed = irbuilder.CreateLoad(Int32, Seed);
  auto *ret = irbuilder.CreateSRem(loaded_seed, ConstantInt::get(Int32, 2147483647));
  auto *tmp = irbuilder.CreateMul(ret, ConstantInt::get(Int32, 16807));
  tmp = irbuilder.CreateSRem(tmp, ConstantInt::get(Int32, 2147483647));
  irbuilder.CreateStore(tmp, Seed);
  irbuilder.CreateRet(ret);

  return {F, Seed};
}


Function *wrapper_builder(string &target, Module *module, Value *op) {
  Function *target_func;
  set<string> declarations, implementations;
  for (auto &F : *module) {
    if (F.isDeclaration()) {
      declarations.insert(F.getName());
    }
    else if (F.getLinkage() == GlobalValue::ExternalLinkage && !F.isDLLImportDependent() && !F.isThreadDependent()) {
      implementations.insert(F.getName());
    }

    if (F.getName() == target) {
      target_func = &F;
    }
  }

  if (IN_SET(target, declarations)) {
    string wrapper_name = compose_wrapper(target);
    if (!IN_SET(wrapper_name, declarations)) {
      vector<Type*> args;
      args.push_back(Int1);
      for (auto &arg : target_func->args()) {
        args.push_back(arg.getType());
      }
      auto args_list = ArrayRef<Type*>(args);
      auto *ftype = FunctionType::get(target_func->getReturnType(), args_list, target_func->isVarArg());
      auto *F = Function::Create(ftype, GlobalValue::ExternalLinkage, wrapper_name, module);
      // fix all function calls
      for (auto &F : *module) {
        if (IN_SET(F.getName(), declarations)) {
          for (auto &B : F) {
            for (auto &I : B) {
              if (auto *call = ISINSTANCE(&I, CallInst)) {
                if (call->getCalledFunction()->getName() == target) {
                  vector<Value*> call_args;
                  call_args.push_back(op);
                  for (unsigned int _ = 0; _ < call->getNumArgOperands(); _++) {
                    call_args.push_back(call->getArgOperand(_));
                  }
                }
              }
            }
          }
        }
      }
    }
  }

}


Value *global_shared_op(string &target, Module *module, Value *op) {
  string op_name = compose_shared_op(target);
  auto *sop = module->getGlobalVariable(op_name);
  if (!sop) {
    sop = new GlobalVariable(*module, Int1, False, GlobalVariable::AvailableExternallyLinkage, nullptr, op_name);
  }
  else {
    for (auto &F : *module) {
      for (auto &B : F) {
        for (auto &I : B) {
          if (auto *call = ISINSTANCE(&I, CallInst)) {
            if (call->getCalledFunction()->getName() == target) {
              StoreInst(op, sop, call);
            }
          }
        }
      }
    }
  }
  return sop;
}
