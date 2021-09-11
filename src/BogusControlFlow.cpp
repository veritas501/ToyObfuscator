/*
 * base on https://blog.quarkslab.com/turning-regular-code-into-atrocities-with-llvm.html
 *
 * p1 and p2 are distinct prime numbers,
 * a1 and a2 are distinct strictly positive random numbers,
 * x and y are two variables picked from the program,
 * (they have to be reachable from the obfuscation instructions),
 * then p1*(x|a1)*(x|a1) != p2*(y|a2)*(y|a2) is constant
 */

#include "BogusControlFlow.hpp"
#include "LegacyIndirectBrExpand.hpp"
#include "LegacyLowerSwitch.hpp"
#include "Utils.hpp"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/ToyObfuscator/BogusControlFlowPass.hpp"

#define OVERFLOW_MASK (0x3FF)

// p*(a|b)*(a|b) <= 0xffffffff
// pow(0xffffffff, 1/3) = 1625.498
static const primeTy primes[] = {
    127, 131, 137, 139, 149, 151, 157, 163, 167, 173,
    179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
    233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
    283, 293, 307, 311, 313, 317, 331, 337, 347, 349,
    353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
    419, 421, 431, 433, 439, 443, 449, 457, 461, 463,
    467, 479, 487, 491, 499, 503, 509, 521, 523, 541,
    547, 557, 563, 569, 571, 577, 587, 593, 599, 601,
    607, 613, 617, 619, 631, 641, 643, 647, 653, 659,
    661, 673, 677, 683, 691, 701, 709, 719, 727, 733,
    739, 743, 751, 757, 761, 769, 773, 787, 797, 809,
    811, 821, 823, 827, 829, 839, 853, 857, 859, 863,
    877, 881, 883, 887, 907, 911, 919, 929, 937, 941,
    947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013,
    1019, 1021, 1031, 1033, 1039, 1049, 1051, 1061, 1063, 1069,
    1087, 1091, 1093, 1097, 1103, 1109, 1117, 1123, 1129, 1151,
    1153, 1163, 1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223,
    1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283, 1289, 1291,
    1297, 1301, 1303, 1307, 1319, 1321, 1327, 1361, 1367, 1373,
    1381, 1399, 1409, 1423, 1427, 1429, 1433, 1439, 1447, 1451,
    1453, 1459, 1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511,
    1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571, 1579, 1583,
    1597, 1601, 1607, 1609, 1613, 1619, 1621};

static cl::opt<int> BCFRate(
    "bcf_rate", cl::init(30),
    cl::desc("The probability that each basic block will be obfuscated. (default=30, max=100)"));

BogusControlFlow::BogusControlFlow() {
    // init random numeral generator
    rng = std::mt19937(std::random_device{}());
}

bool BogusControlFlow::doBogusControlFlow(Function &F) {
    // if only one basic block in this function, skip this function
    if (F.size() <= 1) {
        return false;
    }

    uint32_t bcfRate = BCFRate % 101; // [0,100]

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

    BasicBlock *prologue = &*F.begin();

    // insert all basic blocks which can become bcf fake
    // jump target into jumpTraget list
    // insert all obfuscatable basic block into useful list
    std::vector<BasicBlock *> jumpTarget;
    std::vector<BasicBlock *> useful;
    for (BasicBlock &bb : F) {
        // found in exception block list
        if (find(excepts.begin(), excepts.end(),
                 &bb) != excepts.end()) {
            continue;
        }
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
        // bcf need at least one successor
        if (bb.getTerminator()->getNumSuccessors() >= 1) {
            useful.emplace_back(&bb);
        }
        if (&bb != prologue) {
            jumpTarget.emplace_back(&bb);
        }
    }

    // collect all usable variables
    collectUsableVars(useful);
    if (!usableVars.size()) {
        // no usable variables found
        return false;
    }

    // do bcf for each useful block
    for (BasicBlock *bb : useful) {
        if (rng() % 100 >= bcfRate) {
            continue;
        }
        Instruction *term = bb->getTerminator();
        if (isa<InvokeInst>(term)) {
            BasicBlock *normalSuccessor = term->getSuccessor(0);
            BasicBlock *bcfTrampoline = BasicBlock::Create(
                F.getContext(), "bcfTrampoline", &F);
            InvokeInst *invokeTerm = dyn_cast<InvokeInst>(term);
            invokeTerm->setNormalDest(bcfTrampoline);
            jumpTarget.emplace_back(bcfTrampoline);
            buildBCF(bcfTrampoline, normalSuccessor, jumpTarget, F);
        } else if (term->getNumSuccessors() == 1) {
            BasicBlock *succ = term->getSuccessor(0);
            buildBCF(bb, succ, jumpTarget, F);
        } else if (term->getNumSuccessors() == 2) {
            int targetIdx = rng() % 2;
            BasicBlock *succ = term->getSuccessor(targetIdx);
            BasicBlock *bcfTrampoline = BasicBlock::Create(
                F.getContext(), "bcfTrampoline", &F);
            term->setSuccessor(targetIdx, bcfTrampoline);
            jumpTarget.emplace_back(bcfTrampoline);
            buildBCF(bcfTrampoline, succ, jumpTarget, F);
        } else {
            assert(0 && "WTF, how is this possible ???");
        }
    }

    fixStack(F);
    return true;
}

