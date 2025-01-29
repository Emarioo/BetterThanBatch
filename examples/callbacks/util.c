/*
    gcc examples/callbacks/util.c -g -shared -o examples/callbacks/util.dll
*/

typedef int(*FnInt)(int);
FnInt g_func;

#include "stdio.h"

void set_callback(void* f) {
    g_func = f;
    printf("util.dll: Callback was set\n");
}

int call_func(int n) {
    printf("util.dll: enter call_func\n");
    int res = g_func(n);
    printf("util.dll: exit call_func with %d\n", res);
    return res;
}