#pragma once

struct TryBlock {
    // all addresses are relative to the beginning of the function
    int bc_start;
    int bc_end;
    int asm_start;
    int asm_end;
    
    int bc_catch_start;
    int asm_catch_start;

    int frame_offset_before_try; // a negative number, rbp + frame_offset
    int filter_exception_code;
    int offset_to_exception_info = 0; // offset to this variable: 'catch ex_info : EXCEPTION_ANY'
};