@echo off
@setlocal enabledelayedexpansion

@REM #############################
@REM  Run vcvars64.bat before running this script. Scripts needs cl, lib, and link (you can manually setup environment variables to Visual Studio tool chain).
@REM  Make sure you have MinGW for g++.
@REM #####################################


SET arg=%1
if !arg!==run (
    @REM Run compiler with compiling it
    @REM btb -dev

    goto RUN_COMPILER
)

@REM SET USE_GCC=1
SET USE_DEBUG=1
SET USE_MSVC=1
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

SET MSVC_LINK_OPTIONS=/nologo /ignore:4099 Advapi32.lib gdi32.lib user32.lib OpenGL32.lib libs/glfw-3.3.9/lib-vc2022/glfw3_mt.lib
SET MSVC_INCLUDE_DIRS=/Iinclude /Ilibs/stb/include /Ilibs/glfw-3.3.9/include /Ilibs/glad/include /Ilibs/tracy-0.10/public
SET MSVC_DEFINITIONS=/DOS_WINDOWS /DTRACY_ENABLE
SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /std:c++14 /nologo /TP /EHsc /wd4129

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
@REM       Get files
@REM ############################
type nul > !srcfile!

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
echo #include ^"../src/Native/NativeLayer.cpp^" >> !srcfile!

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

    cl /c /nologo /std:c++14 /Ilibs/glad/include libs\glad\src\glad.c /Fobin/glad.obj

    @REM COMPILE TRACY
    cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! libs\tracy-0.10\public\TracyClient.cpp /Fobin/tracy.obj
    SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /FI pch.h

    cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Fobin/all.obj /link !MSVC_LINK_OPTIONS! bin\tracy.obj bin\glad.obj shell32.lib /OUT:!output!
    SET compileSuccess=!errorlevel!
    @REM cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! /Ycpch.h src/pch.cpp
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Yupch.h /Fobin/all.obj /link !MSVC_LINK_OPTIONS! pch.obj shell32.lib /OUT:bin/program.exe
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfiles! /link !MSVC_LINK_OPTIONS! shell32.lib /OUT:bin/program.exe
)
if !USE_GCC!==1 (
    echo USE_GCC in build.bat incomplete?
    g++ !GCC_WARN! !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! !srcfile! -o !output!
    SET compileSuccess=!errorlevel!
)

set /a endTime=6000*(100%time:~3,2% %% 100 )+100*(100%time:~6,2% %% 100 )+(100%time:~9,2% %% 100 )
set /a finS=(endTime-startTime)/100
set /a finS2=(endTime-startTime)%%100

echo Compiled in %finS%.%finS2% seconds

@REM Not using MSVC_COMPILE_OPTIONS because debug information may be added
cl /c /std:c++14 /nologo /TP /EHsc !MSVC_INCLUDE_DIRS! /DOS_WINDOWS /DNO_PERF /DNO_TRACKER /DNATIVE_BUILD src\Native\NativeLayer.cpp /Fo:bin/NativeLayer.obj
lib /nologo bin/NativeLayer.obj /ignore:4006 gdi32.lib user32.lib OpenGL32.lib libs/glfw-3.3.9/lib-vc2022/glfw3_mt.lib Advapi32.lib /OUT:bin/NativeLayer.lib

@REM We need to compiler NativeLayer with MVSC and GCC linker because the user may use GCC on Windows and not just MSVC.
SET GCC_COMPILE_OPTIONS=-std=c++14 -g
@REM GCC_COMPILE_OPTIONS="-std=c++14 -O3"
SET GCC_INCLUDE_DIRS=-Iinclude -Ilibs/stb/include -Ilibs/glfw-3.3.9/include -Ilibs/glad/include -include include/pch.h 
SET GCC_DEFINITIONS=-DOS_WINDOWS
SET GCC_WARN=-Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-nonnull-compare 
SET GCC_WARN=!GCC_WARN! -Wno-sign-compare 

SET GCC_PATHS=-Llibs\glfw-3.3.9\lib-mingw-w64 -Ilibs\glfw-3.3.9\include -Ilibs\glad\include -Lbin

gcc -c !GCC_PATHS! libs/glad/src/glad.c -o bin/glad.o
@REM ar rcs bin/libglad.a glad.o

@REM @REM glfw, glew, opengl is not linked with here, it should be
g++ -c -g !GCC_PATHS! !GCC_INCLUDE_DIRS! !GCC_WARN! -DOS_WINDOWS -DNO_PERF -DNO_TRACKER -DNATIVE_BUILD src/Native/NativeLayer.cpp -o bin/NativeLayer_gcc.o
ar rcs -o bin/NativeLayer_gcc.lib bin/NativeLayer_gcc.o bin/glad.o
@REM libraries to link with the final executable: -lgdi32 -luser32 -lOpenGL32 -lglfw3 -lAdvapi32

if !compileSuccess! == 0 (
    echo f | XCOPY /y /q !output! btb.exe > nul
:RUN_COMPILER
    rem

    btb -dev
)


@REM Compiling exe with GLFW?
@REM set PATHS=-Llibs/glfw-3.3.9/lib-mingw-w64 -Ilibs/glfw-3.3.9/include -Ilibs/glad/include -Lbin

@REM gcc -c libs/glad/src/glad.c !PATHS! -o glad.o
@REM ar rcs bin/libglad.a glad.o

@REM g++ -c wa.cpp !PATHS! -o wa.o

@REM g++ -o wa.exe !PATHS! wa.o -lglfw3 -lglad -luser32 -lopengl32 -lkernel32 -lgdi32