@echo off
@setlocal enabledelayedexpansion

@REM #############################
@REM  Run vcvars64.bat before running this script. Scripts needs cl, lib, and link (you can manually setup environment variables to Visual Studio tool chain).
@REM  Make sure you have MinGW for g++.
@REM #####################################

@REM python build.py clean
@REM goto END

if "%~1"=="run" (
    @REM Run compiler without compiling it
    @REM btb -dev

    goto RUN_COMPILER
)

SET RUN_AT_END=1

python build.py use_tracy=true

SET compileSuccess=!errorlevel!

if !compileSuccess! == 0 if !RUN_AT_END!==1 (
:RUN_COMPILER
    rem
    @REM bin\btb -dev
    bin\btb examples/dev

    @REM bin\btb examples/crawler/GameCore -d -o main.exe -r
    @REM bin\btb examples/crawler/GameCore -d -o bin/code.dll

    @REM if !errorlevel! == 0 (
    @REM     tasklist /fi "IMAGENAME eq main.exe" | find /I "main.exe"
    @REM     if not !errorlevel! == 0 (
    @REM         bin\btb examples/crawler/main -d -o main.exe -r
    @REM     )
    @REM )
    
    @REM bin\btb examples/dev -d -o test.exe
    @REM bin\btb --test
    
    @REM bin\btb examples/dev
    @REM if !errorlevel!==0 (
    @REM     start main server
    @REM     timeout 1
    @REM     start main client
    @REM )
    
)
:END