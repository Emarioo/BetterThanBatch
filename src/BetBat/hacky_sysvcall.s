# Compile With:
#  as -c src/BetBat/hacky_sysvcall.s -o hacky.o

.intel_syntax noprefix

.text
.globl Makeshift_sysvcall
Makeshift_sysvcall:
    push rbx     # callee saved register
    mov rbx, rsp # save pointer for safe keeping
    
    mov rax, rdi # set function pointer
    mov rsp, rsi # set makeshift stack

    mov rdi, QWORD PTR [rsp]      # Set arguments even if we don't use all since
    mov rsi, QWORD PTR [rsp + 8]  # it is easier than conditional jumps and stuff
    mov rdx, QWORD PTR [rsp + 16]
    mov rcx, QWORD PTR [rsp + 24] # we always allocate 32 bytes so we won't read out of bounds
    
    # TODO: Handle floats

    call rax          # call function pointer
    mov [rsp-24], rax # put return on stack where bytecode expects it

    mov rsp, rbx      # restore original stack
    pop rbx
    ret

