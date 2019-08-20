//
// Created by neil on 10/11/18.
//

#include "bases.hpp"
#include "utils.hpp"
#include "inheritance.hpp"

#include <random>

using namespace std;
using namespace llvm;


const string DeepArrayPuzzle::id = "DeepArrayPuzzle";


Value* DeepArrayPuzzle::build(SymvarLoc &svs_locs, Instruction* insert_point) {
  ArrayType *a_type = ArrayType::get(Int8, array_size);
  vector<vector<uint8_t>> a_data;

  if (!insert_point)
    return nullptr;

  // array builder
  for (auto _ : range<int>(0, fst_depth + scd_depth)) {
    a_data.push_back(range<uint8_t>(0, array_size));
    shuffle(a_data[_].begin(), a_data[_].end(), rand_eng);
  }

  for (auto i : range<int>(array_size)) {
    auto tmp = a_data[fst_depth - 1][i];
    for (auto d : range<int>(scd_depth - 1)) {
      tmp = a_data[d + fst_depth][tmp];
    }
    a_data[fst_depth + scd_depth - 1][tmp] = a_data[fst_depth - 1][i];
  }

  // create global arrays
  vector<GlobalVariable*> glb_arrays;
  vector<Constant*> data_row;
  for (auto _ : range<int>(fst_depth + scd_depth)) {
    data_row.clear();
    for (auto n : a_data[_])
      data_row.push_back(ConstantInt::get(Int8, n));

    auto *tmp = new GlobalVariable(*module, a_type, True, GlobalVariable::InternalLinkage,
      ConstantArray::get(a_type, data_row), "fusor");
    glb_arrays.push_back(tmp);
  }

  // do the load job
  Value *array_ref_array[2] = {ConstantInt::get(Int8, 0), ConstantInt::get(Int8, 0)};
  IRBuilder<> irbuilder(insert_point);

  vector<pair<Value*, Value*>> load_res;
  for (auto &p : svs_locs) {
    auto *sv = p.first;
    auto *loaded = irbuilder.CreateAnd(p.second, ConstantInt::get(Int8, 127));
    Value *fst_index, *scd_index;

    // first level
    auto *res = loaded;
    for (auto i : range<int>(fst_depth)) {
      if (array_size != 128) {
        res = irbuilder.CreateURem(res, ConstantInt::get(Int8, array_size));
      }
      array_ref_array[1] = res;
      auto inbound_index = ArrayRef<Value *>(array_ref_array, 2);
      res = irbuilder.CreateInBoundsGEP(glb_arrays[i], inbound_index, "index");
      res = irbuilder.CreateLoad(res, "loaded");
    }
    fst_index = res;

    // second level
    for (auto i : range<int>(scd_depth)) {
      if (array_size < 255) {
        res = irbuilder.CreateURem(res, ConstantInt::get(Int8, array_size), "remed");
      }
      array_ref_array[1] = res;
      auto inbound_index = ArrayRef<Value *>(array_ref_array, 2);
      res = irbuilder.CreateInBoundsGEP(glb_arrays[i + fst_depth], inbound_index, "index");
      res = irbuilder.CreateLoad(res, "loaded");
    }
    scd_index = res;

    load_res.emplace_back(pair<Value*, Value*>(fst_index, scd_index));
  }

  Value *b_res = ConstantInt::get(Int1, 1);
  for (auto p : load_res) {
    auto *tmp = irbuilder.CreateICmpEQ(p.first, p.second, "res");
    b_res = irbuilder.CreateAnd(b_res, tmp, "ares");
  }

  return b_res;
}

size_t DeepArrayPuzzle::build(std::vector<FusorSymVar *> &fsym_vars) {
  if (fsym_vars.empty()) {
//    errs() << "0.0\n";
    return fsym_vars.size();
  }

  ArrayType *a_type = ArrayType::get(Int8, array_size);
  vector<vector<uint8_t>> a_data;

//  errs() << "0\n";

  // array builder
  for (auto _ : range<int>(0, fst_depth + scd_depth)) {
    a_data.push_back(range<uint8_t>(0, array_size));
    shuffle(a_data[_].begin(), a_data[_].end(), rand_eng);
  }

  for (auto i : range<int>(array_size)) {
    auto tmp = a_data[fst_depth - 1][i];
    for (auto d : range<int>(scd_depth - 1)) {
      tmp = a_data[d + fst_depth][tmp];
    }
    a_data[fst_depth + scd_depth - 1][tmp] = a_data[fst_depth - 1][i];
  }

//  errs() << "0.1\n";

  // create global arrays
  vector<GlobalVariable*> glb_arrays;
  vector<Constant*> data_row;
  for (auto _ : range<int>(fst_depth + scd_depth)) {
    data_row.clear();
    for (auto n : a_data[_])
      data_row.push_back(ConstantInt::get(Int8, n));

    auto *tmp = new GlobalVariable(*module, a_type, True, GlobalVariable::InternalLinkage,
                                   ConstantArray::get(a_type, data_row), "fusor");
    glb_arrays.push_back(tmp);
  }

//  errs() << "1\n";
  // do the load job
  Value *array_ref_array[2] = {ConstantInt::get(Int8, 0), ConstantInt::get(Int8, 0)};
  for (auto *fsv : fsym_vars) {
    auto r = fsv->insert_point == nullptr;
//    errs() << r << "\n";
//    errs() << "1.-1\n" << *fsv->insert_point << "\n";
    IRBuilder<> irbuilder(fsv->insert_point);
//    errs() << "1.0-1\n";
    auto *loaded = irbuilder.CreateAnd(fsv->casted_val, ConstantInt::get(Int8, 127));
    Value *fst_index, *scd_index;
    auto *res = loaded;

//    errs() << "1.0\n";
    // first level
    for (auto i : range<int>(fst_depth)) {
      if (array_size != 128) {
        res = irbuilder.CreateURem(res, ConstantInt::get(Int8, array_size));
      }
      array_ref_array[1] = res;
      auto inbound_index = ArrayRef<Value *>(array_ref_array, 2);
      res = irbuilder.CreateInBoundsGEP(glb_arrays[i], inbound_index, "index");
      res = irbuilder.CreateLoad(res, "loaded");
    }
    fst_index = res;

//    errs() << "1.1\n";

    // second level
    for (auto i : range<int>(scd_depth)) {
      if (array_size < 255) {
        res = irbuilder.CreateURem(res, ConstantInt::get(Int8, array_size), "remed");
      }
      array_ref_array[1] = res;
      auto inbound_index = ArrayRef<Value *>(array_ref_array, 2);
      res = irbuilder.CreateInBoundsGEP(glb_arrays[i + fst_depth], inbound_index, "index");
      res = irbuilder.CreateLoad(res, "loaded");
    }
    scd_index = res;

    auto *obf_pred = irbuilder.CreateICmpEQ(fst_index, scd_index);
    if (fsv->obf_val) {
      fsv->obf_val = irbuilder.CreateAnd(obf_pred, fsv->obf_val);
    }
    else
      fsv->obf_val = obf_pred;

//    errs() << "1.2\n";
  }

//  errs() << "2\n";

  return fsym_vars.size();
}

unique_ptr<PuzzleBuilder> DeepArrayPuzzle::clone(uint64_t puzzle_code, llvm::Module *M) {
  return std::make_unique<DeepArrayPuzzle>(puzzle_code, M);
}
