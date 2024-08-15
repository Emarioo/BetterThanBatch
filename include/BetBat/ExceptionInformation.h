#pragma once

struct TryBlock {
    // all addresses are relative to the beginning of the function
    int bc_start;
    int bc_end;
    int asm_start;
    int asm_end;
    
    int bc_catch_start;
    int asm_catch_start;
};