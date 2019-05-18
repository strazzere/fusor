#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/CommandLine.h"
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
    ofstream func_names;

    FusorPass() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      if (!load_config()) {
        errs() << "[Error] Fail to load configuration file, exiting...\n\n";
        exit(1);
      }
      // errs() << M.getName() << "\n";
      if (IN_SET(M.getName(), module_black_list))
        return false;

      for (auto &F : M) {
        int op_cnt = 0;
        // ignore declaration
        if (F.isDeclaration())
          continue;

        if (IN_SET(F.getName(), func_black_list)) {
          errs() << "Black " << F.getName() << "\n";
          continue;
        }

        if (!use_random) {
          if (IN_MAP(F.getName(), guide_map)) {
            op_cnt = guide_map[F.getName()];
          }
        }
        else {
          op_cnt = 1;
        }

        if (op_cnt <= 0)
          continue;

        vector<Value *> sym_vars;
        deque<BasicBlock *> BBs;

        rand_engine.seed(++seed);

        errs() << "Obfuscating \"" << F.getName() << "\"\n";

        // append arguments
        for (auto &a : F.args()) {
          sym_vars.emplace_back(&a);
        }

        // Initialize
        if (sym_vars.empty()) { // nothing will be changed if this function's arg list is empty
          errs() << "Pass " << F.getName() << "\n";
          continue;
        }

        func_names.open("/home/neil/Fusion/func.csv", ios::out | ios::app);
        func_names << F.getName().str() << ", " << sym_vars.size() << ", " << M.getName().str() << endl;
        func_names.close();

        for (auto &B : F)
          BBs.emplace_back(&B);

        // move symvar alloca and store instruction into front
        auto *sv_bb = BasicBlock::Create(F.getContext(), "sv_bb", &F, BBs.front());
        BranchInst::Create(BBs.front(), sv_bb);

        auto svs_loc = move_symvar_to_front(sv_bb, sym_vars);
        // after moving, then you can do whatever you want with symvar

        PuzzleBuilderFactory pz_factory;
        FunctionTransformerFactory tr_factory;

        auto pz_builder = pz_factory.get_builder_randomly(pred_conf, &M);
        if (pz_builder == nullptr) {
          errs() << "[Error] Fail to get opaque predicates builder, exiting...\n\n";
          exit(1);
        }

        IRBuilder<> irbuilder(sv_bb->getTerminator());
        auto *predicate = pz_builder->build(svs_loc, sv_bb->getTerminator());
        for (int i : range<size_t >(op_cnt - 1)) {
          auto *tmp = pz_builder->build(svs_loc, sv_bb->getTerminator());
          irbuilder.SetInsertPoint(sv_bb->getTerminator());
          predicate = irbuilder.CreateAnd(tmp, predicate);
        }

        // merge sv bb
        sv_bb->getTerminator()->eraseFromParent();
        auto *merge_point = ISINSTANCE(BBs.front()->getFirstInsertionPt(), Instruction);
        vector<Instruction *> backup_ins_sv;
        for (auto &I : *sv_bb) {
          backup_ins_sv.emplace_back(&I);
        }
        for (auto *I : backup_ins_sv) {
          I->moveBefore(merge_point);
        }
        sv_bb->eraseFromParent();

        auto tr_builder = tr_factory.get_transformer_randomly(trans_conf);
        if (tr_builder == nullptr) {
          errs() << "[Error] Fail to get transformer, exiting...\n\n";
          exit(1);
        }
        tr_builder->transform(&F, predicate);

        errs() << "====== DONE ======\n";
      }

      return True;
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
          errs() << "--- JSON format ERROR! --- \n";
          return False;
        }
      } else {
        errs() << "--- Config file " << config_path << " not found! ---\n";
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
              errs() << "--- Missing \"name\" field ---\n";
              return False;
            }
            if (!IN_MAP("code", p)) {
              errs() << "--- Missing \"code\" field in " << p["name"].get<string>() << " ---\n";
              return False;
            }

            if (k == "predicates") {
              pred_conf[p["name"].get<string>()] = p["code"].get<uint64_t>();
            } else if (k == "transformations") {
              trans_conf[p["name"].get<string>()] = p["code"].get<uint64_t>();
            }
          }
        } else {
          errs() << "--- Missing key: " << k << " ---\n";
          return False;
        }
      }

      if (IN_MAP("random", config)) {
        use_random = config["random"];
      }
      else {
        use_random = true;
      }

      if (IN_MAP("guide", config)) {
        for (auto &func_name : config["guide"].items()) {
          guide_map[func_name.key()] = func_name.value();
        }
      }
      else if (!use_random) {
        errs() << "--- Must provide guider information if random is set false ---\n";
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
