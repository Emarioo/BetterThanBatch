


// #include "errno.h"
#include "libs/glad/include/glad/glad.h"
#include "libs/glfw-3.3.9/include/GLFW/glfw3.h"
// #define APIENTRYP __stdcall*
// #  define GLAPI extern
INT_FAST64_MAX
// typedef unsigned int GLenum;
// typedef unsigned char GLboolean;
// typedef unsigned int GLbitfield;
// typedef void GLvoid;
// typedef khronos_int8_t GLbyte;
// typedef khronos_uint8_t GLubyte;
// typedef khronos_int16_t GLshort;
// typedef khronos_uint16_t GLushort;
// typedef int GLint;
// typedef unsigned int GLuint;
// typedef khronos_int32_t GLclampx;
// typedef int GLsizei;
// typedef khronos_float_t GLfloat;
// typedef khronos_float_t GLclampf;
// typedef double GLdouble;
// typedef double GLclampd;
// typedef void *GLeglClientBufferEXT;
// typedef void *GLeglImageOES;
// typedef char GLchar;
// typedef char GLcharARB;
// #ifdef __APPLE__
// typedef void *GLhandleARB;
// #else
// typedef unsigned int GLhandleARB;
// #endif
// typedef khronos_uint16_t GLhalf;
// typedef khronos_uint16_t GLhalfARB;
// typedef khronos_int32_t GLfixed;
// typedef khronos_intptr_t GLintptr;
// typedef khronos_intptr_t GLintptrARB;
// typedef khronos_ssize_t GLsizeiptr;
// typedef khronos_ssize_t GLsizeiptrARB;
// typedef khronos_int64_t GLint64;
// typedef khronos_int64_t GLint64EXT;
// typedef khronos_uint64_t GLuint64;
// typedef khronos_uint64_t GLuint64EXT;
// typedef struct __GLsync *GLsync;
// struct _cl_context;
// struct _cl_event;

// typedef void (APIENTRYP PFNGLBINDVERTEXBUFFERPROC)(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
// GLAPI PFNGLBINDVERTEXBUFFERPROC glad_glBindVertexBuffer;
// // #define OSVERSION_MASK      0xFFFF0000

// typedef struct { int x; } *PVE;

// #define NTDDI_VERSION    0x0A00

// #define OSVER(Version)  ((Version) & OSVERSION_MASK)

// ((OSVER(NTDDI_VERSION) == NTDDI_WIN2K) && (_WIN32_WINNT != _WIN32_WINNT_WIN2K))

// #if ((OSVER(NTDDI_VERSION) == NTDDI_WIN2K) && (_WIN32_WINNT != _WIN32_WINNT_WIN2K))
// YES
// #endif

// #define HELLO

// #define MAC 1 + defined(HELLO) + 2


// MAC

// #if MAC == 4
// yes
// #endif

