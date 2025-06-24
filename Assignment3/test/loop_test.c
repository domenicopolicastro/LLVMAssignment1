// Una funzione con un loop semplice
int function_with_loop(int max) {
  int sum = 0;
  for (int i = 0; i < max; ++i) {
    sum += i;
  }
  return sum;
}

// Una funzione senza loop
int function_without_loop(int a, int b) {
  if (a > b) {
    return a - b;
  }
  return b - a;
}