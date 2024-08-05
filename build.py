### What is this ###
# This is the new build system that replaces makefiles and most of build.bat and build.sh.
# Build the project with: python build.py
# For extra options: python build.py msvc use_debug use_optimizations
# Run this if you have linking problems: python build.py clean
#   the system may not have recognized a file as changed

import glob, os, sys, time, platform, threading, shutil, multiprocessing

global global_config
global_config = {}
def enabled(what):
    global global_config
    return what in global_config and global_config[what]

# main is called at the bottom
def main():
    min_ver = (3,9)
    if sys.version_info < min_ver:
        print("WARNING in build.py: Script is tested with "+str(min_ver[0])+"."+str(min_ver[1])+", earlier python versions ("+str(sys.version_info[0])+"."+str(sys.version_info[1])+") may not work.")

    config = {}
    global global_config
    global_config = config
    #####################
    #   CONFIGURATIONS
    #     Comment/uncomment the options you want
    #####################

    if platform.system() == "Windows":
        config["output"] = "bin/btb.exe"
    else:
        config["output"] = "bin/btb"

    # config["use_compiler"] = "gcc"
    config["use_compiler"] = "msvc"

    config["use_debug"] = True
    config["use_tracy"] = True
    # config["use_optimizations"] = True
    # config["run_when_finished"] = True

    # config["log_sources"] = True
    # config["log_objects"] = True
    # config["log_compilation"] = True
    # config["log_cmds"] = True
    # config["silent"] = True # TODO: Fix

    # config["use_opengl"] = True # rarely used
    # config["thread_count"] = 8
    config["thread_count"] = multiprocessing.cpu_count()
    yes = compile(config)

    if not yes:
        print("Compilation failed")
    else:
        filepath = config["output"]
        filepath = filepath.replace("\\","/")
        ind = filepath.rfind("/")
        filename = filepath
        if ind != -1:
            filename = filepath[ind+1:]
        shutil.copy(config["output"], filename)

    if yes and enabled("run_when_finished"):
        # cmd("bin/btb -dev")
        cmd("./bin/btb -dev")

        # cmd("bin/btb --test")
        
        # err = cmd("bin/btb examples/graphics/chat")

        # if err == 0:
        #     cmd("start main server")
        #     cmd("timeout 1")
        #     cmd("start main ")

        # err = cmd("bin/btb examples/graphics/game")
        
        # if err == 0:
        #     cmd("start test")
        #     cmd("timeout 1")
        #     cmd("start test client")


    sys.exit(0 if yes else 1)

