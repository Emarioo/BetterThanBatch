#!/usr/bin/python3

### What is this ###
# This is the new build system that replaces makefiles, build.bat and build.sh.
# Build the project with: python build.py
# For extra options: python build.py msvc use_debug use_optimizations
# Run this if you have linking problems: python build.py clean
#   the system may not have recognized a file as changed
#   if you change compile options then you must run clean (we will automatically detect this later)

import glob, os, sys, time, platform, threading, shutil, multiprocessing, subprocess, dataclasses, shlex

@dataclasses.dataclass
class BuildConfig:
    toolchain: str = "gcc"
    debug: bool = True
    optimize: bool = False
    tracy: bool = False
    int_dir: str = "bin/int"
    build_steps: list[str] = dataclasses.field(default_factory=list) # TODO: 

    # extra options, does not affect the binary
    exe_output: str = "bin/btb"
    threads: int = multiprocessing.cpu_count()
    verbose: bool = False
    silent: bool = False

    # methods
    def get_state(self):
        config = f'''
        toolchain {self.toolchain}
        debug {self.debug}
        optimize {self.optimize}
        tracy {self.tracy}
        int_dir {self.int_dir}
        '''
        config = "\n".join([l.strip() for l in config.split("\n") if len(l) > 0]) + "\n" # remove trailing whitespace on lines
        
        # TODO: Include build steps in state. We aren't currently
        #   because of threading and incremental compilation.
        config += "build_steps\n"
        for step in self.build_steps:
            config += f"  {step}\n"
        return config

    def save(self, path: str):
        config = self.get_state()

        # default should be config_path = f"{self.int_dir}/last_build.config"
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(config)

    def has_config_changed(self, path: str):
        cur_config = self.get_state()
        try:
            with open(path, "r") as f:
                file_config = f.read()
        except FileNotFoundError as ex:
            return True

        return cur_config != file_config

