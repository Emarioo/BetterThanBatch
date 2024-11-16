#!/usr/bin/python3

### What is this ###
# This is the new build system that replaces makefiles and most of build.bat and build.sh.
# Build the project with: python build.py
# For extra options: python build.py msvc use_debug use_optimizations
# Run this if you have linking problems: python build.py clean
#   the system may not have recognized a file as changed
#   if you change compile options then you must run clean (we will automatically detect this later)

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

    config["bin_dir"] = "bin"
    if platform.system() == "Windows":
        config["output"] = "bin/btb.exe"
    else:
        config["output"] = "bin/btb"

    # config["use_compiler"] = "gcc"
    config["use_compiler"] = "msvc"
    # config["use_compiler"] = "clang"

    config["use_debug"] = True
    # config["use_tracy"] = True
    # config["use_optimizations"] = True
    # config["run_when_finished"] = True

    # config["log_sources"] = True
    # config["log_objects"] = True
    # config["log_compilation"] = True
    # config["verbose"] = True
    # config["log_cmds"] = True
    # config["silent"] = True # TODO: Fix

    # config["use_opengl"] = True # rarely used
    # config["thread_count"] = 8
    config["thread_count"] = multiprocessing.cpu_count()
    yes = compile(config)
    
    # print(config)

    if not yes:
        print("Compile failed")
    elif os.path.exists(config["output"]):
        filepath = config["output"]
        filepath = filepath.replace("\\","/")
        ind = filepath.rfind("/")
        filename = filepath
        if ind != -1:
            filename = filepath[ind+1:]
        # print("cp ", config["output"], filename)
        try:
            shutil.copy(config["output"], filename)
        except PermissionError as ex:
            print("CANNOT copy",config["output"],"->",filename)

    if yes and os.path.exists(config["output"]) and enabled("run_when_finished"):
        # cmd("bin/btb -dev")
        cmd("./"+config["output"]+" -dev")

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

    do_clean = False
    
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
        elif arg == "gcc" or arg == "msvc" or arg == "clang":
            config["use_compiler"] = arg
        elif arg == "clean":
            do_clean = True
        else:
            config[arg] = True
            # this allows use to write
            #   py build.py use_tracy
            # instead of
            #   py build.py use_tracy=true

    if do_clean:
        counter = 0
        counter += remove_files(config["bin_dir"] + "/*")
        counter += remove_files("libs/stb/lib-*")
        counter += remove_files("libs/glad/lib-*")
        
        print("Removed " + str(counter) + " files in bin")
        return True
        
    # TODO: How do we check that cl exists, cmd("cl") will print compiler version information if it does exist. Piping it won't get us the exit code we want.
    # if config["use_compiler"] == "msvc":
    #     if 0 != cmd("cl /nologo 2> nul > nul"): # cl isn't available, use gcc instead
    #         print("NOTE: Using gcc instead of msvc because 'cl' compiler isn't available.")
    #         config["use_compiler"] = "gcc"
    #         # TODO: Find and run vcvars64.bat
    if config["use_compiler"] == "msvc" and platform.system() == "Linux":
        config["use_compiler"] = "gcc"

    if config["use_compiler"] == "msvc":
        # Check if MSVC is configured
        if not is_msvc_configured():
            if configure_msvc_toolchain(enabled("verbose")):
                print("MSVC toolchain configured automatically.")
            else:
                print("MSVC toolchain could not be configured.")
                return False

    if enabled("verbose"):
        print("Options:",config)
        if "log_cmds" not in config:
            config["log_cmds"] = True

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
        object_files.append(f.replace(".cpp",".o").replace("src/",config["bin_dir"]+"/"))
    
    # With MSVC, we compile all source files every time, we therefore don't need to compute modified files
    # However, if we want to skip compiling the executable because it's up to date, we must compute modified files
    global modified_files
    modified_files = compute_modified_files(source_files, object_files, config["output"])

    if enabled("log_sources"):
        print("Source files:", source_files)

    ##############################
    #   COMPILE based on config
    ##############################
        
    global head_compiled
    global object_failed

    compile_success = False

    if not enabled("silent"):
        print("Compiling...")
        
    start_time = int(time.time())

    os.makedirs(config["bin_dir"], exist_ok = True)

    if platform.system() == "Windows" and config["use_compiler"] == "msvc":
        MSVC_COMPILE_OPTIONS    = "/std:c++17 /nologo /EHsc /TP /wd4129"
        MSVC_LINK_OPTIONS       = "/nologo /ignore:4099 Advapi32.lib shell32.lib"
        MSVC_INCLUDE_DIRS       = "/src/Iinclude /Ilibs/tracy-0.10/public"
        MSVC_DEFINITIONS        = "/DOS_WINDOWS /DCOMPILER_MSVC"

        if enabled("use_debug"):
            MSVC_COMPILE_OPTIONS += " /Z7"
            MSVC_LINK_OPTIONS += " /DEBUG" # /PROFILE

        if enabled("use_optimizations"):
            MSVC_COMPILE_OPTIONS += " /O2"
        
        if enabled("use_tracy"):
            MSVC_DEFINITIONS  += " /DTRACY_ENABLE"
            MSVC_LINK_OPTIONS += " "+config["bin_dir"]+"/tracy.obj"
            if not os.path.exists(config["bin_dir"]+"/tracy.obj"):
                cmd("cl /c "+MSVC_COMPILE_OPTIONS+" "+MSVC_INCLUDE_DIRS+" "+ MSVC_DEFINITIONS+" libs/tracy-0.10/public/TracyClient.cpp /Fo"+config["bin_dir"]+"/tracy.obj")

        MSVC_COMPILE_OPTIONS += " /FI pch.h"

        if not os.path.exists(config["bin_dir"]+"/hacky_stdcall.obj"):
            cmd("ml64 /nologo /Fo"+config["bin_dir"]+"/hacky_stdcall.obj /c src/BetBat/hacky_stdcall.asm > nul") # TODO: piping output to nul might not work with os.system
        object_files.append(config["bin_dir"]+"/hacky_stdcall.obj")

        # Create sub directories in bin, this part must be single-threaded
        for f in modified_files:
            index = source_files.index(f)
            obj = object_files[index]
            at = obj.rfind("/")
            predir = obj[0:at]
            if not os.path.exists(predir):
                os.mkdir(predir)

        if len(modified_files) == 0 and os.path.exists(config["output"]):
            compile_success = True
            # print("files up to date, skipping")
            # skipping like this won't work if compile flags change
            # because we should recompile if so.
        else:
            if enabled("unity_build"):
                # With MSVC we do a unity build, compile on source file that includes all other source files
                srcfile = config["bin_dir"]+"/all.cpp"
                fd = open(srcfile, "w")
                for file in source_files:
                    fd.write("#include \"" + os.path.abspath(file) + "\"\n")
                fd.close()

                err = cmd("cl "+MSVC_COMPILE_OPTIONS+" "+MSVC_INCLUDE_DIRS+" "+MSVC_DEFINITIONS+" "+srcfile+" /link "+MSVC_LINK_OPTIONS+" "+config["bin_dir"]+"/hacky_stdcall.obj /OUT:"+config["output"])
                # err = cmd("cl "+MSVC_COMPILE_OPTIONS+" "+MSVC_INCLUDE_DIRS+" "+MSVC_DEFINITIONS+" "+srcfile+" /Fobin/all.obj /link "+MSVC_LINK_OPTIONS+" bin/hacky_stdcall.obj /OUT:"+config["output"])
                # TODO: How do we silence cl, it prints out all.cpp. If a user specifies silent then we definitively don't want that.

                compile_success = err == 0
            else:
                # Compile the object files using multiple threads
                head_compiled = 0
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
                        err = cmd("cl /c "+MSVC_COMPILE_OPTIONS+" "+MSVC_INCLUDE_DIRS+" "+MSVC_DEFINITIONS+" "+src+" /Fo:"+obj)
                        
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

                err = cmd("link "+MSVC_LINK_OPTIONS+" "+objs_str +" /OUT:"+config["output"])
                compile_success = err == 0
        
        # TODO: Precompiled headers?

    elif config["use_compiler"] == "gcc" or config["use_compiler"] == "clang":
        GCC_COMPILE_OPTIONS = "-std=c++14"
        GCC_INCLUDE_DIRS    = "-Isrc/include -Ilibs/tracy-0.10/public -include src/include/pch.h "
        GCC_DEFINITIONS     = "-DCOMPILER_GNU"
        GCC_LINK_OPTIONS    = ""
        if platform.system() == "Windows":
            GCC_DEFINITIONS += " -DOS_WINDOWS"
        else:
            GCC_DEFINITIONS += " -DOS_LINUX"

        GCC_WARN = "-Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unused-but-set-variable -Wno-nonnull-compare -Wno-sequence-point -Wno-class-conversion -Wno-address -Wno-strict-aliasing"
        # GCC complains about dereferencing nullptr (-Wsequence-point)
        GCC_WARN += " -Wno-sign-compare"

        if enabled("use_debug"):
            GCC_COMPILE_OPTIONS += " -g"
            GCC_LINK_OPTIONS += " -g"

        if enabled("use_optimizations"):
            GCC_COMPILE_OPTIONS += " -O3"
            
        CC = "g++"
        if config["use_compiler"] == "clang":
            CC = "clang++"

        if enabled("use_tracy"):
            if platform.system() == "Windows":
                GCC_DEFINITIONS += " -DTRACY_ENABLE"
                GCC_LINK_OPTIONS += " "+config["bin_dir"]+"/tracy.o"
                if not os.path.exists(config["bin_dir"]+"/tracy.o"):
                    cmd(CC+" -c "+GCC_COMPILE_OPTIONS+" "+GCC_INCLUDE_DIRS+" "+GCC_DEFINITIONS+" libs/tracy-0.10/public/TracyClient.cpp -o "+config["bin_dir"]+"/tracy.o")
                
            else:
                print("build.py doesn't support tracy on Linux, tracy is ignored")
            
        # Code below compiles the necessary object files
      
        if platform.system() == "Windows":
            if not os.path.exists(config["bin_dir"]+"/hacky_stdcall.o"):
                cmd("as -c src/BetBat/hacky_stdcall.s -o "+config["bin_dir"]+"/hacky_stdcall.o")
            object_files.append(config["bin_dir"]+"/hacky_stdcall.o")
        else:
            if not os.path.exists(config["bin_dir"]+"/hacky_sysvcall.o"):
                cmd("as -c src/BetBat/hacky_sysvcall.s -o "+config["bin_dir"]+"/hacky_sysvcall.o")
            object_files.append(config["bin_dir"]+"/hacky_sysvcall.o")
        
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
        head_compiled = 0
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
                err = cmd(CC+" "+GCC_WARN+" "+GCC_COMPILE_OPTIONS+" "+GCC_INCLUDE_DIRS+" "+GCC_DEFINITIONS+" -c "+src +" -o "+ obj)
                
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

        err = cmd(CC+" "+GCC_LINK_OPTIONS+" "+objs_str+" -o "+config["output"])
        compile_success = err == 0
    else:
        print("Platform/compiler ",platform.system()+"/"+config["use_compiler"],"is not supported in build.py")

    finish_time = time.time() - start_time
    if compile_success and not enabled("silent"):
        print("Compiled in", int(finish_time*100)/100)

    ################################
    #   COMPILE VENDOR LIBRARIES
    ################################
    if platform.system() == "Windows":
        compile_vendor("glad","glad.c", "glad", "GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD")
        # NOTE: stb_image was modified to support STB_IMAGE_BUILD_DLL.
        compile_vendor("stb","stb_image.c", "stb_image", "STB_IMAGE_BUILD_DLL")
        
    elif platform.system() == "Linux":
        compile_vendor("glad","glad.c", "glad", "GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD")
        # NOTE: stb_image was modified to support STB_IMAGE_BUILD_DLL.
        compile_vendor("stb","stb_image.c", "stb_image", "STB_IMAGE_BUILD_DLL")
        
    
    return compile_success

