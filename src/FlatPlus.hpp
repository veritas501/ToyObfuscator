#pragma once

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <random>
#include <set>
#include <vector>

using namespace llvm;

struct labelInfo {
    uint32_t x;
    uint32_t y;
    uint32_t label;
};

class FlatPlus {
public:
    FlatPlus();

    bool doFlat(Function &F);

private:
    std::mt19937 rng;
    uint32_t subTransCnt = 0;
    uint32_t imm32 = 0;
    uint8_t *imm8 = nullptr;
    AllocaInst *bakPtr[2] = {nullptr};

    // init random parameter
    void initRandom();

    // detail translate algo
    uint32_t genLabel(uint32_t x);

    void allocTransBlockPtr(IRBuilder<> &builder);

    BasicBlock **genTransBlocks(Function &F, AllocaInst *xPtr,
                                AllocaInst *yPtr, AllocaInst *labelPtr);

    void shuffleBlock(SmallVector<BasicBlock *, 0> &bb);
};

class FlatPlusPass : public FunctionPass {
public:
    static char ID;

    FlatPlusPass() : FunctionPass(ID) {
        flatPlus = new FlatPlus();
    }

    bool runOnFunction(Function &F) override {
        return flatPlus->doFlat(F);
    }

private:
    FlatPlus *flatPlus = nullptr;
};