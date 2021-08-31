#pragma once

namespace llvm {
class FunctionPass;
FunctionPass *createLegacyLowerSwitchPass();
} // namespace llvm
