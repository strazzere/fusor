#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "utils.hpp"
#include "inheritance.hpp"
#include "factories.hpp"
#include "json.hpp"

#include <algorithm>
#include <random>
#include <chrono>
#include <set>
#include <cstdlib>
#include <fstream>

using namespace std;
using namespace llvm;


Type *Int1 = llvm::Type::getInt1Ty(llvm::getGlobalContext());
Type *Int1_ptr = llvm::Type::getInt1PtrTy(llvm::getGlobalContext());
Type *Int8 = llvm::Type::getInt8Ty(llvm::getGlobalContext());
Type *Int8_ptr = llvm::Type::getInt8PtrTy(llvm::getGlobalContext());
Type *Int16 = llvm::Type::getInt16Ty(llvm::getGlobalContext());
Type *Int16_ptr = llvm::Type::getInt16PtrTy(llvm::getGlobalContext());
Type *Int32 = llvm::Type::getInt32Ty(llvm::getGlobalContext());
Type *Int32_ptr = llvm::Type::getInt32PtrTy(llvm::getGlobalContext());
Type *Int64 = llvm::Type::getInt64Ty(llvm::getGlobalContext());
Type *Int64_ptr = llvm::Type::getInt64PtrTy(llvm::getGlobalContext());
Type *Float = llvm::Type::getFloatTy(llvm::getGlobalContext());
Type *Float_ptr = llvm::Type::getFloatPtrTy(llvm::getGlobalContext());
Type *Double = llvm::Type::getDoubleTy(llvm::getGlobalContext());
Type *Double_ptr = llvm::Type::getDoublePtrTy(llvm::getGlobalContext());
Type *Void = llvm::Type::getVoidTy(getGlobalContext());

using json = nlohmann::json;

namespace {
  struct FusorPass : public ModulePass {
    static char ID;

    FusorPass() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      bool changed = false;

      if (!load_config()) {
        errs() << "\033[91m[ERROR]: Fail to load configuration file, exiting...\033[0mn\n";
        exit(1);
      }
      // errs() << M.getName() << "\n";
      if (IN_SET(M.getName(), module_black_list))
        return false;

      for (auto &F : M) {
        // ignore declaration
        if (F.isDeclaration()) {
          continue;
        }

        if (IN_SET(F.getName(), func_black_list)) {
          if (dbg_level > 1)
            errs() << "\033[96m[INFO]: '" << F.getName() << "' in the blacklist, ignoring...\033[0m\n";
          continue;
        }

        int op_cnt = 0;
        if (!use_random && IN_MAP(F.getName(), guide_map)) {
          op_cnt = guide_map[F.getName()];
        } else {
          op_cnt = 1;
        }

        if (op_cnt <= 0) {
          if (dbg_level > 1)
            errs() << "\033[96m[INFO]: '" << F.getName() << "' is ignored by guided mode...\033[0m\n";
          continue;
        }

        vector<Value *> sym_vars;

        rand_engine.seed(++seed);

        // append arguments
        for (auto &a : F.args()) {
          sym_vars.emplace_back(&a);
        }

        // Initialize
        if (sym_vars.empty()) { // nothing will be changed if this function's arg list is empty
          if (dbg_level > 2)
            errs() << "\033[95m[DEBUG]: '" << F.getName() << "' has zero arguments, skipping...\033[0m\n";
          continue;
        }

        if (func_names.is_open()) {
          func_names << F.getName().str() << "," << sym_vars.size() << "," << M.getName().str() << endl;
          func_names.close();
        }

        PuzzleBuilderFactory pz_factory;
        FunctionTransformerFactory tr_factory;

        auto pz_builder = pz_factory.get_builder_randomly(pred_conf, &M);
        if (pz_builder == nullptr) {
          errs() << "\033[91m[ERROR]: Fail to get an opaque predicates builder, exiting...\033[0m\n\n";
          exit(1);
        }

        auto tr_builder = tr_factory.get_transformer_randomly(trans_conf);
        if (tr_builder == nullptr) {
          errs() << "\033[91m[ERROR]: Fail to get a transformer, exiting...\033[0m\n\n";
          exit(1);
        }

        // start of obfuscation
        if (dbg_level > 1)
          errs() << "\033[96m[INFO]: Obfuscating '" << F.getName() << "'\033[0m\n";
        DominatorTree dom_tree(F);
        deque<BasicBlock *> BBs;

        for (auto &B : F) {
          BBs.emplace_back(&B);
        }

        auto *entry_BB = BBs.front();
//        auto svs_loc = move_symvar_to_front(entry_BB, sym_vars);
        vector<FusorSymVar* > fsym_vars;
        for (auto *sv : sym_vars) {
          auto *fsv = new FusorSymVar(sv, F);
          if (fsv->build_effective_region(dom_tree)) {
            fsym_vars.emplace_back(fsv);
            errs() << "t: " << *fsv->insert_point << "\n";
          }
        }

        if (fsym_vars.empty()) {
          if (dbg_level > 1)
            errs() << "\033[96m[INFO]: '" << F.getName() << "' is ignored because of lack of valid sym. vars...\033[0m\n";
        }
        changed = true;

        if (dbg_level > 2)
          errs() << "\033[95m[DEBUG] Before building predicates\033[0m\n";

        for (auto &B : F)
          B.setName("dbg");

//        for (auto *fsv : fsym_vars) {
//          errs() << fsv->work_list
//        }

//        errs() << op_cnt << "\ta0\n";
//        errs() << *entry_BB << "\n";
        IRBuilder<> irbuilder(entry_BB->getTerminator());
//        errs() << op_cnt << "\ta1\n";
        for (size_t i : range<size_t>(op_cnt)) {
//          errs() << i << "\ta2\n";
          pz_builder->build(fsym_vars);
        }

//        for (int i : range<size_t>(op_cnt - 1)) {
//          auto predicates_new = pz_builder->build(fsym_vars, entry_BB->getTerminator());
//          for (auto &p : predicates_new) {
//            auto *fsv = p.first; auto *val = p.second;
//            irbuilder.SetInsertPoint(fsv->insert_point);
//            predicates[fsv] = irbuilder.CreateAnd(predicates[fsv], val);
//          }
//        }

        if (dbg_level > 2)
          errs() << "\033[95m[DEBUG] Before transformation\033[0m\n";

        tr_builder->transform(&F, fsym_vars);

//        errs() << "\033[92m====== DONE ======\033[0m\n";
      }