# main is called at the bottom
def main(arguments: list[str]):
    # build.py               - default btb dev build (with debug info)
    # build.py release 0.1.2 - build and bundle a btb release (no debug, with optimizations)
    # build.py clean         - remove intermediate files
    # build.py clean build   - clean and build

    action_clean = False
    action_release = False
    action_vendor = True
    action_build = True
    action_create_wrapper = False
    _had_explicit_build = False
    _had_explicit_output = False

    release_version = ""

    # Parse arguments
    config = BuildConfig()
    argi = 0
    while argi < len(arguments):
        arg = arguments[argi]
        argi+=1

        index_of_equal = arg.find("=")
        if index_of_equal == -1:
            if arg == "gcc" or arg == "msvc" or arg == "clang":
                config.toolchain = arg
            elif arg == "debug":
                config.debug = True
            elif arg == "optimize":
                config.optimize = True
            elif arg == "tracy":
                config.tracy = True
            elif arg == "clean":
                action_clean = True
                if not _had_explicit_build:
                    action_build = False
            elif arg == "build":
                action_build = True
                _had_explicit_build = True
            elif arg == "release":
                if argi >= len(arguments):
                    print(f"Expected version after {arg}")
                    exit(1)
                action_release = True
                action_build = False
                release_version = arguments[argi]
                argi += 1
            elif arg == 'verbose':
                config.verbose = True
            elif arg == 'silent':
                config.silent = True
            elif arg == "wrap":
                action_create_wrapper = True
            else:
                print(f"Unknown argument '{arg}'")
        else:
            key = arg[0:index_of_equal].strip()
            val = arg[index_of_equal+1:].strip() # NOTE: We may not want to remove whitespace from value
            
            if key == "toolchain":
                if val != "gcc" or val != "msvc" or val != "clang":
                    print(f"Argument '{val}' is not a supported toolchain.\nUse one of these: gcc, msvc, clang")
                    exit(1)
                config.toolchain = val
            elif key == "debug":
                assert val.lower() == "true" or val.lower() == "false"
                if val.lower() == "true":
                    config.debug = True
                elif val.lower() == "false":
                    config.debug = False
                else:
                    print(f"Value to argument '{key}' should be 'true' or 'false'. Not '{val}'.")
                    exit(1)
            elif key == "optimize":
                assert val.lower() == "true" or val.lower() == "false"
                if val.lower() == "true":
                    config.optimize = True
                elif val.lower() == "false":
                    config.optimize = False
                else:
                    print(f"Value to argument '{key}' should be 'true' or 'false'. Not '{val}'.")
                    exit(1)
            elif key == "tracy":
                assert val.lower() == "true" or val.lower() == "false"
                if val.lower() == "true":
                    config.tracy = True
                elif val.lower() == "false":
                    config.tracy = False
                else:
                    print(f"Value to argument '{key}' should be 'true' or 'false'. Not '{val}'.")
                    exit(1)
            elif key == "output":
                config.exe_output = val
                _had_explicit_output = True
            elif key == "threads":
                config.threads = int(val)
            else:
                print(f"Unknown argument key '{arg}'")

    if action_release and action_build:
        print("Cannot 'release' and 'build' at the same time.")
        exit(1)

    # Perform build, release, clean actions

    if action_clean:
        if os.path.exists("bin"):
            shutil.rmtree("bin")
        # TODO: Remove libraries in libs

    if action_create_wrapper:
        try_create_btb_wrapper()

    if has_wrapper() and not _had_explicit_output and action_build:
        config.exe_output = "bin/btb-dev"
        if platform.system() == "Windows":
            config.exe_output += ".exe"


    if action_build:
        start = time.time()
        newly_compiled_btb = build_btb(config)
        comp_time = time.time() - start
        if not config.silent or config.verbose:
            if newly_compiled_btb:
                print(f"Compiled BTB in {comp_time:.3} s")
            else:
                print("btb is up to date")

    if action_vendor:
        if platform.system() == "Windows":
            compile_vendor(config, "glad","glad.c", "glad", "GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD")
            # NOTE: stb_image was modified to support STB_IMAGE_BUILD_DLL.
            compile_vendor(config, "stb","stb_image.c", "stb_image", "STB_IMAGE_BUILD_DLL")
            
        elif platform.system() == "Linux":
            compile_vendor(config, "glad","glad.c", "glad", "GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD")
            # NOTE: stb_image was modified to support STB_IMAGE_BUILD_DLL.
            compile_vendor(config, "stb","stb_image.c", "stb_image", "STB_IMAGE_BUILD_DLL")
            
    if action_release:
        release_btb(config, release_version)


