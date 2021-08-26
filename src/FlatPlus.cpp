/**
 * FlatPlusPass skeleton from https://github.com/chenx6/baby_obfuscator/blob/master/src/Flattening.cpp
 * Some function copied from https://github.com/obfuscator-llvm/obfuscator
 */

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

using namespace llvm;

#define BITS_PER_BYTE (8)

template <typename T>
T rol(T val, size_t count) {
    size_t bitcount = sizeof(T) * BITS_PER_BYTE;
    count %= bitcount;
    return (val << count) | (val >> (bitcount - count));
}

template <typename T>
T ror(T val, size_t count) {
    size_t bitcount = sizeof(T) * BITS_PER_BYTE;
    count %= bitcount;
    return (val >> count) | (val << (bitcount - count));
}

struct labelInfo {
    uint32_t x;
    uint32_t y;
    uint32_t label;
};

class FlatPlusPass : public FunctionPass {
public:
    static char ID;

    FlatPlusPass() : FunctionPass(ID) {
        // init random numeral generator
        rng = std::mt19937(std::random_device{}());
    }

    bool runOnFunction(Function &F) override;

private:
    std::mt19937 rng;
    uint32_t subTransCnt = 0;
    uint32_t imm32 = 0;
    uint8_t *imm8 = nullptr;
    AllocaInst *bakPtr[2] = {nullptr};

    void initRandom();

    // detail translate algo
    uint32_t genLabel(uint32_t x);

    void allocTransBlockPtr(IRBuilder<> &builder);

    BasicBlock **genTransBlocks(Function &F, AllocaInst *xPtr,
                                AllocaInst *yPtr, AllocaInst *labelPtr);

    void shuffleBlock(SmallVector<BasicBlock *, 0> &bb);

    // copied from ollvm
    bool valueEscapes(Instruction *Inst);

    // copied from ollvm
    void fixStack(Function *f);
};

char FlatPlusPass::ID = 0;

