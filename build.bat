@echo off
@setlocal enabledelayedexpansion

@REM ########
@REM  Hello!
@REM  run vcvars64.bat before running this script.
@REM  
@REM ########

SET arg=%1
if !arg!==run (
    @REM Run compiler with compiling it
    @REM btb -dev

    goto RUN_COMPILER

    @REM exit /b
)

@REM @REM cl /c src/Other/test.c /Z7
@REM cl /c src/Other/test.c /Zi
@REM @REM cl /c src/Other/test.c
@REM @REM cl /c src/Other/test.c /Z7
@REM dumpbin /ALL test.obj > out
@REM cvdump test.obj > out2
@REM cvdump vc140.pdb > out3

@REM exit /B

@REM SET USE_GCC=1
SET USE_DEBUG=1
SET USE_MSVC=1
@REM SET USE_TRACY=1
@REM SET USE_OPENGL=1
@REM SET USE_OPTIMIZATIONS=1

@REM Advapi is used for winreg which accesses the windows registry
@REM to get cpu clock frequency which is used with rdtsc.
@REM I have not found an easier way to get the frequency.

@REM SET GCC_INCLUDE_DIRS=-Iinclude -lAdvapi32
@REM SET GCC_DEFINITIONS=-DOS_WINDOWS
@REM SET GCC_COMPILE_OPTIONS=-std=c++14
@REM SET GCC_WARN=-Wall -Werror -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable

if !USE_OPTIMIZATIONS!==1 (
    SET MSVC_COMPILE_OPTIONS=/std:c++14 /nologo /TP /EHsc /O2
) else (
    SET MSVC_COMPILE_OPTIONS=/std:c++14 /nologo /TP /EHsc
)

SET MSVC_LINK_OPTIONS=/nologo /ignore:4099 Advapi32.lib gdi32.lib user32.lib 
SET MSVC_INCLUDE_DIRS=/Iinclude /Ilibs/stb/include /Ilibs/glfw-3.3.8/include /Ilibs/glew-2.1.0/include /Ilibs/tracy-0.10/public
SET MSVC_DEFINITIONS=/DOS_WINDOWS
SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /std:c++14 /nologo /TP /EHsc /wd4129

if !USE_OPENGL!==1 (
    SET MSVC_DEFINITIONS=/DUSE_GRAPHICS
    SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! OpenGL32.lib libs/glfw-3.3.8/lib/glfw3_mt.lib libs/glew-2.1.0/lib/glew32s.lib
)

if !USE_DEBUG!==1 (
    SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /Zi
    SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! /DEBUG /PROFILE
    
    SET GCC_COMPILE_OPTIONS=!GCC_COMPILE_OPTIONS! -g
)

mkdir bin 2> nul
SET srcfile=bin\all.cpp
SET srcfiles=
SET output=bin\btb.exe

@REM ############################
@REM       Retrieve files
@REM ############################
type nul > !srcfile!
@REM echo #include ^"pch.h^" >> !srcfile!
for /r %%i in (*.cpp) do (
    SET file=%%i
    if "x!file:__=!"=="x!file!" if "x!file:bin=!"=="x!file!" (
        if !USE_OPENGL!==1 (
            goto jump0
        )
        if "x!file:UIModule=!"=="x!file!" (
            :jump0
            if not "x!file:BetBat=!"=="x!file!" (
                echo #include ^"!file:\=/!^">> !srcfile!
                @REM set srcfiles=!srcfiles! !file!
            ) else if not "x!file:Engone=!"=="x!file!" (
                @REM set srcfiles=!srcfiles! !file!
                echo #include ^"!file:\=/!^">> !srcfile!
            )
        )
    )
)
@REM echo #include ^"../src/Native/NativeLayer.cpp^" >> !srcfile!

@REM #####################
@REM      Compiling
@REM #####################

echo | set /p="Compiling..."
set /a startTime=6000*( 100%time:~3,2% %% 100 ) + 100* ( 100%time:~6,2% %% 100 ) + ( 100%time:~9,2% %% 100 )

SET compileSuccess=0

if !USE_MSVC!==1 (
    
    @REM ICON of compiler executable
    @REM rc /nologo /fo bin\resources.res res\resources.rc
    @REM SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! bin\resources.res

    @REM COMPILE TRACY
    if !USE_TRACY!==1 (
        SET MSVC_DEFINITIONS=!MSVC_DEFINITIONS! /DTRACY_ENABLE
        SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! bin\tracy.obj
        if not exist bin/tracy.obj (
            cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! libs\tracy-0.10\public\TracyClient.cpp /Fobin/tracy.obj
        )
    )
    SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /FI pch.h

    @REM Used in Virtual Machine to programmatically pass arguments to a function pointer
    if not exist bin/hacky_stdcall.o (
        ml64 /nologo /Fobin/hacky_stdcall.o /c src/BetBat/hacky_stdcall.asm > nul
    )

    cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Fobin/all.obj /link !MSVC_LINK_OPTIONS! bin/hacky_stdcall.o shell32.lib /OUT:!output!
    SET compileSuccess=!errorlevel!
    @REM cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! /Ycpch.h src/pch.cpp
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Yupch.h /Fobin/all.obj /link !MSVC_LINK_OPTIONS! pch.obj shell32.lib /OUT:bin/program.exe
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfiles! /link !MSVC_LINK_OPTIONS! shell32.lib /OUT:bin/program.exe
)
if !USE_GCC!==1 (
    g++ !GCC_WARN! !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! !srcfile! -o !output!
    SET compileSuccess=!errorlevel!
)

