# Cartella principale
clang-18 -S -O0 -emit-llvm -Xclang -disable-O0-optnone test/test_loop_fusion.c -o test_fusion.ll
opt-18 -passes=mem2reg,loop-simplify -S test_fusion.ll -o test_fusion.clean.ll
# Cartella build
opt-18 -load-pass-plugin=./libMyLLVMPasses.so -passes=loop-fusion-pass ../test_fusion.clean.ll -o ../test_fusion.fused.ll -S