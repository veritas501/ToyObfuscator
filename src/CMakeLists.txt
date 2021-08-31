cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)

add_library(MyObfuscator
  MODULE
  Utils.cpp
  LegacyLowerSwitch.cpp
  FlatPlus.cpp
)