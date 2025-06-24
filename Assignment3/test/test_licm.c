// test/test_licm.c
int licm_test(int a, int b, int n) {
    int result = 0;
    // 'c' è loop-invariant. Il suo calcolo può essere spostato fuori.
    int c = a * b; 

    for (int i = 0; i < n; i++) {
        // Questa istruzione usa 'c', ma 'c' non cambia mai dentro il loop.
        result = result + c; 
    }

    return result;
}