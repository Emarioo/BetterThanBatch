// #include "xuartps.h"
// #include "printf.h"

// int assembly();

#include "hi.c"

int global = 123;

int num() {
    static int local_global = 123;
    global += 23;
    local_global += global;
    return global;
}
void main() {
    // init_uart0_RxTx_115200_8N1();
    
    // int val = assembly();
    
    int k = 23 + extra_glob;
    int b = ~k;

    // int(*f)() = num;
    // int c = f();
    // int c = num();
    // printf("Value: %d\n",val+glob);
}
 


// #include "printf.c"
// #include "xuartps.c"