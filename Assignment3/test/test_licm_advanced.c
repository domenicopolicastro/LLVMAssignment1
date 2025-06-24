#include <stdio.h>

// Variabili globali e volatili per rendere i branch non prevedibili dal compilatore
volatile int condition_a = 1;
volatile int condition_b = 0;

// --- CASO 1: Invariante Semplice e Sicuro ---
// L'istruzione `inv = a * b` è invariante e domina l'unica uscita del loop.
// DEVE essere spostata.
int test_case_1_simple_invariant(int a, int b, int n) {
    int result = 0;
    int inv = a * b; // Invariante

    for (int i = 0; i < n; i++) {
        result += inv;
    }
    return result;
}


// --- CASO 2: Istruzione Variante ---
// L'istruzione `var = a * i` dipende dal contatore del loop 'i'. NON è invariante.
// NON deve essere spostata.
int test_case_2_variant_instruction(int a, int n) {
    int result = 0;
    
    for (int i = 0; i < n; i++) {
        int var = a * i; // Variante!
        result += var;
    }
    return result;
}


// --- CASO 3: Fallimento della Dominanza ---
// `inv = a * b` è invariante, ma si trova in un ramo di un 'if'.
// Il suo risultato, `inv`, è usato FUORI dal loop.
// Poiché il blocco 'if' non domina l'uscita del loop (potremmo passare dal ramo 'else'),
// spostarla sarebbe SBAGLIATO. NON deve essere spostata.
int test_case_3_dominance_fail(int a, int b, int n) {
    int inv = 1; // Valore di default
    for (int i = 0; i < n; i++) {
        if (condition_a) {
            inv = a * b; // Invariante, ma non domina l'uscita
        } else {
            // L'altro percorso
        }
    }
    // `inv` è "vivo" qui, quindi non può essere spostato
    return inv; 
}


// --- CASO 4: Successo con "Dead at Exit" ---
// Simile al caso 3, `inv = a * b` è in un 'if' e non domina l'uscita.
// PERÒ, il suo risultato è usato SOLO all'interno del loop per calcolare `sum`.
// La variabile `inv` è "morta" fuori dal loop.
// Questa è la condizione perfetta per testare il tuo `isDeadOutsideLoop`.
// DEVE essere spostata.
int test_case_4_dead_at_exit(int a, int b, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        if (condition_a) {
            int inv = a * b; // Non domina l'uscita, ma è morta fuori dal loop
            sum += inv;
        } else {
            sum++;
        }
    }
    return sum;
}


// --- CASO 5: Non Sicura da Spostare (Rischio Crash) ---
// L'istruzione `inv_div = a / b` è invariante.
// Ma si trova in un 'if' che la protegge dal caso b=0.
// Se la spostassimo nel preheader, il programma crasherebbe se b=0.
// `isSafeToSpeculativelyExecute` dovrebbe rilevarlo. NON deve essere spostata.
int test_case_5_unsafe_to_move(int a, int b, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        if (b != 0) { // Protezione dalla divisione per zero
            int inv_div = a / b;
            sum += inv_div;
        }
    }
    return sum;
}