#include "FlatV2.hpp"
#include "LegacyLowerSwitch.hpp"
#include "Utils.hpp"

FlatV2::FlatV2() {
    // init random numeral generator
    rng = std::mt19937(std::random_device{}());
}

void FlatV2::reinitVariable() {
    dispatchStrucTy = nullptr;
    dispatchTblTy = nullptr;
    fnTy = nullptr;
    fnPtrTy = nullptr;

    dispatchFunc = nullptr;
    labelGVar = nullptr;
    labelSet.clear();
    jumpTable = nullptr;

    labelFuncMap.clear();
    oriFunc.clear();
}

bool FlatV2::doFlat(Module &M) {
    reinitVariable();

    // collect all functions
    for (Function &F : M) {
        oriFunc.emplace_back(&F);
    }

    // create global label
    Type *varTy = Type::getInt32Ty(M.getContext());
    labelGVar = new GlobalVariable(
        M, varTy, false, GlobalValue::ExternalLinkage, nullptr, "label");
    labelGVar->setDSOLocal(true);
    labelGVar->setInitializer(UndefValue::get(varTy));

    // create dispatch function symbol (empry function)
    if (!createDispatchFuncSymbol(M)) {
        return false;
    }

    // run function level pass
    for (Function *F : oriFunc) {
        doFlat(*F);
    }

    // create jump table from labelFuncMap
    if (!createJumpTable(M, labelFuncMap)) {
        return false;
    }
    // use jump table to create dispatch function
    if (!createDispatchFunc(M)) {
        return false;
    }

    return true;
}

bool FlatV2::createDispatchFuncSymbol(Module &M) {
    if (dispatchFunc) {
        errs() << "[!] "
               << "createDispatchFuncSymbol: symbol already created"
               << "\n";
        // already created
        return true;
    }

    // just create an empty function now
    fnTy = FunctionType::get(Type::getVoidTy(M.getContext()), false);
    fnPtrTy = PointerType::get(fnTy, 0);
    FunctionType *fn2Ty = FunctionType::get(
        fnPtrTy, false);
    dispatchFunc = Function::Create(
        fn2Ty, GlobalValue::ExternalLinkage, "dispatchFunc", M);

    return true;
}

bool FlatV2::createDispatchFunc(Module &M) {
    if (!dispatchFunc || !labelGVar ||
        !dispatchTblTy || !jumpTable) {
        errs() << "[!] "
               << "createDispatchFunc: something not initialized"
               << "\n";
        return false;
    }

    BasicBlock *bb1 = BasicBlock::Create(M.getContext(), "", dispatchFunc);
    BasicBlock *bb2 = BasicBlock::Create(M.getContext(), "", dispatchFunc);
    BasicBlock *bb3 = BasicBlock::Create(M.getContext(), "", dispatchFunc);
    IRBuilder<> builder1(bb1);
    IRBuilder<> builder2(bb2);
    IRBuilder<> builder3(bb3);

    // bb1
    LoadInst *loadLabel = builder1.CreateLoad(labelGVar);
    builder1.CreateBr(bb2);

    // bb2
    Argument *dummyIdx = new Argument(Type::getInt64Ty(M.getContext()));
    PHINode *phiIdx = builder2.CreatePHI(Type::getInt64Ty(M.getContext()), 2);
    phiIdx->addIncoming(dummyIdx, bb2);
    phiIdx->addIncoming(getConst64(M, 0), bb1);

    Value *labelElemPtr = builder2.CreateGEP(
        dispatchTblTy, jumpTable, {getConst64(M, 0), phiIdx, getConst32(M, 0)});
    LoadInst *labelElem = builder2.CreateLoad(labelElemPtr);
    Value *cmpLabel = builder2.CreateICmpEQ(labelElem, loadLabel);
    Value *Idx = builder2.CreateAdd(phiIdx, getConst64(M, 1));
    builder2.CreateCondBr(cmpLabel, bb3, bb2);

    // bb3
    Value *funcElemPtr = builder3.CreateGEP(
        dispatchTblTy, jumpTable, {getConst64(M, 0), phiIdx, getConst32(M, 1)});
    LoadInst *FuncElem = builder3.CreateLoad(funcElemPtr);
    builder3.CreateRet(FuncElem);

    // replace reference
    dummyIdx->replaceAllUsesWith(Idx);
    delete dummyIdx;

    return true;
}

