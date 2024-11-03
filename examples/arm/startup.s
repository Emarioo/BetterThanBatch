.global _Reset
_Reset:
    LDR sp, =stack_top
    BL main
    
    # angel_SWIreason_ReportException
    mov r0, #0x18 
    
    # mov r1, #20026 # ADP_Stopped_ApplicationExit
    mov r1, #0x2
    lsl r1, r1, #16
    orr r1, r1, #0x26
    # interrupt
    svc 0x00123456
    
    # infinite loop if interrupt didn't work
    B .

# .global _Reset64
# _Reset64:
#     ldr x30, =stack_top
#     mov sp, x30