def build_btb(config: BuildConfig) -> bool:
    if config.verbose:
        print("Build btb with", config)

    if config.toolchain == 'msvc':
        try_configure_msvc_toolchain(config.verbose)

    start_compute_time = time.time()
    files = gather_btb_files(config)
    if config.tracy:
        files.append(("libs/tracy-0.10/public/TracyClient.cpp", config.int_dir + "/TracyClient.o"))

    @dataclasses.dataclass
    class BuildRuntime:
        config: BuildConfig
        commands: list[str] = dataclasses.field(default_factory=list)
        next_command_index: int = 0
        failed: bool = False

    runtime = BuildRuntime(config)

    for srcdst in files:
        src_file = srcdst[0]
        obj_file = srcdst[1]
        if config.toolchain == 'msvc':
            FLAGS = "/std:c++17 /nologo /EHsc /TP /wd4129 /Isrc/include /Ilibs/tracy-0.10/public /FI pch.h /DCOMPILER_MSVC"
            if platform.system() == "Windows":
                FLAGS += " -DOS_WINDOWS"
            elif platform.system() == "Linux":
                FLAGS += " -DOS_LINUX"
            if config.debug:
                FLAGS += " /Z7"
            if config.optimize:
                FLAGS += " /O2"
            if config.tracy:
                FLAGS += " /DTRACY_ENABLE"
            pdb_path, _ = os.path.splitext(obj_file)
            pdb_path += ".pdb"
            command = f"cl {FLAGS} /c /Fd:{pdb_path} /Fo:{obj_file} {src_file}"
        elif config.toolchain == 'gcc' or config.toolchain == 'clang':
            FLAGS = "-std=c++17 -Isrc/include -Ilibs/tracy-0.10/public -include src/include/pch.h -DCOMPILER_GNU"
            FLAGS += " -Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unused-but-set-variable -Wno-nonnull-compare -Wno-sequence-point -Wno-class-conversion -Wno-address -Wno-strict-aliasing -Wno-sign-compare"

            if platform.system() == "Windows":
                FLAGS += " -DOS_WINDOWS"
            elif platform.system() == "Linux":
                FLAGS += " -DOS_LINUX"
            if config.debug:
                FLAGS += " -g"
            if config.optimize:
                FLAGS += " -O3"
            if config.tracy:
                FLAGS += " -DTRACY_ENABLE"
            CC = 'g++' if config.toolchain == 'gcc' else 'clang++'
            command = f"{CC} {FLAGS} -c -o {obj_file} {src_file}"

        config.build_steps.append(command)

    object_files = " ".join([dst for src, dst in files])

    config.exe_output = os.path.abspath(config.exe_output) # relative and absolute path can refer to the same file, it should not trigger a changed config.

    if config.toolchain == 'msvc':
        FLAGS = "/nologo /ignore:4099 Advapi32.lib shell32.lib"
        if config.debug:
            FLAGS += " /DEBUG"

        link_command = f"link {FLAGS} {object_files} /OUT:{config.exe_output}"
    elif config.toolchain == 'gcc' or config.toolchain == 'clang':
        FLAGS = ""
        if config.debug:
            FLAGS += " -g"

        CC = 'g++' if config.toolchain == 'gcc' else 'clang++'
        
        no_backlash = config.exe_output.replace('\\','/') # mingw on windows can't handle backslash in output path.
        link_command = f"{CC} {FLAGS} {object_files} -o {no_backlash}"

    config.build_steps.append(link_command)

    CONFIG_PATH = "bin/last_build.config"
    config_changed = config.has_config_changed(CONFIG_PATH)

    if config_changed:
        if config.verbose:
            print("Config changed, needs rebuild")
        # do a full rebuild
        modified_files = files
    else:
        modified_files = compute_modified_files(config, files)
        if config.verbose:
                print("Modified files: ", modified_files)
    compute_time = time.time() - start_compute_time

    if config.verbose:
        print(f"Computed dependencies in {compute_time:.3} s")

    if len(modified_files) == 0 and os.path.exists(config.exe_output):
        return False
    
    config.save(CONFIG_PATH)

    for i in range(0, len(files)):
        if files[i] in modified_files:
            runtime.commands.append(config.build_steps[i])

    os.makedirs(config.int_dir, exist_ok=True)
    os.makedirs(os.path.dirname(config.exe_output), exist_ok=True)
    for f in modified_files:
        os.makedirs(os.path.dirname(f[1]), exist_ok=True)

    def compile_objects(runtime: BuildRuntime):
        while runtime.next_command_index < len(runtime.commands):
            command = runtime.commands[runtime.next_command_index]
            runtime.next_command_index += 1

            if runtime.config.verbose:
                print(command, flush=True)
            result = cmd(command, config.silent)
            
            if result != 0:
                runtime.next_command_index = len(runtime.commands)
                runtime.failed = True
    threads = []
    for i in range(config.threads):
        t = threading.Thread(target=compile_objects, args=[runtime])
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    if runtime.failed:
        exit(1)
    
    if runtime.config.verbose:
        print(link_command)
    result = cmd(link_command, config.silent)
    if result != 0:
        exit(1)

    return True

