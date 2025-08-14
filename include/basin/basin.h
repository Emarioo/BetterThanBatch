/*
    Public C API for the Basin compiler

        When linking with dynamic library of the compiler define BASIN_DLL before including headers.
*/

#pragma once

#if defined(_MSC_VER)
    #ifdef BASIN_DLL
        #define BASIN_API extern __declspec(dllimport)
    #elif define(BASIN_DLL_EXPORT)
        #define BASIN_API extern __declspec(dllexport)
    #else
        #define BASIN_API extern
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(BASIN_DLL) || defined(BASIN_DLL_EXPORT)
        #define BASIN_API extern __attribute__((visibility("default")))
    #else
        #define BASIN_API extern
    #endif
#else
    // TODO: Maybe default to __attribute__ visibility when using DLL.
    #error Possibly unsupported C/C++ compiler.
#endif


enum BasinError {
    BASIN_SUCCESS,
    BASIN_INVALID_OPTIONS,
    BASIN_COMPILE_ERROR,
    // TODO: BASIN_VM_ERROR,
};

enum BasinTarget {
    BASIN_TARGET_host,
    BASIN_TARGET_windows_x86_64,
    BASIN_TARGET_linux_x86_64,
    BASIN_TARGET_arm,     // baremetal
    BASIN_TARGET_aarch64, // baremetal
};

enum BasinBinaryType {
    BASIN_BINARY_executable,
    BASIN_BINARY_static_library,
    BASIN_BINARY_dynamic_library,
    BASIN_BINARY_object_file,
};

struct BasinResult {
    BasinError error_type;
    const char* error_message;

    const char* const* compile_errors;
    int compile_errors_len;
};

/*
    You can zero this structure for default behaviour (debug info, no optimizations, executable on host's architecture)
*/
struct BasinCompileOptions {
    BasinTarget target;
    BasinBinaryType binary_output_type;
    bool disable_debug; // yes debug is default
    bool optimize; /* does nothing, reserved for future */
    const char* const* include_dirs;
    int include_dirs_len;
};



#ifdef __cplusplus
extern "C" {
#endif

/*###########################
       CORE FUNCTIONS 
############################*/

/*
    Retrieves the version of the compiler.

    Parameters:
        version - Optional array where major, minor, and revision is written to.

    Returns:
        A string in the format "major.minor.revision-suffix".
        The suffix only exists on pre releases.
*/
BASIN_API const char* basin_version(int version[3]);

/*
    Compiles a source file into an object file.

    Parameters:
        path        - Path to input file.
        output:     - Path to ouput file.
        options:    - Optional structure for specifying debug info, optimizations, include directories,
                         ouput file format, architecture, etc. A zeroed struct is used if null is passed.
        result      - Optional structure to receive details about compilation such as errors.
    
    Returns:
        Success or kind of error.
*/
BASIN_API BasinError basin_compile_file(const char* path, const char* output, const BasinCompileOptions* options, BasinResult* result);


typedef struct {

} BasinContext;

typedef enum {
    BASIN_STEP_PARSE_AST = 0x1,
    BASIN_STEP_GEN_BYTECODE = 0x2,
    BASIN_STEP_GEN_MACHINE = 0x4,
} BasinContextSteps;

/*
    Gives you finer control over the compile process
        

    Returns:
        A compile context. Is null if error occured.
*/
BASIN_API BasinContext* basin_create_context(BasinResult* result);

BASIN_API BasinError basin_context_set_options(BasinContext* context, const BasinCompileOptions* options, BasinResult* result);

BASIN_API BasinError basin_context_run_steps(BasinContext* context, , BasinResult* result);


void main() {
    BasinContext* context = basin_create_context(NULL);

    BasinCompileOptions options = {};
    basin_context_set_options(context, &options, NULL);

    basin_context_run_steps(context);
}

#ifdef __cplusplus
} // extern "C"
#endif