def compile(config):
    global global_config
    ###################################
    #   Validate and fix config
    ###################################

    if len(sys.argv) > 1:
        # clean config, use settings from command line instead of those specified above
        # YO! this is strange, perhaps skip it completly?
        #new_config = {}
        #new_config["output"] = config["output"]
        #new_config["use_compiler"] = config["use_compiler"]
        #new_config["thread_count"] = config["thread_count"]
        #config = new_config
        global_config = config

    # parse command line arguments
    for i in range(1,len(sys.argv)):
        arg = sys.argv[i]
        # print(i, arg)
        index_of_equal = arg.find("=")
        if index_of_equal != -1:
            key = arg[0:index_of_equal].strip()
            val = arg[index_of_equal+1:].strip() # NOTE: We may not want to remove whitespace from value
            
            if val == "False" or val == "false":
                val = False
            elif val == "True" or val == "true":
                val = True
            else:
                try:
                    val = int(val)
                except ValueError:
                    pass
            config[key] = val
        elif arg == "gcc" or arg == "msvc":
            config["use_compiler"] = arg
        elif arg == "clean":
            counter = 0
            counter += remove_files("bin/*")
            counter += remove_files("libs/stb/lib-*")
            counter += remove_files("libs/glad/lib-*")
            
            print("Removed " + str(counter) + " files in bin")
            return True
        else:
            config[arg] = True
            # this allows use to write
            #   py build.py use_tracy
            # instead of
            #   py build.py use_tracy=true

        # print(config)
    # return False
    # TODO: How do we check that cl exists, cmd("cl") will print compiler version information if it does exist. Piping it won't get us the exit code we want.
    # if config["use_compiler"] == "msvc":
    #     if 0 != cmd("cl /nologo 2> nul > nul"): # cl isn't available, use gcc instead
    #         print("NOTE: Using gcc instead of msvc because 'cl' compiler isn't available.")
    #         config["use_compiler"] = "gcc"
    #         # TODO: Find and run vcvars64.bat

    if config["use_compiler"] == "msvc" and platform.system() == "Linux":
        config["use_compiler"] == "gcc"

    # print(config)

    if not "output" in config:
        print("config does not specify output path")
        return False

    #########################
    #    FIND SOURCE FILES
    #########################

    global source_files
    source_files = []

    for file in glob.glob("src/**/*.cpp", recursive = True):
        file = file.replace("\\","/")
        if file.find("__") != -1:
            continue
        if file.find("UIModule") != -1:
            continue

        if platform.system() == "Windows":
            if file.find("Linux.cpp") != -1:
                continue
        elif platform.system() == "Linux":
            if file.find("Win32.cpp") != -1:
                continue

        if file.find("/BetBat/") == -1 and file.find("/Engone/") == -1:
            continue

        source_files.append(file)

    global object_files
    object_files = []

    for f in source_files:
        object_files.append(f.replace(".cpp",".o").replace("src/","bin/"))
    
    # With MSVC, we compile all source files every time, we therefore don't need to compute modified files
    # However, if we want to skip compiling the executable because it's up to date, we must compute modified files
    global modified_files
    modified_files = compute_modified_files(source_files, object_files, config["output"])

    if enabled("log_sources"):
        print("Source files:", source_files)

    ##############################
    #   COMPILE based on config
    ##############################

    compile_success = False

    if not enabled("silent"):
        print("Compiling...")
        
    start_time = int(time.time())

    if not os.path.exists("bin"):
        os.mkdir("bin")

    if platform.system() == "Windows" and config["use_compiler"] == "msvc":
        MSVC_COMPILE_OPTIONS    = "/std:c++17 /nologo /EHsc /TP /wd4129"
        MSVC_LINK_OPTIONS       = "/nologo /ignore:4099 Advapi32.lib shell32.lib"
        MSVC_INCLUDE_DIRS       = "/Iinclude /Ilibs/tracy-0.10/public"
        MSVC_DEFINITIONS        = "/DOS_WINDOWS /DCOMPILER_MSVC"

        if enabled("use_debug"):
            MSVC_COMPILE_OPTIONS += " /Zi"
            MSVC_LINK_OPTIONS += " /DEBUG" # /PROFILE

        if enabled("use_optimizations"):
            MSVC_COMPILE_OPTIONS += " /O2"
        
        if enabled("use_tracy"):
            MSVC_DEFINITIONS  += " /DTRACY_ENABLE"
            MSVC_LINK_OPTIONS += " bin/tracy.obj"
            if not os.path.exists("bin/tracy.obj") or True:
                cmd("cl /c "+MSVC_COMPILE_OPTIONS+" "+MSVC_INCLUDE_DIRS+" "+ MSVC_DEFINITIONS+" libs/tracy-0.10/public/TracyClient.cpp /Fobin/tracy.obj")

        MSVC_COMPILE_OPTIONS += " /FI pch.h"

        if not os.path.exists("bin/hacky_stdcall.obj"):
            cmd("ml64 /nologo /Fobin/hacky_stdcall.obj /c src/BetBat/hacky_stdcall.asm > nul") # TODO: piping output to nul might not work with os.system

        if len(modified_files) == 0 and os.path.exists(config["output"]):
            compile_success = True
            # print("files up to date, skipping")
            # skipping like this won't work if compile flags change
            # because we should recompile if so.
        else:
            # With MSVC we do a unity build, compile on source file that includes all other source files
            srcfile = "bin/all.cpp"
            fd = open(srcfile, "w")
            for file in source_files:
                fd.write("#include \"" + os.path.abspath(file) + "\"\n")
            fd.close()

            err = cmd("cl "+MSVC_COMPILE_OPTIONS+" "+MSVC_INCLUDE_DIRS+" "+MSVC_DEFINITIONS+" "+srcfile+" /link "+MSVC_LINK_OPTIONS+" bin/hacky_stdcall.obj /OUT:"+config["output"])
            # err = cmd("cl "+MSVC_COMPILE_OPTIONS+" "+MSVC_INCLUDE_DIRS+" "+MSVC_DEFINITIONS+" "+srcfile+" /Fobin/all.obj /link "+MSVC_LINK_OPTIONS+" bin/hacky_stdcall.obj /OUT:"+config["output"])
            # TODO: How do we silence cl, it prints out all.cpp. If a user specifies silent then we definitively don't want that.

            compile_success = err == 0
        
        # TODO: Precompiled headers?

    elif config["use_compiler"] == "gcc":
        GCC_COMPILE_OPTIONS = "-std=c++14"
        GCC_INCLUDE_DIRS    = "-Iinclude -Ilibs/tracy-0.10/public -include include/pch.h "
        GCC_DEFINITIONS     = "-DCOMPILER_GNU"
        GCC_LINK_OPTIONS    = ""
        if platform.system() == "Windows":
            GCC_DEFINITIONS += " -DOS_WINDOWS"
        else:
            GCC_DEFINITIONS += " -DOS_LINUX"

        GCC_WARN = "-Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-nonnull-compare -Wno-sequence-point"
        # GCC complains about dereferencing nullptr (-Wsequence-point)
        GCC_WARN += " -Wno-sign-compare"

        if enabled("use_debug"):
            GCC_COMPILE_OPTIONS += " -g"

        if enabled("use_optimizations"):
            GCC_COMPILE_OPTIONS += " -O3"

        if enabled("use_tracy"):
            if platform.system() == "Windows":
                GCC_DEFINITIONS += " -DTRACY_ENABLE"
                GCC_LINK_OPTIONS += " bin/tracy.o"
                if not os.path.exists("bin/tracy.o"):
                    cmd("g++ -c "+GCC_COMPILE_OPTIONS+" "+GCC_INCLUDE_DIRS+" "+GCC_DEFINITIONS+" libs/tracy-0.10\public\TracyClient.cpp -o bin/tracy.o")
                
            else:
                print("build.py doesn't support tracy on Linux, needs testing")
            
        # Code below compiles the necessary object files
      
        if platform.system() == "Windows":
            if not os.path.exists("bin/hacky_stdcall.o"):
                cmd("as -c src/BetBat/hacky_stdcall.s -o bin/hacky_stdcall.o")
            object_files.append("bin/hacky_stdcall.o")
        else:
            if not os.path.exists("bin/hacky_sysvcall.o"):
                cmd("as -c src/BetBat/hacky_sysvcall.s -o bin/hacky_sysvcall.o")
            object_files.append("bin/hacky_sysvcall.o")
        
        # TODO: Add include directories to compute_modified_files? We assume that all includes come from "include/"

        # Find source files that were updated since last compilation, incremental build
        # NOTE: This was moved up
        # global modified_files
        # modified_files = compute_modified_files(source_files, object_files, config["output"])

        # print(modified_files)

        if enabled("log_objects"):
            print("Object files", object_files)

        # Create sub directories in bin, this part must be single-threaded
        for f in modified_files:
            index = source_files.index(f)
            obj = object_files[index]
            at = obj.rfind("/")
            predir = obj[0:at]
            if not os.path.exists(predir):
                os.mkdir(predir)
        
        # Compile the object files using multiple threads
        global head_compiled
        head_compiled = 0
        global object_failed
        object_failed = False

        def compile_objects():
            global head_compiled
            global modified_files
            global object_files
            global object_failed
            while head_compiled < len(modified_files):
                src = modified_files[head_compiled]
                index = source_files.index(src)
                obj = object_files[index]
                head_compiled+=1

                trimmed_obj = obj[4:] # skip bin/

                if enabled("log_compilation"):
                    print("Compile", trimmed_obj)
                err = cmd("g++ "+GCC_WARN+" "+GCC_COMPILE_OPTIONS+" "+GCC_INCLUDE_DIRS+" "+GCC_DEFINITIONS+" -c "+src +" -o "+ obj)
                
                if err != 0:
                    object_failed = True

                if enabled("log_compilation"):
                    print("Done", trimmed_obj)

        thread_count = 8
        if "thread_count" in config:
            thread_count = config["thread_count"]
        threads = []
        for i in range(thread_count-1):
            t = threading.Thread(target=compile_objects)
            threads.append(t)
            t.start()

        compile_objects()

        for t in threads:
            t.join()

        if object_failed:
            return False

        objs_str = ""
        for f in object_files:
            objs_str += " " + f

            if not os.path.exists(f):
                print(f,"was not compiled")
                return False

        err = cmd("g++ "+GCC_LINK_OPTIONS+" "+objs_str+" -o "+config["output"])
        compile_success = err == 0
    else:
        print("Platform/compiler ",platform.system()+"/"+config["use_compiler"],"is not supported in build.py")

    finish_time = time.time() - start_time
    if not compile_success:
        print("Compile failed")
    else:
        if not enabled("silent"):
            print("Compiled in", int(finish_time*100)/100)

    ################################
    #   COMPILE VENDOR LIBRARIES
    ################################

    if platform.system() == "Windows":
      
        compile_vendor("glad","glad.c", "glad", "GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD")
        # NOTE: stb_image was modified to support STB_IMAGE_BUILD_DLL.
        compile_vendor("stb","stb_image.c", "stb_image", "STB_IMAGE_BUILD_DLL")

    return compile_success

