#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/ValueTracking.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <vector>
#include <algorithm>

using namespace llvm;

void printDomTree(const DomTreeNode *Node, unsigned level) {
    errs().indent(level * 2) << "[" << Node->getBlock()->getName() << "]\n";
    for (DomTreeNode *ChildNode : *Node) {
        printDomTree(ChildNode, level + 1);
    }
}

bool isDeadOutsideLoop(Loop *L, Instruction *Inst) {
    for (User *U : Inst->users()) {
        Instruction *UseInst = cast<Instruction>(U);
        if (!L->contains(UseInst)) {
            return false;
        }
    }
    return true;
}

bool dominatesAllUsesInLoop(DominatorTree &DT, Loop *L, Instruction* Inst) {
    for (User *U : Inst->users()) {
        Instruction *UseInst = cast<Instruction>(U);
        if (L->contains(UseInst)) {
            if (!DT.dominates(Inst, UseInst)) {
                return false;
            }
        }
    }
    return true;
}

struct DominatorTreePass : public PassInfoMixin<DominatorTreePass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "--- Dominator Tree per la funzione '" << F.getName() << "' ---\n";
        DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
        DomTreeNode *RootNode = DT.getRootNode();
        if (!RootNode) {
            errs() << "Nessun nodo radice trovato.\n";
            return PreservedAnalyses::all();
        }
        printDomTree(RootNode, 0);
        errs() << "\n";
        return PreservedAnalyses::all();
    }
};

struct LoopPass : public PassInfoMixin<LoopPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
        errs() << "Eseguo LoopPass sulla funzione: '" << F.getName() << "'\n";
        if (LI.empty()) {
            errs() << "  Nessun loop trovato in questa funzione.\n\n";
            return PreservedAnalyses::all();
        }
        for (Loop *L : LI) {
            errs() << "Trovato loop con header '" << L->getHeader()->getName() << "'\n";
        }
        errs() << "\n";
        return PreservedAnalyses::all();
    }
};

struct LICMPass : public PassInfoMixin<LICMPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
        DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
        bool Transformed = false;

        for (Loop *L : LI) {
            BasicBlock *Preheader = L->getLoopPreheader();
            if (!Preheader) continue;

            std::vector<Instruction*> Candidates;
            for (BasicBlock *BB : L->getBlocks()) {
                for (Instruction &I : *BB) {
                    bool isInvariant = L->isLoopInvariant(&I);
                    bool isSafeToMove = isSafeToSpeculativelyExecute(&I) && !I.mayReadFromMemory() && !I.isTerminator();

                    if (!isInvariant || !isSafeToMove) continue;

                    bool dominatesExits = true;
                    SmallVector<BasicBlock*, 8> ExitBlocks;
                    L->getExitBlocks(ExitBlocks);
                    for (BasicBlock *ExitBB : ExitBlocks) {
                        if (!DT.dominates(I.getParent(), ExitBB)) {
                            dominatesExits = false;
                            break;
                        }
                    }

                    if ((dominatesExits || isDeadOutsideLoop(L, &I)) && dominatesAllUsesInLoop(DT, L, &I)) {
                        Candidates.push_back(&I);
                    }
                }
            }

            bool movedInThisIteration;
            do {
                movedInThisIteration = false;
                for (auto it = Candidates.begin(); it != Candidates.end(); ) {
                    Instruction *InstToMove = *it;
                    bool canMove = true;
                    for (Value *Op : InstToMove->operands()) {
                        if (auto *OpInst = dyn_cast<Instruction>(Op)) {
                            if (L->contains(OpInst) && std::find(Candidates.begin(), Candidates.end(), OpInst) != Candidates.end()) {
                                canMove = false;
                                break;
                            }
                        }
                    }

                    if (canMove) {
                        InstToMove->moveBefore(Preheader->getTerminator());
                        errs() << "LICM: Spostata istruzione '" << InstToMove->getOpcodeName() << "' nel preheader.\n";
                        it = Candidates.erase(it);
                        movedInThisIteration = true;
                        Transformed = true;
                    } else {
                        ++it;
                    }
                }
            } while (movedInThisIteration);
        }

        return Transformed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};


extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "MyLLVMAssignmentPlugins", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    
                    if (Name == "loop-pass") {
                        FPM.addPass(LoopPass());
                        return true;
                    }
                    if (Name == "dominator-tree-pass") {
                        FPM.addPass(DominatorTreePass());
                        return true;
                    }
                    if (Name == "licm-pass") {
                        FPM.addPass(LICMPass());
                        return true;
                    }

                    return false;
                }
            );
        }
    };
}