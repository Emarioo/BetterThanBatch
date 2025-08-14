
#include "basin/basin.h"

#include "basin/Config.h"

#include "basin/Compiler.h"

BASIN_API const char* basin_version(int version[3]) {
    if(version) {
        char *start, *end;
        int part;

        start = (char*)COMPILER_VERSION;
        part = strtol(start, &end, 10);
        version[0] = part;
        
        start = end + 1; // +1 to skip dot
        part = strtol(start, &end, 10);
        version[1] = part;

        start = end + 1;
        part = strtol(start, &end, 10);
        version[2] = part;
    }
    return COMPILER_VERSION;
}


BASIN_API BasinError basin_compile_file(const char* path, const char* output, const BasinCompileOptions* options, BasinResult* error_result) {
    BasinCompileOptions default_options = {};

    if(!options) {
        options = &default_options;
    }

    CompileOptions compile_options = {};
    compile_options.useDebugInformation = !options->disable_debug;
    // TODO: Add optimization in the compiler, options->optimize
    switch(options->target) {
        case BASIN_TARGET_host: {
            #ifdef OS_WINDOWS
                compile_options.target = TARGET_WINDOWS_x64;
            #elif OS_LINUX
                compile_options.target = TARGET_LINUX_x64;
            #else
                if(error_result) {
                    // TODO: Specify which value was specified.
                    error_result->error_type = BASIN_INVALID_OPTIONS;
                    error_result->error_message = "Host is not Windows or Linux, unsupported target.";
                }
                return BASIN_INVALID_OPTIONS;
            #endif
        } break;
        case BASIN_TARGET_windows_x86_64: compile_options.target = TARGET_WINDOWS_x64; break;
        case BASIN_TARGET_linux_x86_64:   compile_options.target = TARGET_LINUX_x64; break;
        case BASIN_TARGET_arm:            compile_options.target = TARGET_ARM; break;
        case BASIN_TARGET_aarch64:        compile_options.target = TARGET_AARCH64; break;
        default: {
            if(error_result) {
                // TODO: Specify which value was specified.
                error_result->error_type = BASIN_INVALID_OPTIONS;
                error_result->error_message = "Invalid target.";
            }
            return BASIN_INVALID_OPTIONS;
        }
    }

    if(options->include_dirs_len && options->include_dirs) {
        for(int i=0;i<options->include_dirs_len;i++) {
            compile_options.importDirectories.add(options->include_dirs[i]);
        }
    }

    // TODO: Handle binary file type

    compile_options.source_file = path;
    compile_options.output_file = output;
    

    Compiler compiler{};
    compiler.run(&compile_options);

    if(compiler.compile_stats.errors) {
        if(error_result) {
            error_result->error_type = BASIN_COMPILE_ERROR;
            error_result->error_message = "Compile error.";

            auto& msgs = compiler.reporter.error_messages;
            
            int size = 0;
            for(int i=0;i<msgs.size();i++) {
                size += msgs.size() + 1;
            }
            
            // TODO: MEMORY LEAK.
            void* raw = (char*)malloc(size);
            char** list = (char**)raw;
            char* data = (char*)raw + msgs.size() * 8;
            
            int offset = 0;
            for(int i=0;i<msgs.size();i++) {
                auto& msg = msgs[i];
                auto& ptr = list[i];
                ptr = data + offset;
                memcpy(ptr, msg.c_str(), msgs.size());
                ptr[msg.size()] = '\0';
                offset += msgs.size() + 1;
            }
            
            error_result->compile_errors = list;
            error_result->compile_errors_len = msgs.size();
        }
        return BASIN_COMPILE_ERROR;
    }

    if(error_result) {
        *error_result = {};
    }
    return BASIN_SUCCESS;
}