def compile_vendor(vendor, src, bin_name, dll_defs = ""):
    GCC_PATHS = "-Llibs/glfw-3.3.9/lib-mingw-w64 -Ilibs/glfw-3.3.9/include -Ilibs/glad/include -Lbin -Ilibs/stb/include"
    MSVC_PATHS = "/Ilibs/glfw-3.3.9/include /Ilibs/glad/include /Ilibs/stb/include"

    src = "libs/"+vendor+"/src/" + src
    path_mingw = "libs/"+vendor+"/lib-mingw-w64/"
    path_vc = "libs/"+vendor+"/lib-vc2022/"
    mingw_lib = path_mingw + bin_name + ".lib"
    mingw_dll = path_mingw + bin_name + ".dll"
    mingw_obj = "bin/" + bin_name + ".o"
    vc_lib = path_vc + bin_name + ".lib"
    vc_dll = path_vc + bin_name + ".dll"
    vc_dlllib = path_vc + bin_name + "dll.lib"
    vc_obj = "bin/" + bin_name + ".obj"

    mingw_dll_defs = ""
    vc_dll_defs = ""
    for v in dll_defs.split(" "):
        mingw_dll_defs += "-D"+v + " "
        vc_dll_defs += "/D"+v + " "

    if not os.path.exists(path_mingw):
        os.mkdir(path_mingw)
    if not os.path.exists(path_vc):
        os.mkdir(path_vc)

    if not os.path.exists(mingw_lib):
        cmd("gcc -c "+GCC_PATHS+" " + src + " -o "+ mingw_obj)
        cmd("ar rcs "+mingw_lib+" " + mingw_obj)
    if not os.path.exists(mingw_dll):
        cmd("gcc -shared -fPIC "+GCC_PATHS + " "+ mingw_dll_defs + " " + src + " -o "+mingw_dll)
    
    if not os.path.exists(vc_lib):
        cmd("cl /c /nologo /TC "+MSVC_PATHS+" " + src + " /Fo:"+vc_obj)
        cmd("lib /nologo "+vc_obj+" /OUT:"+vc_lib)
    
    if not os.path.exists(vc_dll) or not os.path.exists(vc_dlllib):
        cmd("cl /nologo /TC "+MSVC_PATHS+" "+vc_dll_defs +" "+src+" /link /DLL /OUT:"+vc_dll+" /IMPLIB:"+vc_dlllib)

