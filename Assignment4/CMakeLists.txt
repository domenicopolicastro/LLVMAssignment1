# Versione minima di CMake
cmake_minimum_required(VERSION 3.20)

# Nome del progetto
project(MyLLVMPasses)

#===============================================================================
# 1. TROVA LLVM (versione di sistema installata con apt)
#===============================================================================
# Cerca la versione 18.1 di LLVM installata sul sistema.
find_package(LLVM 18.1 REQUIRED CONFIG)

# Aggiungi le directory di include di LLVM al progetto in modo che #include <...> funzioni.
# L'opzione SYSTEM sopprime i warning provenienti dalle librerie esterne.
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

#===============================================================================
# 2. CONFIGURAZIONE DI BUILD (presa dal file del prof)
#===============================================================================
# Usa lo stesso standard C++ di LLVM (buona pratica).
set(CMAKE_CXX_STANDARD 17)

# Flag FONDAMENTALE: compila senza RTTI se anche LLVM è stato compilato così.
# Questo previene una categoria di errori di linking molto comuni.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

#===============================================================================
# 3. CREA IL NOSTRO PASS (adattato dal file del prof)
#===============================================================================
# Crea la libreria condivisa a partire dal nostro file sorgente.
add_library(MyLLVMPasses SHARED MyPasses.cpp)

# Il "trucco" del prof: non linkare esplicitamente a nessuna libreria LLVM.
# I simboli verranno risolti quando 'opt' caricherà il plugin.
# Questo risolve alla radice l'errore "option registered more than once".
target_link_libraries(MyLLVMPasses
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")