void BogusControlFlow::collectUsableVars(std::vector<BasicBlock *> &useful) {
    usableVars.clear();
    for (BasicBlock *bb : useful) {
        for (Instruction &inst : *bb) {
            if (inst.getType()->isIntegerTy()) {
                usableVars.emplace_back(&inst);
            }
        }
    }
}

void BogusControlFlow::buildBCF(
    BasicBlock *src, BasicBlock *dst,
    std::vector<BasicBlock *> &jumpTarget,
    Function &F) {
    auto term = src->getTerminator();
    IRBuilder<> builder(src, src->end());

    size_t primeCnt = sizeof(primes) / sizeof(*primes);
    Type *primeLLVMTy = IntegerType::getIntNTy(
        F.getContext(), sizeof(primeTy) * 8);
    primeTy p1Raw = primes[rng() % primeCnt];
    primeTy p2Raw;
    do {
        p2Raw = primes[rng() % primeCnt];
    } while (p2Raw == p1Raw);

    primeTy a1Raw;
    primeTy a2Raw;
    do {
        a1Raw = (rng() & OVERFLOW_MASK);
        a2Raw = (rng() & OVERFLOW_MASK);
    } while ((a2Raw == a1Raw) || !a1Raw || !a2Raw);

    // if no enough variables, create them :)
    if (usableVars.size() < 2) {
        auto v1 = builder.CreateAlloca(primeLLVMTy, nullptr);
        auto v2 = builder.CreateAlloca(primeLLVMTy, nullptr);
        auto l1 = builder.CreateLoad(v1);
        auto l2 = builder.CreateLoad(v2);
        usableVars.emplace_back(l1);
        usableVars.emplace_back(l2);
    }
    size_t usableIdx1 = rng() % usableVars.size();
    size_t usableIdx2;
    do {
        usableIdx2 = rng() % usableVars.size();
    } while (usableIdx2 == usableIdx1);

    BasicBlock *fakeDst;
    do {
        fakeDst = jumpTarget[rng() % jumpTarget.size()];
    } while (fakeDst == dst && jumpTarget.size() == 1);

    Value *usable1 = usableVars[usableIdx1];
    Value *usable2 = usableVars[usableIdx2];

    Constant *p1 = builder.getIntN(sizeof(primeTy) * 8, p1Raw);
    Constant *p2 = builder.getIntN(sizeof(primeTy) * 8, p2Raw);
    Constant *a1 = builder.getIntN(sizeof(primeTy) * 8, a1Raw);
    Constant *a2 = builder.getIntN(sizeof(primeTy) * 8, a2Raw);
    Constant *overflowMask = builder.getIntN(
        sizeof(primeTy) * 8, OVERFLOW_MASK);

    Value *LhsCast = builder.CreateZExtOrTrunc(usable1, primeLLVMTy);
    Value *LhsAnd = builder.CreateAnd(LhsCast, overflowMask);
    Value *LhsOr = builder.CreateOr(LhsAnd, a1);
    Value *LhsSquare = builder.CreateMul(LhsOr, LhsOr);
    Value *LhsTot = builder.CreateMul(LhsSquare, p1);

    Value *RhsCast = builder.CreateZExtOrTrunc(usable2, primeLLVMTy);
    Value *RhsAnd = builder.CreateAnd(RhsCast, overflowMask);
    Value *RhsOr = builder.CreateOr(RhsAnd, a2);
    Value *RhsSquare = builder.CreateMul(RhsOr, RhsOr);
    Value *RhsTot = builder.CreateMul(RhsSquare, p2);

    Value *comp = builder.CreateICmp(
        CmpInst::Predicate::ICMP_EQ, LhsTot, RhsTot);
    Value *castComp = builder.CreateCondBr(comp, fakeDst, dst);
    if (term) {
        term->eraseFromParent();
    }

    //add new constant to usableVars
    usableVars.emplace_back(LhsCast);
    usableVars.emplace_back(LhsAnd);
    usableVars.emplace_back(LhsOr);
    usableVars.emplace_back(LhsSquare);
    usableVars.emplace_back(LhsTot);
    usableVars.emplace_back(RhsCast);
    usableVars.emplace_back(RhsAnd);
    usableVars.emplace_back(RhsOr);
    usableVars.emplace_back(RhsSquare);
    usableVars.emplace_back(RhsTot);
}

bool BogusControlFlowPass::runOnFunction(Function &F) {
    if (doObfuscation(F, "bcf", flag)) {
        return bogusControlFlow->doBogusControlFlow(F);
    }
    return false;
}

char BogusControlFlowPass::ID = 0;
static RegisterPass<BogusControlFlowPass> X("bcf", "bogus control flow");

Pass *llvm::createBogusControlFlow(bool flag) {
    return new BogusControlFlowPass(flag);
}