# returns a list of object files to compile
def compute_modified_files(source_files, object_files, exe_file):
    modified_files = []
    file_dependencies = {}
    source_times = []

    # Calculate the modified timestamp for each source file
    # source files that include other files will "inherit"
    # the timestamp of the include if it's newer.
    for fi in range(len(source_files)):
        f = source_files[fi]
        dependencies = [f]
        index_of_deps = 0

        while index_of_deps < len(dependencies):
            dep = dependencies[index_of_deps]
            index_of_deps+=1

            new_dependencies = None
            if dep in file_dependencies:
                new_dependencies = file_dependencies[dep]
            else:
                # print(dep)
                new_dependencies = compute_dependencies(dep)
                # Store dependencies for the computed file
                # so we don't have to compute them again
                # if another file includes the same file.
                file_dependencies[dep] = new_dependencies
            
            for new_dep in new_dependencies:
                if not new_dep in dependencies:
                    dependencies.append(new_dep)
        
        # find latest time
        latest_time = -1
        for dep in dependencies:
            if not os.path.exists(dep):
                continue
            time = os.path.getmtime(dep)
            if latest_time < time:
                latest_time = time
            
        source_times.append(latest_time)

    exe_time = -1
    if os.path.exists(exe_file):
        exe_time = os.path.getmtime(exe_file)

    # Find the newly modified files
    for i in range(len(source_times)):
        time = source_times[i]
        obj_time = -1
        if not os.path.exists(object_files[i]):
            modified_files.append(source_files[i])
        else:
            obj_time = os.path.getmtime(object_files[i])
            if time > exe_time and time > obj_time:
                modified_files.append(source_files[i])

    return modified_files

