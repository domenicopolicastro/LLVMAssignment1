
#include "llvm/IR/PassManager.h"      
#include "llvm/IR/Function.h"          
#include "llvm/IR/BasicBlock.h"        
#include "llvm/Analysis/LoopInfo.h"    
#include "llvm/Support/raw_ostream.h"  

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

struct LoopPass : public PassInfoMixin<LoopPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

        LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

        errs() << "Eseguo LoopPass sulla funzione: '" << F.getName() << "'\n";

        if (LI.empty()) {
            errs() << "  Nessun loop trovato in questa funzione.\n\n";
            
            return PreservedAnalyses::all();
        }

        errs() << "--- Controllo degli Header dei Loop ---\n";
        for (auto &BB : F) {
            if (LI.isLoopHeader(&BB)) {
                errs() << "Il Basic Block '" << BB.getName() << "' è un header di loop.\n";
            }
        }

        errs() << "\n--- Analisi Dettagliata dei Loop ---\n";
        int loop_counter = 1;
        for (Loop *L : LI) {
            errs() << "Loop #" << loop_counter++ << ":\n";

            if (L->isLoopSimplifyForm()) {
                errs() << "  - Il loop è in forma 'simplify'.\n";
            } else {
                errs() << "  - Il loop NON è in forma 'simplify'.\n";
            }

            BasicBlock *Header = L->getHeader();
            Function *ParentFunc = Header->getParent(); 
            errs() << "  - L'header è '" << Header->getName() << "'. La funzione genitore è '" << ParentFunc->getName() << "'.\n";

            errs() << "  - Blocchi che compongono questo loop:\n";
            for (BasicBlock *Block : L->getBlocks()) {
                if (Block->hasName()) {
                    errs() << "    -> " << Block->getName() << "\n";
                } else {
                    errs() << "    -> (blocco senza nome) " << Block << "\n";
                }
            }
            errs() << "\n";
        }
        
        return PreservedAnalyses::all();
    }
};


extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "LoopPassPlugin", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "loop-pass") {
                        FPM.addPass(LoopPass());
                        return true;
                    }

                    return false;
                }
            );
        }
    };
}