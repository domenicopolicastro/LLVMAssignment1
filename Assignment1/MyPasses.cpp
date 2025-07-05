#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MathExtras.h"
#include <vector>
#include <map>

using namespace llvm;

// --- Funzioni di utilità per l'analisi ---

// Funzione per separare operando costante e non costante
void getConstantAndNonConstantOperands(BinaryOperator *BinOp, Value *&ConstantOp, Value *&NonConstantOp) {
    if (isa<ConstantInt>(BinOp->getOperand(0))) {
        ConstantOp = BinOp->getOperand(0);
        NonConstantOp = BinOp->getOperand(1);
    } else if (isa<ConstantInt>(BinOp->getOperand(1))) {
        ConstantOp = BinOp->getOperand(1);
        NonConstantOp = BinOp->getOperand(0);
    } else {
        ConstantOp = nullptr;
        NonConstantOp = nullptr;
    }
}


// --- Pass di Ottimizzazione ---

struct AlgebraicIdentityPass : public PassInfoMixin<AlgebraicIdentityPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        std::vector<Instruction*> toErase;
        bool changed = false;

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                    Value *lhs = op->getOperand(0);
                    Value *rhs = op->getOperand(1);

                    // Gestisce X + 0 = X e X - 0 = X
                    if (op->getOpcode() == Instruction::Add || op->getOpcode() == Instruction::Sub) {
                        if (auto *constRHS = dyn_cast<ConstantInt>(rhs)) {
                            if (constRHS->isZero()) {
                                op->replaceAllUsesWith(lhs);
                                toErase.push_back(op);
                                changed = true;
                            }
                        }
                    }

                    // Gestisce 0 + X = X (solo per Add, non per Sub)
                    if (op->getOpcode() == Instruction::Add) {
                         if (auto *constLHS = dyn_cast<ConstantInt>(lhs)) {
                            if (constLHS->isZero()) {
                                op->replaceAllUsesWith(rhs);
                                toErase.push_back(op);
                                changed = true;
                            }
                        }
                    }

                    // Gestisce X * 1 = X e X / 1 = X
                    if (op->getOpcode() == Instruction::Mul || op->getOpcode() == Instruction::SDiv || op->getOpcode() == Instruction::UDiv) {
                        if (auto *constRHS = dyn_cast<ConstantInt>(rhs)) {
                            if (constRHS->isOne()) {
                                op->replaceAllUsesWith(lhs);
                                toErase.push_back(op);
                                changed = true;
                            }
                        }
                    }

                    // Gestisce 1 * X = X (solo per Mul)
                    if (op->getOpcode() == Instruction::Mul) {
                        if (auto *constLHS = dyn_cast<ConstantInt>(lhs)) {
                            if (constLHS->isOne()) {
                                op->replaceAllUsesWith(rhs);
                                toErase.push_back(op);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        for (Instruction *I : toErase) {
            I->eraseFromParent();
        }

        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};


struct StrengthReductionPass : public PassInfoMixin<StrengthReductionPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        std::vector<Instruction*> toErase;
        bool changed = false;

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                    Value *ConstantOp = nullptr;
                    Value *NonConstantOp = nullptr;
                    getConstantAndNonConstantOperands(op, ConstantOp, NonConstantOp);

                    if (!ConstantOp) continue;

                    ConstantInt *C = cast<ConstantInt>(ConstantOp);
                    IRBuilder<> Builder(&I);

                    // --- Riduzione per la Moltiplicazione ---
                    if (op->getOpcode() == Instruction::Mul) {
                        int64_t val = C->getSExtValue();
                        
                        // Caso 1: Moltiplicazione per una potenza di 2 -> x * 8 => x << 3
                        if (isPowerOf2_64(val)) {
                            uint64_t shiftAmt = Log2_64(val);
                            Value *Shift = Builder.CreateShl(NonConstantOp, shiftAmt);
                            op->replaceAllUsesWith(Shift);
                            toErase.push_back(op);
                            changed = true;
                        }
                        // Caso 2: Moltiplicazione per 2^k - 1 -> x * 15 => (x << 4) - x
                        else if (isPowerOf2_64(val + 1)) {
                            uint64_t shiftAmt = Log2_64(val + 1);
                            Value *Shift = Builder.CreateShl(NonConstantOp, shiftAmt);
                            Value *Sub = Builder.CreateSub(Shift, NonConstantOp);
                            op->replaceAllUsesWith(Sub);
                            toErase.push_back(op);
                            changed = true;
                        }
                        // Caso 3: Moltiplicazione per 2^k + 1 -> x * 9 => (x << 3) + x
                        else if (isPowerOf2_64(val - 1)) {
                             uint64_t shiftAmt = Log2_64(val - 1);
                             Value* Shift = Builder.CreateShl(NonConstantOp, shiftAmt);
                             Value* Add = Builder.CreateAdd(Shift, NonConstantOp);
                             op->replaceAllUsesWith(Add);
                             toErase.push_back(op);
                             changed = true;
                        }
                    }
                    // --- Riduzione per la Divisione ---
                    else if (op->getOpcode() == Instruction::SDiv || op->getOpcode() == Instruction::UDiv) {
                        // Assicuriamoci che il divisore sia la costante
                        if (op->getOperand(1) == ConstantOp) {
                            int64_t val = C->getSExtValue();
                            if (val > 0 && isPowerOf2_64(val)) {
                                uint64_t shiftAmt = Log2_64(val);
                                Value *Shift = nullptr;
                                if (op->getOpcode() == Instruction::SDiv) {
                                    Shift = Builder.CreateAShr(op->getOperand(0), shiftAmt); // Shift aritmetico
                                } else {
                                    Shift = Builder.CreateLShr(op->getOperand(0), shiftAmt); // Shift logico
                                }
                                op->replaceAllUsesWith(Shift);
                                toErase.push_back(op);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        for (Instruction *I : toErase) {
            I->eraseFromParent();
        }

        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

struct MultiInstructionOptPass : public PassInfoMixin<MultiInstructionOptPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        std::vector<Instruction*> toErase;
        bool changed = false;

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *firstOp = dyn_cast<BinaryOperator>(&I)) {
                    Value *ConstantOp = nullptr;
                    Value *NonConstantOp = nullptr;
                    getConstantAndNonConstantOperands(firstOp, ConstantOp, NonConstantOp);

                    if (!ConstantOp) continue;

                    // Itera su tutti gli usi della prima istruzione
                    for (auto &U : firstOp->uses()) {
                        User *user = U.getUser();
                        if (auto *secondOp = dyn_cast<BinaryOperator>(user)) {
                            // Controlla se l'uso è un'altra operazione binaria
                            if (secondOp->getOperand(0) == firstOp && secondOp->getOperand(1) == ConstantOp) {
                                bool patternFound = false;
                                // Pattern: (b + k) - k => b
                                if (firstOp->getOpcode() == Instruction::Add && secondOp->getOpcode() == Instruction::Sub) {
                                    patternFound = true;
                                }
                                // Pattern: (b * k) / k => b
                                else if (firstOp->getOpcode() == Instruction::Mul && 
                                           (secondOp->getOpcode() == Instruction::SDiv || secondOp->getOpcode() == Instruction::UDiv)) {
                                    patternFound = true;
                                }

                                if (patternFound) {
                                    secondOp->replaceAllUsesWith(NonConstantOp);
                                    toErase.push_back(secondOp);
                                    changed = true;
                                }
                            }
                             // Controlla il caso commutativo per Add e Mul
                            else if (secondOp->getOperand(1) == firstOp && secondOp->getOperand(0) == ConstantOp) {
                                bool patternFound = false;
                                // Pattern: k + (b - k) => b
                                if (firstOp->getOpcode() == Instruction::Sub && secondOp->getOpcode() == Instruction::Add) {
                                     patternFound = true;
                                }
                                // Pattern: k * (b / k) => b
                                else if ((firstOp->getOpcode() == Instruction::SDiv || firstOp->getOpcode() == Instruction::UDiv) && secondOp->getOpcode() == Instruction::Mul) {
                                     patternFound = true;
                                }

                                if(patternFound) {
                                    secondOp->replaceAllUsesWith(NonConstantOp);
                                    toErase.push_back(secondOp);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Rimuovi le istruzioni marcate in modo sicuro
        for (Instruction* I : toErase) {
            I->eraseFromParent();
        }

        // Ora, un secondo passaggio per pulire le istruzioni originali che potrebbero essere diventate inutili
        std::vector<Instruction*> deadInstructions;
        for (auto &BB : F) {
            for (auto &I : BB) {
                // FIX: Usa I.isTerminator() invece di isa<TerminatorInst>(I)
                if (I.use_empty() && isa<BinaryOperator>(I) && !I.isTerminator()) {
                     bool isMarked = false;
                     for(Instruction* marked : toErase) {
                         if (&I == marked) isMarked = true;
                     }
                     if(!isMarked) deadInstructions.push_back(&I);
                }
            }
        }

        for(Instruction* I : deadInstructions) {
            I->eraseFromParent();
            changed = true;
        }


        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};


// --- Registrazione del Plugin ---

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "MyLLVMPasses", "v0.4",
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
                    if (Name == "multi-instruction-opt"){
                        FPM.addPass(MultiInstructionOptPass());
                        return true;
                    }
                    if (Name == "all-opts") {
                        FPM.addPass(AlgebraicIdentityPass());
                        FPM.addPass(StrengthReductionPass());
                        FPM.addPass(MultiInstructionOptPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}
