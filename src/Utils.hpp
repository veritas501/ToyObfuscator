#pragma once

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
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

// copied from ollvm
bool valueEscapes(Instruction *Inst);

// copied from ollvm
void fixStack(Function *f);