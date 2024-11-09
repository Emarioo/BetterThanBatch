// #include "xuartps.h"
// #include "printf.h"

// int assembly();

int glob = 2415;

void main() {
    // init_uart0_RxTx_115200_8N1();
    
    // int val = assembly();
    
    int c = glob;
    int* a = &glob;
    int* b = *a;
    // printf("Value: %d\n",val+glob);
}


// #include "printf.c"
// #include "xuartps.c"