bool FlatV2::createJumpTable(
    Module &M, std::set<std::pair<uint32_t, Constant *>> &labelFuncSet) {
    if (!fnTy || !fnPtrTy) {
        // something not initialized
        errs() << "[!] "
               << "createJumpTable: something not initialized"
               << "\n";
        return false;
    }
    size_t setSize = labelFuncSet.size();

    // create map struct type
    dispatchStrucTy = StructType::create(
        "", Type::getInt32Ty(M.getContext()), fnPtrTy);
    dispatchTblTy = ArrayType::get(dispatchStrucTy, setSize);

    // fill it with value
    std::vector<Constant *> tableElems;
    for (auto elem : labelFuncSet) {
        Constant *structElem = ConstantStruct::get(
            dispatchStrucTy,
            std::vector<Constant *>{
                ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), elem.first),
                ConstantExpr::getCast(Instruction::BitCast, elem.second, fnPtrTy)});
        tableElems.emplace_back(structElem);
    }

    // create global variable
    jumpTable = new GlobalVariable(
        M, dispatchTblTy, /* const */ true, GlobalValue::ExternalLinkage, nullptr);
    Constant *jumpTableInitializer = ConstantArray::get(dispatchTblTy, tableElems);
    jumpTable->setInitializer(jumpTableInitializer);

    return true;
}

// optimize multi return blocks into one block
bool FlatV2::oneReturn(Function &F) {
    std::vector<BasicBlock *> retBlock;
    for (BasicBlock &bb : F) {
        if (bb.getTerminator()->getNumSuccessors() == 0) {
            retBlock.emplace_back(&bb);
        }
    }
    // only one end block
    if (retBlock.size() <= 1) {
        return true;
    }

    // alloc tmp ret variable in prologue block
    BasicBlock *prologue = &*F.begin();
    IRBuilder<> varBuilder(prologue, prologue->begin());
    AllocaInst *retVarPtr = varBuilder.CreateAlloca(F.getReturnType());

    // create new return block
    BasicBlock *newRetBlock = BasicBlock::Create(F.getContext(), "", &F);
    IRBuilder<> newRetBuilder(newRetBlock);
    auto retVar = newRetBuilder.CreateLoad(retVarPtr);
    newRetBuilder.CreateRet(retVar);

    // edit each origin return block
    for (BasicBlock *bb : retBlock) {
        Instruction *term = bb->getTerminator();
        if (isa<ReturnInst>(term)) {
            Value *value = term->getOperand(0);
            bb->getTerminator()->eraseFromParent();
            IRBuilder<> tmp_builder(bb);
            tmp_builder.CreateStore(value, retVarPtr);
            tmp_builder.CreateBr(newRetBlock);
        }
    }

    return true;
}

// move stack variable to global variable
bool FlatV2::stackToGlobal(Function &F) {
    Module &M = *F.getParent();

    // search for AllocaInst, then create the corresponding global variable,
    // then search origin variable's usage and replace them with global
    // variable, finally, erase AllocaInst from basic block.
    for (BasicBlock &bb : F) {
        for (auto itr = bb.begin(); itr != bb.end();) {
            Instruction &inst = *itr;
            if (isa<AllocaInst>(inst)) {
                AllocaInst *allocaInst = dyn_cast<AllocaInst>(&inst);
                Type *instType = allocaInst->getAllocatedType();
                GlobalVariable *tmpGVar = new GlobalVariable(
                    M, instType, false, GlobalValue::ExternalLinkage,
                    nullptr, "g");
                tmpGVar->setDSOLocal(true);
                tmpGVar->setInitializer(UndefValue::get(instType));
                inst.replaceAllUsesWith(tmpGVar);
                itr = inst.eraseFromParent();
            } else {
                itr++;
            }
        }
    }

    return true;
}

