#include <stdio.h>

// Dichiariamo una variabile volatile per assicurarci che il compilatore
// non ottimizzi via i nostri branch 'if'.
volatile int condition_a, condition_c, condition_f;

void a_func() { printf("Blocco A\n"); }
void b_func() { printf("Blocco B\n"); }
void c_func() { printf("Blocco C\n"); }
void d_func() { printf("Blocco D\n"); }
void e_func() { printf("Blocco E\n"); }
void f_func() { printf("Blocco F\n"); }
void g_func() { printf("Blocco G\n"); }

void dominator_tree_test() {
A:
    a_func();
    if (condition_a) goto B;
    else goto C;

B:
    b_func();
    goto G;

C:
    c_func();
    if (condition_c) goto D;
    else goto E;

D:
    d_func();
    goto F;

E:
    e_func();
    goto F;

F:
    f_func();
    if (condition_f) goto G;
    else goto F; // Un piccolo loop per rendere il CFG più interessante

G:
    g_func();
    return;
}