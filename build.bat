@echo off
@setlocal enabledelayedexpansion

@REM ########
@REM  Hello! cl from MSVC is used to compile the project.
@REM  g++ is used when debugging. You will unfortunately
@REM  have to try to make this project build by yourself
@REM  at this moment in time.
@REM ########

SET USE_GCC=1
SET USE_MSVC=1

SET GCC_INCLUDE_DIRS=-Iinclude
SET GCC_DEFINITIONS=-DWIN32
SET GCC_COMPILE_OPTIONS=-std=c++14 -g
SET GCC_WARN=-Wall -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable

SET MSVC_COMPILE_OPTIONS=/std:c++14 /nologo /TP /EHsc
SET MSVC_LINK_OPTIONS=/nologo
SET MSVC_INCLUDE_DIRS=/Iinclude
SET MSVC_DEFINITIONS=/DWIN32

@REM #############  Unity build
mkdir bin 2> nul
SET srcfile=bin\all.cpp
SET srcfiles=

type nul > !srcfile!
@REM echo #include ^"pch.h^" >> !srcfile!
for /r %%i in (*.cpp) do (
    SET file=%%i
    if "x!file:__=!"=="x!file!" if "x!file:bin=!"=="x!file!" (
        if not "x!file:BetBat=!"=="x!file!" (
            echo #include ^"!file:\=/!^">> !srcfile!
            @REM set srcfiles=!srcfiles! !file!
        ) else if not "x!file:Engone=!"=="x!file!" (
            @REM set srcfiles=!srcfiles! !file!
            echo #include ^"!file:\=/!^">> !srcfile!
        )
    )
)
@REM echo HOHO

@REM #####   Compiling
set /a startTime=6000*( 100%time:~3,2% %% 100 ) + 100* ( 100%time:~6,2% %% 100 ) + ( 100%time:~9,2% %% 100 )

if !USE_MSVC!==1 (
    if !USE_GCC!==1 (
        @REM Compiling this too when using GDB debugger in vscode
        start /b g++ !GCC_WARN! !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! !srcfile! -o bin/program_debug.exe
    )
    cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Z7 /Fobin/all.obj /link !MSVC_LINK_OPTIONS! shell32.lib /OUT:bin/program.exe
    @REM cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! /Ycpch.h src/pch.cpp
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Yupch.h /Fobin/all.obj /link !MSVC_LINK_OPTIONS! pch.obj shell32.lib /OUT:bin/program.exe
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfiles! /link !MSVC_LINK_OPTIONS! shell32.lib /OUT:bin/program.exe
) else (
    g++ !GCC_WARN! !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! !srcfile! -o bin/program.exe
)

set /a endTime=6000*(100%time:~3,2% %% 100 )+100*(100%time:~6,2% %% 100 )+(100%time:~9,2% %% 100 )
set /a finS=(endTime-startTime)/100
set /a finS2=(endTime-startTime)%%100

echo Compiled in %finS%.%finS2% seconds

if !errorlevel! == 0 (
    echo f | XCOPY /y /q bin\program.exe prog.exe > nul
    prog
)