bool FlatPlusPass::runOnFunction(Function &F) {
    // if only one basic block in this function, skip this function
    if (F.size() <= 1) {
        return false;
    }

    // re-init random for different function
    initRandom();

    // insert all basic block except prologue into list
    SmallVector<BasicBlock *, 0> useful;
    for (BasicBlock &bb : F) {
        useful.emplace_back(&bb);

        // early check, we should run pass `-lowerswitch` before flat,
        // or we can't processed SwitchInst
        if (isa<InvokeInst>(bb.getTerminator()) ||
            isa<SwitchInst>(bb.getTerminator())) {
            return false;
        }
    }
    useful.erase(useful.begin());

    // if prologue's terminator is BranchInst or IndirectBrInst,
    // then split it into two blocks
    BasicBlock *prologue = &*F.begin();
    if (isa<BranchInst>(prologue->getTerminator()) ||
        isa<IndirectBrInst>(prologue->getTerminator())) {
        auto iter = prologue->end();
        iter--;
        if (prologue->size() > 1) {
            iter--;
        }
        BasicBlock *tempBlock = prologue->splitBasicBlock(iter);
        useful.insert(useful.begin(), tempBlock);
    }
    // remove prologue's terminator
    prologue->getTerminator()->eraseFromParent();
    // get first block from list
    BasicBlock *firstBlock = *useful.begin();

    // create main loop
    BasicBlock *dispatcher = BasicBlock::Create(F.getContext(), "Dispatcher", &F);
    BasicBlock *defaultBr = BasicBlock::Create(F.getContext(), "DefaultBranch", &F);

    // give each useful block  a pair of (x, y), x is unique
    std::map<BasicBlock *, struct labelInfo> blockInfos;
    std::set<uint32_t> xValSet;
    for (BasicBlock *bb : useful) {
        uint32_t x, y, label;
        do {
            x = rng();
        } while (xValSet.find(x) != xValSet.end());
        xValSet.insert(x);
        y = rng();
        label = genLabel(x);
        blockInfos.insert(std::make_pair(bb, labelInfo{x, y, label}));
    }

    // initial x, y in prologue (jump to first block)
    uint32_t xInitVal = blockInfos[firstBlock].x;
    uint32_t yInitVal = blockInfos[firstBlock].y;

    // build prologue
    IRBuilder<> prologueBuilder(prologue, prologue->end());
    AllocaInst *labelPtr =
        prologueBuilder.CreateAlloca(prologueBuilder.getInt32Ty(), nullptr, "label");
    AllocaInst *xPtr = prologueBuilder.CreateAlloca(prologueBuilder.getInt32Ty(), nullptr, "x");
    AllocaInst *yPtr = prologueBuilder.CreateAlloca(prologueBuilder.getInt32Ty(), nullptr, "y");
    allocTransBlockPtr(prologueBuilder);
    // label been patched later (label is not allocated at this point)
    StoreInst *labelStore =
        prologueBuilder.CreateStore(prologueBuilder.getInt32(0xdeadbeef), labelPtr);
    prologueBuilder.CreateStore(prologueBuilder.getInt32(xInitVal), xPtr);
    prologueBuilder.CreateStore(prologueBuilder.getInt32(yInitVal), yPtr);
    prologueBuilder.CreateBr(dispatcher);

    // build translate blocks
    BasicBlock **translates = genTransBlocks(F, xPtr, yPtr, labelPtr);

    // give each translate block a pair of (x, y), x is unique
    for (size_t i = 0; i < subTransCnt; i++) {
        BasicBlock *bb = translates[i];
        uint32_t x, y, label;
        do {
            x = rng();
        } while (xValSet.find(x) != xValSet.end());
        xValSet.insert(x);
        y = rng();

        label = genLabel(x);
        blockInfos.insert(std::make_pair(bb, labelInfo{x, y, label}));
    }

    // build dispatcher block
    IRBuilder<> dispatcherBuilder(dispatcher);
    LoadInst *label = dispatcherBuilder.CreateLoad(labelPtr);
    SwitchInst *dispatchSwitch = dispatcherBuilder.CreateSwitch(label, defaultBr, 0);

    // build default block
    BranchInst::Create(dispatcher, defaultBr);

    // move all useful blocks and translate blocks before loopBack block,
    // and add their's label into dispatcher's switch case
    for (BasicBlock *bb : useful) {
        // bb->moveBefore(loopBack);
        dispatchSwitch->addCase(dispatcherBuilder.getInt32(blockInfos[bb].label), bb);
    }
    for (size_t i = 0; i < subTransCnt; i++) {
        BasicBlock *bb = translates[i];
        // bb->moveBefore(loopBack);
        dispatchSwitch->addCase(dispatcherBuilder.getInt32(blockInfos[bb].label), bb);
    }

    // correct init label in prologue block
    labelStore->setOperand(0, dispatchSwitch->findCaseDest(translates[0]));

    // Recalculate switch Instruction
    ConstantInt *firstTransBlockLabel = dispatchSwitch->findCaseDest(translates[0]);
    for (BasicBlock *bb : useful) {
        switch (bb->getTerminator()->getNumSuccessors()) {
        case 0: {
            // no terminator
            break;
        }
        case 1: {
            // non-condition jump
            Instruction *terminator = bb->getTerminator();
            BasicBlock *successor = terminator->getSuccessor(0);
            uint32_t xNow = blockInfos[bb].x;
            uint32_t yNow = blockInfos[bb].y;
            uint32_t xTarget = blockInfos[successor].x;
            uint32_t yTarget = blockInfos[successor].y;

            IRBuilder<> caseBuilder(bb, bb->end());

            auto v1 = caseBuilder.CreateLoad(yPtr);
            auto v2 = caseBuilder.CreateXor(v1, xTarget ^ yNow);
            caseBuilder.CreateStore(v2, xPtr);

            auto v3 = caseBuilder.CreateLoad(xPtr);
            auto v4 = caseBuilder.CreateXor(v3, xTarget ^ yTarget);
            caseBuilder.CreateStore(v4, yPtr);

            caseBuilder.CreateStore(firstTransBlockLabel, labelPtr);
            caseBuilder.CreateBr(dispatcher);
            terminator->eraseFromParent();
            break;
        }
        case 2: {
            // condition jump
            Instruction *terminator = bb->getTerminator();
            BasicBlock *trueSuccessor = terminator->getSuccessor(0);
            BasicBlock *falseSuccessor = terminator->getSuccessor(1);
            uint32_t xNow = blockInfos[bb].x;
            uint32_t yNow = blockInfos[bb].y;
            uint32_t xTrueTarget = blockInfos[trueSuccessor].x;
            uint32_t yTrueTarget = blockInfos[trueSuccessor].y;
            uint32_t xFalseTarget = blockInfos[falseSuccessor].x;
            uint32_t yFalseTarget = blockInfos[falseSuccessor].y;

            IRBuilder<> caseBuilder(bb, bb->end());
            if (BranchInst *endBr = dyn_cast<BranchInst>(bb->getTerminator())) {
                auto v1 = caseBuilder.CreateLoad(yPtr);
                auto v2 = caseBuilder.CreateXor(v1, xTrueTarget ^ yNow);
                auto v3 = caseBuilder.CreateXor(v1, xFalseTarget ^ yNow);
                auto v4 = caseBuilder.CreateSelect(endBr->getCondition(), v2, v3);
                caseBuilder.CreateStore(v4, xPtr);

                auto v5 = caseBuilder.CreateLoad(xPtr);
                auto v6 = caseBuilder.CreateXor(v5, xTrueTarget ^ yTrueTarget);
                auto v7 = caseBuilder.CreateXor(v5, xFalseTarget ^ yFalseTarget);
                auto v8 = caseBuilder.CreateSelect(endBr->getCondition(), v6, v7);
                caseBuilder.CreateStore(v8, yPtr);

                caseBuilder.CreateStore(firstTransBlockLabel, labelPtr);
                caseBuilder.CreateBr(dispatcher);
                terminator->eraseFromParent();
            }
            break;
        }
        default: {
            // should not happen, may be a SwitchInst
            break;
        }
        }
    }
    for (size_t i = 0; i < subTransCnt; i++) {
        BasicBlock *bb = translates[i];
        Instruction *terminator = bb->getTerminator();
        IRBuilder<> caseBuilder(bb, bb->end());
        if (i != subTransCnt - 1) {
            // not last translate block
            caseBuilder.CreateStore(dispatchSwitch->findCaseDest(translates[i + 1]), labelPtr);
        }
        caseBuilder.CreateBr(dispatcher);
        if (terminator) {
            terminator->eraseFromParent();
        }
    }

    // shuffle blocks
    SmallVector<BasicBlock *, 0> allBlocks;
    for (BasicBlock *bb : useful) {
        allBlocks.emplace_back(bb);
    }
    for (size_t i = 0; i < subTransCnt; i++) {
        BasicBlock *bb = translates[i];
        allBlocks.emplace_back(bb);
    }

    shuffleBlock(allBlocks);

    fixStack(&F);
    return true;
}

