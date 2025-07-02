

// __declspec(WAAA)

Def this dude? __cdecl
Def this dude? __stdcall
Def this dude? __fadadstcall

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