// #include "na.cpp"


// struct Cake {
//     int n;
//     Cake* b;
//     Cake** bbb;
// };

// void hey(int a) {
//     Cake na = {a, nullptr};
// }

// #include <unistd.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// void vulnerableFunction(char* input) {
//     char buffer[10];
//     int magicNumber = 42;
//     int userInputNum = atoi(input);
//     strcpy(buffer, input);
//     printf("Your input: %s\n", buffer);
//     int result = magicNumber + userInputNum;
//     printf("Your result added to magic: %d\n", result);
//     if(!result)
//     printf("Gotcha!\n");
// }
int main() {
    int glob=0;
    {
        int inner = 92;
        inner += glob;
        {
            int innenras = 923;
            inner += innenras;
        }
    }
    {
        int sibling = 222;
        sibling += 23;   
    }
    return 0;
}
