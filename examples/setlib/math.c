/*
    gcc -c math.c -o math.o
    ar rcs math.lib math.o
*/
#include "stdio.h"

#define API __declspec(dllexport)

API int add(int x, int y) {
    int res = x+y;
    printf("ADD %d %d -> %d\n",x,y,res);
    return res;
}