def compile_vendor(vendor, src, bin_name, dll_defs = ""):
    # GCC_PATHS = "-Llibs/glfw-3.3.9/lib-mingw-w64 -Ilibs/glfw-3.3.9/include -Ilibs/glad/include -Lbin -Ilibs/stb/include"
    global global_config
    config = global_config
    GCC_PATHS = "-Ilibs/glad/include -L"+config["bin_dir"]+" -Ilibs/stb/include"
    # MSVC_PATHS = "/Ilibs/glfw-3.3.9/include /Ilibs/glad/include /Ilibs/stb/include"
    MSVC_PATHS = "/Ilibs/glad/include /Ilibs/stb/include"

    src = "libs/"+vendor+"/src/" + src
    
    mingw_path = "libs/"+vendor+"/lib-mingw-w64/"
    mingw_lib = mingw_path + bin_name + ".lib"
    mingw_dll = mingw_path + bin_name + ".dll"
    mingw_obj = config["bin_dir"]+"/" + bin_name + ".o"
    
    vc_path = "libs/"+vendor+"/lib-vc2022/"
    vc_lib = vc_path + bin_name + ".lib"
    vc_dll = vc_path + bin_name + ".dll"
    vc_dlllib = vc_path + bin_name + "dll.lib"
    vc_obj = config["bin_dir"]+"/" + bin_name + ".obj"
    
    ubuntu_path = "libs/"+vendor+"/lib-ubuntu/"
    ubuntu_lib = ubuntu_path + "lib" + bin_name + ".a"
    ubuntu_dll = ubuntu_path + "lib" +bin_name + ".so"
    ubuntu_obj = config["bin_dir"]+"/" + bin_name + ".o"

    mingw_dll_defs = ""
    vc_dll_defs = ""
    ubuntu_dll_defs = ""
    for v in dll_defs.split(" "):
        mingw_dll_defs += "-D"+v + " "
        vc_dll_defs += "/D"+v + " "
        ubuntu_dll_defs += "-D"+v + " "

    if platform.system() == "Windows":
        if not os.path.exists(mingw_path):
            os.mkdir(mingw_path)
        if not os.path.exists(vc_path):
            os.mkdir(vc_path)
            

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
        
    if platform.system() == "Linux":
        if not os.path.exists(ubuntu_path):
            os.mkdir(ubuntu_path)
            
        if not os.path.exists(ubuntu_lib):
            # Use clang if available? if it's faster?
            cmd("gcc -c "+GCC_PATHS+" " + src + " -o "+ ubuntu_obj)
            cmd("ar rcs "+ubuntu_lib+" " + ubuntu_obj)
        if not os.path.exists(ubuntu_dll):
            cmd("gcc -shared -fPIC "+GCC_PATHS + " "+ ubuntu_dll_defs + " " + src + " -o "+ubuntu_dll)

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

