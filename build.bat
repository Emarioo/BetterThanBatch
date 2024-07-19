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

python build.py

SET compileSuccess=!errorlevel!

if !compileSuccess! == 0 if !RUN_AT_END!==1 (
    echo f | XCOPY /y /q bin\btb.exe btb.exe > nul
:RUN_COMPILER
    rem

    @REM bin\btb examples/dev -d -o test.exe
    @REM bin\btb -dev
    @REM bin\btb --test
    
    bin\btb examples/dev

    if !errorlevel!==0 (
        start main server
        timeout 1
        start main client
    )

    @REM bin\btb examples/graphics/game
    
    @REM if !errorlevel!==0 (
    @REM     start test
    @REM     timeout 1
    @REM     start test client
    @REM )
    
)
:END