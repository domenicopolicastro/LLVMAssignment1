#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"

#include <functional>
#include <vector>
#include <unordered_set>

using namespace llvm;

//Caso speciale:
//Ignora la condizione DOMINA TUTTI I BLOCCHI DEL LOOP
//SE E SOLO SE NON VIENE USATA ALL'USCITA

bool isDeadAtExit(Loop* L, Instruction* inst) {
    // Trova i blocchi di uscita del loop
    SmallVector<BasicBlock*, 4> exitBlocks;
    L->getExitBlocks(exitBlocks);

    // Usa un set per tracciare i blocchi visitati
    SmallPtrSet<BasicBlock*, 16> visited;
    SmallVector<BasicBlock*, 16> worklist(exitBlocks.begin(), exitBlocks.end());

    // Effettua una ricerca in ampiezza (BFS) a partire dai blocchi di uscita
    while (!worklist.empty()) {
        BasicBlock *BB = worklist.pop_back_val();
        if (!visited.insert(BB).second)
            continue; // Già visitato

        for (Instruction &I : *BB) {
            for (Use &U : I.operands()) {
                if (U.get() == inst) {
                    return false; // Trovato uso di inst fuori dal loop
                }
            }
        }

        // Aggiungi i successori alla worklist
        for (BasicBlock *Succ : successors(BB)) {
            worklist.push_back(Succ);
        }
    }

    return true;
}

bool hasUniqueDefinitionInLoop(Loop* L, Instruction* inst) {
    // NOTA: Questa logica basata su `getName()` è fragile 
    // Mantenuta solo a livello didattico, con mem2reg -> SSA assicurata, è ridondante
    if (inst->getName().empty()) {
        // Se non ha nome, non possiamo usare questa logica. Assumiamo che sia unico
        // per non essere troppo restrittivi, o potremmo restituire false.
        return true;
    }

    for (auto *BB : L->getBlocks()) {
        for (auto &I : *BB) {
            // Controlla se un'ALTRA istruzione nello stesso loop ha lo STESSO nome.
            if (&I != inst && !I.getName().empty() && I.getName() == inst->getName()) {
                return false;
            }
        }
    }
    return true;
}

bool dominatesAllUsesInLoop(DominatorTree &DT, Loop* L, Instruction* inst) {
    //Prendo il blocco della mia istruzione candidata (loop semplice unico blocco?)
    BasicBlock* instBB = inst->getParent();
    //Scorri tutti i punti dove la mia istruzione è usata come RHS
    for (Use &U : inst->uses()) {
        //L'utilizzatore è una istruzione? Scrive su un registro?
        Instruction* userInst = dyn_cast<Instruction>(U.getUser());
        //Questa istruzione che usa la mia candidata è nel loop stesso?
        if (userInst && L->contains(userInst)) {
            //La definizione domina l'uso?
            if (!DT.dominates(instBB, userInst->getParent())) {
                return false;
            }
        }
    }
    return true;
}

bool isInvariant(Loop* L, std::vector<Instruction*>& invStmts, Instruction* inst) {
    outs() << "Checking if the instruction: ";
    inst->print(outs());
    outs() << " is Loop Invariant\n";
    
    bool all_operands_defined_outside = true;
    bool all_operands_loop_invariant = true;
    bool is_constant = false;

    // Questa condizione è troppo restrittiva, la modifichiamo leggermente per includere
    // istruzioni come 'load' che possono essere invarianti.
    if (inst->isTerminator() || isa<PHINode>(inst) || inst->mayHaveSideEffects()) {
         outs() << "The instruction is not a candidate (terminator, phi, or has side-effects)\n";
         return false;
    }
    
    outs() << "The instruction is a candidate\n";
    if (isa<Constant>(inst)) {
        outs() << "The instruction is a constant \n";
        is_constant = true;
    } else {
        outs() << "The instruction is NOT a constant \n";
        
        // Scorro gli operandi dell'istruzione
        for (User::op_iterator OI = inst->op_begin(), OE = inst->op_end(); OI != OE; ++OI) {
            Value *op = *OI;
            outs() << "Checking operand: ";
            op->print(outs());
            outs() << ":\n";

            // Se l'operando non è una istruzione (es. una costante), è per definizione invariante
            Instruction *opInst = dyn_cast<Instruction>(op);
            if (!opInst) {
                outs() << "The operand is not an instruction (e.g., a constant), it is invariant.\n";
                continue;
            }

            // Se la reaching definition dell'operando si trova all'interno del loop
            if (L->contains(opInst)) {
                outs() << "The operand is defined inside the loop ";
                all_operands_defined_outside = false; // Non tutti gli operandi sono definiti esternamente

                // Verifica se l'istruzione che definisce l'operando è invariante nel loop
                bool isOpInvariant = false;
                for (Instruction *invInst : invStmts) {
                    if (invInst == opInst) {
                        isOpInvariant = true;
                        break;
                    }
                }
                
                if (!isOpInvariant) {
                    outs() << "and it is NOT (yet) known to be loop invariant \n";
                    all_operands_loop_invariant = false;
                } else {
                    outs() << "and it is loop invariant \n";
                }
            } else {
                outs() << "The operand is defined outside the loop \n";
            }

            if (!all_operands_loop_invariant) {
                outs() << "Found a variant operand, so this instruction cannot be invariant.\n";
                break;
            }
        }
    }

    outs() << "The instruction ";
    inst->print(outs());
    if (is_constant || (all_operands_defined_outside || all_operands_loop_invariant)) {
        outs() << " is Loop Invariant\n\n";
        return true;
    }   
    
    outs() << " is NOT Loop Invariant\n\n";
    return false;
}


