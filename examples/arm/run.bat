@echo off
@setlocal enabledelayedexpansion

@REM %cc% main.s

@REM set AS=aarch64-none-elf-as.exe
@REM set OBJDUMP=aarch64-none-elf-objdump.exe
@REM set LD=aarch64-none-elf-ld.exe
@REM set QEMU=qemu-system-aarch64
set AS=arm-none-eabi-as.exe
set GCC=arm-none-eabi-gcc.exe
set OBJDUMP=arm-none-eabi-objdump.exe
set LD=arm-none-eabi-ld.exe
set QEMU=qemu-system-arm

%GCC% -c -O0 main.c -o main.o
%AS% startup.s -o startup.o
@REM %AS% assembly.s -o assembly.o
@REM %OBJDUMP% -d main.o
@REM %LD% main.o startup.o assembly.o -T link.ld -o main.elf
%LD% main.o startup.o -T link.ld -o main.elf
@REM %OBJDUMP% -d main.elf

%QEMU% -semihosting -M xilinx-zynq-a9 -cpu cortex-a9 -nographic -serial mon:stdio -kernel main.elf
@REM  -s -S

@REM Ctrl+A (release) X - To exit qemu