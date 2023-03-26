@echo off
@setlocal enabledelayedexpansion

@REM SET LIBRARIES=-lshell32

SET DEBUG=1

SET GCC_INCLUDE_DIRS=-Iinclude
SET GCC_DEFINITIONS=-DWIN32
SET GCC_COMPILE_OPTIONS=-std=c++14 -g
SET WARN=-Wall -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable

SET COMPILE_OPTIONS=/std:c++17 /nologo /TP /EHsc
SET LINK_OPTIONS=/DEBUG /nologo
SET INCLUDE_DIRS=/Iinclude
SET DEFINITIONS=/DWIN32

mkdir bin 2> nul

SET srcfile=bin\all.cpp

type nul > !srcfile!

for /r %%i in (*.cpp) do (
    SET file=%%i
    if "x!file:__=!"=="x!file!" if "x!file:bin=!"=="x!file!" (
        if not "x!file:BetBat=!"=="x!file!" (
            echo #include ^"!file:\=/!^">> !srcfile!
        ) else if not "x!file:Engone=!"=="x!file!" (
            echo #include ^"!file:\=/!^">> !srcfile!
        )
    )
)
set /a startTime=6000*( 100%time:~3,2% %% 100 ) + 100* ( 100%time:~6,2% %% 100 ) + ( 100%time:~9,2% %% 100 )

@REM if !DEBUG!==1 (
start /b g++ !WARN! !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! !srcfile! -o bin/program_debug.exe > nul
@REM ) else (
    cl !COMPILE_OPTIONS! !INCLUDE_DIRS! !DEFINITIONS! !srcfile! /Fobin/all.obj /link shell32.lib /OUT:bin/program.exe
@REM )
set /a endTime=6000*(100%time:~3,2% %% 100 )+100*(100%time:~6,2% %% 100 )+(100%time:~9,2% %% 100 )

set /a finS=(endTime-startTime)/100
set /a finS2=(endTime-startTime)%%100

echo Finished in %finS%.%finS2% seconds

if !errorlevel! == 0 (
    bin\program
)