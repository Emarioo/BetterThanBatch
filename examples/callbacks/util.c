/*
    gcc examples/callbacks/util.c -g -shared -fPIC -o examples/callbacks/util.dll
    gcc examples/callbacks/util.c -g -shared -fPIC -o examples/callbacks/util.so
*/

typedef float(*FnFloat9)(float,float,float,float ,float,float,float,float, float);
typedef float(*FnFloat4)(float,float,float,float);
// typedef float(*FnInt)(long long int,long long int,long long int,long long int,long long int);
typedef int(*FnInt4)(int,int,int,int);
typedef int(*FnInt5)(int,int,int,int,int);
FnInt4 g_func4;
FnInt5 g_func5;
FnFloat4 g_func4f;
FnFloat9 g_func9f;

#include "stdio.h"

void set_callback(void* f) {
    g_func4 = f;
    g_func5 = f;
    g_func9f = f;
    g_func4f = f;
    printf("util.dll: Callback was set\n");
}

float call_func4f(float a,float b,float c,float d) {
    printf("util.dll: enter call_func\n");
    float res = g_func4f(a,b,c,d);
    printf("util.dll: exit call_func with %f\n", res);
    return res;
}
float call_func9f(float a,float b,float c,float d, float e,float f, float g, float h, float i) {
    printf("util.dll: enter call_func\n");
    float res = g_func9f(a,b,c,d,e,f,g,h,i);
    printf("util.dll: exit call_func with %f\n", res);
    return res;
}
int call_func4(int a, int b, int c, int d) {
    printf("util.dll: enter call_func(%d,%d,%d,%d)\n",a,b,c,d);
    int res = g_func4(a,b,c,d);
    printf("util.dll: exit call_func with %d\n", res);
    return res;
}
int call_func5(int a, int b, int c, int d, int e) {
    printf("util.dll: enter call_func(%d,%d,%d,%d,%d)\n",a,b,c,d,e);
    int res = g_func5(a,b,c,d,e);
    printf("util.dll: exit call_func with %d\n", res);
    return res;
}