clang-18 -S -O0 -emit-llvm -Xclang -disable-O0-optnone test_licm.c -o test_licm.ll

opt-18 -load-pass-plugin=./libMyLLVMPasses.so -passes="mem2reg,loop-simplify,licm-pass" -S ../test/test_licm.ll
