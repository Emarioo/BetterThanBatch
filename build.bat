@echo off
@setlocal enabledelayedexpansion

set /a startTime=6000*( 100%time:~3,2% %% 100 ) + 100* ( 100%time:~6,2% %% 100 ) + ( 100%time:~9,2% %% 100 )

@REM SET INCLUDE_DIRS=/Iinclude
@REM SET DEFINITIONS=/DWIN32

@REM SET LIBRARIES=-lshell32

@REM SET COMPILE_OPTIONS=/DEBUG /std:c++14 /EHsc /TP /Z7 /MTd /nologo
@REM SET LINK_OPTIONS=/IGNORE:4006 /DEBUG /NOLOGO /MACHINE:X64

SET INCLUDE_DIRS=-Iinclude
SET DEFINITIONS=-DWIN32
SET COMPILE_OPTIONS=-g -std=c++17

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

@REM cl !COMPILE_OPTIONS! !INCLUDE_DIRS! !DEFINITIONS! !srcfile! /Fobin\all.obj /link !LINK_OPTIONS! !LIBRARIES! /OUT:bin/program.exe

g++ !COMPILE_OPTIONS! !INCLUDE_DIRS! !DEFINITIONS! !srcfile! -o bin/program.exe

set /a endTime=6000*(100%time:~3,2% %% 100 )+100*(100%time:~6,2% %% 100 )+(100%time:~9,2% %% 100 )

set /a finS=(endTime-startTime)/100
set /a finS2=(endTime-startTime)%%100

echo Finished in %finS%.%finS2% seconds

if !errorlevel! == 0 (
    bin\program tests/inst/apicalls.txt
)