def release_btb(config: BuildConfig, version):
    if config.verbose:
        print("Build btb release with", config)

    bundle_name = f"btb-{version}"
    if platform.system() == "Windows":
        bundle_name += "-windows-x86_64"
    elif platform.system() == "Linux":
        bundle_name += "-linux-x86_64"
    else:
        assert False

    bundle_dir = f"bin/{bundle_name}"

    if os.path.exists(bundle_dir):
        shutil.rmtree(bundle_dir) # make sure we deleted files from docs/libs/modules don't remain in the bundle dir

    os.makedirs(f"{bundle_dir}", exist_ok=True)
    os.makedirs(f"{bundle_dir}/bin", exist_ok=True)
    os.makedirs("releases", exist_ok=True)

    config.debug = False
    config.optimize = True
    config.tracy = False

    config.exe_output = f"{bundle_dir}/bin/btb-{version}"
    if platform.system() == "Windows":
        config.exe_output += ".exe"

    def get_commit_hash():
        proc = subprocess.run(["git", "rev-parse","--short=12","HEAD"], text=True,stdout=subprocess.PIPE)
        if proc.returncode != 0:
            print(proc.stdout)
            exit(1)
        return proc.stdout.strip()

    git_commit = get_commit_hash()

    commit_file = "src/BetBat/const_commit.cpp"
    wanted_text = f'// THIS FILE IS AUTO-GENERATED BY build.py\nconst char* GIT_COMMIT="{git_commit}";\n'
    current_text = ""
    if os.path.exists(commit_file):
        with open(commit_file, "r") as f:
            current_text = f.read()
    # Only write if it needs updating. Otherwise we'll be recompiling.
    if current_text != wanted_text:
        with open("src/BetBat/const_commit.cpp", "w") as f:
            f.write(wanted_text)

    build_btb(config)

    shutil.copytree("docs", f"{bundle_dir}/docs", dirs_exist_ok=True)
    shutil.copytree("libs", f"{bundle_dir}/libs", dirs_exist_ok=True)
    shutil.copytree("modules", f"{bundle_dir}/modules", dirs_exist_ok=True)
    shutil.copy("README.md", f"{bundle_dir}/README.md")
    shutil.copy(config.exe_output, f"{bundle_dir}/bin/btb{'.exe' if platform.system() == 'Windows' else ''}")

    path_release = os.path.abspath(f"releases/{bundle_name}")

    if platform.system() == "Windows":
        path_release += ".zip"
        command = f"7z a -tzip {path_release} {os.path.abspath(bundle_dir)}/"
    elif platform.system() == "Linux":
        path_release += ".tar.gz"
        # TODO: This has not been tested on Linux yet
        command = f"tar -czf {path_release} -C bin {bundle_name}"

    if config.verbose:
        print(command)
    result = cmd(command, config.silent)
    if result != 0:
        exit(1)
    
    print(f"Prepared release in {path_release}")

def has_wrapper():
    if platform.system() == "Windows":
        file = "btb.bat"
    else:
        file = "btb"
    wrapper = os.path.dirname(__file__) + "/bin/" + file

    # wrapper is not a bash script (it shouldn't be this large so it's probably btb executable)
    return os.path.exists(wrapper) and os.stat(wrapper).st_size <= 200

def try_create_btb_wrapper():
    if platform.system() == "Windows":
        file = "btb.bat"
        code = '''
        @echo off
        python %~dp0../build.py output=%~dp0btb-dev.exe
        IF %errorlevel%==0 (
            %~dp0btb-dev.exe %*
        )
        '''
    else:
        file = "btb"
        code = '''
        set -e
        SCRIPT_DIR=$(dirname ${BASH_SOURCE[0]})
        python3 $SCRIPT_DIR/../build.py 
        btb-dev $@
        '''

    wrapper = os.path.dirname(__file__) + "/bin/" + file

    if not os.path.exists(wrapper) or os.stat(wrapper).st_size > 200:
        os.makedirs(os.path.dirname(wrapper), exist_ok=True)
        with open(wrapper, "w") as f:
            f.write(code)
        os.chmod(wrapper, 0o764)
    

def gather_btb_files(config: BuildConfig) -> list[tuple[str,str]]:
    files = []

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

        if os.path.basename(file) in ["PDB.cpp", "Fuzzer.cpp", "UserProfile.cpp"]:
            continue

        obj_file = file.replace(".cpp",".o").replace("src/", config.int_dir + "/")
        files.append((file, obj_file))

    return files

