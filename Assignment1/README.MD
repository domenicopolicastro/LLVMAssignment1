# Comandi da cartella principale (dopo build)

clang-18 -S -O0 -emit-llvm -Xclang -disable-O0-optnone ./test/test_algebraic_identity.c -o before.ll

### Opt di canonicalizzazione - mem2reg
opt-18 -passes=mem2reg -S before.ll -o before.clean.ll
### Opt Custom
opt-18 -load-pass-plugin=./build/libMyLLVMPasses.so -passes="all-opts" -S ./before.clean.ll -o ./optimized.ll

### test differenze
code --diff before.clean.ll optimized.ll