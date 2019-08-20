//
// Created by neil on 10/4/18.
//

#ifndef PROJECT_UTILS_HPP
#define PROJECT_UTILS_HPP

#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <vector>
#include <deque>
#include <set>
#include <random>
#include <chrono>
#include <memory>
#include <map>
#include <algorithm>

#define True true
#define False false // For Python lovers!
#define IN_SET(ELEM, SET) (SET.find(ELEM) != SET.end())
#define IN_MAP(KEY, MAP) (MAP.find(KEY) != MAP.end())
#define ISINSTANCE(OBJ_P, CLASS) (llvm::dyn_cast<CLASS>(OBJ_P))

#define SYMVAR_FUNC "fusor_symvar"

// get common types
extern llvm::Type *Int1;
extern llvm::Type *Int1_ptr;
extern llvm::Type *Int8;
extern llvm::Type *Int8_ptr;
extern llvm::Type *Int16;
extern llvm::Type *Int16_ptr;
extern llvm::Type *Int32;
extern llvm::Type *Int32_ptr;
extern llvm::Type *Int64;
extern llvm::Type *Int64_ptr;
extern llvm::Type *Float;
extern llvm::Type *Float_ptr;
extern llvm::Type *Double;
extern llvm::Type *Double_ptr;
extern llvm::Type *Void;

typedef std::map<llvm::Value*, llvm::Instruction*> SymvarLoc;


struct SymVar {
    llvm::Instruction *start_point = nullptr;
    llvm::Value *var = nullptr;
    llvm::PointerType *type = nullptr;

    SymVar(llvm::Instruction *sp, llvm::Value *v, llvm::PointerType *t, llvm::Instruction *ref = nullptr)
            : start_point(sp), var(v), type(t) {
      to_be_deleted.emplace_back(sp);
      if (ref)
        to_be_deleted.emplace_back(ref);
    }

    void delete_all() {
      while (!to_be_deleted.empty()) {
        to_be_deleted.back()->eraseFromParent();
        to_be_deleted.pop_back();
      }
    }

private:
    std::deque<llvm::Instruction*> to_be_deleted;
};


std::vector<llvm::Instruction *> search_symvar_func_call(llvm::Function &F);


std::vector<SymVar> search_symvar_declare(llvm::Function &F);


inline std::map<llvm::Value*, std::set<llvm::BasicBlock*>>
        get_effective_regions(llvm::Function*, const std::vector<llvm::Value *>&);


const SymvarLoc move_symvar_to_front(llvm::BasicBlock *BB, const std::vector<llvm::Value *> &sym_var);


template <typename T>
inline std::vector<T> range(T start, T end, T step) {
  std::vector<T> res;

  for (T i = start; step > 0 ? i < end : i > end; i += step) {
    res.push_back(i);
  }

  return res;
}


template <typename T>
inline std::vector<T> range(T start, T end) {
  return range<T>(start, end, 1);
}


template <typename T>
inline std::vector<T> range(T end) {
  return range<T>(0, end, 1);
}


std::map<llvm::Value*, llvm::Value*> cast_sv_to_uint8(SymvarLoc &svs_loc, llvm::IRBuilder<> &irbuilder);


llvm::BasicBlock *split_bb_randomly(llvm::BasicBlock *bb, std::set<llvm::Value*> pre_req,
        std::default_random_engine *rand_eng);


llvm::BasicBlock *fake_bb_builder(llvm::BasicBlock *base_bb, std::vector<llvm::Instruction*> seed);


struct LLVMArray {
  LLVMArray() = default;

  LLVMArray(llvm::IRBuilder<> &irBuilder, llvm::Value *array) : array(array), irBuilder(irBuilder) { }

  llvm::Value* operator [] (const size_t index) {
    llvm::Value *vi[2] = {llvm::ConstantInt::get(Int64, 0), llvm::ConstantInt::get(Int64, index)};
    auto i = llvm::ArrayRef<llvm::Value *>(vi, 2);
    return irBuilder.CreateInBoundsGEP(array, i);
  }

private:
  llvm::Value *array;
  llvm::IRBuilder<> irBuilder;
};


struct FusorSymVar {
  llvm::Value *sym_var, *casted_val = nullptr, *obf_val = nullptr;
  llvm::Instruction *insert_point = nullptr;
  std::set<llvm::BasicBlock*> work_list;

  FusorSymVar(llvm::Value* sym_var, llvm::Function &F) : sym_var(sym_var), F(F) {}

  bool build_effective_region(llvm::DominatorTree &dom_tree) {
    if (!sym_var->getType()->isPointerTy()) {
      auto *inst_p = &F.front().front();
      llvm::IRBuilder<> irbuilder(inst_p);
      auto *sv_space = irbuilder.CreateAlloca(sym_var->getType());
      irbuilder.CreateStore(sym_var, sv_space);
      auto *casted_ptr = irbuilder.CreateBitCast(sv_space, Int8_ptr, "casted_ptr");
      casted_val = irbuilder.CreateLoad(casted_ptr);
      insert_point = inst_p;
      for (auto &B : F)
        work_list.insert(&B);
    }
    else { // search for *first* load
      std::set<llvm::Value*> aliases;
      aliases.insert(sym_var);
      for (auto &u : sym_var->uses()) {
        if (ISINSTANCE(u.getUser(), llvm::StoreInst) ||
                ISINSTANCE(u.getUser(), llvm::GetElementPtrInst)) {
          aliases.insert(u.getUser());
        }
      }

      std::set<llvm::Instruction*> uses;
      for (auto *v : aliases) {
        for (auto &u : v->uses()) {
          if (auto *loadI = ISINSTANCE(u.getUser(), llvm::LoadInst)) {
            uses.insert(loadI);
          }
        }
      }

      llvm::Instruction *final_inst = nullptr;
      std::set<llvm::BasicBlock*> effective_region;
      for (auto *u : uses) {
        std::set<llvm::BasicBlock*> use_region;
        for (auto &B : F) {
          if (dom_tree.dominates(u, &B)) {
            use_region.insert(&B);
          }
        }
        if (use_region.size() > effective_region.size()) {
          final_inst = u;
          effective_region = use_region;
        }
      }

      if (final_inst) {
        llvm::IRBuilder<> irBuilder(final_inst);
        auto *casted_ptr = irBuilder.CreateBitCast(sym_var, Int8_ptr, "casted_ptr");
        casted_val = irBuilder.CreateLoad(casted_ptr);
        work_list = effective_region;
        insert_point = final_inst;
//        llvm::errs() << "y: " << *casted_val << "\n";
//        llvm::errs() << "c: " << *insert_point << "\n";
      }
      else {
        // symvar is not used
        return false;
      }
    }
    return true;
  }

//  ~FusorSymVar() {llvm::errs() << "destroying...\n";}

private:
  llvm::Function &F;
};

#endif //PROJECT_UTILS_HPP