// #if defined(GLFW_INCLUDE_GLU)
// x
// #elif !defined(GLFW_INCLUDE_NONE) && \
//       !defined(__gl_h_) && \
//       !defined(__gles1_gl_h_) && \
//       !defined(__gles2_gl2_h_) && \
//       !defined(__gles2_gl3_h_) && \
//       !defined(__gles2_gl31_h_) && \
//       !defined(__gles2_gl32_h_) && \
//       !defined(__gl_glcorearb_h_) && \
//       !defined(__gl2_h_) /*legacy*/ && \
//       !defined(__gl3_h_) /*legacy*/ && \
//       !defined(__gl31_h_) /*legacy*/ && \
//       !defined(__gl32_h_) /*legacy*/ && \
//       !defined(__glcorearb_h_) /*legacy*/ && \
//       !defined(__GL_H__) /*non-standard*/ && \
//       !defined(__gltypes_h_) /*non-standard*/ && \
//       !defined(__glee_h_) /*non-standard*/
// eaedad
//       #endif
// #include "stdlib.h"
/* C/C++ specific language defines.  */
// #ifndef __GNUC__
// # ifndef __MINGW_IMPORT
// #  define __MINGW_IMPORT  __declspec(dllimport)
// # endif
// # ifndef _CRTIMP
// #  define _CRTIMP  __declspec(dllimport)
// # endif
// # define __DECLSPEC_SUPPORTED
// # define __attribute__(x) /* nothing */
// #else /* __GNUC__ */
// RTIGHT?
// # ifdef __declspec
// RTIGHT ES ESEAE?
// #  ifndef __MINGW_IMPORT
// /* Note the extern. This is needed to work around GCC's
// limitations in handling dllimport attribute.  */
// #   define __MINGW_IMPORT  extern __attribute__ ((__dllimport__))
// #  endif
// #  ifndef _CRTIMP
// #    undef __USE_CRTIMP
// #    if !defined (_CRTBLD) && !defined (_SYSCRT)
// #      define __USE_CRTIMP 1
// #    endif
// #    ifdef __USE_CRTIMP
// #      define _CRTIMP  __attribute__ ((__dllimport__))
// #    else
// #      define _CRTIMP
// #    endif
// #  endif
// #  define __DECLSPEC_SUPPORTED
// # else /* __declspec */
// #  undef __DECLSPEC_SUPPORTED
// #  undef __MINGW_IMPORT
// #  ifndef _CRTIMP
// #   define _CRTIMP
// #  endif
// # endif /* __declspec */
// #endif /* __GNUC__ */

// _CRTIMP BUFFERDU

// #define MAC 24 + 2

// #define STRING(X) "dad" + # X + "aea"
// #define CON(X,Y) # X ## Y

// // STRING(he\nllo)
// CON(b, c)

// #if MAC == 26

// #endif
// #  define __MINGW_IMP_SYMBOL(sym) _imp__##sym

// extern unsigned int * __MINGW_IMP_SYMBOL(_osplatform);
// #define _osplatform (* __MINGW_IMP_SYMBOL(_osplatform))
// #define __NO_ISOCEXT 23

// #if defined(__INTRIN_H_) || \
//    (defined(_X86INTRIN_H_INCLUDED) && \
//      ((__MINGW_GCC_VERSION >= 40902) || defined(__LP64__) || defined(_X86_)))
// yes
// #else
// no
// #endif

// #  define __MINGW_IMP_SYMBOL(sym) __imp_##sym

// extern int __mb_cur_max;
// #define __mb_cur_max	__mb_cur_max
// extern int * __MINGW_IMP_SYMBOL(__mb_cur_max);


// #if __MINGW_USE_UNDERSCORE_PREFIX == 0
// // #  define __MINGW_IMP_SYMBOL(sym) __imp_##sym
// // #  define __MINGW_IMP_LSYMBOL(sym) __imp_##sym
// // #  define __MINGW_USYMBOL(sym) sym
// #  define __MINGW_LSYMBOL(sym) _##sym
// #else /* ! if __MINGW_USE_UNDERSCORE_PREFIX == 0 */
// #  define __MINGW_IMP_SYMBOL(sym) _imp__##sym
// #  define __MINGW_IMP_LSYMBOL(sym) __imp__##sym
// #  define __MINGW_USYMBOL(sym) _##sym
// #  define __MINGW_LSYMBOL(sym) sym
// #endif /* if __MINGW_USE_UNDERSCORE_PREFIX == 0 */


// #if !defined(_UCRT) && ((__MSVCRT_VERSION__ >= 0x1400) || (__MSVCRT_VERSION__ >= 0xE00 && __MSVCRT_VERSION__ < 0x1000))
// /* Allow both 0x1400 and 0xE00 to identify UCRT */
// adad
// #endif


// #if defined (_WIN32) && !defined (_WIN64) && !defined (__MINGW_USE_VC2005_COMPAT) && !defined (_UCRT)
// #ifndef _USE_32BIT_TIME_T
// NO
// #endif
// #endif