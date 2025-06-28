#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h" 
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

//=======================================================================================
// 1. DEFINIZIONE DELLE STRUTTURE E FUNZIONI HELPER (COPIATE DAL TUO CODICE)
//=======================================================================================

struct fusionCandidate {
    const SCEV *tripCount;
    Loop *loop;
};

void updateAnalysisInfo(Function &F, DominatorTree &DT, PostDominatorTree &PDT) {
    // Ricalcolare il Dominator Tree e il Post Dominator Tree
    DT.recalculate(F);
    PDT.recalculate(F);
}

void fuseLoops(Loop *L1, Loop *L2, DominatorTree &DT, PostDominatorTree &PDT, LoopInfo &LI, Function &F, DependenceInfo &DI, ScalarEvolution &SE, FunctionAnalysisManager &AM) {
    //Replace the uses of the induction variable of the second loop with the induction variable of the first loop.
    PHINode *index1 = L1->getCanonicalInductionVariable();
    PHINode *index2 = L2->getCanonicalInductionVariable();

    //check if the induction variables were found
    if (!index1 || !index2) {
        outs() << "Induction variables not found\n";
        return;
    }

    //link L1's parent exit to L2's parent exit
    if (!L1->isOutermost() && !L2->isOutermost()) {
        Loop *L1Parent = L1->getOutermostLoop();
        Loop *L2Parent = L2->getOutermostLoop();
        if (L1Parent && L2Parent) {
            L1Parent->getHeader()->getTerminator()->replaceUsesOfWith(L1Parent->getExitBlock(), L2Parent->getExitBlock());
        }
    }

    //replace the uses of L2's induction variable with the uses of L1's
    index2->replaceAllUsesWith(index1);
    index2->eraseFromParent();

    //compute the exit blocks of L2
    SmallVector<BasicBlock *> L2_exit_blocks;
    L2->getExitBlocks(L2_exit_blocks);

    //get loop headers
    BasicBlock *L1_header = L1->getHeader();
    BasicBlock *L2_header = L2->getHeader();

    //get loop latches
    BasicBlock *L1_latch = L1->getLoopLatch();
    BasicBlock *L2_latch = L2->getLoopLatch();

    //get loops body blocks
    BasicBlock *L1_body_end = L1_latch->getUniquePredecessor();
    BasicBlock *L2_body_end = L2_latch->getUniquePredecessor();
    BasicBlock *L2_body_start = nullptr;
    for (auto sit1 = succ_begin(L2_header); sit1 != succ_end(L2_header); sit1++) {
        BasicBlock *header_successor = dyn_cast<BasicBlock>(*sit1);
        if (L2->contains(header_successor)) {
            L2_body_start = header_successor;
            break;
        }
    }
    //link L1's header to L2's exit
    for (BasicBlock *BB : L2_exit_blocks) {
        for (pred_iterator pit = pred_begin(BB); pit != pred_end(BB); pit++) {
            BasicBlock *predecessor = dyn_cast<BasicBlock>(*pit);
            if (predecessor == L2_header) {
                L1_header->getTerminator()->replaceUsesOfWith(L2->getLoopPreheader(), BB);
            }
        }
    }

    //link loop bodies
    if (L1_latch->getTerminator()->getNumSuccessors() == 1) {
        //guarded loops, the latch has 1 successor: br label %for.inc
        //Link L1 body to L2 body: br label %for.inc => br label %for.body4
        BranchInst *jump_to_L2_body = BranchInst::Create(L2_body_start);
        ReplaceInstWithInst(L1_body_end->getTerminator(), jump_to_L2_body);
    } else {
        //unguarded loops, the latch has 2 successors: br i1 %cmp, label %do.body, label %do.end9
        //update the phi node at start of L1: [ 0, %entry ], [ %inc, %do.cond ] => [ 0, %entry ], [ %inc, %do.cond7 ]
        Instruction &first = L1_header->front();
        BasicBlock *oldBB = L1_body_end->getTerminator()->getSuccessor(0);
        if (PHINode *phi = dyn_cast<PHINode>(&first)) {
            // Iterate over all incoming values in the phi node
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                if (phi->getIncomingBlock(i) == oldBB) {
                    phi->setIncomingBlock(i, L2_body_start);
                    oldBB->getTerminator()->eraseFromParent();
                    break;
                }
            }
        }
        //Link L1 body to L2 body: br label %do.cond => br label %do.body1
        BranchInst *jump_to_L2_body = BranchInst::Create(L2_body_start->getTerminator()->getSuccessor(0));
        ReplaceInstWithInst(L1_body_end->getTerminator(), jump_to_L2_body);
        //update branch instruction in order to jump to the body of L1
        //br i1 %cmp8, label %do.body1, label %do.end9 => br i1 %cmp8, label %do.body0, label %do.end9
        L2_body_start->getTerminator()->replaceUsesOfWith(L2_header, L1_header);
    }

    //link L2 body to L1 latch
    BranchInst *jump_to_L1_latch = BranchInst::Create(L1_latch);
    ReplaceInstWithInst(L2_body_end->getTerminator(), jump_to_L1_latch);

    //link L2 header to L2 latch
    BranchInst *jump_to_L2_latch = BranchInst::Create(L2_latch);
    ReplaceInstWithInst(L2_header->getTerminator(), jump_to_L2_latch);


    //delete unreachable blocks
    EliminateUnreachableBlocks(F);


    //update analysis info after the change
    updateAnalysisInfo(F, DT, PDT);

    outs() << "Deleted unreachable blocks\n";
}

