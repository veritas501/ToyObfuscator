#pragma once

namespace llvm {
class FunctionPass;
FunctionPass *createLegacyIndirectBrExpandPass();
} // namespace llvm
