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

@REM #########################
@REM    CONFIGURATIONS
@REM #########################

@REM SET USE_GCC=1
SET USE_MSVC=1

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
type nul > !srcfile!

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

@REM Advapi is used for winreg which accesses the windows registry
@REM to get cpu clock frequency which is used with rdtsc.
@REM I have not found an easier way to get the frequency.

if !USE_MSVC!==1 (
    SET MSVC_COMPILE_OPTIONS=/std:c++14 /nologo /EHsc /TP /wd4129
    SET MSVC_LINK_OPTIONS=/nologo /ignore:4099 Advapi32.lib shell32.lib
    SET MSVC_INCLUDE_DIRS=/Iinclude /Ilibs/stb/include
    SET MSVC_DEFINITIONS=/DOS_WINDOWS /DCOMPILER_MSVC

    if !USE_OPTIMIZATIONS!==1 (
        SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /O2
    )
    if !USE_DEBUG!==1 (
        SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /Zi
        SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! /DEBUG /PROFILE
    )

    @REM ICON of compiler executable
    @REM rc /nologo /fo bin\resources.res res\resources.rc
    @REM SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! bin\resources.res

    if !USE_OPENGL!==1 (
        SET MSVC_INCLUDE_DIRS=!MSVC_INCLUDE_DIRS! /Ilibs/glfw-3.3.9/include /Ilibs/glad/include
        @REM Yes we compile glad.c with C++14. Sue me. (we get C5105 warning when using C11)
        cl /c /nologo /std:c++14 /Ilibs/glad/include libs\glad\src\glad.c /Fobin/glad.obj
        SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! gdi32.lib user32.lib OpenGL32.lib libs/glfw-3.3.9/lib-vc2022/glfw3_mt.lib bin\glad.obj
    ) else (
        SET MSVC_DEFINITIONS=!MSVC_DEFINITIONS! /DNO_UIMODULE
    )
    @REM always include tracy, but it¨s not always used
    SET MSVC_INCLUDE_DIRS=!MSVC_INCLUDE_DIRS! /Ilibs/tracy-0.10/public
    if !USE_TRACY!==1 (
        SET MSVC_DEFINITIONS=!MSVC_DEFINITIONS! /DTRACY_ENABLE
        SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! bin\tracy.obj
        if not exist bin/tracy.obj (
                cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! libs\tracy-0.10\public\TracyClient.cpp /Fobin/tracy.obj
        )
    )
        SET MSVC_LINK_OPTIONS=!MSVC_LINK_OPTIONS! bin\tracy.obj 
    )
    @REM pch.h after tracy
    SET MSVC_COMPILE_OPTIONS=!MSVC_COMPILE_OPTIONS! /FI pch.h

    @REM Used in Virtual Machine to programmatically pass arguments to a function pointer
    if not exist bin/hacky_stdcall.o (
        ml64 /nologo /Fobin/hacky_stdcall.o /c src/BetBat/hacky_stdcall.asm > nul
    )

    cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Fobin/all.obj /link !MSVC_LINK_OPTIONS! bin/hacky_stdcall.o shell32.lib /OUT:!output!
    SET compileSuccess=!errorlevel!

    @REM Precompiled header, disabled for now
    @REM cl /c !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! /Ycpch.h src/pch.cpp
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfile! /Yupch.h /Fobin/all.obj /link !MSVC_LINK_OPTIONS! pch.obj shell32.lib /OUT:bin/program.exe
    @REM cl !MSVC_COMPILE_OPTIONS! !MSVC_INCLUDE_DIRS! !MSVC_DEFINITIONS! !srcfiles! /link !MSVC_LINK_OPTIONS! shell32.lib /OUT:bin/program.exe
)
if !USE_GCC!==1 (
    SET GCC_COMPILE_OPTIONS=-std=c++14
    SET GCC_INCLUDE_DIRS=-Iinclude -Ilibs/stb/include
    SET GCC_DEFINITIONS=-DOS_WINDOWS -DCOMPILER_GNU
    SET GCC_WARN=-Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable
    @REM SET GCC_WARN=-Wall -Werror -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable
    SET GCC_LINK_OPTIONS=-lAdvapi32 -lshell32
    if !USE_DEBUG!==1 (
        SET GCC_COMPILE_OPTIONS=!GCC_COMPILE_OPTIONS! -g
    )
    if !USE_OPTIMIZATIONS!==1 (
        SET GCC_COMPILE_OPTIONS=!GCC_COMPILE_OPTIONS! -O3
    )
    if !USE_OPENGL!==1 (
        gcc -c -std=c11 -Ilibs/glad/include libs\glad\src\glad.c -o bin/glad.o
        SET GCC_INCLUDE_DIRS=!GCC_INCLUDE_DIRS! -Ilibs/glfw-3.3.9/include -Ilibs/glad/include
        SET GCC_LINK_OPTIONS=!GCC_LINK_OPTIONS! -lgdi32 -luser32 -lOpenGL32 -Llibs/glfw-3.3.9/lib-mingw-w64 -lglfw3 bin\glad.o
    )
    @REM always include tracy, but it¨s not always used
    SET GCC_INCLUDE_DIRS=!GCC_INCLUDE_DIRS! -Ilibs/tracy-0.10/public
    if !USE_TRACY!==1 (
        SET GCC_DEFINITIONS=!GCC_DEFINITIONS! -DTRACY_ENABLE
        g++ -c !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! libs\tracy-0.10\public\TracyClient.cpp -o bin/tracy.o
        SET GCC_LINK_OPTIONS=!GCC_LINK_OPTIONS! bin\tracy.o
    )
    @REM pch.h after tracy
    SET GCC_COMPILE_OPTIONS=!GCC_COMPILE_OPTIONS! -include pch.h

    g++ !GCC_WARN! !GCC_COMPILE_OPTIONS! !GCC_INCLUDE_DIRS! !GCC_DEFINITIONS! !srcfile! -o !output! !GCC_LINK_OPTIONS!
    SET compileSuccess=!errorlevel!
)

