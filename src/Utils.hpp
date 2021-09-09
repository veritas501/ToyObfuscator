#pragma once

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include <iostream>

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

template <typename T>
ConstantInt *getConst64(T &x, uint64_t v) {
    return ConstantInt::get(Type::getInt64Ty(x.getContext()), v);
}

template <typename T>
ConstantInt *getConst32(T &x, uint32_t v) {
    return ConstantInt::get(Type::getInt32Ty(x.getContext()), v);
}

std::string readAnnotate(Function &F);

bool doObfuscation(Function &F, std::string anno, bool flag);

// copied from ollvm
bool valueEscapes(Instruction *Inst);

// copied from ollvm
void fixStack(Function &F);
