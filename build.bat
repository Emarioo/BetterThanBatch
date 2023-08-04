@echo off
@setlocal enabledelayedexpansion

@REM ########
@REM  Hello!
@REM  run vcvars64.bat before running this script.
@REM  
@REM  g++ is used when debugging.
@REM ########

@REM SET USE_GCC=1
SET USE_MSVC=1
SET USE_DEBUG=1

@REM Advapi is used for winreg which accesses the windows registry
@REM to get cpu clock frequency which is used with rdtsc.
@REM I have not found an easier way to get the frequency.

SET GCC_INCLUDE_DIRS=-Iinclude -lAdvapi32
SET GCC_DEFINITIONS=-DOS_WINDOWS
SET GCC_COMPILE_OPTIONS=-std=c++14
SET GCC_WARN=-Wall -Werror -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable

@REM SET MSVC_COMPILE_OPTIONS=/std:c++14 /nologo /TP /EHsc /O2
SET MSVC_COMPILE_OPTIONS=/std:c++14 /nologo /TP /EHsc
SET MSVC_LINK_OPTIONS=/nologo Advapi32.lib
SET MSVC_INCLUDE_DIRS=/Iinclude
SET MSVC_DEFINITIONS=/DOS_WINDOWS

if !USE_DEBUG!==1 (
    SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /Zi
    SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! /DEBUG /PROFILE
    
    SET GCC_COMPILE_OPTIONS=!GCC_COMPILE_OPTIONS! -g
)

@REM #############  Unity build
mkdir bin 2> nul
SET srcfile=bin\all.cpp
SET srcfiles=
SET output=bin\compiler.exe

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

echo | set /p="Compiling..."
set /a startTime=6000*( 100%time:~3,2% %% 100 ) + 100* ( 100%time:~6,2% %% 100 ) + ( 100%time:~9,2% %% 100 )

SET compileSuccess=0

if !USE_MSVC!==1 (
    cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Fobin/all.obj /link !MSVC_LINK_OPTIONS! shell32.lib /OUT:!output!
    SET compileSuccess=!errorlevel!
    @REM cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! /Ycpch.h src/pch.cpp
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Yupch.h /Fobin/all.obj /link !MSVC_LINK_OPTIONS! pch.obj shell32.lib /OUT:bin/program.exe
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfiles! /link !MSVC_LINK_OPTIONS! shell32.lib /OUT:bin/program.exe
)
if !USE_GCC!==1 (
    g++ !GCC_WARN! !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! !srcfile! -o !output!
    SET compileSuccess=!errorlevel!
)

set /a endTime=6000*(100%time:~3,2% %% 100 )+100*(100%time:~6,2% %% 100 )+(100%time:~9,2% %% 100 )
set /a finS=(endTime-startTime)/100
set /a finS2=(endTime-startTime)%%100

echo Compiled in %finS%.%finS2% seconds

@REM Not using MSVC_COMPILE_OPTIONS because debug information may be added
cl /c /std:c++14 /nologo /TP /EHsc !MSVC_INCLUDE_DIRS! /DOS_WINDOWS src\BetBat\External\NativeLayer.cpp /Fo:bin/NativeLayer.obj
cl /c /std:c++14 /nologo /TP /EHsc !MSVC_INCLUDE_DIRS! /DOS_WINDOWS src\Engone\Win32.cpp /Fo:bin/NativeLayer2.obj
lib bin/NativeLayer.obj bin/NativeLayer2.obj Advapi32.lib /OUT:bin/NativeLayer.lib
dumpbin /ALL bin/NativeLayer.obj > nat.out

if !compileSuccess! == 0 (
    echo f | XCOPY /y /q !output! prog.exe > nul

    @REM cl /c /std:c11 /Tc src/BetBat/obj_test.c /Fo: bin/obj_test.obj /nologo
    @REM dumpbin bin/obj_test.obj /ALL > dump.out

    @REM link bin/obj_test.obj bin/NativeLayer.obj

    @REM obj_test

    prog -dev
    @REM  > temp
    @REM prog examples/x64_test.btb -target win-x64 -out test.exe
    dumpbin bin/dev.obj /ALL > dev.out
    @REM dumpbin bin/obj_min.obj /ALL > min.out


    @REM prog examples/linecounter.btb -out test.exe -target win-x64

    @REM link objtest.obj /DEFAULTLIB:LIBCMT

    @REM objtest
    @REM echo !errorlevel!

    @REM link objtest2.obj /DEFAULTLIB:LIBCMT

    @REM gcc -c src/BetBat/obj_test.c
    @REM prog
    @REM link obj_test.o

)