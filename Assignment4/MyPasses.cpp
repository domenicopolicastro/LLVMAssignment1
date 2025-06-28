#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

// --- FUNZIONE HELPER PER IL CONTROLLO DI ADIACENZA (VERSIONE FINALE) ---
bool areLoopsAdjacent(Loop *L1, Loop *L2) {
    if (!L1 || !L2 || L1 == L2) return false;

    // La condizione di adiacenza più stringente, come da slide e implementazione del collega:
    // l'UNICO blocco di uscita di L1 deve essere il PREHEADER di L2.
    BasicBlock *L1_Exit = L1->getExitBlock();      // Restituisce il blocco solo se l'uscita è unica.
    BasicBlock *L2_Preheader = L2->getLoopPreheader(); // Il blocco che precede l'header di L2.

    // Se L1 ha uscite multiple o L2 non ha un preheader, non sono candidati semplici.
    if (!L1_Exit || !L2_Preheader) {
        return false;
    }
    
    // Controlliamo se i due blocchi sono esattamente lo stesso blocco.
    if (L1_Exit == L2_Preheader) {
        // Un'ulteriore condizione di sicurezza: questo blocco "ponte"
        // non deve contenere istruzioni, a parte il suo terminatore (un 'br').
        // Se contiene solo un'istruzione, quella è il terminatore.
        if (L1_Exit->getTerminator() == &L1_Exit->front()) {
            return true;
        }
    }

    return false;
}


struct LoopFusionPass : public PassInfoMixin<LoopFusionPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "Eseguo LoopFusionPass sulla funzione: '" << F.getName() << "'\n";
        
        LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

        SmallVector<Loop*, 8> Loops;
        for (Loop *L : LI) {
            Loops.push_back(L);
        }

        if (Loops.size() < 2) {
            errs() << "  Trovati meno di 2 loop, nessuna fusione possibile.\n\n";
            return PreservedAnalyses::all();
        }

        for (size_t i = 0; i < Loops.size(); ++i) {
            for (size_t j = 0; j < Loops.size(); ++j) {
                if (i == j) continue;

                Loop *L1 = Loops[i];
                Loop *L2 = Loops[j];

                if (areLoopsAdjacent(L1, L2)) {
                    errs() << "  -> Trovati loop adiacenti: l'uscita di '" 
                           << L1->getHeader()->getName() << "' porta all'ingresso di '" 
                           << L2->getHeader()->getName() << "'.\n";
                }
            }
        }
        errs() << "\n";
        return PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "MyLoopFusionPlugin", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "loop-fusion-pass") {
                        FPM.addPass(LoopFusionPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}