# returns a list of #includes in the file
def compute_dependencies(file):
    deps = []
    
    if not os.path.exists(file):
        # print("File not found",file)
        # Some files may not be found such as "signal.h" "tracy/Tracy.hpp" "stdlib.h" but we can ignore those since they won't be modified (shouldn't be modified).
        return deps

    f = open(file, "r")
    text = f.read(-1)

    match_include = True
    match_quote = False
    path_start = -1

    # NOTE: #includes inside #if will be found even if it's inactive.
    #   This isn't a big deal, we're just computing dependencies.
    #   Knowing whether a macro in the #if condition is defined or not is very complicated.

    head = 0
    while head < len(text):
        chr = text[head]
        head+=1

        if chr == '\n':
            match_quote = False
            match_include = True
            continue

        if match_include:
            if chr == ' ' or chr == '\r' or chr == '\t':
                continue
            if chr == '#':
                if text.find("include", head) == head:
                    head += len("include")
                    match_include = False
                    match_quote = True
                else:
                    match_include = False
            else:
                match_include = False
        elif match_quote:
            if (chr == ' ' or chr == '\t') and path_start != -1:
                continue
            if chr == '"':
                if path_start == -1:
                    path_start = head
                else:
                    # NOTE: We add include/ because every include assumes that path
                    path = "include/" + text[path_start : head-1]
                    deps.append(path)
                    path_start = -1
                    match_quote = False
            elif chr == '<': # we ignore #include <path>, we only check quotes, <> should be used for standard library or third part libraries and is not something we modify. If we know the include directories then we could check third party libraries at least.
                match_include = False
        else:
            pass

    f.close()
    return deps

# files or directories
def remove_files(path):
    counter = 0
    for f in glob.glob(path):
        # print("del: " + f)
        if os.path.isdir(f):
            shutil.rmtree(f)
        else:
            os.remove(f)
        counter+=1
    return counter

def cmd(c):
    # Convert Windows style command to Linux style and vice versa.
    if platform.system() == "Windows":
        if len(c) > 1 and c[0:2] == './':
            c = c[2:]
        head = 0
        while head < len(c):
            chr = c[head]
            head+=1
            if chr == ' ':
                break
        c = c[0:head].replace('/','\\') + c[head:]
    elif platform.system() == "Linux":
        head = 0
        while head < len(c):
            chr = c[head]
            head+=1
            if chr == ' ':
                break
        
        c = c[0:head].replace('\\','/') + c[head:]
        is_relative = c.find("/", 0, head) != -1
        if is_relative:
            c = "./" + c

    if enabled("log_cmds"):
        print(c)

    return os.system(c)

if __name__ == "__main__":
    main()