# Comandi da cartella principale (dopo build)

clang-18 -S -O0 -emit-llvm -Xclang -disable-O0-optnone ./test/test_licm_advanced.c -o before.ll

### Opt di canonicalizzazione - mem2reg, loop-simplify
opt-18 -passes=mem2reg,loop-simplify,lcssa -S before.ll -o before.clean.ll
### Opt Loop Invariant Code Motion
opt-18 -load-pass-plugin=./build/libMyLLVMPasses.so -passes="loop(custom-licm)" -S ./before.clean.ll -o ./optimized.ll

### test differenze
code --diff before.clean.ll optimized.ll