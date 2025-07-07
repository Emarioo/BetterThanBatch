


#include "libs/glfw-3.3.9/include/GLFW/glfw3.h"
// #include "errno.h"
// #include "libs/glad/include/glad/glad.h"
// __attribute__((dllimport)) void __attribute__((__stdcall__)) glAccum(GLenum op,GLfloat value);


// typedef struct threadlocaleinfostruct {
//     int x;
// } hellos;

// #define OSVERSION_MASK      0xFFFF0000

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