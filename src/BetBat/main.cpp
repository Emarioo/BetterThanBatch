#include <stdio.h>

#include "BetBat/Utility.h"
#include "BetBat/Tokenizer.h"

int main(int argc, char* argv){
    engone::Memory sample = ReadFile("tests/token/words.txt");
    
    Tokens tokens = Tokenize(sample);
    
    tokens.printTokens();
    // tokens.printData();
}