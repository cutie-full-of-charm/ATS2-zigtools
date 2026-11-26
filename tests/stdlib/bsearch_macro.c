#include <stdio.h>
#include <stdlib.h>
#include "pats_ccomp_basics.h"
#include "pats_ccomp_typedefs.h"
#ifndef bsearch
#define bsearch(key, base, count, size, cmp) \
    _Generic((base), default: (bsearch))((key), (base), (count), (size), (cmp))
#endif
#include "prelude/CATS/array.cats"

static int compare_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(void) {
    int values[] = { 1, 3, 5, 7 };
    int key = 5;
    int *found = atspre_array_bsearch(&key, values, 4, sizeof(int), compare_int);
    if (found != &values[2]) return 1;
    key = 4;
    if (atspre_array_bsearch(&key, values, 4, sizeof(int), compare_int) != NULL) return 2;
    puts("bsearch macro ok");
    return 0;
}
