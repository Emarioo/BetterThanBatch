# Compile With:
#   as -c src/BetBat/hacky_stdcall.s -o hacky.o

# There are two assemblies for hacky_stdcall, one when compiling with ml64 (MSVC) and one with as (GCC GNU, MinGW)

.intel_syntax noprefix

.text
.globl Makeshift_stdcall
Makeshift_stdcall:
    push rbx
    mov rbx, rsp # save pointer for safe keeping
    
    mov rax, rcx # set function pointer
    mov rsp, rdx # set makeshift stack

    mov rcx, QWORD PTR [rsp]      # Set arguments even if we don't use all since
    mov rdx, QWORD PTR [rsp + 8]  # it is easier than conditional jumps and stuff
    mov r8,  QWORD PTR [rsp + 16]
    mov r9,  QWORD PTR [rsp + 24] # we always allocate 32 bytes so we won't read out of bounds

    # TODO: Handle 64-bit floats
    movss xmm0, [rsp]
    movss xmm1, [rsp + 8]
    movss xmm2, [rsp + 16]
    movss xmm3, [rsp + 24]

    sub rsp, 32
    call rax          # call function pointer
    add rsp, 32
    
    mov [rsp-24], rax # put return on stack where bytecode expects it
    
    mov rsp, rbx
    pop rbx
    ret
