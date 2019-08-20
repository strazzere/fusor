//
// Created by neil on 12/30/18.
//

#include "utils.hpp"
#include "inheritance.hpp"

using namespace std;
using namespace llvm;

const string SecondOpaqueTransformer::id = "SecondOpaqueTransformer";

Function *SecondOpaqueTransformer::transform(Function *F, Value *predicate) {
  for (auto &B : *F) {
    if (ISINSTANCE(B.getTerminator(), BranchInst) && B.getTerminator()->getNumOperands() > 1) {
      auto *old_pred = B.getTerminator()->getOperand(0);
      auto *right_des = B.getTerminator()->getOperand(1), *left_des = B.getTerminator()->getOperand(2);
      IRBuilder<> irbuilder(B.getTerminator());
//      auto *covered = irbuilder.CreateAnd(old_pred, predicate);
      auto *negate = irbuilder.CreateXor(predicate, 1);
      auto *covered = irbuilder.CreateXor(old_pred, negate);
      B.getTerminator()->eraseFromParent();
      BranchInst::Create(ISINSTANCE(left_des, BasicBlock), ISINSTANCE(right_des, BasicBlock), covered, &B);
    }
  }

  return F;
}

llvm::Function * SecondOpaqueTransformer::transform(llvm::Function *F, std::vector<FusorSymVar*> &predicates) {

  for (auto &B : *F) {
    if (ISINSTANCE(B.getTerminator(), BranchInst) && B.getTerminator()->getNumOperands() > 1) {
      auto *old_pred = B.getTerminator()->getOperand(0);
      auto *right_des = B.getTerminator()->getOperand(1), *left_des = B.getTerminator()->getOperand(2);
      IRBuilder<> irbuilder(B.getTerminator());

      // search for all available predicates
      Value * pred = ConstantInt::get(Int1, 1);
      bool obfed = false;
      for (auto *fsv : predicates) {
        if (IN_SET(&B, fsv->work_list)) {
          pred = irbuilder.CreateAnd(pred, fsv->obf_val);
          obfed = true;
        }
      }

//      auto *covered = irbuilder.CreateAnd(old_pred, predicate);
      if (obfed) {
        auto *negate = irbuilder.CreateXor(pred, 1);
        auto *covered = irbuilder.CreateXor(old_pred, negate);
        B.getTerminator()->eraseFromParent();
        BranchInst::Create(ISINSTANCE(left_des, BasicBlock), ISINSTANCE(right_des, BasicBlock), covered, &B);
      }
    }
  }
  return F;
}

unique_ptr<Transformer<Function>> SecondOpaqueTransformer::clone(uint64_t trans_code) {
  return std::make_unique<SecondOpaqueTransformer>(trans_code);
}
