cmake_minimum_required(VERSION 3.20)

project(MyLLVMPasses)

find_package(LLVM 18.1 REQUIRED CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)

if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

add_library(MyLLVMPasses SHARED MyPasses.cpp)

target_link_libraries(MyLLVMPasses
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")