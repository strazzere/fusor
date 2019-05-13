//
// Created by neil on 5/10/19.
//

#include "bases.hpp"
#include "utils.hpp"
#include "inheritance.hpp"

#include <random>

using namespace std;
using namespace llvm;

const string NoResiliencePuzzle::id = "NoResiliencePuzzle";

Value* NoResiliencePuzzle::build(SymvarLoc &svs_locs, Instruction *insert_point)  {
  IRBuilder<> irbuilder(insert_point);
//  uniform_int_distribution<uint8_t> int_generator(0, 127);
//  uint8_t left = int_generator(rand_eng), right = int_generator(rand_eng), result = left + right;
//  auto *left_m = irbuilder.CreateAlloca(Int32), *right_m = irbuilder.CreateAlloca(Int32);
//  irbuilder.CreateStore(ConstantInt::get(Int32, left), left_m);
//  irbuilder.CreateStore(ConstantInt::get(Int32, right), right_m);
//
//  auto *left_v = irbuilder.CreateLoad(left_m), *right_v = irbuilder.CreateLoad(right_m);
//  auto *squre = irbuilder.CreateMul(left_v, left_v);
//  auto *db = irbuilder.CreateMul(left_v, ConstantInt::get(Int32, 2));
//  auto *add_res = irbuilder.CreateAdd(squre, db);
  Value *load;
  for (auto &p : cast_sv_to_uint8(svs_locs, irbuilder)) {
    load = irbuilder.CreateAnd(p.second, ConstantInt::get(Int8, 127));
    break;
  }
  auto *tmp = irbuilder.CreateAdd(load, ConstantInt::get(Int8, 1));
  auto *tmp2 = irbuilder.CreateMul(tmp, load);
  auto *add_res = irbuilder.CreateURem(tmp2, ConstantInt::get(Int8, 2));
  auto *cmp_res = irbuilder.CreateICmpEQ(add_res, ConstantInt::get(Int8, 0));
  return cmp_res;
}