bool areLoopsAdjacent(Loop *L1, Loop *L2) {
    if (!L1 || !L2) {
        outs() << "Either L1 or L2 is a NULL pointer \n";
        return false;
    }

    if (L1->isGuarded() && L2->isGuarded()) {
        BranchInst *L1Guard = L1->getLoopGuardBranch();
        outs() << "L1 and L2 are guarded loops \n";
        for (unsigned i = 0; i < L1Guard->getNumSuccessors(); ++i) {
            if (!L1->contains(L1Guard->getSuccessor(i)) && L1Guard->getSuccessor(i) == L2->getHeader()) {
                outs() << "The non-loop successor of the guard branch of L1 corresponds to L2's entry block \n";
                return true;
            }
        }
    } else if (!L1->isGuarded() && !L2->isGuarded()) {
        outs() << "L1 and L2 are unguarded loops \n";
        BasicBlock *L1ExitingBlock = L1->getExitBlock();
        BasicBlock *L2Preheader = L2->getLoopPreheader();

        if (!L2Preheader) {
            outs() << "L2 has a NULL Preheader! \n";
            return false;
        }

        outs() << "L2Preheader: ";
        L2Preheader->print(outs());
        outs() << "\n";


        if (!L1ExitingBlock) {
            SmallVector<BasicBlock *> exits_blocks;
            L1->getExitBlocks(exits_blocks);
            for (BasicBlock *BB : exits_blocks) {
                if (BB == L2Preheader) {
                    return true;
                }
            }
            return false;
        }


        outs() << "L1ExitingBlock: ";
        L1ExitingBlock->print(outs());
        outs() << "\n";

        if (L1ExitingBlock && L2Preheader) {
            if (L1ExitingBlock == L2Preheader) {
                int instructionCount = 0;
                for (Instruction &I : *L1ExitingBlock) {
                    (void)I; // Suppress unused variable warning
                    ++instructionCount;
                    outs() << "Found instruction " << I << " inside the preheader \n";
                }
                if (instructionCount == 1) {
                    outs() << "The exit block of L1 corresponds to the preheader of L2 \n";
                    return true;
                }
                return false;
            }
        }


    } else {
        outs() << "One loop is guarded, the other one is not \n";
    }

    return false;
}

bool controlFlowEquivalent(Loop *L1, Loop *L2, DominatorTree &DT, PostDominatorTree &PDT) {
    return (DT.dominates(L1->getHeader(), L2->getHeader()) && PDT.dominates(L2->getHeader(), L1->getHeader()));
}

