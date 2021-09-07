/**
 * FlatPlusPass skeleton from https://github.com/chenx6/baby_obfuscator/blob/master/src/Flattening.cpp
 * Some function copied from https://github.com/obfuscator-llvm/obfuscator
 */
#include "FlatPlus.hpp"
#include "LegacyIndirectBrExpand.hpp"
#include "LegacyLowerSwitch.hpp"
#include "Utils.hpp"
#include "llvm/Transforms/ToyObfuscator/FlatPlusPass.hpp"
#include <algorithm>

FlatPlus::FlatPlus() {
    // init random numeral generator
    rng = std::mt19937(std::random_device{}());
}

bool FlatPlus::doFlat(Function &F) {
    // if only one basic block in this function, skip this function
    if (F.size() <= 1) {
        return false;
    }

    // re-init random for different function
    initRandom();

    // expand IndirectBrInst to SwitchInst
    createLegacyIndirectBrExpandPass()->runOnFunction(F);

    // lower switch
    createLegacyLowerSwitchPass()->runOnFunction(F);

    // get blocks that has LandingPadInst inside or are successors of
    // the blocks who has LandingPadInst inside.
    std::vector<BasicBlock *> excepts;
    {
        std::vector<BasicBlock *> pending;
        for (BasicBlock &bb : F) {
            if (isa<LandingPadInst>(bb.getFirstNonPHI())) {
                pending.emplace_back(&bb);
            }
        }
        while (pending.size()) {
            BasicBlock *tmpBB = pending.back();
            pending.pop_back();
            // found in excepts
            if (find(excepts.begin(), excepts.end(),
                     tmpBB) != excepts.end()) {
                continue;
            }
            excepts.emplace_back(tmpBB);
            auto term = tmpBB->getTerminator();
            for (size_t i = 0; i < term->getNumSuccessors(); i++) {
                BasicBlock *tmpSucc = term->getSuccessor(i);
                // not in pending
                if (find(pending.begin(), pending.end(),
                         tmpSucc) == pending.end()) {
                    pending.emplace_back(tmpSucc);
                }
            }
        }
    }

    // insert all basic block except prologue and expections into list
    SmallVector<BasicBlock *, 4> useful;
    for (BasicBlock &bb : F) {
        // found in exception block list
        if (find(excepts.begin(), excepts.end(),
                 &bb) != excepts.end()) {
            continue;
        }
        useful.emplace_back(&bb);

        // we should run pass lower switch before flat,
        // or we can't deal with SwitchInst
        if (isa<SwitchInst>(bb.getTerminator())) {
            errs() << "[-] "
                   << "Can't deal with `SwitchInst` in function: "
                   << F.getName() << "\n";
            return false;
        }
        // we should run pass lower switch before flat,
        // or we can't deal with IndirectBrInst
        if (isa<IndirectBrInst>(bb.getTerminator())) {
            errs() << "[-] "
                   << "Can't deal with `IndirectBrInst` in function: "
                   << F.getName() << "\n";
            return false;
        }
    }
    useful.erase(useful.begin());

    // if prologue's terminator is BranchInst or InvokeInst,
    // then split it into two blocks
    BasicBlock *prologue = &*F.begin();
    if (isa<BranchInst>(prologue->getTerminator()) ||
        isa<InvokeInst>(prologue->getTerminator())) {
        auto iter = prologue->end();
        while (iter != prologue->begin()) {
            --iter;
            if (isa<AllocaInst>(*iter)) {
                ++iter;
                break;
            }
        };
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

    initBlockInfo();

    // give each useful block a pair of (x, y), x is uniqu
    for (BasicBlock *bb : useful) {
        genBlockInfo(bb);
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
        genBlockInfo(translates[i]);
    }

    // give each exception block a pair of (x, y), x is unique
    for (BasicBlock *bb : excepts) {
        genBlockInfo(bb);
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
        dispatchSwitch->addCase(dispatcherBuilder.getInt32(blockInfos[bb].label), bb);
    }
    for (size_t i = 0; i < subTransCnt; i++) {
        BasicBlock *bb = translates[i];
        dispatchSwitch->addCase(dispatcherBuilder.getInt32(blockInfos[bb].label), bb);
    }

    // correct init label in prologue block
    labelStore->setOperand(0, dispatchSwitch->findCaseDest(translates[0]));

    // Recalculate switch Instruction
    ConstantInt *firstTransBlockLabel = dispatchSwitch->findCaseDest(translates[0]);
    for (BasicBlock *bb : useful) {
        // is InvokeInst
        if (isa<InvokeInst>(bb->getTerminator())) {
            auto term = bb->getTerminator();
            BasicBlock *normalSuccessor = term->getSuccessor(0);
            uint32_t yNow = blockInfos[bb].y;
            uint32_t xTarget = blockInfos[normalSuccessor].x;
            uint32_t yTarget = blockInfos[normalSuccessor].y;

            auto trampolineBlock = BasicBlock::Create(F.getContext(), "invokeTrampoline", &F);

            auto invokeTerm = dyn_cast<InvokeInst>(term);
            invokeTerm->setNormalDest(trampolineBlock);

            IRBuilder<> trampolineBuilder(trampolineBlock);

            auto v1 = trampolineBuilder.CreateLoad(yPtr);
            auto v2 = trampolineBuilder.CreateXor(v1, xTarget ^ yNow);
            trampolineBuilder.CreateStore(v2, xPtr);

            auto v3 = trampolineBuilder.CreateLoad(xPtr);
            auto v4 = trampolineBuilder.CreateXor(v3, xTarget ^ yTarget);
            trampolineBuilder.CreateStore(v4, yPtr);

            trampolineBuilder.CreateStore(firstTransBlockLabel, labelPtr);
            trampolineBuilder.CreateBr(dispatcher);
            continue;
        }

        // not InvokeInst
        switch (bb->getTerminator()->getNumSuccessors()) {
        case 0: {
            // no terminator
            break;
        }
        case 1: {
            // non-condition jump
            Instruction *terminator = bb->getTerminator();
            BasicBlock *successor = terminator->getSuccessor(0);
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
            // should not happen, may be a SwitchInst or IndirectBrInst
            assert(0 && "Maybe SwitchInst or IndirectBrInst still exist ???");
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
    for (BasicBlock *bb : excepts) {
        allBlocks.emplace_back(bb);
    }

    shuffleBlock(allBlocks);

    fixStack(F);
    return true;
}

void FlatPlus::initRandom() {
    subTransCnt = (rng() % 3) + 2; // 2,3,4
    imm32 = rng();
    imm8 = new uint8_t[subTransCnt];
    for (size_t i = 0; i < subTransCnt; i++) {
        do {
            imm8[i] = (rng() % (BITS_PER_BYTE * sizeof(uint32_t)));
        } while (!imm8[i]);
    }
}

uint32_t FlatPlus::genLabel(uint32_t x) {
    uint32_t a, b, label;
    a = imm32;
    b = x;

    for (size_t i = 0; i < subTransCnt; i++) {
        a = a + b;
        b = a ^ rol(b, imm8[i]);
    }

    label = b;
    return label;
}

void FlatPlus::initBlockInfo() {
    labelSet.clear();
    blockInfos.clear();
}

void FlatPlus::genBlockInfo(BasicBlock *bb) {
    uint32_t x, y, label;
    do {
        x = rng();
        y = rng();
        label = genLabel(x);
    } while ((labelSet.find(label) != labelSet.end()) && label);
    labelSet.insert(label);
    blockInfos.insert(std::make_pair(bb, labelInfo{x, y, label}));
}

void FlatPlus::allocTransBlockPtr(IRBuilder<> &builder) {
    for (size_t j = 0; j < sizeof(bakPtr) / sizeof(*bakPtr); j++) {
        bakPtr[j] = builder.CreateAlloca(builder.getInt32Ty());
    }
}

BasicBlock **FlatPlus::genTransBlocks(Function &F, Value *xPtr,
                                      Value *yPtr, Value *labelPtr) {
    BasicBlock **translates = new BasicBlock *[subTransCnt];
    char tmpBuf[0x40];
    for (size_t i = 0; i < subTransCnt; i++) {
        snprintf(tmpBuf, sizeof(tmpBuf), "Trans_%ld", i);
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

void FlatPlus::shuffleBlock(SmallVector<BasicBlock *, 0> &bb) {
    size_t cnt = bb.size();
    for (size_t i = 0; i < cnt * 2; i++) {
        auto first = bb[rng() % cnt];
        auto second = bb[rng() % cnt];
        first->moveBefore(second);
    }
}

char FlatPlusPass::ID = 0;
static RegisterPass<FlatPlusPass> X("fla_plus", "cfg flatten plus");
Pass *llvm::createFlatPlus(bool flag) { return new FlatPlusPass(flag); }
