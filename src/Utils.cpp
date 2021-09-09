#include "Utils.hpp"
#include "llvm/Transforms/Utils/Local.h"

std::string readAnnotate(Function &F) {
    std::string annotation = "";

    // Get annotation variable
    GlobalVariable *glob =
        F.getParent()->getGlobalVariable("llvm.global.annotations");

    if (!glob) {
        return "";
    }
    if (!isa<ConstantArray>(glob->getInitializer())) {
        return "";
    }
    ConstantArray *ca = dyn_cast<ConstantArray>(glob->getInitializer());
    for (unsigned i = 0; i < ca->getNumOperands(); ++i) {
        if (isa<ConstantStruct>(ca->getOperand(i))) {
            // Get the struct
            ConstantStruct *structAn = dyn_cast<ConstantStruct>(ca->getOperand(i));
            ConstantExpr *expr = dyn_cast<ConstantExpr>(structAn->getOperand(0));
            if (expr && expr->getOpcode() == Instruction::BitCast &&
                expr->getOperand(0) == &F) {
                // If it's a bitcast we can check if the annotation is concerning
                // the current function
                ConstantExpr *note = cast<ConstantExpr>(structAn->getOperand(1));
                // If it's a GetElementPtr, that means we found the variable
                // containing the annotations
                if (note->getOpcode() == Instruction::GetElementPtr) {
                    GlobalVariable *annoteStr =
                        dyn_cast<GlobalVariable>(note->getOperand(0));
                    if (annoteStr) {
                        ConstantDataSequential *data =
                            dyn_cast<ConstantDataSequential>(annoteStr->getInitializer());
                        if (data && data->isString()) {
                            annotation += data->getAsString().lower() + " ";
                        }
                    }
                }
            }
        }
    }

    return annotation;
}

bool doObfuscation(Function &F, std::string attr, bool flag) {
    // Check if declaration
    if (F.isDeclaration()) {
        return false;
    }

    // Check external linkage
    if (F.hasAvailableExternallyLinkage() != 0) {
        return false;
    }

    std::string attrYes = attr;
    std::string attrNo = "no_" + attr;

    std::string anno = readAnnotate(F);

    if (anno.find(attrNo) != std::string::npos) {
        return false;
    }
    if (flag || anno.find(attrYes) != std::string::npos) {
        return true;
    }

    return false;
}

bool valueEscapes(Instruction *inst) {
    BasicBlock *bb = inst->getParent();
    for (auto &u : inst->uses()) {
        const Instruction *xrefInst = cast<Instruction>(u.getUser());
        const PHINode *PN = dyn_cast<PHINode>(xrefInst);
        if (!PN) {
            if (xrefInst->getParent() != bb) {
                if (isa<InvokeInst>(inst) && isa<StoreInst>(xrefInst)) {
                    auto invokeInst = dyn_cast<InvokeInst>(inst);
                    auto normalDest = invokeInst->getNormalDest();
                    auto storeInst = dyn_cast<StoreInst>(xrefInst);
                    auto storeTarget = storeInst->getOperand(1);
                    if (storeInst->getParent() == normalDest &&
                        isa<AllocaInst>(storeTarget)) {
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