//returns a polynomial recurrence on the trip count of a load/store instruction
const SCEVAddRecExpr *getSCEVAddRec(Instruction *I, Loop *L, ScalarEvolution &SE) {
    SmallPtrSet<const SCEVPredicate *, 4> preds;
    //SCEV representation of the pointer operand of the load/store instruction inside the scope of the loop
    const SCEV *Instruction_SCEV = SE.getSCEVAtScope(getLoadStorePointerOperand(I), L);
    //convert the SCEV instruction to a polynomial recurrence on the trip count of the specified loop
    const SCEVAddRecExpr *rec = SE.convertSCEVToAddRecWithPredicates(Instruction_SCEV, L, preds);
    return rec;
}

bool isDistanceNegative(Loop *loop1, Loop *loop2, Instruction *inst1, Instruction *inst2, ScalarEvolution &SE) {
    outs() << "Checking if the access distance between " << *inst1 << " and " << *inst2 << " is negative\n";
    //get polynomial recurrences on the trip count for the dependend instructions
    const SCEVAddRecExpr *inst1_add_rec = getSCEVAddRec(inst1, loop1, SE); //es: {%a,+,4}<nw><%for.cond>
    const SCEVAddRecExpr *inst2_add_rec = getSCEVAddRec(inst2, loop2, SE);

    //check if both polynomial recurrences were found
    if (!(inst1_add_rec && inst2_add_rec)) {
        outs() << "Can't find a polynomial recurrence for inst!\n";
        return false;
    }

    outs() << "Polynomial recurrence of " << *inst1 << ": ";
    inst1_add_rec->print(outs());
    outs() << "\n";

    outs() << "Pointer base of ";
    inst1_add_rec->print(outs());
    outs() << ": ";
    SE.getPointerBase(inst1_add_rec)->print(outs());
    outs() << "\n";


    outs() << "Polynomial recurrence of " << *inst2 << ": ";
    inst2_add_rec->print(outs());
    outs() << "\n";

    outs() << "Pointer base of ";
    inst2_add_rec->print(outs());
    outs() << ": ";
    SE.getPointerBase(inst2_add_rec)->print(outs());
    outs() << "\n";

    //if the instructions don't share the same pointer base, then the dependence is not negative
    if (SE.getPointerBase(inst1_add_rec) != SE.getPointerBase(inst2_add_rec)) { //es: %a != %b
        outs() << "Different pointer base\n";
        return false;
    }

    //extract the start addresses of the polynomial recurrences
    //address value at the start of the loop.
    const SCEV *start_first_inst = inst1_add_rec->getStart(); //es: %a
    const SCEV *start_second_inst = inst2_add_rec->getStart();

    outs() << "Start index of ";
    inst1_add_rec->print(outs());
    outs() << ": ";
    start_first_inst->print(outs());
    outs() << "\n";

    outs() << "Start index of ";
    inst2_add_rec->print(outs());
    outs() << ": ";
    start_second_inst->print(outs());
    outs() << "\n";

    //extract the stride of the polynomial recurrences
    //change of the address at each loop iteration
    const SCEV *stride_first_inst = inst1_add_rec->getStepRecurrence(SE); //es: 4
    const SCEV *stride_second_inst = inst2_add_rec->getStepRecurrence(SE);

    outs() << "Stride index of ";
    inst1_add_rec->print(outs());
    outs() << ": ";
    stride_first_inst->print(outs());
    outs() << "\n";

    outs() << "Stride index of ";
    inst2_add_rec->print(outs());
    outs() << ": ";
    stride_second_inst->print(outs());
    outs() << "\n";

    //ensure the stride is non-zero and both strides are equal
    if (!SE.isKnownNonZero(stride_first_inst) || stride_first_inst != stride_second_inst) {
        outs() << "Cannot compute distance\n";
        return true;
    }

    //compute the distance (delta) between the start addresses
    const SCEV *inst_delta = SE.getMinusSCEV(start_first_inst, start_second_inst);

    outs() << "Delta: ";
    inst_delta->print(outs());
    outs() << "\n";

    //cast the delta and the stride to SCEVConstant
    const SCEVConstant *const_delta = dyn_cast<SCEVConstant>(inst_delta);
    const SCEVConstant *const_stride = dyn_cast<SCEVConstant>(stride_first_inst);
    const SCEV *dependence_dist = nullptr;

    if (const_delta && const_stride) {
        //retrieve the integer value of the delta and the stride
        APInt int_stride = const_stride->getAPInt();
        APInt int_delta = const_delta->getAPInt();

        //check if |delta| % |stride| != 0
        if ((int_delta != 0 && int_delta.abs().urem(int_stride.abs()) != 0)) {
            //delta is not multiple of the stride
            outs() << "|delta|: ";
            int_delta.abs().print(outs(), false);
            outs() << " not multiple of |stride|: ";
            int_stride.abs().print(outs(), false);
            outs() << "\n";
            return false;
        }

        //create APInt with value 0 and the number of bits of the stride
        unsigned n_bits = int_stride.getBitWidth();
        APInt int_zero = APInt(n_bits, 0);

        //reverse the delta if the stride is negative
        if (int_stride.slt(int_zero)) {
            dependence_dist = SE.getNegativeSCEV(inst_delta);
        } else {
            dependence_dist = inst_delta;
        }

    } else {
        outs() << "Cannot compute distance\n";
        return true;
    }

    //check if the dependence distance is negative
    bool isDistanceNegative = SE.isKnownPredicate(ICmpInst::ICMP_SLT, dependence_dist, SE.getZero(stride_first_inst->getType()));
    if (isDistanceNegative) {
        outs() << *inst1 << " and " << *inst2 << " are dependent with a negative distance \n";
    } else {
        outs() << *inst1 << " and " << *inst2 << " are dependent with a NON-negative distance \n";
    }

    outs() << "\n";
    return isDistanceNegative;
}

