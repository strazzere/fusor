//
// Created by neil on 3/19/19.
//

#ifndef PROJECT_UTIL_FUNCS_HPP
#define PROJECT_UTIL_FUNCS_HPP

#include "llvm/IR/Function.h"

std::pair<llvm::Function*, llvm::Value*> rand_16807(llvm::Module *m, uint32_t seed);


inline std::string compose_wrapper(std::string &fname) {
  return "FUSOR_" + fname + "_WRAPPER";
}


inline std::string compose_shared_op(std::string &fname) {
  return "FUSOR_" + fname + "_SHARED_SV";
}

#endif //PROJECT_UTIL_FUNCS_HPP