Function *FlatV2::extractBlockFromFunction(BasicBlock &BB) {
    Module &M = *BB.getParent()->getParent();

    // create a function
    FunctionType *ft = FunctionType::get(
        Type::getVoidTy(M.getContext()), false);
    Function *extractFunc = Function::Create(ft, Function::ExternalLinkage, "extract", M);
    BB.removeFromParent();
    BB.insertInto(extractFunc);

    // may be unnecessary, but just in case
    fixStack(extractFunc);

    return extractFunc;
}

bool FlatV2::argToStack(Function &F) {
    // TODO 可能得处理一下varargs的问题
    // 用 arg_size() 获取不到 varargs
    size_t argSize = F.arg_size();

    if (F.size() < 1) {
        errs() << "[!] "
               << "Empty function: " << F.getName() << "\n";
        return true;
    }
    if (F.isVarArg()) {
        errs() << "[-] "
               << "Varargs function: " << F.getName() << "\n";
        return false;
    }

    BasicBlock *prologue = &*F.begin();
    IRBuilder<> builder(prologue, prologue->begin());

    for (size_t i = 0; i < argSize; i++) {
        Argument *funcArg = F.getArg(i);
        Type *argTy = funcArg->getType();
        AllocaInst *stackArg = builder.CreateAlloca(argTy);
        Argument *dummyArg = new Argument(argTy);
        builder.CreateStore(dummyArg, stackArg);
        auto load = builder.CreateLoad(stackArg);
        funcArg->replaceAllUsesWith(load);
        dummyArg->replaceAllUsesWith(funcArg);
    }

    return true;
}