//check if all the dependencies between the two loops are non-negative
bool dependencesAllowFusion(Loop *L0, Loop *L1, DominatorTree &DT, ScalarEvolution &SE, DependenceInfo &DI) {
    std::vector<Instruction *> L0MemReads;
    std::vector<Instruction *> L0MemWrites;

    std::vector<Instruction *> L1MemReads;
    std::vector<Instruction *> L1MemWrites;

    //collect load and store instructions of L0
    for (BasicBlock *BB : L0->blocks()) {
        for (Instruction &I : *BB) {
            if (I.mayWriteToMemory()) {
                L0MemWrites.push_back(&I);
            }

            if (I.mayReadFromMemory()) {
                L0MemReads.push_back(&I);
            }
        }
    }

    //collect load and store instructions of L1
    for (BasicBlock *BB : L1->blocks()) {
        for (Instruction &I : *BB) {
            if (I.mayWriteToMemory()) {
                L1MemWrites.push_back(&I);
            }

            if (I.mayReadFromMemory()) {
                L1MemReads.push_back(&I);
            }
        }
    }

    //check for any negative distance dependency between the store instructions of L0 and the load instructions of L1
    for (Instruction *WriteL0 : L0MemWrites) {
        for (Instruction *ReadL1 : L1MemReads) {
            if (DI.depends(WriteL0, ReadL1, true) && isDistanceNegative(L0, L1, WriteL0, ReadL1, SE)) {
                return false;
            }
        }
    }

    //check for any negative distance dependency between the store instructions of L1 and the load instructions of L0
    for (Instruction *WriteL1 : L1MemWrites) {
        for (Instruction *ReadL0 : L0MemReads) {
            if (DI.depends(WriteL1, ReadL0, true) && isDistanceNegative(L0, L1, ReadL0, WriteL1, SE)) {
                return false;
            }
        }
    }

    //if we reach this point, all the dependencies are non-negative
    return true;
}

