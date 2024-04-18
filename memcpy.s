; Compile With:
;  ml64 memcpy.s /link /entry:main
;  ml64 memcpy.s /link /entry:main && memcpy && echo %errorlevel%

.code
main proc
    sub rsp, 4
    mov rsi, rsp
    mov DWORD PTR [rsi], 00305002h
    ; mov DWORD PTR [rsi], 00005002h
    ; mov DWORD PTR [rsi], 00000002h

    mov rcx, 0
    
    cmp rsi, 0
    je loop_end

    loop_:
    mov al, BYTE PTR[rsi]
    cmp al, 0
    je loop_end
    inc ecx
    inc rsi
    jmp loop_
    loop_end:
    
    mov eax, ecx
    add rsp, 4
    ret
main endp
END