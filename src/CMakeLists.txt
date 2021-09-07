cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)

add_library(LLVMToyObfuscator
  MODULE
  Utils.cpp
  LegacyIndirectBrExpand.cpp
  LegacyLowerSwitch.cpp
  FlatPlus.cpp
)

add_dependencies(LLVMToyObfuscator intrinsics_gen)