bool tryFuseLoops(fusionCandidate *C1, fusionCandidate *C2, ScalarEvolution &SE, DominatorTree &DT, PostDominatorTree &PDT, DependenceInfo &DI, LoopInfo &LI, Function &F, FunctionAnalysisManager &AM) {
    Loop *L1 = C1->loop;
    Loop *L2 = C2->loop;

    if (!areLoopsAdjacent(L1->getOutermostLoop(), L2->getOutermostLoop())) {
        outs() << "Loops are not adjacent \n";
        return false;
    }

    outs() << "Loops are adjacent \n";

    // Get the trip counts using getExitCount
    if (!C1->tripCount) {
        const SCEV *tripCountL1 = SE.getExitCount(L1, L1->getExitingBlock(), ScalarEvolution::ExitCountKind::Exact);
        C1->tripCount = tripCountL1;
    }

    if (!C2->tripCount) {
        const SCEV *tripCountL2 = SE.getExitCount(L2, L2->getExitingBlock(), ScalarEvolution::ExitCountKind::Exact);
        C2->tripCount = tripCountL2;
    }


    // Print the trip counts
    outs() << "Trip count of L1: ";
    C1->tripCount->print(outs());
    outs() << "\n";

    outs() << "Trip count of L2: ";
    C2->tripCount->print(outs());
    outs() << "\n";

    // Check if both trip counts are equal
    if (C1->tripCount != C2->tripCount) {
        outs() << "Loops have a different trip count \n";
        return false;
    }

    outs() << "Loops have the same trip count \n";

    if (!controlFlowEquivalent(L1->getOutermostLoop(), L2->getOutermostLoop(), DT, PDT)) {
        outs() << "Loops are not control flow equivalent \n";
        return false;
    }
    outs() << "Loops are control flow equivalent \n";

    if (!dependencesAllowFusion(L1, L2, DT, SE, DI)) {
        outs() << "Loops are dependent \n";
        return false;
    }

    outs() << "Loops don't have any negative distance dependences \n";
    outs() << "All Loop Fusion conditions satisfied. \n";

    fuseLoops(L1, L2, DT, PDT, LI, F, DI, SE, AM);

    outs() << "The code has been transformed. \n";
    return true;
}

bool runOnFunction(Function &F, FunctionAnalysisManager &AM) {
    outs() << "Start \n";
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

    // Build up a worklist of inner-loops to version.
    SmallVector<Loop *, 8> Worklist;
    for (Loop *TopLevelLoop : LI)
        for (Loop *L : depth_first(TopLevelLoop))
            if (L->isInnermost())
                Worklist.push_back(L);

    // Convert the worklist of loops to fusion candidates.
    std::vector<fusionCandidate *> loops;
    for (Loop *L : Worklist) {
        fusionCandidate *f = new fusionCandidate;
        f->tripCount = nullptr;
        f->loop = L;
        loops.push_back(f);
    }

    outs() << "Found " << loops.size() << " loops! \n";

    if (loops.size() < 2) {
        return false;
    }
    
    bool changed = false;
    while (true) {
        bool fused = false;
        fusionCandidate *secondLoop = nullptr;
        // NOTA: Il loop originale `for (size_t i = loops.size() - 1; i > 0; --i)`
        // ha un bug con `size_t` se `loops.size()` è 0 o 1.
        // Lo correggo per evitare un loop infinito o underflow, mantenendo la logica.
        for (size_t i = loops.size() - 1; i > 0; --i) {
            // Tentativo di fusione di loop[i] e loop[i-1]
            if (tryFuseLoops(loops[i], loops[i - 1], SE, DT, PDT, DI, LI, F, AM)) {
                fused = true;
                changed = true;
                secondLoop = loops[i - 1]; // Il secondo loop (quello che precede) viene fuso nel primo e rimosso
                break;
            }
        }

        if (fused && secondLoop) {
            // Rimuovi il loop fuso dalla worklist
            auto it = std::remove(loops.begin(), loops.end(), secondLoop);
            loops.erase(it, loops.end());
            delete secondLoop; // Libera la memoria
        } else {
            // Se non ci sono state fusioni in questa iterazione, esci.
            break;
        }
    }
    
    // Pulisci la memoria rimanente
    for(auto* candidate : loops) {
        delete candidate;
    }
    
    return changed; // Restituisce se sono state apportate modifiche
}

//=======================================================================================
// 2. STRUTTURA DEL PASS PER IL NUOVO PASS MANAGER
//=======================================================================================
struct LoopFusionPass : public PassInfoMixin<LoopFusionPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        bool changed = runOnFunction(F, AM);
        
        // Se `runOnFunction` ha modificato l'IR, dobbiamo invalidare le analisi.
        // Altrimenti, possiamo preservarle tutte.
        if (changed) {
            return PreservedAnalyses::none();
        }
        return PreservedAnalyses::all();
    }
};


//=======================================================================================
// 3. REGISTRAZIONE DEL PLUGIN
//=======================================================================================
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