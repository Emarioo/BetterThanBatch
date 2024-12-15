#include "BetBat/IntrinsicRegistry.h"

const char* ToString(IntrinsicType type) {
    switch(type){
    case INTRINSIC_PRINTS:          return "prints";
    case INTRINSIC_PRINTC:          return "printc";
    case INTRINSIC_MEMZERO:         return "memzero";
    case INTRINSIC_MEMCPY:          return "memcpy";
    case INTRINSIC_STRLEN:          return "strlen";
    case INTRINSIC_RDTSC:           return "rdtsc";
    case INTRINSIC_ATOMIC_CMP_SWAP: return "atomic_cmp_swap";
    case INTRINSIC_ATOMIC_ADD:      return "atomic_add";
    case INTRINSIC_SQRT:            return "sqrt";
    case INTRINSIC_ROUND:           return "round";
    default: Assert(false);
    }
    #undef CASE
    return "unknown";
}
IntrinsicType ToIntrinsic(const std::string& name) {
    #define CASE(N,T) if(name == N) return T;
         CASE("prints",          INTRINSIC_PRINTS)                            
    else CASE("printc",          INTRINSIC_PRINTC)
    else CASE("memzero",         INTRINSIC_MEMZERO)
    else CASE("memcpy",          INTRINSIC_MEMCPY)
    else CASE("strlen",          INTRINSIC_STRLEN)
    else CASE("rdtsc",           INTRINSIC_RDTSC)
    else CASE("atomic_cmp_swap", INTRINSIC_ATOMIC_CMP_SWAP)
    else CASE("atomic_add",      INTRINSIC_ATOMIC_ADD)
    else CASE("sqrt",            INTRINSIC_SQRT)
    else CASE("round",           INTRINSIC_ROUND)
    return INTRINSIC_INVALID;
    #undef CASE
}
