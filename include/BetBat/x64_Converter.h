#pragma once
#include "BetBat/Bytecode.h"

struct ConversionInfo {
    DynamicArray<u8> textSegment{}; // TODO: Bucket array


    void write(u8 byte);
};

void ConvertTox64(Bytecode* bytecode);