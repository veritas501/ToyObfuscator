#include "Utils.hpp"
#include "llvm/Transforms/Utils/Local.h"

bool valueEscapes(Instruction *inst) {
    BasicBlock *bb = inst->getParent();
    for (auto &u : inst->uses()) {
        const Instruction *xrefInst = cast<Instruction>(u.getUser());
        const PHINode *PN = dyn_cast<PHINode>(xrefInst);
        if (!PN) {
            if (xrefInst->getParent() != bb) {
                if (isa<InvokeInst>(inst) && isa<StoreInst>(xrefInst)) {
                    auto storeInst = dyn_cast<StoreInst>(xrefInst);
                    auto storeVarName = storeInst->getOperand(1)->getName();
                    if (storeVarName.find(".reg2mem") != std::string::npos) {
                        continue;
                    }
                }
                return true;
            }
            continue;
        }

        if (PN->getIncomingBlock(u) != bb) {
            return true;
        }
    }
    return false;
}

void fixStack(Function &F) {
    // Try to remove phi node and demote reg to stack
    std::vector<PHINode *> tmpPhi;
    std::vector<Instruction *> tmpReg;
    BasicBlock *bbEntry = &*F.begin();

    do {
        tmpPhi.clear();
        tmpReg.clear();
        for (BasicBlock &bb : F) {
            for (Instruction &inst : bb) {
                if (isa<PHINode>(&inst)) {
                    PHINode *phi = dyn_cast<PHINode>(&inst);
                    tmpPhi.push_back(phi);
                    continue;
                }
                if (!(isa<AllocaInst>(inst) &&
                      inst.getParent() == bbEntry) &&
                    valueEscapes(&inst)) {
                    tmpReg.push_back(&inst);
                    continue;
                }
            }
        }
        for (auto i : tmpReg) {
            DemoteRegToStack(*i, bbEntry->getTerminator());
        }
        for (auto i : tmpPhi) {
            DemotePHIToStack(i, bbEntry->getTerminator());
        }
    } while (tmpReg.size() != 0 || tmpPhi.size() != 0);
}
