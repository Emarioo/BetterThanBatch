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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vulnerableFunction(char* input) {
    char buffer[10];
    int magicNumber = 42;
    int userInputNum = atoi(input);
    strcpy(buffer, input);
    printf("Your input: %s\n", buffer);
    int result = magicNumber + userInputNum;
    printf("Your result added to magic: %d\n", result);
    if(!result)
    printf("Gotcha!\n");
}
int main() {
    char userInput[20];
    printf("Enter your input: ");
    // strcpy(userInput,"1234123412..1234   ");
    // strcpy(userInput,"-2105376__..1234   ");
    strcpy(userInput,"65_              A");
    // strcpy(userInput,"65_               A");
    // strcpy(userInput,"1090519040_");
    // strcpy(userInput,"0__..12 ");
    // fgets(userInput, sizeof(userInput), stdin);
    vulnerableFunction(userInput);
    return 0;
}
