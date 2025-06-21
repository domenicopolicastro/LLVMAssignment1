// test/test.c
int algebraic_identity_test(int x, int y) {
    int res1 = x + 0; // Pattern 1
    int res2 = 0 + res1; // Pattern 2
    int res3 = y * 1; // Pattern 3
    int res4 = 1 * res3; // Pattern 4
    
    return res4; // Alla fine, dovrebbe restituire 'x'
}
int strength_reduction_test(int x) {
    int y = x * 15; // Pattern per la moltiplicazione
    int z = y / 8;  // Pattern per la divisione
    return z;
}