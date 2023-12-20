.code

    mov ecx, 0
    mov eax, 0
loop:
    cmp ecx, 5
    je loop_end
    add ecx, 1
    add eax, ecx
    jmp hak
loop_end:
    push rax 
    
END
