#pragma once
#include "llvm/Pass.h"

namespace llvm {
Pass *createBogusControlFlow(bool flag);
} // namespace llvm