void FlatPlusPass::initRandom() {
    subTransCnt = (rng() & 7) + 3;
    imm32 = rng();
    imm8 = new uint8_t[subTransCnt];
    for (size_t i = 0; i < subTransCnt; i++) {
        imm8[i] = (uint8_t)rng();
    }
}

uint32_t FlatPlusPass::genLabel(uint32_t x) {
    uint32_t a, b, label;
    a = imm32;
    b = x;

    for (int i = 0; i < subTransCnt; i++) {
        a = a + b;
        b = a ^ rol(b, imm8[i]);
    }

    label = b;
    return label;
}

void FlatPlusPass::allocTransBlockPtr(IRBuilder<> &builder) {
    for (int j = 0; j < sizeof(bakPtr) / sizeof(*bakPtr); j++) {
        bakPtr[j] = builder.CreateAlloca(builder.getInt32Ty());
    }
}

BasicBlock **FlatPlusPass::genTransBlocks(Function &F, AllocaInst *xPtr,
                                          AllocaInst *yPtr, AllocaInst *labelPtr) {
    BasicBlock **translates = new BasicBlock *[subTransCnt];
    char tmpBuf[0x40];
    for (int i = 0; i < subTransCnt; i++) {
        snprintf(tmpBuf, sizeof(tmpBuf), "Trans_%d", i);
        translates[i] = BasicBlock::Create(F.getContext(), tmpBuf, &F);

        IRBuilder<> builder(translates[i]);
        if (i == 0) {
            // first trans block
            builder.CreateStore(builder.CreateLoad(xPtr), bakPtr[0]);
            builder.CreateStore(builder.CreateLoad(yPtr), bakPtr[1]);
            builder.CreateStore(builder.CreateLoad(xPtr), yPtr);
            builder.CreateStore(builder.getInt32(imm32), xPtr);
        }
        auto v0 = builder.CreateLoad(xPtr);
        auto v1 = builder.CreateLoad(yPtr);
        auto v2 = builder.getInt8(imm8[i]);
        auto v4 = builder.CreateAdd(v1, v0);
        auto v5 = builder.CreateAnd(v2, 31);
        auto v6 = builder.CreateZExt(v5, builder.getInt32Ty());
        auto v7 = builder.CreateShl(v1, v6);
        auto v8 = builder.CreateSub(builder.getInt32(32), v6);
        auto v9 = builder.CreateLShr(v1, v8);
        auto v10 = builder.CreateOr(v9, v7);
        auto v11 = builder.CreateXor(v10, v4);

        builder.CreateStore(v4, xPtr);
        builder.CreateStore(v11, yPtr);

        if (i == subTransCnt - 1) {
            // last trans block
            builder.CreateStore(builder.CreateLoad(bakPtr[0]), xPtr);
            builder.CreateStore(builder.CreateLoad(bakPtr[1]), yPtr);
            builder.CreateStore(v11, labelPtr);
        }
    }

    return translates;
}