@REM prog examples/dev.btb -out garb.exe

set /a endTime=6000*(100%time:~3,2% %% 100 )+100*(100%time:~6,2% %% 100 )+(100%time:~9,2% %% 100 )
set /a finS=(endTime-startTime)/100
set /a finS2=(endTime-startTime)%%100

echo Compiled in %finS%.%finS2% seconds

@REM Not using MSVC_COMPILE_OPTIONS because debug information may be added
@REM cl /c /std:c++14 /nologo /TP /EHsc !MSVC_INCLUDE_DIRS! /DOS_WINDOWS /DNO_PERF /DNO_TRACKER /DNATIVE_BUILD src\Native\NativeLayer.cpp /Fo:bin/NativeLayer.obj
@REM lib /nologo bin/NativeLayer.obj /ignore:4006 gdi32.lib user32.lib OpenGL32.lib libs/glew-2.1.0/lib/glew32s.lib libs/glfw-3.3.8/lib/glfw3_mt.lib Advapi32.lib /OUT:bin/NativeLayer.lib

@REM We need to compiler NativeLayer with MVSC and GCC linker because the user may use GCC on Windows and not just MSVC.
SET GCC_COMPILE_OPTIONS=-std=c++14 -g
@REM GCC_COMPILE_OPTIONS="-std=c++14 -O3"
SET GCC_INCLUDE_DIRS=-Iinclude -Ilibs/stb/include -Ilibs/glfw-3.3.8/include -Ilibs/glew-2.1.0/include -include include/pch.h 
SET GCC_DEFINITIONS=-DOS_WINDOWS
SET GCC_WARN=-Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-nonnull-compare 
SET GCC_WARN=!GCC_WARN! -Wno-sign-compare 

@REM glfw, glew, opengl is not linked with here, it should be

if not exist bin/NativeLayer_gcc.o (
    g++ -c -g !GCC_INCLUDE_DIRS! !GCC_WARN! -DOS_WINDOWS -DNO_PERF -DNO_TRACKER -DNATIVE_BUILD src/Native/NativeLayer.cpp -o bin/NativeLayer_gcc.o
    ar rcs -o bin/NativeLayer_gcc.lib bin/NativeLayer_gcc.o
)

@REM lib /nologo bin/NativeLayer.obj Advapi32.lib /OUT:bin/NativeLayer.lib
@REM dumpbin /ALL bin/NativeLayer.obj > nat.out

@REM cl /c /NOLOGO src/Other/test.c /Z7
@REM cl /c /nologo src/Other/test.c /Fdtest.pdb /Zi
@REM cvdump test.pdb > out3
@REM link /nologo test.obj /pdb:testexe.pdb /DEFAULTLIB:LIBCMT /INCREMENTAL:NO /OUT:test.exe /DEBUG 
@REM cl /c src/Other/test.c
@REM cl /c src/Other/test.c /Z7
@REM dumpbin /ALL test.obj > out
@REM cvdump test.obj > out2

@REM cl /nologo /c src/Other/test.c /I. /INCREMENTAL:NO /Zi /Fdwa.pdb 
@REM cl /c src/Other/test.c
@REM cl /c src/Other/test.c /Z7
@REM dumpbin /ALL test.obj > out
@REM cvdump test.obj > out2
@REM cvdump test.pdb > out3

if !compileSuccess! == 0 (
    echo f | XCOPY /y /q !output! btb.exe > nul
:RUN_COMPILER
    rem

    @REM cl /c /TP src/Other/test.cpp /Fo: bin/test2.obj /nologo

    @REM cl /c /std:c11 /Tc src/BetBat/obj_test.c /Fo: bin/obj_test.obj /nologo
    @REM dumpbin bin/obj_test.obj /ALL > dump.out

    @REM link bin/obj_test.obj bin/NativeLayer.obj

    @REM btb -twe *switch.btb
    @REM bin\btb -dev
    @REM bin\btb
    @REM bin\btb --test
    @REM btb -dev
    btb examples/dev.btb
    @REM btb -pm *typeinfo.btb -o dev.exe -g -r

    @REM btb dev.btb -p
    @REM btb -p -pm *dev.btb

    @REM btb --test --profiling
    @REM btb -ss binary_viewer/main.btb -o dev.exe -r -g
    @REM btb examples/dev.btb -p
    @REM btb --test
    @REM btb -sfs dev.btb
    @REM objdump bin/dev.obj -W
    @REM g++ src/Other/test.cpp -g -o test.exe
    @REM gcc -c src/Other/test.c -g -o test.o
    @REM gcc test.o -g -o test.exe
    @REM objdump test.o -W > out
    
    @REM link /NOLOGO /DEBUG test.obj /OUT:test.exe
    
    @REM cvdump test2.pdb
    @REM cvdump bin/dev.pdb
    @REM cvdump dev1.pdb

    @REM cvdump bin/dev.obj
    @REM cvdump dev.obj > dev2
    @REM dumpbin dev.obj /ALL > dev

    @REM cvdump bin/dev.pdb > dev2

    @REM  > temp
    @REM prog examples/x64_test.btb -target win-x64 -out test.exe
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