def compile_vendor(config: BuildConfig, vendor, src, bin_name, dll_defs = ""):
    # GCC_PATHS = "-Llibs/glfw-3.3.9/lib-mingw-w64 -Ilibs/glfw-3.3.9/include -Ilibs/glad/include -Lbin -Ilibs/stb/include"
    GCC_PATHS = "-Ilibs/glad/include -L"+config.int_dir+" -Ilibs/stb/include"
    # MSVC_PATHS = "/Ilibs/glfw-3.3.9/include /Ilibs/glad/include /Ilibs/stb/include"
    MSVC_PATHS = "/Ilibs/glad/include /Ilibs/stb/include"

    src = "libs/"+vendor+"/src/" + src
    
    mingw_path = "libs/"+vendor+"/lib-mingw-w64/"
    mingw_lib = mingw_path + bin_name + ".lib"
    mingw_dll = mingw_path + bin_name + ".dll"
    mingw_obj = config.int_dir+"/" + bin_name + ".o"
    
    vc_path = "libs/"+vendor+"/lib-vc2022/"
    vc_lib = vc_path + bin_name + ".lib"
    vc_dll = vc_path + bin_name + ".dll"
    vc_dlllib = vc_path + bin_name + "dll.lib"
    vc_obj = config.int_dir+"/" + bin_name + ".obj"
    
    ubuntu_path = "libs/"+vendor+"/lib-ubuntu/"
    ubuntu_lib = ubuntu_path + "lib" + bin_name + ".a"
    ubuntu_dll = ubuntu_path + "lib" +bin_name + ".so"
    ubuntu_obj = config.int_dir+"/" + bin_name + ".o"

    mingw_dll_defs = ""
    vc_dll_defs = ""
    ubuntu_dll_defs = ""
    for v in dll_defs.split(" "):
        mingw_dll_defs += "-D"+v + " "
        vc_dll_defs += "/D"+v + " "
        ubuntu_dll_defs += "-D"+v + " "

    if platform.system() == "Windows":
        os.makedirs(mingw_path, exist_ok=True)
        os.makedirs(vc_path, exist_ok=True)

        if not os.path.exists(mingw_lib):
            cmd("gcc -c "+GCC_PATHS+" " + src + " -o "+ mingw_obj, config.silent)
            cmd("ar rcs "+mingw_lib+" " + mingw_obj, config.silent)
        if not os.path.exists(mingw_dll):
            cmd("gcc -shared -fPIC "+GCC_PATHS + " "+ mingw_dll_defs + " " + src + " -o "+mingw_dll, config.silent)
        
        if shutil.which("cl"): # only compile with cl if it's available
            if not os.path.exists(vc_lib):
                cmd("cl /c /nologo /TC "+MSVC_PATHS+" " + src + " /Fo:"+vc_obj, config.silent)
                cmd("lib /nologo "+vc_obj+" /OUT:"+vc_lib, config.silent)
            
            if not os.path.exists(vc_dll) or not os.path.exists(vc_dlllib):
                cmd("cl /nologo /TC "+MSVC_PATHS+" "+vc_dll_defs +" "+src+" /link /DLL /OUT:"+vc_dll+" /IMPLIB:"+vc_dlllib, config.silent)
        
    if platform.system() == "Linux":
        os.makedirs(ubuntu_path, exist_ok=True)
            
        if not os.path.exists(ubuntu_lib):
            # Use clang if available? if it's faster?
            cmd("gcc -c "+GCC_PATHS+" " + src + " -o "+ ubuntu_obj, config.silent)
            cmd("ar rcs "+ubuntu_lib+" " + ubuntu_obj, config.silent)
        if not os.path.exists(ubuntu_dll):
            cmd("gcc -shared -fPIC "+GCC_PATHS + " "+ ubuntu_dll_defs + " " + src + " -o "+ubuntu_dll, config.silent)

# returns a list of object files to compile
def compute_modified_files(config: BuildConfig, files: list[tuple[str,str]]) -> list[str,str]:
    modified_files = []
    file_dependencies = {}
    source_times = []

    SRC=0
    DST=1

    # Calculate the modified timestamp for each source file
    # source files that include other files will "inherit"
    # the timestamp of the include if it's newer.
    for fi in range(len(files)):
        f = files[fi][SRC]
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
                new_dependencies = find_includes(dep)
                # Store dependencies for the computed file
                # so we don't have to compute them again
                # if another file includes the same file.
                file_dependencies[dep] = new_dependencies
            
            for new_dep in new_dependencies:
                if not new_dep in dependencies:
                    dependencies.append(new_dep)
        
        # print("FILE:",f)
        # print("   DEPS:",dependencies)
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
    if os.path.exists(config.exe_output):
        exe_time = os.path.getmtime(config.exe_output)

    # Find the newly modified files
    for i in range(len(source_times)):
        time = source_times[i]
        obj_time = -1
        if not os.path.exists(files[i][DST]):
            modified_files.append(files[i])
        else:
            obj_time = os.path.getmtime(files[i][DST])
            if (time > exe_time and exe_time != -1) or time > obj_time:
                modified_files.append(files[i])

    return modified_files

