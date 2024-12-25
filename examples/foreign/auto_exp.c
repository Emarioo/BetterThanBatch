/*
    gcc auto_exp.c -o auto_exp.exe math.lib
*/

#include "math_decl.h"
#include "stdio.h"

int main() {
    Data d = {0};
    func(&d);
    printf("%s %d\n", d.str.ptr, d.x);
    return 0;
}