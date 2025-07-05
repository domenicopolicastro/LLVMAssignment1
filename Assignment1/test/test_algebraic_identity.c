#include <stdio.h>

// Funzione per testare le identità algebriche
int test_algebraic(int x, int y) {
    int res = 0;

    // Casi da ottimizzare
    res += (x + 0); // x + 0 -> x
    res += (0 + y); // 0 + y -> y
    res -= (x - 0); // x - 0 -> x
    
    res *= (y * 1); // y * 1 -> y
    res *= (1 * x); // 1 * x -> x

    res /= (x / 1); // x / 1 -> x

    // Casi di controllo (non devono essere ottimizzati)
    res += (x + 1);
    res -= (y - 2);
    res *= (x * 0); // Questo potrebbe essere ottimizzato da altri pass, ma non da AlgebraicIdentity
    res /= (y / 2);

    return res;
}

int main() {
    int result = test_algebraic(10, 20);
    printf("Risultato: %d\n", result);
    return 0;
}
