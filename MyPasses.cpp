#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <vector>

using namespace llvm;

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
                    return false;
                }
            );
        }
    };
}