void FlatPlusPass::shuffleBlock(SmallVector<BasicBlock *, 0> &bb) {
    size_t cnt = bb.size();
    for (int i = 0; i < cnt * 2; i++) {
        auto first = bb[rng() % cnt];
        auto second = bb[rng() % cnt];
        first->moveBefore(second);
    }
}

bool FlatPlusPass::valueEscapes(Instruction *Inst) {
    BasicBlock *BB = Inst->getParent();
    for (Value::use_iterator UI = Inst->use_begin(),
                             E = Inst->use_end();
         UI != E; ++UI) {
        Instruction *I = cast<Instruction>(*UI);
        if (I->getParent() != BB || isa<PHINode>(I)) {
            return true;
        }
    }
    return false;
}

void FlatPlusPass::fixStack(Function *f) {
    // Try to remove phi node and demote reg to stack
    std::vector<PHINode *> tmpPhi;
    std::vector<Instruction *> tmpReg;
    BasicBlock *bbEntry = &*f->begin();

    do {
        tmpPhi.clear();
        tmpReg.clear();

        for (Function::iterator i = f->begin(); i != f->end(); ++i) {
            for (BasicBlock::iterator j = i->begin(); j != i->end(); ++j) {
                if (isa<PHINode>(j)) {
                    PHINode *phi = cast<PHINode>(j);
                    tmpPhi.push_back(phi);
                    continue;
                }
                if (!(isa<AllocaInst>(j) && j->getParent() == bbEntry) &&
                    (valueEscapes(&*j) || j->isUsedOutsideOfBlock(&*i))) {
                    tmpReg.push_back(&*j);
                    continue;
                }
            }
        }
        for (unsigned int i = 0; i != tmpReg.size(); ++i) {
            DemoteRegToStack(*tmpReg.at(i), f->begin()->getTerminator());
        }

        for (unsigned int i = 0; i != tmpPhi.size(); ++i) {
            DemotePHIToStack(tmpPhi.at(i), f->begin()->getTerminator());
        }

    } while (tmpReg.size() != 0 || tmpPhi.size() != 0);
}

static RegisterPass<FlatPlusPass> X("fla_plus", "cfg flatten");