      return changed;
    }

  private:
    bool load_config() {
      string config_path = "./config.json";

      if (const char *env_p = getenv("FUSOR_CONFIG")) {
        config_path = env_p;
      }

      ifstream in_file(config_path);

      if (in_file.good()) {
        try {
          in_file >> config;
        }
        catch (json::parse_error) {
          errs() << "\033[91m[ERROR]: JSON format ERROR! Exiting...\033[0m\n";
          return False;
        }
      } else {
        errs() << "\033[91m[ERROR]: Config file '" << config_path << "' not found! Exiting...\033[0m\n";
        return False;
      }

      return config_checker();
    }

    bool config_checker() {
      // check key trans and pred
      string keys[] = {"predicates", "transformations"};

      for (auto &k : keys) {
        if (IN_MAP(k, config)) {
          for (auto &p : config[k]) {
            if (!IN_MAP("name", p)) {
              errs() << "\033[91m[ERROR]: Missing \"name\" field.\033[0m\n";
              return False;
            }
            if (!IN_MAP("code", p)) {
              errs() << "\033[91m[ERROR]: Missing \"code\" field in " << p["name"].get<string>() << ".\033[0m\n";
              return False;
            }

            if (k == "predicates") {
              pred_conf[p["name"].get<string>()] = p["code"].get<uint64_t>();
            } else if (k == "transformations") {
              trans_conf[p["name"].get<string>()] = p["code"].get<uint64_t>();
            }
          }
        } else {
          errs() << "\033[91m[ERROR]: Missing key: '" << k << "'\033[0m\n";
          return False;
        }
      }

      if (IN_MAP("random", config)) {
        use_random = config["random"];
      } else {
        use_random = true;
      }

      if (IN_MAP("guide", config)) {
        for (auto &func_name : config["guide"].items()) {
          guide_map[func_name.key()] = func_name.value();
        }
      } else if (!use_random) {
        errs() << "\033[91m[ERROR]: Must provide guide information if random is set false.\033[0m\n";
      }

      if (IN_MAP("func_black_list", config)) {
        for (auto &p : config["func_black_list"]) {
          func_black_list.emplace(p.get<string>());
        }
      }

      if (IN_MAP("module_black_list", config)) {
        for (auto &p : config["module_black_list"]) {
          module_black_list.emplace(p.get<string>());
        }
      }

      if (IN_MAP("func_info", config)) {
        func_names.open(config["func_info"], ios::out | ios::app);
      }

      if (IN_MAP("debug", config)) {
        dbg_level = config["debug"];
      } else {
        dbg_level = 2;
      }

      return True;
    }

    unsigned seed = static_cast<unsigned>(chrono::system_clock::now().time_since_epoch().count());
    bool use_random;
    default_random_engine rand_engine;
    json config;
    map<string, uint64_t> trans_conf, pred_conf;
    map<string, size_t> guide_map;
    set<string> func_black_list;
    set<string> module_black_list;
    ofstream func_names;
    int dbg_level;
  };
};


char FusorPass::ID = 0;

static RegisterPass<FusorPass> X("fusor", "Fusor Pass", False, False);

// Automatically enable the pass.
static void registerFusorPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  PM.add(new FusorPass());
}

static RegisterStandardPasses RegisterMyPass1(PassManagerBuilder::EP_EnabledOnOptLevel0, registerFusorPass);
static RegisterStandardPasses RegisterMyPass2(PassManagerBuilder::EP_OptimizerLast, registerFusorPass);
