//
// Created by neil on 3/19/19.
//
#include "bases.hpp"
#include "inheritance.hpp"
#include "utils.hpp"
#include "util_funcs.hpp"


using namespace std;
using namespace llvm;


const string FlatArrayPuzzle::id = "FlatArrayPuzzle";


Value *FlatArrayPuzzle::build(SymvarLoc &svs_locs, Instruction *insert_point) {
  IRBuilder<> irBuilder(insert_point);
  ArrayType *a_type = ArrayType::get(Int8, array_size);
  if (dynamic) {
    auto rand_func = rand_16807(module, static_cast<uint32_t >(std::chrono::system_clock::now().time_since_epoch().count()));

  }
  else {
    vector<uint8_t> a_data = range<uint8_t>(0, array_size), b_data;
    shuffle(a_data.begin(), a_data.end(), rand_eng);
    for (auto i : range<size_t>(array_size)) {

    }
  }

}


unique_ptr<PuzzleBuilder> FlatArrayPuzzle::clone(uint64_t puzzle_code, llvm::Module *M) {
  return std::make_unique<FlatArrayPuzzle>(puzzle_code, M);
}