# returns a list of #includes in the file
def find_includes(file):
    include_paths = ["src/include"] # TODO: Don't hard code includes
    deps = []
    
    if not os.path.exists(file):
        # Some files may not be found such as "signal.h" "tracy/Tracy.hpp" "stdlib.h" but we can ignore those since they won't be modified (shouldn't be modified).
        if file.find("BetBat") != -1 or file.find("Engone") != -1:
            # If we don't find files from BetBat or Engone then
            # we have a problem
            print("File not found",file)
        return deps

    f = open(file, "r")
    text = f.read(-1)
    f.close()

    match_include = True
    match_quote = False
    path_start = -1

    # NOTE: #includes inside #if will be found even if it's inactive.
    #   This isn't a big deal, we're just computing dependencies.
    #   Knowing whether a macro in the #if condition is defined or not is very complicated.

    head = 0
    head_len = len(text)
    while head < head_len:
        chr = text[head]
        head+=1

        if chr == '\n':
            match_quote = False
            match_include = True
            continue

        if match_include:
            if chr == ' ' or chr == '\r' or chr == '\t':
                continue
            # A little bit faster
            # if chr == '#' and text[head] == "i"
            if chr == '#' and text[head] == "i":
                # print(text[head-1:head+10])
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
                    relative_path = text[path_start : head-1]
                    path_start = -1
                    match_quote = False
                    
                    for include_dir in include_paths:
                        dep_path = os.path.join(include_dir, relative_path)
                        if os.path.exists(dep_path):
                            deps.append(dep_path)
                            break
            elif chr == '<': # we ignore #include <path>, we only check quotes, <> should be used for standard library or third part libraries and is not something we modify. If we know the include directories then we could check third party libraries at least.
                match_include = False
        else:
            pass

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

def try_configure_msvc_toolchain(verbose):
    if is_msvc_configured():
        # already configured
        return True
    
    # Find and set up MSVC environment variables.
    
    base_vs = "C:\\Program Files\\Microsoft Visual Studio"
    base_kits = "C:\\Program Files (x86)\\Windows Kits\\10\\"

    # Find Visual Studio version
    version_vs = find_first_folder(base_vs)
    if not version_vs:
        print(f"Could not find Visual Studio version in '{base_vs}'")
        exit(1)
    if verbose:
        print(f"Found VS version: {version_vs}")

    # Find MSVC version
    base_tools_nov = os.path.join(base_vs, version_vs, "Community", "VC", "Tools", "MSVC")
    version_msvc = find_first_folder(base_tools_nov)
    if not version_msvc:
        print(f"Could not find Visual Studio stuff in '{base_tools_nov}'")
        exit(1)
    if verbose:
        print(f"Found MSVC version: {version_msvc}")

    # Find Windows Kits version
    base_kits_include = os.path.join(base_kits, "include")
    version_kits = find_first_folder(base_kits_include)
    if not version_kits:
        print(f"Could not stuff in '{base_kits_include}'")
        exit(1)
    if verbose:
        print(f"Found kits version: {version_kits}")

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

# remove files or directories with glob pattern
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

def cmd(c, silent = False):
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

    proc = subprocess.run(shlex.split(c), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (proc.returncode != 0 or len(proc.stdout) > 0) and not silent:
        print(proc.stdout, end="")

    return proc.returncode

if __name__ == "__main__":
    min_ver = (3,9)
    if sys.version_info < min_ver:
        print("WARNING in build.py: Script is tested with "+str(min_ver[0])+"."+str(min_ver[1])+", earlier python versions ("+str(sys.version_info[0])+"."+str(sys.version_info[1])+") may not work.")

    main(sys.argv[1:])