bool FlatV2::doFlat(Function &F) {
    // skip empty (maybe external) function
    if (F.size() == 0) {
        errs() << "[!] "
               << "Skip external function: " << F.getName() << "\n";
        return true;
    }
    // if only one basic block in this function, skip this function
    if (F.size() == 1) {
        errs() << "[!] "
               << "Skip one block function: " << F.getName() << "\n";
        return true;
    }
    if (F.isVarArg()) {
        errs() << "[!] "
               << "Skip varargs function: " << F.getName() << "\n";
        return true;
    }

    // re-init random for different function
    initRandom();

    // lower switch
    FunctionPass *lower = createLegacyLowerSwitchPass();
    lower->runOnFunction(F);

    // do one-return
    if (!oneReturn(F)) {
        errs() << "[-] "
               << "run oneReturn failed"
               << "\n";
        return false;
    }

    // args to stack
    if (!argToStack(F)) {
        errs() << "[-] "
               << "run argToStack failed"
               << "\n";
        return false;
    };

    // insert all basic block except prologue and return into list
    SmallVector<BasicBlock *, 0> useful;
    BasicBlock *retBlock = nullptr;
    for (BasicBlock &bb : F) {
        if (isa<ReturnInst>(bb.getTerminator())) {
            if (!retBlock) {
                retBlock = &bb;
            } else {
                errs() << "[-] "
                       << "Multi return block found after one-return"
                       << "\n";
                return false;
            }
        } else {
            useful.emplace_back(&bb);
        }

        // ollvm can't deal with InvokeInst, which used by
        // @synchronized, try...catch, etc
        if (isa<InvokeInst>(bb.getTerminator())) {
            errs() << "[-] "
                   << "Can't deal with `InvokeInst` in function: "
                   << F.getName() << "\n";
            return false;
        }
        // we should run pass lower switch before flat,
        // or we can't deal with SwitchInst
        if (isa<SwitchInst>(bb.getTerminator())) {
            errs() << "[-] "
                   << "Can't deal with `SwitchInst` in function: "
                   << F.getName() << "\n";
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

    // give each useful block  a pair of (x, y), x is unique
    std::map<BasicBlock *, struct labelInfo> blockInfos;
    for (BasicBlock *bb : useful) {
        uint32_t x, y, label;
        do {
            x = rng();
            y = rng();
            label = genLabel(x);
        } while ((labelSet.find(label) != labelSet.end()) && label);
        labelSet.insert(label);
        blockInfos.insert(std::make_pair(bb, labelInfo{x, y, label}));
    }
    // retblock not in useful, but also need blockinfo
    {
        uint32_t x, y, label;
        do {
            x = rng();
            y = rng();
            label = genLabel(x);
        } while ((labelSet.find(label) != labelSet.end()) && label);
        labelSet.insert(label);
        blockInfos.insert(std::make_pair(retBlock, labelInfo{x, y, label}));
    }

    // initial x, y in prologue (jump to first block)
    uint32_t xInitVal = blockInfos[firstBlock].x;
    uint32_t yInitVal = blockInfos[firstBlock].y;

    // build prologue
    IRBuilder<> prologueBuilder(prologue, prologue->end());
    AllocaInst *xPtr = prologueBuilder.CreateAlloca(
        prologueBuilder.getInt32Ty(), nullptr, "x");
    AllocaInst *yPtr = prologueBuilder.CreateAlloca(
        prologueBuilder.getInt32Ty(), nullptr, "y");
    allocTransBlockPtr(prologueBuilder);
    // label been patched later (label is not allocated at this point)
    StoreInst *labelStore =
        prologueBuilder.CreateStore(prologueBuilder.getInt32(0xdeadbeef), labelGVar);
    prologueBuilder.CreateStore(prologueBuilder.getInt32(xInitVal), xPtr);
    prologueBuilder.CreateStore(prologueBuilder.getInt32(yInitVal), yPtr);

    // generate dispatch block
    BasicBlock *dispatchBlock1 = BasicBlock::Create(F.getContext(), "", &F, 0);
    BasicBlock *dispatchBlock2 = BasicBlock::Create(F.getContext(), "", &F, 0);
    {
        IRBuilder<> vmBuilder1(dispatchBlock1);
        IRBuilder<> vmBuilder2(dispatchBlock2);
        CallInst *nextFunc = vmBuilder1.CreateCall(dispatchFunc);
        ConstantPointerNull *nullPtr = ConstantPointerNull::get(fnPtrTy);
        ICmpInst *funcIsNull = new ICmpInst(*dispatchBlock1, ICmpInst::ICMP_EQ, nextFunc, nullPtr);
        vmBuilder1.CreateCondBr(funcIsNull, retBlock, dispatchBlock2);
        vmBuilder2.CreateCall(nextFunc);
        vmBuilder2.CreateBr(dispatchBlock1);
    }
    prologueBuilder.CreateBr(dispatchBlock1);

    // build translate blocks
    BasicBlock **translates = genTransBlocks(F, xPtr, yPtr, labelGVar);

    // give each translate block a pair of (x, y), x is unique
    for (size_t i = 0; i < subTransCnt; i++) {
        BasicBlock *bb = translates[i];
        uint32_t x, y, label;
        do {
            x = rng();
            y = rng();
            label = genLabel(x);
        } while ((labelSet.find(label) != labelSet.end()) && label);
        labelSet.insert(label);
        blockInfos.insert(std::make_pair(bb, labelInfo{x, y, label}));
    }

    // correct init label in prologue block
    ConstantInt *firstTransBlockLabel = getConst32(F, blockInfos[translates[0]].label);
    labelStore->setOperand(0, firstTransBlockLabel);

    // Recalculate switch Instruction
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

            caseBuilder.CreateStore(firstTransBlockLabel, labelGVar);
            caseBuilder.CreateRetVoid();
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

                caseBuilder.CreateStore(firstTransBlockLabel, labelGVar);
                caseBuilder.CreateRetVoid();
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
            caseBuilder.CreateStore(getConst32(F, blockInfos[translates[i + 1]].label), labelGVar);
        }
        caseBuilder.CreateRetVoid();
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

    // move stack variable to global
    stackToGlobal(F);

    for (auto bb : allBlocks) {
        Function *newFunc = extractBlockFromFunction(*bb);
        if (!newFunc) {
            errs() << "[-] extractBlockFromFunction failed\n";
            return false;
        }
        labelFuncMap.insert(std::make_pair(blockInfos[bb].label, newFunc));
    }
    // retblock.label -> nullptr (dispatch break)
    {
        auto zeroFunc = ConstantExpr::getCast(
            Instruction::BitCast, getConst32(F, 0), fnPtrTy);
        labelFuncMap.insert(std::make_pair(blockInfos[retBlock].label, zeroFunc));
    }

    return true;
}

char FlatV2Pass::ID = 0;
static RegisterPass<FlatV2Pass> X("fla_v2", "cfg flatten");
