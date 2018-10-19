//
// Created by neil on 10/11/18.
//

#ifndef PROJECT_BASES_HPP
#define PROJECT_BASES_HPP

#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"

enum BuilderType {
  MODULE = 1,
  FUNCTION = 1 << 1,
  BASICBLOCK = 1 << 2
};


class PuzzleBuilder {
public:
    int weight;
    BuilderType type;

    PuzzleBuilder(BuilderType bt, int weight=10) : type(bt), weight(weight) {}

    virtual llvm::Value* build(llvm::Module &M) = 0;
    virtual llvm::Value* build(llvm::Function &F) = 0;
    virtual llvm::Value* build(llvm::BasicBlock &B) = 0;
};


class Transformer {
public:
    int weight;
    BuilderType type;

    Transformer(BuilderType bt, int weight=10) : type(bt), weight(weight) {}

    virtual llvm::Module* transform(llvm::Module &M) = 0;
    virtual llvm::Function* transform(llvm::Function &F) = 0;
    virtual llvm::BasicBlock* transform(llvm::BasicBlock &B) = 0;
};


#endif //PROJECT_BASES_HPP