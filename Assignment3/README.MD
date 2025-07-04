# Comandi da cartella principale (dopo build)

clang-18 -S -O0 -emit-llvm -Xclang -disable-O0-optnone ./test/difficult_licm.c -o difficult_licm.ll

### Opt di canonicalizzazione - mem2reg e loop-simplify
opt-18 -passes=mem2reg,loop-simplify -S difficult_licm.ll -o difficult_licm.clean.ll
### Opt Loop Invariant Code Motion
opt-18 -load-pass-plugin=./build/libMyLLVMPasses.so -passes=loop(custom-licm) ./difficult_licm.clean.ll -o ./optimized.ll -S