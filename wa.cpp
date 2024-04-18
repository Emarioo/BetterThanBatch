/*
    This file is purely for experimentation
    (comparing g++, MSVC with my x64 generator)
*/

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
int one(int a) {
    return 1 + a;
}
int two(int a, int b,int c,int d,int e,int f,int g,int h,int j, int k) {
    return 1 + a * b;
}
int main() {
    int a = 23,b=12,c=92,d=11;
    c += one(23);
    int aa = two(2,b+c+d,1,2,3,4,5,6,7,8);
    return aa;
}
