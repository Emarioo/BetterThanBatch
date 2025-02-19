/*
    gcc examples/callbacks/util.c -g -shared -o examples/callbacks/util.dll
*/

// typedef float(*FnInt)(float,float,float,float,float);
typedef int(*FnInt)(int,int,int,int,int);
// typedef int(*FnInt)(int,int,int,int);
// typedef float(*FnInt)(long long int,long long int,long long int,long long int,long long int);
FnInt g_func;

#include "stdio.h"

void set_callback(void* f) {
    g_func = f;
    printf("util.dll: Callback was set\n");
}

// float call_func(float n,float a,float b,float c, float d) {
//     printf("util.dll: enter call_func\n");
//     float res = g_func(n,a,b,c,d);
//     printf("util.dll: exit call_func with %f\n", res);
//     return res;
// }
// int call_func(long long int n, long long int a, long long int b, long long int c, long long int d) {
//     printf("util.dll: enter call_func\n");
//     long long int res = g_func(n,a,b,c,d);
//     printf("util.dll: exit call_func with %lld\n", res);
//     return res;
// }
int call_func(int a, int b, int c, int d, int e) {
    printf("util.dll: enter call_func(%d,%d,%d,%d,%d)\n",a,b,c,d,e);
    int res = g_func(a,b,c,d,e);
    printf("util.dll: exit call_func with %d\n", res);
    return res;
}