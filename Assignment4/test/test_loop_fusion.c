// test/test_fusion.c
void loop_fusion_test(int *a, int *b, int n) {
  // Loop 1
  for (int i = 0; i < n; i++) {
    a[i] = i * 2;
  }

  // Nessuna istruzione in mezzo

  // Loop 2
  for (int i = 0; i < n; i++) {
    b[i] = a[i] + 5;
  }
}

void non_adjacent_test(int *a, int *b, int n) {
  // Loop A
  for (int i = 0; i < n; i++) {
    a[i] = i;
  }

  // Istruzione in mezzo
  int x = 10;
  a[0] = x;

  // Loop B
  for (int i = 0; i < n; i++) {
    b[i] = a[i];
  }
}