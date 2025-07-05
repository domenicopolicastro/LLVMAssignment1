# Comandi da cartella principale (dopo build)
clang-18 -S -O0 -emit-llvm -Xclang -disable-O0-optnone test/test_loop_fusion.c -o before.ll
### Opt di canonicalizzazione - mem2reg e loop-simplify
opt-18 -passes=mem2reg,loop-simplify -S before.ll -o before.clean.ll
### Opt Loop Fusion
opt-18 -load-pass-plugin=./build/libMyLLVMPasses.so -passes=loop-fusion-pass ./before.clean.ll -o ./optimized.ll -S
### test differenze
code --diff before.clean.ll optimized.ll