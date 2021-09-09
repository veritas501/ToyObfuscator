#pragma once

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <random>
#include <set>
#include <vector>

using namespace llvm;

static cl::opt<bool> DontFlaInvoke(
    "dont_fla_invoke", cl::init(false),
    cl::desc("Don't flat this function if find InvokeInst inside"));

struct labelInfo {
    uint32_t x;
    uint32_t y;
    uint32_t label;
};

class FlatPlus {
public:
    FlatPlus();

    bool doFlat(Function &F);

protected:
    std::mt19937 rng;
    uint32_t subTransCnt = 0;
    uint32_t imm32 = 0;
    uint8_t *imm8 = nullptr;
    AllocaInst *bakPtr[2] = {nullptr};

    std::map<BasicBlock *, struct labelInfo> blockInfos;
    std::set<uint32_t> labelSet;

    // init random parameter
    void initRandom();

    // detail translate algo
    uint32_t genLabel(uint32_t x);

    void initBlockInfo();
    void genBlockInfo(BasicBlock *bb);

    void allocTransBlockPtr(IRBuilder<> &builder);

    BasicBlock **genTransBlocks(
        Function &F, Value *xPtr, Value *yPtr, Value *labelPtr);

    void shuffleBlock(SmallVector<BasicBlock *, 0> &bb);
};

class FlatPlusPass : public FunctionPass {
public:
    static char ID;
    bool flag;

    FlatPlusPass() : FunctionPass(ID) {
        flatPlus = new FlatPlus();
        flag = true;
    }
    FlatPlusPass(bool flag) : FunctionPass(ID) {
        flatPlus = new FlatPlus();
        this->flag = flag;
    }

    bool runOnFunction(Function &F) override {
        if (flag) {
            // -dont_fla_invoke
            if (DontFlaInvoke) {
                for (BasicBlock &bb : F) {
                    if (isa<InvokeInst>(bb.getTerminator())) {
                        return true;
                    }
                }
            }
            return flatPlus->doFlat(F);
        }
        return true;
    }

private:
    FlatPlus *flatPlus = nullptr;
};
