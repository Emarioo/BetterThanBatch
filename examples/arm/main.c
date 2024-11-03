#include "xuartps.h"
#include "printf.h"

int assembly();

void main() {
    init_uart0_RxTx_115200_8N1();
    
    int val = assembly();
    
    printf("Value: %d\n",val);
}


#include "printf.c"
#include "xuartps.c"