set /a endTime=6000*(100%time:~3,2% %% 100 )+100*(100%time:~6,2% %% 100 )+(100%time:~9,2% %% 100 )
set /a finS=(endTime-startTime)/100
set /a finS2=(endTime-startTime)%%100

echo Compiled in %finS%.%finS2% seconds

@REM #############################
@REM    COMPILING NATIVE LAYER
@REM ##############################

@REM Not using MSVC_COMPILE_OPTIONS because debug information may be added
@REM #### GLAD ########
@REM cl /c /std:c++14 /nologo /TP /EHsc /Iinclude /Ilibs/stb/include /Ilibs/glad/include /Ilibs/glfw-3.3.9/include /DOS_WINDOWS /DCOMPILER_MSVC /DNO_PERF /DNO_TRACKER /DNATIVE_BUILD src\Native\NativeLayer.cpp /Fo:bin/NativeLayer.obj
@REM lib /nologo bin/NativeLayer.obj /ignore:4006 gdi32.lib user32.lib OpenGL32.lib libs/glfw-3.3.9/lib-vc2022/glfw3_mt.lib Advapi32.lib /OUT:bin/NativeLayer.lib

@REM #### GLEW ########
if not exist bin/NativeLayer.lib (
    cl /c /std:c++14 /nologo /TP /EHsc !MSVC_INCLUDE_DIRS! /DOS_WINDOWS /DNO_PERF /DNO_TRACKER /DNATIVE_BUILD src\Native\NativeLayer.cpp /Fo:bin/NativeLayer.obj
    lib /nologo bin/NativeLayer.obj /ignore:4006 gdi32.lib user32.lib OpenGL32.lib libs/glew-2.1.0/lib/glew32s.lib libs/glfw-3.3.8/lib/glfw3_mt.lib Advapi32.lib /OUT:bin/NativeLayer.lib
)

@REM We need to compiler NativeLayer with MVSC and GCC linker because the user may use GCC on Windows and not just MSVC.
SET GCC_COMPILE_OPTIONS=-std=c++14 -g
@REM GCC_COMPILE_OPTIONS="-std=c++14 -O3"
SET GCC_INCLUDE_DIRS=-Iinclude -Ilibs/stb/include -Ilibs/glfw-3.3.9/include -Ilibs/glad/include -include include/pch.h 
SET GCC_DEFINITIONS=-DOS_WINDOWS -DCOMPILER_GNU
SET GCC_WARN=-Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-nonnull-compare 
SET GCC_WARN=!GCC_WARN! -Wno-sign-compare 

SET GCC_PATHS=-Llibs\glfw-3.3.9\lib-mingw-w64 -Ilibs\glfw-3.3.9\include -Ilibs\glad\include -Lbin

if not exist bin/libglad.a (
    gcc -c !GCC_PATHS! libs/glad/src/glad.c -o bin/glad.o
    ar rcs bin/libglad.a bin/glad.o
)

@REM @REM glfw, glew, opengl is not linked with here, it should be
@REM libraries to link with the final executable: -lgdi32 -luser32 -lOpenGL32 -lglfw3 -lAdvapi32
@REM glfw, glew, opengl is not linked with here, it should be

if not exist bin/NativeLayer_gcc.o (
    g++ -c -g !GCC_INCLUDE_DIRS! !GCC_WARN! -DOS_WINDOWS -DNO_PERF -DNO_TRACKER -DNATIVE_BUILD src/Native/NativeLayer.cpp -o bin/NativeLayer_gcc.o
    ar rcs -o bin/NativeLayer_gcc.lib bin/NativeLayer_gcc.o
)

if !compileSuccess! == 0 (
    echo f | XCOPY /y /q !output! btb.exe > nul
:RUN_COMPILER
    rem

    @REM bin\btb -dev
    bin\btb -dev
    @REM bin\btb examples/dev.btb -g
    @REM bin\btb --test
)