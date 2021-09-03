#pragma once

#include "FlatPlus.hpp"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"
#include <iostream>
#include <random>
#include <set>
#include <vector>

class FlatV2 : public FlatPlus {
public:
    FlatV2();

    bool doFlat(Module &M);

protected:
    StructType *dispatchStrucTy = nullptr;
    ArrayType *dispatchTblTy = nullptr;
    FunctionType *fnTy = nullptr;
    PointerType *fnPtrTy = nullptr;

    Function *dispatchFunc = nullptr;
    GlobalVariable *labelGVar = nullptr;
    std::set<uint32_t> labelSet;
    GlobalVariable *jumpTable = nullptr;

    std::set<std::pair<uint32_t, Constant *>> labelFuncMap;
    std::vector<Function *> oriFunc;

    void reinitVariable();
    bool doFlat(Function &F);
    bool argToStack(Function &F);
    bool oneReturn(Function &F);
    bool stackToGlobal(Function &F);
    Function *extractBlockFromFunction(BasicBlock &BB);
    bool createDispatchFuncSymbol(Module &M);
    bool createDispatchFunc(Module &M);
    bool createDummyJumpTable(Module &M);
    bool createJumpTable(
        Module &M, std::set<std::pair<uint32_t, Constant *>> &labelFuncMap);
};

class FlatV2Pass : public ModulePass {
public:
    static char ID;
    bool flag;

    FlatV2Pass() : ModulePass(ID) {
        flatV2 = new FlatV2();
        flag = true;
    }

    FlatV2Pass(bool flag) : ModulePass(ID) {
        flatV2 = new FlatV2();
        this->flag = flag;
    }

    bool runOnModule(Module &M) override {
        if (flag) {
            return flatV2->doFlat(M);
        }
        return true;
    }

private:
    FlatV2 *flatV2 = nullptr;
};
