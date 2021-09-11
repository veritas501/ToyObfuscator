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

typedef uint32_t primeTy;

class BogusControlFlow {
public:
    BogusControlFlow();

    bool doBogusControlFlow(Function &F);

protected:
    std::mt19937 rng;
    std::vector<Value *> usableVars;
    bool firstObf;

    void collectUsableVars(std::vector<BasicBlock *> &useful);
    void buildBCF(BasicBlock *src, BasicBlock *dst,
                  std::vector<BasicBlock *> &jumpTarget,
                  Function &F);
    BasicBlock * buildJunk(Function &F);
};

class BogusControlFlowPass : public FunctionPass {
public:
    static char ID;
    bool flag;

    BogusControlFlowPass() : FunctionPass(ID) {
        bogusControlFlow = new BogusControlFlow();
        flag = true;
    }
    BogusControlFlowPass(bool flag) : FunctionPass(ID) {
        bogusControlFlow = new BogusControlFlow();
        this->flag = flag;
    }

    bool runOnFunction(Function &F) override;

private:
    BogusControlFlow *bogusControlFlow = nullptr;
};