bool runOnLoop(Loop *L, LoopInfo &LI, DominatorTree &DT) {
    bool modified = false;

    if (!L->isLoopSimplifyForm()) {
        outs() << "Loop is not in simplified form\n";
        return modified;
    }

    BasicBlock* preheader = L->getLoopPreheader();
    if (!preheader) {
        outs() << "Preheader not found\n";
        return modified;
    }
    outs() << "Preheader found\n";

    std::vector<Instruction*> invStmts;
    std::unordered_set<Instruction*> movedStmts;
    bool changedInIteration = true;

    // Approccio iterativo: continua a cercare istruzioni invarianti finché non ne trovi più.
    // Questo gestisce correttamente le catene di dipendenze (es. y = x+1, z = y+2).
    while (changedInIteration) {
        changedInIteration = false;
        for (BasicBlock* BB : L->getBlocks()) {
            for (Instruction &I : *BB) {
                // Controlla se l'istruzione è già stata trovata come invariante
                bool alreadyFound = false;
                for (Instruction* invI : invStmts) {
                    if (&I == invI) {
                        alreadyFound = true;
                        break;
                    }
                }
                if (alreadyFound) continue;

                if (isInvariant(L, invStmts, &I)) {
                    invStmts.push_back(&I);
                    changedInIteration = true;
                }
            }
        }
    }

    outs() << "Found Loop Invariant instructions:\n\n";
    for (size_t j = 0; j < invStmts.size(); ++j) {
        Instruction* inst = invStmts[j];
        outs() << j+1 <<") ";
        inst->print(outs());
        outs() <<"\n\n";
    }

    SmallVector<BasicBlock*, 4> exitBlocks;
    L->getExitBlocks(exitBlocks);
    int n_moved = 0;

    // Ora prova a muovere le istruzioni trovate
    for (Instruction* inst : invStmts) {
        outs() << "Performing code motion check for the loop invariant instruction ";
        inst->print(outs());
        outs() << "\n";
        
        // Condizione 1: L'istruzione domina tutti i suoi usi nel loop
        if (!dominatesAllUsesInLoop(DT, L, inst)) {
             outs() <<"The instruction doesn't dominate all of its uses inside the loop\n\n";
             continue;
        }
        outs() <<"The instruction dominates all of its uses inside the loop\n";

        // Condizione 2: Non ci sono altre definizioni della stessa "variabile" nel loop
        // Caso Didattico, non serve in SSA
        if (!hasUniqueDefinitionInLoop(L, inst)) {
            outs() <<"Multiple definitions of the same variable found inside the loop\n\n";
            continue;
        }
        outs() <<"The variable is defined once inside the loop\n";

        // Condizione 3: L'istruzione domina tutte le uscite del loop O è morta all'uscita
        bool dominatesExits = true;
        for (BasicBlock* exitBB : exitBlocks) {
            if (!DT.dominates(inst->getParent(), exitBB)) {
                dominatesExits = false;
                break;
            }
        }

        if (dominatesExits) {
            outs() <<"The instruction dominates all loop exit blocks\n";
        } else {
             outs() <<"The instruction does NOT dominate all loop exit blocks\n";
             if (!isDeadAtExit(L, inst)) {
                 outs() <<" and the instruction is NOT dead at the exit of the loop\n\n";
                 continue; // Non può essere spostata
             }
             outs() <<" but the instruction is dead at the exits of the loop\n";
        }

        // Se tutte le condizioni sono soddisfatte, sposta l'istruzione
        outs() <<"The instruction is a valid candidate for code motion. \n";
        inst->moveBefore(preheader->getTerminator());
        modified = true;
        outs() <<"The instruction has been moved inside the preheader. \n\n";
        n_moved++;
    }

    outs() << "Moved "<< n_moved <<" instruction(s) inside the preheader \n";
    return modified;
}


struct CustomLICMPass : public PassInfoMixin<CustomLICMPass> {
    PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU) {
        outs() << "Custom LICM Pass running on Loop: " << L.getHeader()->getName() << "\n";
        
        if (runOnLoop(&L, LAR.LI, LAR.DT)) {
            return PreservedAnalyses::none();
        }
        
        return PreservedAnalyses::all();
    }
};


extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "CustomLICMPass", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, LoopPassManager &LPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "custom-licm") {
                        LPM.addPass(CustomLICMPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}