def find_first_folder(path):
    # Find the first folder in the given directory.
    try:
        directories = [d for d in glob.glob(path + "/*/") if os.path.isdir(d)]
        if directories:
            # TODO: Sort by highest version?
            return os.path.basename(directories[0].rstrip("\\/"))
    except Exception as e:
        pass
        # print(f"Error finding folder in path '{path}': {e}")
    return ""

def is_msvc_configured():
    env_path = os.environ.get("PATH","")
    if "\\bin\\HostX64\\x64" in env_path:
        return True
    return False

def configure_msvc_toolchain(verbose):
    # Find and set up MSVC environment variables.
    
    base_vs = "C:\\Program Files\\Microsoft Visual Studio"
    base_kits = "C:\\Program Files (x86)\\Windows Kits\\10\\"

    # Find Visual Studio version
    version_vs = find_first_folder(base_vs)
    if verbose:
        print(f"Found VS version: {version_vs}")
    if not version_vs:
        return False

    # Find MSVC version
    base_tools_nov = os.path.join(base_vs, version_vs, "Community", "VC", "Tools", "MSVC")
    version_msvc = find_first_folder(base_tools_nov)
    if verbose:
        print(f"Found MSVC version: {version_msvc}")
    if not version_msvc:
        return False

    # Find Windows Kits version
    base_kits_include = os.path.join(base_kits, "include")
    version_kits = find_first_folder(base_kits_include)
    if verbose:
        print(f"Found kits version: {version_kits}")
    if not version_kits:
        return False

    # Set paths based on the found versions
    base_tools = os.path.join(base_tools_nov, version_msvc)
    base_kits_include = os.path.join(base_kits, "include", version_kits)
    base_kits_lib = os.path.join(base_kits, "lib", version_kits)

    # Set environment variables to include MSVC tools and Windows Kits
    add_path = ";" + os.path.join(base_tools, "bin", "HostX64", "x64")
    add_libpath = ";" + os.path.join(base_tools, "lib", "x64")
    add_lib = ";" + os.path.join(base_kits_lib, "ucrt", "x64") + ";" + \
                os.path.join(base_kits_lib, "um", "x64") + ";" + \
                os.path.join(base_tools, "lib", "x64")
    add_include = ";" + os.path.join(base_tools, "include") + ";" + \
                os.path.join(base_kits_include, "ucrt") + ";" + \
                os.path.join(base_kits_include, "um") + ";" + \
                os.path.join(base_kits_include, "shared") + ";" + \
                os.path.join(base_kits_include, "winrt") + ";" + \
                os.path.join(base_kits_include, "cppwinrt")

# C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.33.31629\lib\x64

    # Set environment variables
    os.environ["PATH"] = os.environ.get("PATH","") + add_path
    os.environ["LIBPATH"] = os.environ.get("LIBPATH","") + add_libpath
    os.environ["LIB"] = os.environ.get("LIB","") + add_lib
    os.environ["INCLUDE"] = os.environ.get("INCLUDE","") + add_include

    return True

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