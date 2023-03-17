#include <stdio.h>

#include "BetBat/Utility.h"
#include "BetBat/Tokenizer.h"
#include "BetBat/Generator.h"
#include "BetBat/Context.h"

int main(int argc, char* argv){
    engone::Memory sample = ReadFile("tests/token/words.txt");
    
    Tokens tokens = Tokenize(sample);
    Bytecode bytecode = Generate(tokens);
    
    Context context{};
    
    context.load(bytecode);
    context.execute();
    
    // tokens.printTokens();
    // tokens.printData();
}