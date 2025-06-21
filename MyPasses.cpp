#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <vector>

using namespace llvm;

struct StrengthReductionPass : public PassInfoMixin<StrengthReductionPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        
        std::vector<Instruction*> toErase;

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                    
                    if (op->getOpcode() == Instruction::Mul) {
                        ConstantInt *C = nullptr;
                        Value *X = nullptr;

                        if ((C = dyn_cast<ConstantInt>(op->getOperand(0))) && C->getSExtValue() == 15) {
                            X = op->getOperand(1);
                        } else if ((C = dyn_cast<ConstantInt>(op->getOperand(1))) && C->getSExtValue() == 15) {
                            X = op->getOperand(0);
                        }

                        if (X) { 
                            IRBuilder<> Builder(&I);
                            
                            Value *Shift = Builder.CreateShl(X, 4);      
                            Value *Sub = Builder.CreateSub(Shift, X); 

                            op->replaceAllUsesWith(Sub);

                            toErase.push_back(op);
                        }
                    }
                    
                    else if (op->getOpcode() == Instruction::SDiv) { 
                        if (auto *C = dyn_cast<ConstantInt>(op->getOperand(1))) {
                            int64_t Divisor = C->getSExtValue();

                            if (Divisor > 0 && isPowerOf2_64(Divisor)) {
                                errs() << "Trovata divisione per potenza di 2: " << I << "\n";
  
                                uint64_t ShiftAmt = Log2_64(Divisor);

                                IRBuilder<> Builder(&I);
                                Value* X = op->getOperand(0); 

                                Value* Shift = Builder.CreateAShr(X, ShiftAmt); 

                                op->replaceAllUsesWith(Shift);
                                toErase.push_back(op);
                            }
                        }
                    }
                }
            }
        }
        
        for (Instruction *I : toErase) {
            I->eraseFromParent();
        }

        return toErase.empty() ? PreservedAnalyses::all() : PreservedAnalyses::none();
    }
};

struct AlgebraicIdentityPass : public PassInfoMixin<AlgebraicIdentityPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "Eseguo AlgebraicIdentityPass sulla funzione:  \n";
        std::vector<Instruction*> toErase;

        for (auto &BB : F) {
            for (auto &I : BB) {
                
                if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                    
                    Value *lhs = op->getOperand(0);
                    Value *rhs = op->getOperand(1);

                    if (op->getOpcode() == Instruction::Add) {
                        
                        ConstantInt *constOperand = nullptr;
                        Value *otherOperand = nullptr;

                        if (auto *constRHS = dyn_cast<ConstantInt>(rhs)) {
                            if (constRHS->isZero()) {
                                constOperand = constRHS;
                                otherOperand = lhs;
                            }
                        }
                        else if (auto *constLHS = dyn_cast<ConstantInt>(lhs)) {
                            if (constLHS->isZero()) {
                                constOperand = constLHS;
                                otherOperand = rhs;
                            }
                        }
                        if (constOperand && otherOperand) {
                            op->replaceAllUsesWith(otherOperand);
                            toErase.push_back(op);
                        }
                    }

                    else if (op->getOpcode() == Instruction::Mul) {
                        
                        ConstantInt *constOperand = nullptr;
                        Value *otherOperand = nullptr;

                        if (auto *constRHS = dyn_cast<ConstantInt>(rhs)) {
                            if (constRHS->isOne()) {
                                constOperand = constRHS;
                                otherOperand = lhs;
                            }
                        }
                        else if (auto *constLHS = dyn_cast<ConstantInt>(lhs)) {
                            if (constLHS->isOne()) {
                                constOperand = constLHS;
                                otherOperand = rhs;
                            }
                        }
                        
                        if (constOperand && otherOperand) {
                            op->replaceAllUsesWith(otherOperand);
                            toErase.push_back(op);
                        }
                    }
                }
            }
        }
        for (Instruction *I : toErase) {
            I->eraseFromParent();
        }

        return toErase.empty() ? PreservedAnalyses::all() : PreservedAnalyses::none();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "MyLLVMPasses", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "algebraic-identity") {
                        FPM.addPass(AlgebraicIdentityPass());
                        return true;
                    }
                    if (Name == "strength-reduction") {
                        FPM.addPass(StrengthReductionPass());
                        return true;
                    }
                    if (Name == "all-opts") {
                        FPM.addPass(AlgebraicIdentityPass());
                        FPM.addPass(StrengthReductionPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}
