/*
    Debug information for windows

    The code is based on cvdump
    https://github.com/microsoft/microsoft-pdb

    https://llvm.org/docs/PDB/MsfFile.html

    All code in the header and cpp file is very messy.
    I need to successfully create a PDB before designing a 
    system around it.
*/
#pragma once

#include "BetBat/DebugInformation.h"

struct TypeInformation {
    DynamicArray<u32> functionTypeIndices{};
    // u32 PROJECT_FOLDER_id;
    // u32 COMPILER_id;
    // u32 SOURCE_id;
    // u32 PDB_id;
    // u32 BUILDINFO_id;
    // u32 emptyArgList;
    // u32 bareProcedure;

    u32 age = 0;
    u8 guid[16]{0};
};
bool WritePDBFile(const std::string& path, DebugInformation* debugInformation, TypeInformation* inOutTypeInformation);

enum DebugSignature : u32 {
    CV_SIGNATURE_C6       = 0L,  // Actual signature is >64K
    CV_SIGNATURE_C7       = 1L,  // First explicit signature
    CV_SIGNATURE_C11      = 2L,  // C11 (vc5.x) 32-bit types
    CV_SIGNATURE_C13      = 4L,  // C13 (vc7.x) zero terminated names
    CV_SIGNATURE_RESERVED = 5L,  // All signatures from 5 to 64K are reserved
};

enum SubSectionType : u32 {
    DEBUG_S_IGNORE = 0x80000000,    // if this bit is set in a subsection type then ignore the subsection contents

    DEBUG_S_SYMBOLS = 0xf1,
    DEBUG_S_LINES,
    DEBUG_S_STRINGTABLE,
    DEBUG_S_FILECHKSMS,
    DEBUG_S_FRAMEDATA,
    DEBUG_S_INLINEELINES,
    DEBUG_S_CROSSSCOPEIMPORTS,
    DEBUG_S_CROSSSCOPEEXPORTS,

    DEBUG_S_IL_LINES,
    DEBUG_S_FUNC_MDTOKEN_MAP,
    DEBUG_S_TYPE_MDTOKEN_MAP,
    DEBUG_S_MERGED_ASSEMBLYINPUT,

    DEBUG_S_COFF_SYMBOL_RVA,
};
struct PDBHeader {
    u32 impv;
    u32 sig;
    u32 age;
    
    u8 guid[16];
};
struct TPIHeader {
  u32 Version;
  u32 HeaderSize;
  u32 TypeIndexBegin;
  u32 TypeIndexEnd;
  u32 TypeRecordBytes;

  u16 HashStreamIndex;
  u16 HashAuxStreamIndex; // should be -1
  u32 HashKeySize;
  u32 NumHashBuckets; // must be between MIN and MAX_BUCKETS
  static const int MIN_BUCKETS = 0x1000; 
  static const int MAX_BUCKETS = 0x3FFFF;
  

  i32 HashValueBufferOffset;
  u32 HashValueBufferLength;

  i32 IndexOffsetBufferOffset;
  u32 IndexOffsetBufferLength;

  i32 HashAdjBufferOffset;
  u32 HashAdjBufferLength;
};
struct DBIStreamHeader   {
    i32 VersionSignature;
    u32 VersionHeader;
    u32 Age;
    u16 GlobalStreamIndex;
    u16 BuildNumber;
    u16 PublicStreamIndex;
    u16 PdbDllVersion;
    u16 SymRecordStream;
    u16 PdbDllRbld;
    i32 ModInfoSize;
    i32 SectionContributionSize;
    i32 SectionMapSize;
    i32 SourceInfoSize;
    i32 TypeServerMapSize;
    u32 MFCTypeServerIndex;
    i32 OptionalDbgHeaderSize;
    i32 ECSubstreamSize;
    u16 Flags;
    u16 Machine;
    u32 Padding;
};

enum CPUMachine {
    CV_CFL_8080         = 0x00,
    CV_CFL_8086         = 0x01,
    CV_CFL_80286        = 0x02,
    CV_CFL_80386        = 0x03,
    CV_CFL_80486        = 0x04,
    CV_CFL_PENTIUM      = 0x05,
    CV_CFL_PENTIUMII    = 0x06,
    CV_CFL_PENTIUMPRO   = CV_CFL_PENTIUMII,
    CV_CFL_PENTIUMIII   = 0x07,
    CV_CFL_MIPS         = 0x10,
    CV_CFL_MIPSR4000    = CV_CFL_MIPS,  // don't break current code
    CV_CFL_MIPS16       = 0x11,
    CV_CFL_MIPS32       = 0x12,
    CV_CFL_MIPS64       = 0x13,
    CV_CFL_MIPSI        = 0x14,
    CV_CFL_MIPSII       = 0x15,
    CV_CFL_MIPSIII      = 0x16,
    CV_CFL_MIPSIV       = 0x17,
    CV_CFL_MIPSV        = 0x18,
    CV_CFL_M68000       = 0x20,
    CV_CFL_M68010       = 0x21,
    CV_CFL_M68020       = 0x22,
    CV_CFL_M68030       = 0x23,
    CV_CFL_M68040       = 0x24,
    CV_CFL_ALPHA        = 0x30,
    CV_CFL_ALPHA_21064  = 0x30,
    CV_CFL_ALPHA_21164  = 0x31,
    CV_CFL_ALPHA_21164A = 0x32,
    CV_CFL_ALPHA_21264  = 0x33,
    CV_CFL_ALPHA_21364  = 0x34,
    CV_CFL_PPC601       = 0x40,
    CV_CFL_PPC603       = 0x41,
    CV_CFL_PPC604       = 0x42,
    CV_CFL_PPC620       = 0x43,
    CV_CFL_PPCFP        = 0x44,
    CV_CFL_PPCBE        = 0x45,
    CV_CFL_SH3          = 0x50,
    CV_CFL_SH3E         = 0x51,
    CV_CFL_SH3DSP       = 0x52,
    CV_CFL_SH4          = 0x53,
    CV_CFL_SHMEDIA      = 0x54,
    CV_CFL_ARM3         = 0x60,
    CV_CFL_ARM4         = 0x61,
    CV_CFL_ARM4T        = 0x62,
    CV_CFL_ARM5         = 0x63,
    CV_CFL_ARM5T        = 0x64,
    CV_CFL_ARM6         = 0x65,
    CV_CFL_ARM_XMAC     = 0x66,
    CV_CFL_ARM_WMMX     = 0x67,
    CV_CFL_ARM7         = 0x68,
    CV_CFL_OMNI         = 0x70,
    CV_CFL_IA64         = 0x80,
    CV_CFL_IA64_1       = 0x80,
    CV_CFL_IA64_2       = 0x81,
    CV_CFL_CEE          = 0x90,
    CV_CFL_AM33         = 0xA0,
    CV_CFL_M32R         = 0xB0,
    CV_CFL_TRICORE      = 0xC0,
    CV_CFL_X64          = 0xD0,
    CV_CFL_AMD64        = CV_CFL_X64,
    CV_CFL_EBC          = 0xE0,
    CV_CFL_THUMB        = 0xF0,
    CV_CFL_ARMNT        = 0xF4,
    CV_CFL_ARM64        = 0xF6,
    CV_CFL_D3D11_SHADER = 0x100,
};
struct Record_COMPILE3 {
    struct {
        unsigned long   iLanguage       :  8;   // language index
        unsigned long   fEC             :  1;   // compiled for E/C
        unsigned long   fNoDbgInfo      :  1;   // not compiled with debug info
        unsigned long   fLTCG           :  1;   // compiled with LTCG
        unsigned long   fNoDataAlign    :  1;   // compiled with -Bzalign
        unsigned long   fManagedPresent :  1;   // managed code/data present
        unsigned long   fSecurityChecks :  1;   // compiled with /GS
        unsigned long   fHotPatch       :  1;   // compiled with /hotpatch
        unsigned long   fCVTCIL         :  1;   // converted with CVTCIL
        unsigned long   fMSILModule     :  1;   // MSIL netmodule
        unsigned long   fSdl            :  1;   // compiled with /sdl
        unsigned long   fPGO            :  1;   // compiled with /ltcg:pgo or pgu
        unsigned long   fExp            :  1;   // .exp module
        unsigned long   pad             : 12;   // reserved, must be 0
    } flags;
    unsigned short  machine;    // target processor
    unsigned short  verFEMajor; // front end major version #
    unsigned short  verFEMinor; // front end minor version #
    unsigned short  verFEBuild; // front end build version #
    unsigned short  verFEQFE;   // front end QFE version #
    unsigned short  verMajor;   // back end major version #
    unsigned short  verMinor;   // back end minor version #
    unsigned short  verBuild;   // back end build version #
    unsigned short  verQFE;     // back end QFE version #
    // DON'T FORGET ZERO TERMINATED STRING! compiler version string
};
enum class DbiStreamVersion : u32 {
  VC41 = 930803,
  V50 = 19960307,
  V60 = 19970606,
  V70 = 19990903,
  V110 = 20091201
};

enum class TPIStreamVersion : u32 {
  V40 = 19950410,
  V41 = 19951122,
  V50 = 19961031,
  V70 = 19990903,
  V80 = 20040203,
};

struct CV_Line_t {
    u32 offset;             // Offset to start of code bytes for line number
    u32 linenumStart:24;    // line where statement/expression starts
    u32 deltaLineEnd:7;     // delta to line where statement ends (optional)
    u32 fStatement:1;       // true if a statement linenumber, else an expression line num
};
struct CV_Column_t {
    u16 offColumnStart;
    u16 offColumnEnd;
};

enum CV_SourceChksum_t {
    CHKSUM_TYPE_NONE = 0,        // indicates no checksum is available
    CHKSUM_TYPE_MD5,
    CHKSUM_TYPE_SHA1,
    CHKSUM_TYPE_SHA_256,
};
struct FRAMEPROCSYM {
    u32 cbFrame;    // count of bytes of total frame of procedure
    u32 cbPad;      // count of bytes of padding in the frame
    u32 offPad;     // offset (relative to frame poniter) to where
                                //  padding starts
    u32   cbSaveRegs; // count of bytes of callee save registers
    u32     offExHdlr;  // offset of exception handler
    u16  sectExHdlr; // section id of exception handler

    struct {
        u32 fHasAlloca  :  1;   // function uses _alloca()
        u32 fHasSetJmp  :  1;   // function uses setjmp()
        u32 fHasLongJmp :  1;   // function uses longjmp()
        u32 fHasInlAsm  :  1;   // function uses inline asm
        u32 fHasEH      :  1;   // function has EH states
        u32 fInlSpec    :  1;   // function was speced as inline
        u32 fHasSEH     :  1;   // function has SEH
        u32 fNaked      :  1;   // function is __declspec(naked)
        u32 fSecurityChecks :  1;   // function has buffer security check introduced by /GS.
        u32 fAsyncEH    :  1;   // function compiled with /EHa
        u32 fGSNoStackOrdering :  1;   // function has /GS buffer checks, but stack ordering couldn't be done
        u32 fWasInlined :  1;   // function was inlined within another function
        u32 fGSCheck    :  1;   // function is __declspec(strict_gs_check)
        u32 fSafeBuffers : 1;   // function is __declspec(safebuffers)
        u32 encodedLocalBasePointer : 2;  // record function's local pointer explicitly.
        u32 encodedParamBasePointer : 2;  // record function's parameter pointer explicitly.
        u32 fPogoOn      : 1;   // function was compiled with PGO/PGU
        u32 fValidCounts : 1;   // Do we have valid Pogo counts?
        u32 fOptSpeed    : 1;  // Did we optimize for speed?
        u32 fGuardCF    :  1;   // function contains CFG checks (and no write checks)
        u32 fGuardCFW   :  1;   // function contains CFW checks and/or instrumentation
        u32 pad          : 9;   // must be zero
    } flags;
};

// These describe the format of the symbol records
enum RecordType : u16 {
    S_COMPILE       =  0x0001,  // Compile flags symbol
    S_REGISTER_16t  =  0x0002,  // Register variable
    S_CONSTANT_16t  =  0x0003,  // constant symbol
    S_UDT_16t       =  0x0004,  // User defined type
    S_SSEARCH       =  0x0005,  // Start Search
    S_END           =  0x0006,  // Block, procedure, "with" or thunk end
    S_SKIP          =  0x0007,  // Reserve symbol space in $$Symbols table
    S_CVRESERVE     =  0x0008,  // Reserved symbol for CV internal use
    S_OBJNAME_ST    =  0x0009,  // path to object file name
    S_ENDARG        =  0x000a,  // end of argument/return list
    S_COBOLUDT_16t  =  0x000b,  // special UDT for cobol that does not symbol pack
    S_MANYREG_16t   =  0x000c,  // multiple register variable
    S_RETURN        =  0x000d,  // return description symbol
    S_ENTRYTHIS     =  0x000e,  // description of this pointer on entry

    S_BPREL16       =  0x0100,  // BP-relative
    S_LDATA16       =  0x0101,  // Module-local symbol
    S_GDATA16       =  0x0102,  // Global data symbol
    S_PUB16         =  0x0103,  // a public symbol
    S_LPROC16       =  0x0104,  // Local procedure start
    S_GPROC16       =  0x0105,  // Global procedure start
    S_THUNK16       =  0x0106,  // Thunk Start
    S_BLOCK16       =  0x0107,  // block start
    S_WITH16        =  0x0108,  // with start
    S_LABEL16       =  0x0109,  // code label
    S_CEXMODEL16    =  0x010a,  // change execution model
    S_VFTABLE16     =  0x010b,  // address of virtual function table
    S_REGREL16      =  0x010c,  // register relative address

    S_BPREL32_16t   =  0x0200,  // BP-relative
    S_LDATA32_16t   =  0x0201,  // Module-local symbol
    S_GDATA32_16t   =  0x0202,  // Global data symbol
    S_PUB32_16t     =  0x0203,  // a public symbol (CV internal reserved)
    S_LPROC32_16t   =  0x0204,  // Local procedure start
    S_GPROC32_16t   =  0x0205,  // Global procedure start
    S_THUNK32_ST    =  0x0206,  // Thunk Start
    S_BLOCK32_ST    =  0x0207,  // block start
    S_WITH32_ST     =  0x0208,  // with start
    S_LABEL32_ST    =  0x0209,  // code label
    S_CEXMODEL32    =  0x020a,  // change execution model
    S_VFTABLE32_16t =  0x020b,  // address of virtual function table
    S_REGREL32_16t  =  0x020c,  // register relative address
    S_LTHREAD32_16t =  0x020d,  // local thread storage
    S_GTHREAD32_16t =  0x020e,  // global thread storage
    S_SLINK32       =  0x020f,  // static link for MIPS EH implementation

    S_LPROCMIPS_16t =  0x0300,  // Local procedure start
    S_GPROCMIPS_16t =  0x0301,  // Global procedure start

    // if these ref symbols have names following then the names are in ST format
    S_PROCREF_ST    =  0x0400,  // Reference to a procedure
    S_DATAREF_ST    =  0x0401,  // Reference to data
    S_ALIGN         =  0x0402,  // Used for page alignment of symbols

    S_LPROCREF_ST   =  0x0403,  // Local Reference to a procedure
    S_OEM           =  0x0404,  // OEM defined symbol

    // sym records with 32-bit types embedded instead of 16-bit
    // all have 0x1000 bit set for easy identification
    // only do the 32-bit target versions since we don't really
    // care about 16-bit ones anymore.
    S_TI16_MAX          =  0x1000,

    S_REGISTER_ST   =  0x1001,  // Register variable
    S_CONSTANT_ST   =  0x1002,  // constant symbol
    S_UDT_ST        =  0x1003,  // User defined type
    S_COBOLUDT_ST   =  0x1004,  // special UDT for cobol that does not symbol pack
    S_MANYREG_ST    =  0x1005,  // multiple register variable
    S_BPREL32_ST    =  0x1006,  // BP-relative
    S_LDATA32_ST    =  0x1007,  // Module-local symbol
    S_GDATA32_ST    =  0x1008,  // Global data symbol
    S_PUB32_ST      =  0x1009,  // a public symbol (CV internal reserved)
    S_LPROC32_ST    =  0x100a,  // Local procedure start
    S_GPROC32_ST    =  0x100b,  // Global procedure start
    S_VFTABLE32     =  0x100c,  // address of virtual function table
    S_REGREL32_ST   =  0x100d,  // register relative address
    S_LTHREAD32_ST  =  0x100e,  // local thread storage
    S_GTHREAD32_ST  =  0x100f,  // global thread storage

    S_LPROCMIPS_ST  =  0x1010,  // Local procedure start
    S_GPROCMIPS_ST  =  0x1011,  // Global procedure start

    S_FRAMEPROC     =  0x1012,  // extra frame and proc information
    S_COMPILE2_ST   =  0x1013,  // extended compile flags and info

    // new symbols necessary for 16-bit enumerates of IA64 registers
    // and IA64 specific symbols

    S_MANYREG2_ST   =  0x1014,  // multiple register variable
    S_LPROCIA64_ST  =  0x1015,  // Local procedure start (IA64)
    S_GPROCIA64_ST  =  0x1016,  // Global procedure start (IA64)

    // Local symbols for IL
    S_LOCALSLOT_ST  =  0x1017,  // local IL sym with field for local slot index
    S_PARAMSLOT_ST  =  0x1018,  // local IL sym with field for parameter slot index

    S_ANNOTATION    =  0x1019,  // Annotation string literals

    // symbols to support managed code debugging
    S_GMANPROC_ST   =  0x101a,  // Global proc
    S_LMANPROC_ST   =  0x101b,  // Local proc
    S_RESERVED1     =  0x101c,  // reserved
    S_RESERVED2     =  0x101d,  // reserved
    S_RESERVED3     =  0x101e,  // reserved
    S_RESERVED4     =  0x101f,  // reserved
    S_LMANDATA_ST   =  0x1020,
    S_GMANDATA_ST   =  0x1021,
    S_MANFRAMEREL_ST=  0x1022,
    S_MANREGISTER_ST=  0x1023,
    S_MANSLOT_ST    =  0x1024,
    S_MANMANYREG_ST =  0x1025,
    S_MANREGREL_ST  =  0x1026,
    S_MANMANYREG2_ST=  0x1027,
    S_MANTYPREF     =  0x1028,  // Index for type referenced by name from metadata
    S_UNAMESPACE_ST =  0x1029,  // Using namespace

    // Symbols w/ SZ name fields. All name fields contain utf8 encoded strings.
    S_ST_MAX        =  0x1100,  // starting point for SZ name symbols

    S_OBJNAME       =  0x1101,  // path to object file name
    S_THUNK32       =  0x1102,  // Thunk Start
    S_BLOCK32       =  0x1103,  // block start
    S_WITH32        =  0x1104,  // with start
    S_LABEL32       =  0x1105,  // code label
    S_REGISTER      =  0x1106,  // Register variable
    S_CONSTANT      =  0x1107,  // constant symbol
    S_UDT           =  0x1108,  // User defined type
    S_COBOLUDT      =  0x1109,  // special UDT for cobol that does not symbol pack
    S_MANYREG       =  0x110a,  // multiple register variable
    S_BPREL32       =  0x110b,  // BP-relative
    S_LDATA32       =  0x110c,  // Module-local symbol
    S_GDATA32       =  0x110d,  // Global data symbol
    S_PUB32         =  0x110e,  // a public symbol (CV internal reserved)
    S_LPROC32       =  0x110f,  // Local procedure start
    S_GPROC32       =  0x1110,  // Global procedure start
    S_REGREL32      =  0x1111,  // register relative address
    S_LTHREAD32     =  0x1112,  // local thread storage
    S_GTHREAD32     =  0x1113,  // global thread storage

    S_LPROCMIPS     =  0x1114,  // Local procedure start
    S_GPROCMIPS     =  0x1115,  // Global procedure start
    S_COMPILE2      =  0x1116,  // extended compile flags and info
    S_MANYREG2      =  0x1117,  // multiple register variable
    S_LPROCIA64     =  0x1118,  // Local procedure start (IA64)
    S_GPROCIA64     =  0x1119,  // Global procedure start (IA64)
    S_LOCALSLOT     =  0x111a,  // local IL sym with field for local slot index
    S_SLOT          = S_LOCALSLOT,  // alias for LOCALSLOT
    S_PARAMSLOT     =  0x111b,  // local IL sym with field for parameter slot index

    // symbols to support managed code debugging
    S_LMANDATA      =  0x111c,
    S_GMANDATA      =  0x111d,
    S_MANFRAMEREL   =  0x111e,
    S_MANREGISTER   =  0x111f,
    S_MANSLOT       =  0x1120,
    S_MANMANYREG    =  0x1121,
    S_MANREGREL     =  0x1122,
    S_MANMANYREG2   =  0x1123,
    S_UNAMESPACE    =  0x1124,  // Using namespace

    // ref symbols with name fields
    S_PROCREF       =  0x1125,  // Reference to a procedure
    S_DATAREF       =  0x1126,  // Reference to data
    S_LPROCREF      =  0x1127,  // Local Reference to a procedure
    S_ANNOTATIONREF =  0x1128,  // Reference to an S_ANNOTATION symbol
    S_TOKENREF      =  0x1129,  // Reference to one of the many MANPROCSYM's

    // continuation of managed symbols
    S_GMANPROC      =  0x112a,  // Global proc
    S_LMANPROC      =  0x112b,  // Local proc

    // short, light-weight thunks
    S_TRAMPOLINE    =  0x112c,  // trampoline thunks
    S_MANCONSTANT   =  0x112d,  // constants with metadata type info

    // native attributed local/parms
    S_ATTR_FRAMEREL =  0x112e,  // relative to virtual frame ptr
    S_ATTR_REGISTER =  0x112f,  // stored in a register
    S_ATTR_REGREL   =  0x1130,  // relative to register (alternate frame ptr)
    S_ATTR_MANYREG  =  0x1131,  // stored in >1 register

    // Separated code (from the compiler) support
    S_SEPCODE       =  0x1132,

    S_LOCAL_2005    =  0x1133,  // defines a local symbol in optimized code
    S_DEFRANGE_2005 =  0x1134,  // defines a single range of addresses in which symbol can be evaluated
    S_DEFRANGE2_2005 =  0x1135,  // defines ranges of addresses in which symbol can be evaluated

    S_SECTION       =  0x1136,  // A COFF section in a PE executable
    S_COFFGROUP     =  0x1137,  // A COFF group
    S_EXPORT        =  0x1138,  // A export

    S_CALLSITEINFO  =  0x1139,  // Indirect call site information
    S_FRAMECOOKIE   =  0x113a,  // Security cookie information

    S_DISCARDED     =  0x113b,  // Discarded by LINK /OPT:REF (experimental, see richards)

    S_COMPILE3      =  0x113c,  // Replacement for S_COMPILE2
    S_ENVBLOCK      =  0x113d,  // Environment block split off from S_COMPILE2

    S_LOCAL         =  0x113e,  // defines a local symbol in optimized code
    S_DEFRANGE      =  0x113f,  // defines a single range of addresses in which symbol can be evaluated
    S_DEFRANGE_SUBFIELD =  0x1140,           // ranges for a subfield

    S_DEFRANGE_REGISTER =  0x1141,           // ranges for en-registered symbol
    S_DEFRANGE_FRAMEPOINTER_REL =  0x1142,   // range for stack symbol.
    S_DEFRANGE_SUBFIELD_REGISTER =  0x1143,  // ranges for en-registered field of symbol
    S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE =  0x1144, // range for stack symbol span valid full scope of function body, gap might apply.
    S_DEFRANGE_REGISTER_REL =  0x1145, // range for symbol address as register + offset.

    // S_PROC symbols that reference ID instead of type
    S_LPROC32_ID     =  0x1146,
    S_GPROC32_ID     =  0x1147,
    S_LPROCMIPS_ID   =  0x1148,
    S_GPROCMIPS_ID   =  0x1149,
    S_LPROCIA64_ID   =  0x114a,
    S_GPROCIA64_ID   =  0x114b,

    S_BUILDINFO      = 0x114c, // build information.
    S_INLINESITE     = 0x114d, // inlined function callsite.
    S_INLINESITE_END = 0x114e,
    S_PROC_ID_END    = 0x114f,

    S_DEFRANGE_HLSL  = 0x1150,
    S_GDATA_HLSL     = 0x1151,
    S_LDATA_HLSL     = 0x1152,

    S_FILESTATIC     = 0x1153,

#if defined(CC_DP_CXX) && CC_DP_CXX

    S_LOCAL_DPC_GROUPSHARED = 0x1154, // DPC groupshared variable
    S_LPROC32_DPC = 0x1155, // DPC local procedure start
    S_LPROC32_DPC_ID =  0x1156,
    S_DEFRANGE_DPC_PTR_TAG =  0x1157, // DPC pointer tag definition range
    S_DPC_SYM_TAG_MAP = 0x1158, // DPC pointer tag value to symbol record map

#endif // CC_DP_CXX
    
    S_ARMSWITCHTABLE  = 0x1159,
    S_CALLEES = 0x115a,
    S_CALLERS = 0x115b,
    S_POGODATA = 0x115c,
    S_INLINESITE2 = 0x115d,      // extended inline site information

    S_HEAPALLOCSITE = 0x115e,    // heap allocation site

    S_MOD_TYPEREF = 0x115f,      // only generated at link time

    S_REF_MINIPDB = 0x1160,      // only generated at link time for mini PDB
    S_PDBMAP      = 0x1161,      // only generated at link time for mini PDB

    S_GDATA_HLSL32 = 0x1162,
    S_LDATA_HLSL32 = 0x1163,

    S_GDATA_HLSL32_EX = 0x1164,
    S_LDATA_HLSL32_EX = 0x1165,

    S_RECTYPE_MAX,               // one greater than last
    S_RECTYPE_LAST  = S_RECTYPE_MAX - 1,
    S_RECTYPE_PAD   = S_RECTYPE_MAX + 0x100 // Used *only* to verify symbol record types so that current PDB code can potentially read
                                // future PDBs (assuming no format change, etc).
};


enum LeafType : u16 {
    // leaf indices starting records but referenced from symbol records

    LF_MODIFIER_16t     = 0x0001,
    LF_POINTER_16t      = 0x0002,
    LF_ARRAY_16t        = 0x0003,
    LF_CLASS_16t        = 0x0004,
    LF_STRUCTURE_16t    = 0x0005,
    LF_UNION_16t        = 0x0006,
    LF_ENUM_16t         = 0x0007,
    LF_PROCEDURE_16t    = 0x0008,
    LF_MFUNCTION_16t    = 0x0009,
    LF_VTSHAPE          = 0x000a,
    LF_COBOL0_16t       = 0x000b,
    LF_COBOL1           = 0x000c,
    LF_BARRAY_16t       = 0x000d,
    LF_LABEL            = 0x000e,
    LF_NULL             = 0x000f,
    LF_NOTTRAN          = 0x0010,
    LF_DIMARRAY_16t     = 0x0011,
    LF_VFTPATH_16t      = 0x0012,
    LF_PRECOMP_16t      = 0x0013,       // not referenced from symbol
    LF_ENDPRECOMP       = 0x0014,       // not referenced from symbol
    LF_OEM_16t          = 0x0015,       // oem definable type string
    LF_TYPESERVER_ST    = 0x0016,       // not referenced from symbol

    // leaf indices starting records but referenced only from type records

    LF_SKIP_16t         = 0x0200,
    LF_ARGLIST_16t      = 0x0201,
    LF_DEFARG_16t       = 0x0202,
    LF_LIST             = 0x0203,
    LF_FIELDLIST_16t    = 0x0204,
    LF_DERIVED_16t      = 0x0205,
    LF_BITFIELD_16t     = 0x0206,
    LF_METHODLIST_16t   = 0x0207,
    LF_DIMCONU_16t      = 0x0208,
    LF_DIMCONLU_16t     = 0x0209,
    LF_DIMVARU_16t      = 0x020a,
    LF_DIMVARLU_16t     = 0x020b,
    LF_REFSYM           = 0x020c,

    LF_BCLASS_16t       = 0x0400,
    LF_VBCLASS_16t      = 0x0401,
    LF_IVBCLASS_16t     = 0x0402,
    LF_ENUMERATE_ST     = 0x0403,
    LF_FRIENDFCN_16t    = 0x0404,
    LF_INDEX_16t        = 0x0405,
    LF_MEMBER_16t       = 0x0406,
    LF_STMEMBER_16t     = 0x0407,
    LF_METHOD_16t       = 0x0408,
    LF_NESTTYPE_16t     = 0x0409,
    LF_VFUNCTAB_16t     = 0x040a,
    LF_FRIENDCLS_16t    = 0x040b,
    LF_ONEMETHOD_16t    = 0x040c,
    LF_VFUNCOFF_16t     = 0x040d,

// 32-bit type index versions of leaves, all have the 0x1000 bit set
//
    LF_TI16_MAX         = 0x1000,

    LF_MODIFIER         = 0x1001,
    LF_POINTER          = 0x1002,
    LF_ARRAY_ST         = 0x1003,
    LF_CLASS_ST         = 0x1004,
    LF_STRUCTURE_ST     = 0x1005,
    LF_UNION_ST         = 0x1006,
    LF_ENUM_ST          = 0x1007,
    LF_PROCEDURE        = 0x1008,
    LF_MFUNCTION        = 0x1009,
    LF_COBOL0           = 0x100a,
    LF_BARRAY           = 0x100b,
    LF_DIMARRAY_ST      = 0x100c,
    LF_VFTPATH          = 0x100d,
    LF_PRECOMP_ST       = 0x100e,       // not referenced from symbol
    LF_OEM              = 0x100f,       // oem definable type string
    LF_ALIAS_ST         = 0x1010,       // alias (typedef) type
    LF_OEM2             = 0x1011,       // oem definable type string

    // leaf indices starting records but referenced only from type records

    LF_SKIP             = 0x1200,
    LF_ARGLIST          = 0x1201,
    LF_DEFARG_ST        = 0x1202,
    LF_FIELDLIST        = 0x1203,
    LF_DERIVED          = 0x1204,
    LF_BITFIELD         = 0x1205,
    LF_METHODLIST       = 0x1206,
    LF_DIMCONU          = 0x1207,
    LF_DIMCONLU         = 0x1208,
    LF_DIMVARU          = 0x1209,
    LF_DIMVARLU         = 0x120a,

    LF_BCLASS           = 0x1400,
    LF_VBCLASS          = 0x1401,
    LF_IVBCLASS         = 0x1402,
    LF_FRIENDFCN_ST     = 0x1403,
    LF_INDEX            = 0x1404,
    LF_MEMBER_ST        = 0x1405,
    LF_STMEMBER_ST      = 0x1406,
    LF_METHOD_ST        = 0x1407,
    LF_NESTTYPE_ST      = 0x1408,
    LF_VFUNCTAB         = 0x1409,
    LF_FRIENDCLS        = 0x140a,
    LF_ONEMETHOD_ST     = 0x140b,
    LF_VFUNCOFF         = 0x140c,
    LF_NESTTYPEEX_ST    = 0x140d,
    LF_MEMBERMODIFY_ST  = 0x140e,
    LF_MANAGED_ST       = 0x140f,

    // Types w/ SZ names

    LF_ST_MAX           = 0x1500,

    LF_TYPESERVER       = 0x1501,       // not referenced from symbol
    LF_ENUMERATE        = 0x1502,
    LF_ARRAY            = 0x1503,
    LF_CLASS            = 0x1504,
    LF_STRUCTURE        = 0x1505,
    LF_UNION            = 0x1506,
    LF_ENUM             = 0x1507,
    LF_DIMARRAY         = 0x1508,
    LF_PRECOMP          = 0x1509,       // not referenced from symbol
    LF_ALIAS            = 0x150a,       // alias (typedef) type
    LF_DEFARG           = 0x150b,
    LF_FRIENDFCN        = 0x150c,
    LF_MEMBER           = 0x150d,
    LF_STMEMBER         = 0x150e,
    LF_METHOD           = 0x150f,
    LF_NESTTYPE         = 0x1510,
    LF_ONEMETHOD        = 0x1511,
    LF_NESTTYPEEX       = 0x1512,
    LF_MEMBERMODIFY     = 0x1513,
    LF_MANAGED          = 0x1514,
    LF_TYPESERVER2      = 0x1515,

    LF_STRIDED_ARRAY    = 0x1516,    // same as LF_ARRAY, but with stride between adjacent elements
    LF_HLSL             = 0x1517,
    LF_MODIFIER_EX      = 0x1518,
    LF_INTERFACE        = 0x1519,
    LF_BINTERFACE       = 0x151a,
    LF_VECTOR           = 0x151b,
    LF_MATRIX           = 0x151c,

    LF_VFTABLE          = 0x151d,      // a virtual function table
    LF_ENDOFLEAFRECORD  = LF_VFTABLE,

    LF_TYPE_LAST,                    // one greater than the last type record
    LF_TYPE_MAX         = LF_TYPE_LAST - 1,

    LF_FUNC_ID          = 0x1601,    // global func ID
    LF_MFUNC_ID         = 0x1602,    // member func ID
    LF_BUILDINFO        = 0x1603,    // build info: tool, version, command line, src/pdb file
    LF_SUBSTR_LIST      = 0x1604,    // similar to LF_ARGLIST, for list of sub strings
    LF_STRING_ID        = 0x1605,    // string ID

    LF_UDT_SRC_LINE     = 0x1606,    // source and line on where an UDT is defined
                                     // only generated by compiler

    LF_UDT_MOD_SRC_LINE = 0x1607,    // module, source and line on where an UDT is defined
                                     // only generated by linker

    LF_ID_LAST,                      // one greater than the last ID record
    LF_ID_MAX           = LF_ID_LAST - 1,

    LF_NUMERIC          = 0x8000,
    LF_CHAR             = 0x8000,
    LF_SHORT            = 0x8001,
    LF_USHORT           = 0x8002,
    LF_LONG             = 0x8003,
    LF_ULONG            = 0x8004,
    LF_REAL32           = 0x8005,
    LF_REAL64           = 0x8006,
    LF_REAL80           = 0x8007,
    LF_REAL128          = 0x8008,
    LF_QUADWORD         = 0x8009,
    LF_UQUADWORD        = 0x800a,
    LF_REAL48           = 0x800b,
    LF_COMPLEX32        = 0x800c,
    LF_COMPLEX64        = 0x800d,
    LF_COMPLEX80        = 0x800e,
    LF_COMPLEX128       = 0x800f,
    LF_VARSTRING        = 0x8010,

    LF_OCTWORD          = 0x8017,
    LF_UOCTWORD         = 0x8018,

    LF_DECIMAL          = 0x8019,
    LF_DATE             = 0x801a,
    LF_UTF8STRING       = 0x801b,

    LF_REAL16           = 0x801c,
    
    LF_PAD0             = 0xf0,
    LF_PAD1             = 0xf1,
    LF_PAD2             = 0xf2,
    LF_PAD3             = 0xf3,
    LF_PAD4             = 0xf4,
    LF_PAD5             = 0xf5,
    LF_PAD6             = 0xf6,
    LF_PAD7             = 0xf7,
    LF_PAD8             = 0xf8,
    LF_PAD9             = 0xf9,
    LF_PAD10            = 0xfa,
    LF_PAD11            = 0xfb,
    LF_PAD12            = 0xfc,
    LF_PAD13            = 0xfd,
    LF_PAD14            = 0xfe,
    LF_PAD15            = 0xff,
};

enum CV_CFL_LANG {
    CV_CFL_C        = 0x00,
    CV_CFL_CXX      = 0x01,
    CV_CFL_FORTRAN  = 0x02,
    CV_CFL_MASM     = 0x03,
    CV_CFL_PASCAL   = 0x04,
    CV_CFL_BASIC    = 0x05,
    CV_CFL_COBOL    = 0x06,
    CV_CFL_LINK     = 0x07,
    CV_CFL_CVTRES   = 0x08,
    CV_CFL_CVTPGD   = 0x09,
    CV_CFL_CSHARP   = 0x0A,  // C#
    CV_CFL_VB       = 0x0B,  // Visual Basic
    CV_CFL_ILASM    = 0x0C,  // IL (as in CLR) ASM
    CV_CFL_JAVA     = 0x0D,
    CV_CFL_JSCRIPT  = 0x0E,
    CV_CFL_MSIL     = 0x0F,  // Unknown MSIL (LTCG of .NETMODULE)
    CV_CFL_HLSL     = 0x10,  // High Level Shader Language
};
enum TYPE_ENUM_e {
//      Special Types

    T_NOTYPE        = 0x0000,   // uncharacterized type (no type)
    T_ABS           = 0x0001,   // absolute symbol
    T_SEGMENT       = 0x0002,   // segment type
    T_VOID          = 0x0003,   // void
    T_HRESULT       = 0x0008,   // OLE/COM HRESULT
    T_32PHRESULT    = 0x0408,   // OLE/COM HRESULT __ptr32 *
    T_64PHRESULT    = 0x0608,   // OLE/COM HRESULT __ptr64 *

    T_PVOID         = 0x0103,   // near pointer to void
    T_PFVOID        = 0x0203,   // far pointer to void
    T_PHVOID        = 0x0303,   // huge pointer to void
    T_32PVOID       = 0x0403,   // 32 bit pointer to void
    T_32PFVOID      = 0x0503,   // 16:32 pointer to void
    T_64PVOID       = 0x0603,   // 64 bit pointer to void
    T_CURRENCY      = 0x0004,   // BASIC 8 byte currency value
    T_NBASICSTR     = 0x0005,   // Near BASIC string
    T_FBASICSTR     = 0x0006,   // Far BASIC string
    T_NOTTRANS      = 0x0007,   // type not translated by cvpack
    T_BIT           = 0x0060,   // bit
    T_PASCHAR       = 0x0061,   // Pascal CHAR
    T_BOOL32FF      = 0x0062,   // 32-bit BOOL where true is 0xffffffff


//      Character types

    T_CHAR          = 0x0010,   // 8 bit signed
    T_PCHAR         = 0x0110,   // 16 bit pointer to 8 bit signed
    T_PFCHAR        = 0x0210,   // 16:16 far pointer to 8 bit signed
    T_PHCHAR        = 0x0310,   // 16:16 huge pointer to 8 bit signed
    T_32PCHAR       = 0x0410,   // 32 bit pointer to 8 bit signed
    T_32PFCHAR      = 0x0510,   // 16:32 pointer to 8 bit signed
    T_64PCHAR       = 0x0610,   // 64 bit pointer to 8 bit signed

    T_UCHAR         = 0x0020,   // 8 bit unsigned
    T_PUCHAR        = 0x0120,   // 16 bit pointer to 8 bit unsigned
    T_PFUCHAR       = 0x0220,   // 16:16 far pointer to 8 bit unsigned
    T_PHUCHAR       = 0x0320,   // 16:16 huge pointer to 8 bit unsigned
    T_32PUCHAR      = 0x0420,   // 32 bit pointer to 8 bit unsigned
    T_32PFUCHAR     = 0x0520,   // 16:32 pointer to 8 bit unsigned
    T_64PUCHAR      = 0x0620,   // 64 bit pointer to 8 bit unsigned


//      really a character types

    T_RCHAR         = 0x0070,   // really a char
    T_PRCHAR        = 0x0170,   // 16 bit pointer to a real char
    T_PFRCHAR       = 0x0270,   // 16:16 far pointer to a real char
    T_PHRCHAR       = 0x0370,   // 16:16 huge pointer to a real char
    T_32PRCHAR      = 0x0470,   // 32 bit pointer to a real char
    T_32PFRCHAR     = 0x0570,   // 16:32 pointer to a real char
    T_64PRCHAR      = 0x0670,   // 64 bit pointer to a real char


//      really a wide character types

    T_WCHAR         = 0x0071,   // wide char
    T_PWCHAR        = 0x0171,   // 16 bit pointer to a wide char
    T_PFWCHAR       = 0x0271,   // 16:16 far pointer to a wide char
    T_PHWCHAR       = 0x0371,   // 16:16 huge pointer to a wide char
    T_32PWCHAR      = 0x0471,   // 32 bit pointer to a wide char
    T_32PFWCHAR     = 0x0571,   // 16:32 pointer to a wide char
    T_64PWCHAR      = 0x0671,   // 64 bit pointer to a wide char

//      really a 16-bit unicode char

    T_CHAR16         = 0x007a,   // 16-bit unicode char
    T_PCHAR16        = 0x017a,   // 16 bit pointer to a 16-bit unicode char
    T_PFCHAR16       = 0x027a,   // 16:16 far pointer to a 16-bit unicode char
    T_PHCHAR16       = 0x037a,   // 16:16 huge pointer to a 16-bit unicode char
    T_32PCHAR16      = 0x047a,   // 32 bit pointer to a 16-bit unicode char
    T_32PFCHAR16     = 0x057a,   // 16:32 pointer to a 16-bit unicode char
    T_64PCHAR16      = 0x067a,   // 64 bit pointer to a 16-bit unicode char

//      really a 32-bit unicode char

    T_CHAR32         = 0x007b,   // 32-bit unicode char
    T_PCHAR32        = 0x017b,   // 16 bit pointer to a 32-bit unicode char
    T_PFCHAR32       = 0x027b,   // 16:16 far pointer to a 32-bit unicode char
    T_PHCHAR32       = 0x037b,   // 16:16 huge pointer to a 32-bit unicode char
    T_32PCHAR32      = 0x047b,   // 32 bit pointer to a 32-bit unicode char
    T_32PFCHAR32     = 0x057b,   // 16:32 pointer to a 32-bit unicode char
    T_64PCHAR32      = 0x067b,   // 64 bit pointer to a 32-bit unicode char

//      8 bit int types

    T_INT1          = 0x0068,   // 8 bit signed int
    T_PINT1         = 0x0168,   // 16 bit pointer to 8 bit signed int
    T_PFINT1        = 0x0268,   // 16:16 far pointer to 8 bit signed int
    T_PHINT1        = 0x0368,   // 16:16 huge pointer to 8 bit signed int
    T_32PINT1       = 0x0468,   // 32 bit pointer to 8 bit signed int
    T_32PFINT1      = 0x0568,   // 16:32 pointer to 8 bit signed int
    T_64PINT1       = 0x0668,   // 64 bit pointer to 8 bit signed int

    T_UINT1         = 0x0069,   // 8 bit unsigned int
    T_PUINT1        = 0x0169,   // 16 bit pointer to 8 bit unsigned int
    T_PFUINT1       = 0x0269,   // 16:16 far pointer to 8 bit unsigned int
    T_PHUINT1       = 0x0369,   // 16:16 huge pointer to 8 bit unsigned int
    T_32PUINT1      = 0x0469,   // 32 bit pointer to 8 bit unsigned int
    T_32PFUINT1     = 0x0569,   // 16:32 pointer to 8 bit unsigned int
    T_64PUINT1      = 0x0669,   // 64 bit pointer to 8 bit unsigned int


//      16 bit short types

    T_SHORT         = 0x0011,   // 16 bit signed
    T_PSHORT        = 0x0111,   // 16 bit pointer to 16 bit signed
    T_PFSHORT       = 0x0211,   // 16:16 far pointer to 16 bit signed
    T_PHSHORT       = 0x0311,   // 16:16 huge pointer to 16 bit signed
    T_32PSHORT      = 0x0411,   // 32 bit pointer to 16 bit signed
    T_32PFSHORT     = 0x0511,   // 16:32 pointer to 16 bit signed
    T_64PSHORT      = 0x0611,   // 64 bit pointer to 16 bit signed

    T_USHORT        = 0x0021,   // 16 bit unsigned
    T_PUSHORT       = 0x0121,   // 16 bit pointer to 16 bit unsigned
    T_PFUSHORT      = 0x0221,   // 16:16 far pointer to 16 bit unsigned
    T_PHUSHORT      = 0x0321,   // 16:16 huge pointer to 16 bit unsigned
    T_32PUSHORT     = 0x0421,   // 32 bit pointer to 16 bit unsigned
    T_32PFUSHORT    = 0x0521,   // 16:32 pointer to 16 bit unsigned
    T_64PUSHORT     = 0x0621,   // 64 bit pointer to 16 bit unsigned


//      16 bit int types

    T_INT2          = 0x0072,   // 16 bit signed int
    T_PINT2         = 0x0172,   // 16 bit pointer to 16 bit signed int
    T_PFINT2        = 0x0272,   // 16:16 far pointer to 16 bit signed int
    T_PHINT2        = 0x0372,   // 16:16 huge pointer to 16 bit signed int
    T_32PINT2       = 0x0472,   // 32 bit pointer to 16 bit signed int
    T_32PFINT2      = 0x0572,   // 16:32 pointer to 16 bit signed int
    T_64PINT2       = 0x0672,   // 64 bit pointer to 16 bit signed int

    T_UINT2         = 0x0073,   // 16 bit unsigned int
    T_PUINT2        = 0x0173,   // 16 bit pointer to 16 bit unsigned int
    T_PFUINT2       = 0x0273,   // 16:16 far pointer to 16 bit unsigned int
    T_PHUINT2       = 0x0373,   // 16:16 huge pointer to 16 bit unsigned int
    T_32PUINT2      = 0x0473,   // 32 bit pointer to 16 bit unsigned int
    T_32PFUINT2     = 0x0573,   // 16:32 pointer to 16 bit unsigned int
    T_64PUINT2      = 0x0673,   // 64 bit pointer to 16 bit unsigned int


//      32 bit long types

    T_LONG          = 0x0012,   // 32 bit signed
    T_ULONG         = 0x0022,   // 32 bit unsigned
    T_PLONG         = 0x0112,   // 16 bit pointer to 32 bit signed
    T_PULONG        = 0x0122,   // 16 bit pointer to 32 bit unsigned
    T_PFLONG        = 0x0212,   // 16:16 far pointer to 32 bit signed
    T_PFULONG       = 0x0222,   // 16:16 far pointer to 32 bit unsigned
    T_PHLONG        = 0x0312,   // 16:16 huge pointer to 32 bit signed
    T_PHULONG       = 0x0322,   // 16:16 huge pointer to 32 bit unsigned

    T_32PLONG       = 0x0412,   // 32 bit pointer to 32 bit signed
    T_32PULONG      = 0x0422,   // 32 bit pointer to 32 bit unsigned
    T_32PFLONG      = 0x0512,   // 16:32 pointer to 32 bit signed
    T_32PFULONG     = 0x0522,   // 16:32 pointer to 32 bit unsigned
    T_64PLONG       = 0x0612,   // 64 bit pointer to 32 bit signed
    T_64PULONG      = 0x0622,   // 64 bit pointer to 32 bit unsigned


//      32 bit int types

    T_INT4          = 0x0074,   // 32 bit signed int
    T_PINT4         = 0x0174,   // 16 bit pointer to 32 bit signed int
    T_PFINT4        = 0x0274,   // 16:16 far pointer to 32 bit signed int
    T_PHINT4        = 0x0374,   // 16:16 huge pointer to 32 bit signed int
    T_32PINT4       = 0x0474,   // 32 bit pointer to 32 bit signed int
    T_32PFINT4      = 0x0574,   // 16:32 pointer to 32 bit signed int
    T_64PINT4       = 0x0674,   // 64 bit pointer to 32 bit signed int

    T_UINT4         = 0x0075,   // 32 bit unsigned int
    T_PUINT4        = 0x0175,   // 16 bit pointer to 32 bit unsigned int
    T_PFUINT4       = 0x0275,   // 16:16 far pointer to 32 bit unsigned int
    T_PHUINT4       = 0x0375,   // 16:16 huge pointer to 32 bit unsigned int
    T_32PUINT4      = 0x0475,   // 32 bit pointer to 32 bit unsigned int
    T_32PFUINT4     = 0x0575,   // 16:32 pointer to 32 bit unsigned int
    T_64PUINT4      = 0x0675,   // 64 bit pointer to 32 bit unsigned int


//      64 bit quad types

    T_QUAD          = 0x0013,   // 64 bit signed
    T_PQUAD         = 0x0113,   // 16 bit pointer to 64 bit signed
    T_PFQUAD        = 0x0213,   // 16:16 far pointer to 64 bit signed
    T_PHQUAD        = 0x0313,   // 16:16 huge pointer to 64 bit signed
    T_32PQUAD       = 0x0413,   // 32 bit pointer to 64 bit signed
    T_32PFQUAD      = 0x0513,   // 16:32 pointer to 64 bit signed
    T_64PQUAD       = 0x0613,   // 64 bit pointer to 64 bit signed

    T_UQUAD         = 0x0023,   // 64 bit unsigned
    T_PUQUAD        = 0x0123,   // 16 bit pointer to 64 bit unsigned
    T_PFUQUAD       = 0x0223,   // 16:16 far pointer to 64 bit unsigned
    T_PHUQUAD       = 0x0323,   // 16:16 huge pointer to 64 bit unsigned
    T_32PUQUAD      = 0x0423,   // 32 bit pointer to 64 bit unsigned
    T_32PFUQUAD     = 0x0523,   // 16:32 pointer to 64 bit unsigned
    T_64PUQUAD      = 0x0623,   // 64 bit pointer to 64 bit unsigned


//      64 bit int types

    T_INT8          = 0x0076,   // 64 bit signed int
    T_PINT8         = 0x0176,   // 16 bit pointer to 64 bit signed int
    T_PFINT8        = 0x0276,   // 16:16 far pointer to 64 bit signed int
    T_PHINT8        = 0x0376,   // 16:16 huge pointer to 64 bit signed int
    T_32PINT8       = 0x0476,   // 32 bit pointer to 64 bit signed int
    T_32PFINT8      = 0x0576,   // 16:32 pointer to 64 bit signed int
    T_64PINT8       = 0x0676,   // 64 bit pointer to 64 bit signed int

    T_UINT8         = 0x0077,   // 64 bit unsigned int
    T_PUINT8        = 0x0177,   // 16 bit pointer to 64 bit unsigned int
    T_PFUINT8       = 0x0277,   // 16:16 far pointer to 64 bit unsigned int
    T_PHUINT8       = 0x0377,   // 16:16 huge pointer to 64 bit unsigned int
    T_32PUINT8      = 0x0477,   // 32 bit pointer to 64 bit unsigned int
    T_32PFUINT8     = 0x0577,   // 16:32 pointer to 64 bit unsigned int
    T_64PUINT8      = 0x0677,   // 64 bit pointer to 64 bit unsigned int


//      128 bit octet types

    T_OCT           = 0x0014,   // 128 bit signed
    T_POCT          = 0x0114,   // 16 bit pointer to 128 bit signed
    T_PFOCT         = 0x0214,   // 16:16 far pointer to 128 bit signed
    T_PHOCT         = 0x0314,   // 16:16 huge pointer to 128 bit signed
    T_32POCT        = 0x0414,   // 32 bit pointer to 128 bit signed
    T_32PFOCT       = 0x0514,   // 16:32 pointer to 128 bit signed
    T_64POCT        = 0x0614,   // 64 bit pointer to 128 bit signed

    T_UOCT          = 0x0024,   // 128 bit unsigned
    T_PUOCT         = 0x0124,   // 16 bit pointer to 128 bit unsigned
    T_PFUOCT        = 0x0224,   // 16:16 far pointer to 128 bit unsigned
    T_PHUOCT        = 0x0324,   // 16:16 huge pointer to 128 bit unsigned
    T_32PUOCT       = 0x0424,   // 32 bit pointer to 128 bit unsigned
    T_32PFUOCT      = 0x0524,   // 16:32 pointer to 128 bit unsigned
    T_64PUOCT       = 0x0624,   // 64 bit pointer to 128 bit unsigned


//      128 bit int types

    T_INT16         = 0x0078,   // 128 bit signed int
    T_PINT16        = 0x0178,   // 16 bit pointer to 128 bit signed int
    T_PFINT16       = 0x0278,   // 16:16 far pointer to 128 bit signed int
    T_PHINT16       = 0x0378,   // 16:16 huge pointer to 128 bit signed int
    T_32PINT16      = 0x0478,   // 32 bit pointer to 128 bit signed int
    T_32PFINT16     = 0x0578,   // 16:32 pointer to 128 bit signed int
    T_64PINT16      = 0x0678,   // 64 bit pointer to 128 bit signed int

    T_UINT16        = 0x0079,   // 128 bit unsigned int
    T_PUINT16       = 0x0179,   // 16 bit pointer to 128 bit unsigned int
    T_PFUINT16      = 0x0279,   // 16:16 far pointer to 128 bit unsigned int
    T_PHUINT16      = 0x0379,   // 16:16 huge pointer to 128 bit unsigned int
    T_32PUINT16     = 0x0479,   // 32 bit pointer to 128 bit unsigned int
    T_32PFUINT16    = 0x0579,   // 16:32 pointer to 128 bit unsigned int
    T_64PUINT16     = 0x0679,   // 64 bit pointer to 128 bit unsigned int


//      16 bit real types

    T_REAL16        = 0x0046,   // 16 bit real
    T_PREAL16       = 0x0146,   // 16 bit pointer to 16 bit real
    T_PFREAL16      = 0x0246,   // 16:16 far pointer to 16 bit real
    T_PHREAL16      = 0x0346,   // 16:16 huge pointer to 16 bit real
    T_32PREAL16     = 0x0446,   // 32 bit pointer to 16 bit real
    T_32PFREAL16    = 0x0546,   // 16:32 pointer to 16 bit real
    T_64PREAL16     = 0x0646,   // 64 bit pointer to 16 bit real


//      32 bit real types

    T_REAL32        = 0x0040,   // 32 bit real
    T_PREAL32       = 0x0140,   // 16 bit pointer to 32 bit real
    T_PFREAL32      = 0x0240,   // 16:16 far pointer to 32 bit real
    T_PHREAL32      = 0x0340,   // 16:16 huge pointer to 32 bit real
    T_32PREAL32     = 0x0440,   // 32 bit pointer to 32 bit real
    T_32PFREAL32    = 0x0540,   // 16:32 pointer to 32 bit real
    T_64PREAL32     = 0x0640,   // 64 bit pointer to 32 bit real


//      32 bit partial-precision real types

    T_REAL32PP      = 0x0045,   // 32 bit PP real
    T_PREAL32PP     = 0x0145,   // 16 bit pointer to 32 bit PP real
    T_PFREAL32PP    = 0x0245,   // 16:16 far pointer to 32 bit PP real
    T_PHREAL32PP    = 0x0345,   // 16:16 huge pointer to 32 bit PP real
    T_32PREAL32PP   = 0x0445,   // 32 bit pointer to 32 bit PP real
    T_32PFREAL32PP  = 0x0545,   // 16:32 pointer to 32 bit PP real
    T_64PREAL32PP   = 0x0645,   // 64 bit pointer to 32 bit PP real


//      48 bit real types

    T_REAL48        = 0x0044,   // 48 bit real
    T_PREAL48       = 0x0144,   // 16 bit pointer to 48 bit real
    T_PFREAL48      = 0x0244,   // 16:16 far pointer to 48 bit real
    T_PHREAL48      = 0x0344,   // 16:16 huge pointer to 48 bit real
    T_32PREAL48     = 0x0444,   // 32 bit pointer to 48 bit real
    T_32PFREAL48    = 0x0544,   // 16:32 pointer to 48 bit real
    T_64PREAL48     = 0x0644,   // 64 bit pointer to 48 bit real


//      64 bit real types

    T_REAL64        = 0x0041,   // 64 bit real
    T_PREAL64       = 0x0141,   // 16 bit pointer to 64 bit real
    T_PFREAL64      = 0x0241,   // 16:16 far pointer to 64 bit real
    T_PHREAL64      = 0x0341,   // 16:16 huge pointer to 64 bit real
    T_32PREAL64     = 0x0441,   // 32 bit pointer to 64 bit real
    T_32PFREAL64    = 0x0541,   // 16:32 pointer to 64 bit real
    T_64PREAL64     = 0x0641,   // 64 bit pointer to 64 bit real


//      80 bit real types

    T_REAL80        = 0x0042,   // 80 bit real
    T_PREAL80       = 0x0142,   // 16 bit pointer to 80 bit real
    T_PFREAL80      = 0x0242,   // 16:16 far pointer to 80 bit real
    T_PHREAL80      = 0x0342,   // 16:16 huge pointer to 80 bit real
    T_32PREAL80     = 0x0442,   // 32 bit pointer to 80 bit real
    T_32PFREAL80    = 0x0542,   // 16:32 pointer to 80 bit real
    T_64PREAL80     = 0x0642,   // 64 bit pointer to 80 bit real


//      128 bit real types

    T_REAL128       = 0x0043,   // 128 bit real
    T_PREAL128      = 0x0143,   // 16 bit pointer to 128 bit real
    T_PFREAL128     = 0x0243,   // 16:16 far pointer to 128 bit real
    T_PHREAL128     = 0x0343,   // 16:16 huge pointer to 128 bit real
    T_32PREAL128    = 0x0443,   // 32 bit pointer to 128 bit real
    T_32PFREAL128   = 0x0543,   // 16:32 pointer to 128 bit real
    T_64PREAL128    = 0x0643,   // 64 bit pointer to 128 bit real


//      32 bit complex types

    T_CPLX32        = 0x0050,   // 32 bit complex
    T_PCPLX32       = 0x0150,   // 16 bit pointer to 32 bit complex
    T_PFCPLX32      = 0x0250,   // 16:16 far pointer to 32 bit complex
    T_PHCPLX32      = 0x0350,   // 16:16 huge pointer to 32 bit complex
    T_32PCPLX32     = 0x0450,   // 32 bit pointer to 32 bit complex
    T_32PFCPLX32    = 0x0550,   // 16:32 pointer to 32 bit complex
    T_64PCPLX32     = 0x0650,   // 64 bit pointer to 32 bit complex


//      64 bit complex types

    T_CPLX64        = 0x0051,   // 64 bit complex
    T_PCPLX64       = 0x0151,   // 16 bit pointer to 64 bit complex
    T_PFCPLX64      = 0x0251,   // 16:16 far pointer to 64 bit complex
    T_PHCPLX64      = 0x0351,   // 16:16 huge pointer to 64 bit complex
    T_32PCPLX64     = 0x0451,   // 32 bit pointer to 64 bit complex
    T_32PFCPLX64    = 0x0551,   // 16:32 pointer to 64 bit complex
    T_64PCPLX64     = 0x0651,   // 64 bit pointer to 64 bit complex


//      80 bit complex types

    T_CPLX80        = 0x0052,   // 80 bit complex
    T_PCPLX80       = 0x0152,   // 16 bit pointer to 80 bit complex
    T_PFCPLX80      = 0x0252,   // 16:16 far pointer to 80 bit complex
    T_PHCPLX80      = 0x0352,   // 16:16 huge pointer to 80 bit complex
    T_32PCPLX80     = 0x0452,   // 32 bit pointer to 80 bit complex
    T_32PFCPLX80    = 0x0552,   // 16:32 pointer to 80 bit complex
    T_64PCPLX80     = 0x0652,   // 64 bit pointer to 80 bit complex


//      128 bit complex types

    T_CPLX128       = 0x0053,   // 128 bit complex
    T_PCPLX128      = 0x0153,   // 16 bit pointer to 128 bit complex
    T_PFCPLX128     = 0x0253,   // 16:16 far pointer to 128 bit complex
    T_PHCPLX128     = 0x0353,   // 16:16 huge pointer to 128 bit real
    T_32PCPLX128    = 0x0453,   // 32 bit pointer to 128 bit complex
    T_32PFCPLX128   = 0x0553,   // 16:32 pointer to 128 bit complex
    T_64PCPLX128    = 0x0653,   // 64 bit pointer to 128 bit complex


//      boolean types

    T_BOOL08        = 0x0030,   // 8 bit boolean
    T_PBOOL08       = 0x0130,   // 16 bit pointer to  8 bit boolean
    T_PFBOOL08      = 0x0230,   // 16:16 far pointer to  8 bit boolean
    T_PHBOOL08      = 0x0330,   // 16:16 huge pointer to  8 bit boolean
    T_32PBOOL08     = 0x0430,   // 32 bit pointer to 8 bit boolean
    T_32PFBOOL08    = 0x0530,   // 16:32 pointer to 8 bit boolean
    T_64PBOOL08     = 0x0630,   // 64 bit pointer to 8 bit boolean

    T_BOOL16        = 0x0031,   // 16 bit boolean
    T_PBOOL16       = 0x0131,   // 16 bit pointer to 16 bit boolean
    T_PFBOOL16      = 0x0231,   // 16:16 far pointer to 16 bit boolean
    T_PHBOOL16      = 0x0331,   // 16:16 huge pointer to 16 bit boolean
    T_32PBOOL16     = 0x0431,   // 32 bit pointer to 18 bit boolean
    T_32PFBOOL16    = 0x0531,   // 16:32 pointer to 16 bit boolean
    T_64PBOOL16     = 0x0631,   // 64 bit pointer to 18 bit boolean

    T_BOOL32        = 0x0032,   // 32 bit boolean
    T_PBOOL32       = 0x0132,   // 16 bit pointer to 32 bit boolean
    T_PFBOOL32      = 0x0232,   // 16:16 far pointer to 32 bit boolean
    T_PHBOOL32      = 0x0332,   // 16:16 huge pointer to 32 bit boolean
    T_32PBOOL32     = 0x0432,   // 32 bit pointer to 32 bit boolean
    T_32PFBOOL32    = 0x0532,   // 16:32 pointer to 32 bit boolean
    T_64PBOOL32     = 0x0632,   // 64 bit pointer to 32 bit boolean

    T_BOOL64        = 0x0033,   // 64 bit boolean
    T_PBOOL64       = 0x0133,   // 16 bit pointer to 64 bit boolean
    T_PFBOOL64      = 0x0233,   // 16:16 far pointer to 64 bit boolean
    T_PHBOOL64      = 0x0333,   // 16:16 huge pointer to 64 bit boolean
    T_32PBOOL64     = 0x0433,   // 32 bit pointer to 64 bit boolean
    T_32PFBOOL64    = 0x0533,   // 16:32 pointer to 64 bit boolean
    T_64PBOOL64     = 0x0633,   // 64 bit pointer to 64 bit boolean


//      ???

    T_NCVPTR        = 0x01f0,   // CV Internal type for created near pointers
    T_FCVPTR        = 0x02f0,   // CV Internal type for created far pointers
    T_HCVPTR        = 0x03f0,   // CV Internal type for created huge pointers
    T_32NCVPTR      = 0x04f0,   // CV Internal type for created near 32-bit pointers
    T_32FCVPTR      = 0x05f0,   // CV Internal type for created far 32-bit pointers
    T_64NCVPTR      = 0x06f0,   // CV Internal type for created near 64-bit pointers

};

enum PDBIMPV {
    PDBImpvVC2      = 19941610,
    PDBImpvVC4      = 19950623,
    PDBImpvVC41     = 19950814,
    PDBImpvVC50     = 19960307,
    PDBImpvVC98     = 19970604,
    PDBImpvVC70     = 20000404,
    PDBImpvVC70Dep  = 19990604,  // deprecated vc70 implementation version
    PDBImpvVC80     = 20030901,
    PDBImpvVC110    = 20091201,
    PDBImpvVC140    = 20140508,
    // PDBImpv         = PDBImpvVC110,
};

enum CV_call_e {
    CV_CALL_NEAR_C      = 0x00, // near right to left push, caller pops stack
    CV_CALL_FAR_C       = 0x01, // far right to left push, caller pops stack
    CV_CALL_NEAR_PASCAL = 0x02, // near left to right push, callee pops stack
    CV_CALL_FAR_PASCAL  = 0x03, // far left to right push, callee pops stack
    CV_CALL_NEAR_FAST   = 0x04, // near left to right push with regs, callee pops stack
    CV_CALL_FAR_FAST    = 0x05, // far left to right push with regs, callee pops stack
    CV_CALL_SKIPPED     = 0x06, // skipped (unused) call index
    CV_CALL_NEAR_STD    = 0x07, // near standard call
    CV_CALL_FAR_STD     = 0x08, // far standard call
    CV_CALL_NEAR_SYS    = 0x09, // near sys call
    CV_CALL_FAR_SYS     = 0x0a, // far sys call
    CV_CALL_THISCALL    = 0x0b, // this call (this passed in register)
    CV_CALL_MIPSCALL    = 0x0c, // Mips call
    CV_CALL_GENERIC     = 0x0d, // Generic call sequence
    CV_CALL_ALPHACALL   = 0x0e, // Alpha call
    CV_CALL_PPCCALL     = 0x0f, // PPC call
    CV_CALL_SHCALL      = 0x10, // Hitachi SuperH call
    CV_CALL_ARMCALL     = 0x11, // ARM call
    CV_CALL_AM33CALL    = 0x12, // AM33 call
    CV_CALL_TRICALL     = 0x13, // TriCore Call
    CV_CALL_SH5CALL     = 0x14, // Hitachi SuperH-5 call
    CV_CALL_M32RCALL    = 0x15, // M32R Call
    CV_CALL_CLRCALL     = 0x16, // clr call
    CV_CALL_INLINE      = 0x17, // Marker for routines always inlined and thus lacking a convention
    CV_CALL_NEAR_VECTOR = 0x18, // near left to right push with regs, callee pops stack
    CV_CALL_RESERVED    = 0x19  // first unused call enumeration

    // Do NOT add any more machine specific conventions.  This is to be used for
    // calling conventions in the source only (e.g. __cdecl, __stdcall).
};
enum CV_REGISTERS {
    CV_AMD64_AL       =   1,
    CV_AMD64_CL       =   2,
    CV_AMD64_DL       =   3,
    CV_AMD64_BL       =   4,
    CV_AMD64_AH       =   5,
    CV_AMD64_CH       =   6,
    CV_AMD64_DH       =   7,
    CV_AMD64_BH       =   8,
    CV_AMD64_AX       =   9,
    CV_AMD64_CX       =  10,
    CV_AMD64_DX       =  11,
    CV_AMD64_BX       =  12,
    CV_AMD64_SP       =  13,
    CV_AMD64_BP       =  14,
    CV_AMD64_SI       =  15,
    CV_AMD64_DI       =  16,
    CV_AMD64_EAX      =  17,
    CV_AMD64_ECX      =  18,
    CV_AMD64_EDX      =  19,
    CV_AMD64_EBX      =  20,
    CV_AMD64_ESP      =  21,
    CV_AMD64_EBP      =  22,
    CV_AMD64_ESI      =  23,
    CV_AMD64_EDI      =  24,
    CV_AMD64_ES       =  25,
    CV_AMD64_CS       =  26,
    CV_AMD64_SS       =  27,
    CV_AMD64_DS       =  28,
    CV_AMD64_FS       =  29,
    CV_AMD64_GS       =  30,
    CV_AMD64_FLAGS    =  32,
    CV_AMD64_RIP      =  33,
    CV_AMD64_EFLAGS   =  34,

    // Control registers
    CV_AMD64_CR0      =  80,
    CV_AMD64_CR1      =  81,
    CV_AMD64_CR2      =  82,
    CV_AMD64_CR3      =  83,
    CV_AMD64_CR4      =  84,
    CV_AMD64_CR8      =  88,

    // Debug registers
    CV_AMD64_DR0      =  90,
    CV_AMD64_DR1      =  91,
    CV_AMD64_DR2      =  92,
    CV_AMD64_DR3      =  93,
    CV_AMD64_DR4      =  94,
    CV_AMD64_DR5      =  95,
    CV_AMD64_DR6      =  96,
    CV_AMD64_DR7      =  97,
    CV_AMD64_DR8      =  98,
    CV_AMD64_DR9      =  99,
    CV_AMD64_DR10     =  100,
    CV_AMD64_DR11     =  101,
    CV_AMD64_DR12     =  102,
    CV_AMD64_DR13     =  103,
    CV_AMD64_DR14     =  104,
    CV_AMD64_DR15     =  105,

    CV_AMD64_GDTR     =  110,
    CV_AMD64_GDTL     =  111,
    CV_AMD64_IDTR     =  112,
    CV_AMD64_IDTL     =  113,
    CV_AMD64_LDTR     =  114,
    CV_AMD64_TR       =  115,

    CV_AMD64_ST0      =  128,
    CV_AMD64_ST1      =  129,
    CV_AMD64_ST2      =  130,
    CV_AMD64_ST3      =  131,
    CV_AMD64_ST4      =  132,
    CV_AMD64_ST5      =  133,
    CV_AMD64_ST6      =  134,
    CV_AMD64_ST7      =  135,
    CV_AMD64_CTRL     =  136,
    CV_AMD64_STAT     =  137,
    CV_AMD64_TAG      =  138,
    CV_AMD64_FPIP     =  139,
    CV_AMD64_FPCS     =  140,
    CV_AMD64_FPDO     =  141,
    CV_AMD64_FPDS     =  142,
    CV_AMD64_ISEM     =  143,
    CV_AMD64_FPEIP    =  144,
    CV_AMD64_FPEDO    =  145,

    CV_AMD64_MM0      =  146,
    CV_AMD64_MM1      =  147,
    CV_AMD64_MM2      =  148,
    CV_AMD64_MM3      =  149,
    CV_AMD64_MM4      =  150,
    CV_AMD64_MM5      =  151,
    CV_AMD64_MM6      =  152,
    CV_AMD64_MM7      =  153,

    CV_AMD64_XMM0     =  154,   // KATMAI registers
    CV_AMD64_XMM1     =  155,
    CV_AMD64_XMM2     =  156,
    CV_AMD64_XMM3     =  157,
    CV_AMD64_XMM4     =  158,
    CV_AMD64_XMM5     =  159,
    CV_AMD64_XMM6     =  160,
    CV_AMD64_XMM7     =  161,

    CV_AMD64_XMM0_0   =  162,   // KATMAI sub-registers
    CV_AMD64_XMM0_1   =  163,
    CV_AMD64_XMM0_2   =  164,
    CV_AMD64_XMM0_3   =  165,
    CV_AMD64_XMM1_0   =  166,
    CV_AMD64_XMM1_1   =  167,
    CV_AMD64_XMM1_2   =  168,
    CV_AMD64_XMM1_3   =  169,
    CV_AMD64_XMM2_0   =  170,
    CV_AMD64_XMM2_1   =  171,
    CV_AMD64_XMM2_2   =  172,
    CV_AMD64_XMM2_3   =  173,
    CV_AMD64_XMM3_0   =  174,
    CV_AMD64_XMM3_1   =  175,
    CV_AMD64_XMM3_2   =  176,
    CV_AMD64_XMM3_3   =  177,
    CV_AMD64_XMM4_0   =  178,
    CV_AMD64_XMM4_1   =  179,
    CV_AMD64_XMM4_2   =  180,
    CV_AMD64_XMM4_3   =  181,
    CV_AMD64_XMM5_0   =  182,
    CV_AMD64_XMM5_1   =  183,
    CV_AMD64_XMM5_2   =  184,
    CV_AMD64_XMM5_3   =  185,
    CV_AMD64_XMM6_0   =  186,
    CV_AMD64_XMM6_1   =  187,
    CV_AMD64_XMM6_2   =  188,
    CV_AMD64_XMM6_3   =  189,
    CV_AMD64_XMM7_0   =  190,
    CV_AMD64_XMM7_1   =  191,
    CV_AMD64_XMM7_2   =  192,
    CV_AMD64_XMM7_3   =  193,

    CV_AMD64_XMM0L    =  194,
    CV_AMD64_XMM1L    =  195,
    CV_AMD64_XMM2L    =  196,
    CV_AMD64_XMM3L    =  197,
    CV_AMD64_XMM4L    =  198,
    CV_AMD64_XMM5L    =  199,
    CV_AMD64_XMM6L    =  200,
    CV_AMD64_XMM7L    =  201,

    CV_AMD64_XMM0H    =  202,
    CV_AMD64_XMM1H    =  203,
    CV_AMD64_XMM2H    =  204,
    CV_AMD64_XMM3H    =  205,
    CV_AMD64_XMM4H    =  206,
    CV_AMD64_XMM5H    =  207,
    CV_AMD64_XMM6H    =  208,
    CV_AMD64_XMM7H    =  209,

    CV_AMD64_MXCSR    =  211,   // XMM status register

    CV_AMD64_EMM0L    =  220,   // XMM sub-registers (WNI integer)
    CV_AMD64_EMM1L    =  221,
    CV_AMD64_EMM2L    =  222,
    CV_AMD64_EMM3L    =  223,
    CV_AMD64_EMM4L    =  224,
    CV_AMD64_EMM5L    =  225,
    CV_AMD64_EMM6L    =  226,
    CV_AMD64_EMM7L    =  227,

    CV_AMD64_EMM0H    =  228,
    CV_AMD64_EMM1H    =  229,
    CV_AMD64_EMM2H    =  230,
    CV_AMD64_EMM3H    =  231,
    CV_AMD64_EMM4H    =  232,
    CV_AMD64_EMM5H    =  233,
    CV_AMD64_EMM6H    =  234,
    CV_AMD64_EMM7H    =  235,

    // do not change the order of these regs, first one must be even too
    CV_AMD64_MM00     =  236,
    CV_AMD64_MM01     =  237,
    CV_AMD64_MM10     =  238,
    CV_AMD64_MM11     =  239,
    CV_AMD64_MM20     =  240,
    CV_AMD64_MM21     =  241,
    CV_AMD64_MM30     =  242,
    CV_AMD64_MM31     =  243,
    CV_AMD64_MM40     =  244,
    CV_AMD64_MM41     =  245,
    CV_AMD64_MM50     =  246,
    CV_AMD64_MM51     =  247,
    CV_AMD64_MM60     =  248,
    CV_AMD64_MM61     =  249,
    CV_AMD64_MM70     =  250,
    CV_AMD64_MM71     =  251,

    // Extended KATMAI registers
    CV_AMD64_XMM8     =  252,   // KATMAI registers
    CV_AMD64_XMM9     =  253,
    CV_AMD64_XMM10    =  254,
    CV_AMD64_XMM11    =  255,
    CV_AMD64_XMM12    =  256,
    CV_AMD64_XMM13    =  257,
    CV_AMD64_XMM14    =  258,
    CV_AMD64_XMM15    =  259,

    CV_AMD64_XMM8_0   =  260,   // KATMAI sub-registers
    CV_AMD64_XMM8_1   =  261,
    CV_AMD64_XMM8_2   =  262,
    CV_AMD64_XMM8_3   =  263,
    CV_AMD64_XMM9_0   =  264,
    CV_AMD64_XMM9_1   =  265,
    CV_AMD64_XMM9_2   =  266,
    CV_AMD64_XMM9_3   =  267,
    CV_AMD64_XMM10_0  =  268,
    CV_AMD64_XMM10_1  =  269,
    CV_AMD64_XMM10_2  =  270,
    CV_AMD64_XMM10_3  =  271,
    CV_AMD64_XMM11_0  =  272,
    CV_AMD64_XMM11_1  =  273,
    CV_AMD64_XMM11_2  =  274,
    CV_AMD64_XMM11_3  =  275,
    CV_AMD64_XMM12_0  =  276,
    CV_AMD64_XMM12_1  =  277,
    CV_AMD64_XMM12_2  =  278,
    CV_AMD64_XMM12_3  =  279,
    CV_AMD64_XMM13_0  =  280,
    CV_AMD64_XMM13_1  =  281,
    CV_AMD64_XMM13_2  =  282,
    CV_AMD64_XMM13_3  =  283,
    CV_AMD64_XMM14_0  =  284,
    CV_AMD64_XMM14_1  =  285,
    CV_AMD64_XMM14_2  =  286,
    CV_AMD64_XMM14_3  =  287,
    CV_AMD64_XMM15_0  =  288,
    CV_AMD64_XMM15_1  =  289,
    CV_AMD64_XMM15_2  =  290,
    CV_AMD64_XMM15_3  =  291,

    CV_AMD64_XMM8L    =  292,
    CV_AMD64_XMM9L    =  293,
    CV_AMD64_XMM10L   =  294,
    CV_AMD64_XMM11L   =  295,
    CV_AMD64_XMM12L   =  296,
    CV_AMD64_XMM13L   =  297,
    CV_AMD64_XMM14L   =  298,
    CV_AMD64_XMM15L   =  299,

    CV_AMD64_XMM8H    =  300,
    CV_AMD64_XMM9H    =  301,
    CV_AMD64_XMM10H   =  302,
    CV_AMD64_XMM11H   =  303,
    CV_AMD64_XMM12H   =  304,
    CV_AMD64_XMM13H   =  305,
    CV_AMD64_XMM14H   =  306,
    CV_AMD64_XMM15H   =  307,

    CV_AMD64_EMM8L    =  308,   // XMM sub-registers (WNI integer)
    CV_AMD64_EMM9L    =  309,
    CV_AMD64_EMM10L   =  310,
    CV_AMD64_EMM11L   =  311,
    CV_AMD64_EMM12L   =  312,
    CV_AMD64_EMM13L   =  313,
    CV_AMD64_EMM14L   =  314,
    CV_AMD64_EMM15L   =  315,

    CV_AMD64_EMM8H    =  316,
    CV_AMD64_EMM9H    =  317,
    CV_AMD64_EMM10H   =  318,
    CV_AMD64_EMM11H   =  319,
    CV_AMD64_EMM12H   =  320,
    CV_AMD64_EMM13H   =  321,
    CV_AMD64_EMM14H   =  322,
    CV_AMD64_EMM15H   =  323,

    // Low byte forms of some standard registers
    CV_AMD64_SIL      =  324,
    CV_AMD64_DIL      =  325,
    CV_AMD64_BPL      =  326,
    CV_AMD64_SPL      =  327,

    // 64-bit regular registers
    CV_AMD64_RAX      =  328,
    CV_AMD64_RBX      =  329,
    CV_AMD64_RCX      =  330,
    CV_AMD64_RDX      =  331,
    CV_AMD64_RSI      =  332,
    CV_AMD64_RDI      =  333,
    CV_AMD64_RBP      =  334,
    CV_AMD64_RSP      =  335,

    // 64-bit integer registers with 8-, 16-, and 32-bit forms (B, W, and D)
    CV_AMD64_R8       =  336,
    CV_AMD64_R9       =  337,
    CV_AMD64_R10      =  338,
    CV_AMD64_R11      =  339,
    CV_AMD64_R12      =  340,
    CV_AMD64_R13      =  341,
    CV_AMD64_R14      =  342,
    CV_AMD64_R15      =  343,

    CV_AMD64_R8B      =  344,
    CV_AMD64_R9B      =  345,
    CV_AMD64_R10B     =  346,
    CV_AMD64_R11B     =  347,
    CV_AMD64_R12B     =  348,
    CV_AMD64_R13B     =  349,
    CV_AMD64_R14B     =  350,
    CV_AMD64_R15B     =  351,

    CV_AMD64_R8W      =  352,
    CV_AMD64_R9W      =  353,
    CV_AMD64_R10W     =  354,
    CV_AMD64_R11W     =  355,
    CV_AMD64_R12W     =  356,
    CV_AMD64_R13W     =  357,
    CV_AMD64_R14W     =  358,
    CV_AMD64_R15W     =  359,

    CV_AMD64_R8D      =  360,
    CV_AMD64_R9D      =  361,
    CV_AMD64_R10D     =  362,
    CV_AMD64_R11D     =  363,
    CV_AMD64_R12D     =  364,
    CV_AMD64_R13D     =  365,
    CV_AMD64_R14D     =  366,
    CV_AMD64_R15D     =  367,

    // AVX registers 256 bits
    CV_AMD64_YMM0     =  368,
    CV_AMD64_YMM1     =  369,
    CV_AMD64_YMM2     =  370,
    CV_AMD64_YMM3     =  371,
    CV_AMD64_YMM4     =  372,
    CV_AMD64_YMM5     =  373,
    CV_AMD64_YMM6     =  374,
    CV_AMD64_YMM7     =  375,
    CV_AMD64_YMM8     =  376, 
    CV_AMD64_YMM9     =  377,
    CV_AMD64_YMM10    =  378,
    CV_AMD64_YMM11    =  379,
    CV_AMD64_YMM12    =  380,
    CV_AMD64_YMM13    =  381,
    CV_AMD64_YMM14    =  382,
    CV_AMD64_YMM15    =  383,

    // AVX registers upper 128 bits
    CV_AMD64_YMM0H    =  384,
    CV_AMD64_YMM1H    =  385,
    CV_AMD64_YMM2H    =  386,
    CV_AMD64_YMM3H    =  387,
    CV_AMD64_YMM4H    =  388,
    CV_AMD64_YMM5H    =  389,
    CV_AMD64_YMM6H    =  390,
    CV_AMD64_YMM7H    =  391,
    CV_AMD64_YMM8H    =  392, 
    CV_AMD64_YMM9H    =  393,
    CV_AMD64_YMM10H   =  394,
    CV_AMD64_YMM11H   =  395,
    CV_AMD64_YMM12H   =  396,
    CV_AMD64_YMM13H   =  397,
    CV_AMD64_YMM14H   =  398,
    CV_AMD64_YMM15H   =  399,

    //Lower/upper 8 bytes of XMM registers.  Unlike CV_AMD64_XMM<regnum><H/L>, these
    //values reprsesent the bit patterns of the registers as 64-bit integers, not
    //the representation of these registers as a double.
    CV_AMD64_XMM0IL    = 400,
    CV_AMD64_XMM1IL    = 401,
    CV_AMD64_XMM2IL    = 402,
    CV_AMD64_XMM3IL    = 403,
    CV_AMD64_XMM4IL    = 404,
    CV_AMD64_XMM5IL    = 405,
    CV_AMD64_XMM6IL    = 406,
    CV_AMD64_XMM7IL    = 407,
    CV_AMD64_XMM8IL    = 408,
    CV_AMD64_XMM9IL    = 409,
    CV_AMD64_XMM10IL    = 410,
    CV_AMD64_XMM11IL    = 411,
    CV_AMD64_XMM12IL    = 412,
    CV_AMD64_XMM13IL    = 413,
    CV_AMD64_XMM14IL    = 414,
    CV_AMD64_XMM15IL    = 415,

    CV_AMD64_XMM0IH    = 416,
    CV_AMD64_XMM1IH    = 417,
    CV_AMD64_XMM2IH    = 418,
    CV_AMD64_XMM3IH    = 419,
    CV_AMD64_XMM4IH    = 420,
    CV_AMD64_XMM5IH    = 421,
    CV_AMD64_XMM6IH    = 422,
    CV_AMD64_XMM7IH    = 423,
    CV_AMD64_XMM8IH    = 424,
    CV_AMD64_XMM9IH    = 425,
    CV_AMD64_XMM10IH    = 426,
    CV_AMD64_XMM11IH    = 427,
    CV_AMD64_XMM12IH    = 428,
    CV_AMD64_XMM13IH    = 429,
    CV_AMD64_XMM14IH    = 430,
    CV_AMD64_XMM15IH    = 431,

    CV_AMD64_YMM0I0    =  432,        // AVX integer registers
    CV_AMD64_YMM0I1    =  433,
    CV_AMD64_YMM0I2    =  434,
    CV_AMD64_YMM0I3    =  435,
    CV_AMD64_YMM1I0    =  436,
    CV_AMD64_YMM1I1    =  437,
    CV_AMD64_YMM1I2    =  438,
    CV_AMD64_YMM1I3    =  439,
    CV_AMD64_YMM2I0    =  440,
    CV_AMD64_YMM2I1    =  441,
    CV_AMD64_YMM2I2    =  442,
    CV_AMD64_YMM2I3    =  443,
    CV_AMD64_YMM3I0    =  444,
    CV_AMD64_YMM3I1    =  445,
    CV_AMD64_YMM3I2    =  446,
    CV_AMD64_YMM3I3    =  447,
    CV_AMD64_YMM4I0    =  448,
    CV_AMD64_YMM4I1    =  449,
    CV_AMD64_YMM4I2    =  450,
    CV_AMD64_YMM4I3    =  451,
    CV_AMD64_YMM5I0    =  452,
    CV_AMD64_YMM5I1    =  453,
    CV_AMD64_YMM5I2    =  454,
    CV_AMD64_YMM5I3    =  455,
    CV_AMD64_YMM6I0    =  456,
    CV_AMD64_YMM6I1    =  457,
    CV_AMD64_YMM6I2    =  458,
    CV_AMD64_YMM6I3    =  459,
    CV_AMD64_YMM7I0    =  460,
    CV_AMD64_YMM7I1    =  461,
    CV_AMD64_YMM7I2    =  462,
    CV_AMD64_YMM7I3    =  463,
    CV_AMD64_YMM8I0    =  464,
    CV_AMD64_YMM8I1    =  465,
    CV_AMD64_YMM8I2    =  466,
    CV_AMD64_YMM8I3    =  467,
    CV_AMD64_YMM9I0    =  468,
    CV_AMD64_YMM9I1    =  469,
    CV_AMD64_YMM9I2    =  470,
    CV_AMD64_YMM9I3    =  471,
    CV_AMD64_YMM10I0    =  472,
    CV_AMD64_YMM10I1    =  473,
    CV_AMD64_YMM10I2    =  474,
    CV_AMD64_YMM10I3    =  475,
    CV_AMD64_YMM11I0    =  476,
    CV_AMD64_YMM11I1    =  477,
    CV_AMD64_YMM11I2    =  478,
    CV_AMD64_YMM11I3    =  479,
    CV_AMD64_YMM12I0    =  480,
    CV_AMD64_YMM12I1    =  481,
    CV_AMD64_YMM12I2    =  482,
    CV_AMD64_YMM12I3    =  483,
    CV_AMD64_YMM13I0    =  484,
    CV_AMD64_YMM13I1    =  485,
    CV_AMD64_YMM13I2    =  486,
    CV_AMD64_YMM13I3    =  487,
    CV_AMD64_YMM14I0    =  488,
    CV_AMD64_YMM14I1    =  489,
    CV_AMD64_YMM14I2    =  490,
    CV_AMD64_YMM14I3    =  491,
    CV_AMD64_YMM15I0    =  492,
    CV_AMD64_YMM15I1    =  493,
    CV_AMD64_YMM15I2    =  494,
    CV_AMD64_YMM15I3    =  495,

    CV_AMD64_YMM0F0    =  496,        // AVX floating-point single precise registers
    CV_AMD64_YMM0F1    =  497,
    CV_AMD64_YMM0F2    =  498,
    CV_AMD64_YMM0F3    =  499,
    CV_AMD64_YMM0F4    =  500,
    CV_AMD64_YMM0F5    =  501,
    CV_AMD64_YMM0F6    =  502,
    CV_AMD64_YMM0F7    =  503,
    CV_AMD64_YMM1F0    =  504,
    CV_AMD64_YMM1F1    =  505,
    CV_AMD64_YMM1F2    =  506,
    CV_AMD64_YMM1F3    =  507,
    CV_AMD64_YMM1F4    =  508,
    CV_AMD64_YMM1F5    =  509,
    CV_AMD64_YMM1F6    =  510,
    CV_AMD64_YMM1F7    =  511,
    CV_AMD64_YMM2F0    =  512,
    CV_AMD64_YMM2F1    =  513,
    CV_AMD64_YMM2F2    =  514,
    CV_AMD64_YMM2F3    =  515,
    CV_AMD64_YMM2F4    =  516,
    CV_AMD64_YMM2F5    =  517,
    CV_AMD64_YMM2F6    =  518,
    CV_AMD64_YMM2F7    =  519,
    CV_AMD64_YMM3F0    =  520,
    CV_AMD64_YMM3F1    =  521,
    CV_AMD64_YMM3F2    =  522,
    CV_AMD64_YMM3F3    =  523,
    CV_AMD64_YMM3F4    =  524,
    CV_AMD64_YMM3F5    =  525,
    CV_AMD64_YMM3F6    =  526,
    CV_AMD64_YMM3F7    =  527,
    CV_AMD64_YMM4F0    =  528,
    CV_AMD64_YMM4F1    =  529,
    CV_AMD64_YMM4F2    =  530,
    CV_AMD64_YMM4F3    =  531,
    CV_AMD64_YMM4F4    =  532,
    CV_AMD64_YMM4F5    =  533,
    CV_AMD64_YMM4F6    =  534,
    CV_AMD64_YMM4F7    =  535,
    CV_AMD64_YMM5F0    =  536,
    CV_AMD64_YMM5F1    =  537,
    CV_AMD64_YMM5F2    =  538,
    CV_AMD64_YMM5F3    =  539,
    CV_AMD64_YMM5F4    =  540,
    CV_AMD64_YMM5F5    =  541,
    CV_AMD64_YMM5F6    =  542,
    CV_AMD64_YMM5F7    =  543,
    CV_AMD64_YMM6F0    =  544,
    CV_AMD64_YMM6F1    =  545,
    CV_AMD64_YMM6F2    =  546,
    CV_AMD64_YMM6F3    =  547,
    CV_AMD64_YMM6F4    =  548,
    CV_AMD64_YMM6F5    =  549,
    CV_AMD64_YMM6F6    =  550,
    CV_AMD64_YMM6F7    =  551,
    CV_AMD64_YMM7F0    =  552,
    CV_AMD64_YMM7F1    =  553,
    CV_AMD64_YMM7F2    =  554,
    CV_AMD64_YMM7F3    =  555,
    CV_AMD64_YMM7F4    =  556,
    CV_AMD64_YMM7F5    =  557,
    CV_AMD64_YMM7F6    =  558,
    CV_AMD64_YMM7F7    =  559,
    CV_AMD64_YMM8F0    =  560,
    CV_AMD64_YMM8F1    =  561,
    CV_AMD64_YMM8F2    =  562,
    CV_AMD64_YMM8F3    =  563,
    CV_AMD64_YMM8F4    =  564,
    CV_AMD64_YMM8F5    =  565,
    CV_AMD64_YMM8F6    =  566,
    CV_AMD64_YMM8F7    =  567,
    CV_AMD64_YMM9F0    =  568,
    CV_AMD64_YMM9F1    =  569,
    CV_AMD64_YMM9F2    =  570,
    CV_AMD64_YMM9F3    =  571,
    CV_AMD64_YMM9F4    =  572,
    CV_AMD64_YMM9F5    =  573,
    CV_AMD64_YMM9F6    =  574,
    CV_AMD64_YMM9F7    =  575,
    CV_AMD64_YMM10F0    =  576,
    CV_AMD64_YMM10F1    =  577,
    CV_AMD64_YMM10F2    =  578,
    CV_AMD64_YMM10F3    =  579,
    CV_AMD64_YMM10F4    =  580,
    CV_AMD64_YMM10F5    =  581,
    CV_AMD64_YMM10F6    =  582,
    CV_AMD64_YMM10F7    =  583,
    CV_AMD64_YMM11F0    =  584,
    CV_AMD64_YMM11F1    =  585,
    CV_AMD64_YMM11F2    =  586,
    CV_AMD64_YMM11F3    =  587,
    CV_AMD64_YMM11F4    =  588,
    CV_AMD64_YMM11F5    =  589,
    CV_AMD64_YMM11F6    =  590,
    CV_AMD64_YMM11F7    =  591,
    CV_AMD64_YMM12F0    =  592,
    CV_AMD64_YMM12F1    =  593,
    CV_AMD64_YMM12F2    =  594,
    CV_AMD64_YMM12F3    =  595,
    CV_AMD64_YMM12F4    =  596,
    CV_AMD64_YMM12F5    =  597,
    CV_AMD64_YMM12F6    =  598,
    CV_AMD64_YMM12F7    =  599,
    CV_AMD64_YMM13F0    =  600,
    CV_AMD64_YMM13F1    =  601,
    CV_AMD64_YMM13F2    =  602,
    CV_AMD64_YMM13F3    =  603,
    CV_AMD64_YMM13F4    =  604,
    CV_AMD64_YMM13F5    =  605,
    CV_AMD64_YMM13F6    =  606,
    CV_AMD64_YMM13F7    =  607,
    CV_AMD64_YMM14F0    =  608,
    CV_AMD64_YMM14F1    =  609,
    CV_AMD64_YMM14F2    =  610,
    CV_AMD64_YMM14F3    =  611,
    CV_AMD64_YMM14F4    =  612,
    CV_AMD64_YMM14F5    =  613,
    CV_AMD64_YMM14F6    =  614,
    CV_AMD64_YMM14F7    =  615,
    CV_AMD64_YMM15F0    =  616,
    CV_AMD64_YMM15F1    =  617,
    CV_AMD64_YMM15F2    =  618,
    CV_AMD64_YMM15F3    =  619,
    CV_AMD64_YMM15F4    =  620,
    CV_AMD64_YMM15F5    =  621,
    CV_AMD64_YMM15F6    =  622,
    CV_AMD64_YMM15F7    =  623,
    
    CV_AMD64_YMM0D0    =  624,        // AVX floating-point double precise registers
    CV_AMD64_YMM0D1    =  625,
    CV_AMD64_YMM0D2    =  626,
    CV_AMD64_YMM0D3    =  627,
    CV_AMD64_YMM1D0    =  628,
    CV_AMD64_YMM1D1    =  629,
    CV_AMD64_YMM1D2    =  630,
    CV_AMD64_YMM1D3    =  631,
    CV_AMD64_YMM2D0    =  632,
    CV_AMD64_YMM2D1    =  633,
    CV_AMD64_YMM2D2    =  634,
    CV_AMD64_YMM2D3    =  635,
    CV_AMD64_YMM3D0    =  636,
    CV_AMD64_YMM3D1    =  637,
    CV_AMD64_YMM3D2    =  638,
    CV_AMD64_YMM3D3    =  639,
    CV_AMD64_YMM4D0    =  640,
    CV_AMD64_YMM4D1    =  641,
    CV_AMD64_YMM4D2    =  642,
    CV_AMD64_YMM4D3    =  643,
    CV_AMD64_YMM5D0    =  644,
    CV_AMD64_YMM5D1    =  645,
    CV_AMD64_YMM5D2    =  646,
    CV_AMD64_YMM5D3    =  647,
    CV_AMD64_YMM6D0    =  648,
    CV_AMD64_YMM6D1    =  649,
    CV_AMD64_YMM6D2    =  650,
    CV_AMD64_YMM6D3    =  651,
    CV_AMD64_YMM7D0    =  652,
    CV_AMD64_YMM7D1    =  653,
    CV_AMD64_YMM7D2    =  654,
    CV_AMD64_YMM7D3    =  655,
    CV_AMD64_YMM8D0    =  656,
    CV_AMD64_YMM8D1    =  657,
    CV_AMD64_YMM8D2    =  658,
    CV_AMD64_YMM8D3    =  659,
    CV_AMD64_YMM9D0    =  660,
    CV_AMD64_YMM9D1    =  661,
    CV_AMD64_YMM9D2    =  662,
    CV_AMD64_YMM9D3    =  663,
    CV_AMD64_YMM10D0    =  664,
    CV_AMD64_YMM10D1    =  665,
    CV_AMD64_YMM10D2    =  666,
    CV_AMD64_YMM10D3    =  667,
    CV_AMD64_YMM11D0    =  668,
    CV_AMD64_YMM11D1    =  669,
    CV_AMD64_YMM11D2    =  670,
    CV_AMD64_YMM11D3    =  671,
    CV_AMD64_YMM12D0    =  672,
    CV_AMD64_YMM12D1    =  673,
    CV_AMD64_YMM12D2    =  674,
    CV_AMD64_YMM12D3    =  675,
    CV_AMD64_YMM13D0    =  676,
    CV_AMD64_YMM13D1    =  677,
    CV_AMD64_YMM13D2    =  678,
    CV_AMD64_YMM13D3    =  679,
    CV_AMD64_YMM14D0    =  680,
    CV_AMD64_YMM14D1    =  681,
    CV_AMD64_YMM14D2    =  682,
    CV_AMD64_YMM14D3    =  683,
    CV_AMD64_YMM15D0    =  684,
    CV_AMD64_YMM15D1    =  685,
    CV_AMD64_YMM15D2    =  686,
    CV_AMD64_YMM15D3    =  687
};
struct MSFHeader {
    char magic[30] = "Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53";
    u16 _padding2; // 2 byte padding
    u32 blockSize;
    u32 blockOfValidFPM;
    u32 blockCount;
    u32 bytesOfStreamTable;
    u32 _padding4; // 4 byte padding
    u32 block_of_block_to_streamTable;
};
/*
    The purpose of this class is to create or deserialize a PDB file into memory
    which is easy to modify. Then you can write it to a new file.

    PDB divides it's data into blocks and streams. The implementation of blocks
    is quite difficult to use without making mistakes which is why it's mostly hidden.
    You usually work with streams when interacting with this class.
    
    There may be some unexepected copy constructors which you can remove.
    At the momemnt, we need this class to be simple and understandable rather than performant.

    There may be bugs when using many blocks and streams. Some cases may not have been accounted for.
*/
struct PDBFile {
    /*
        Interface
    */

    // Serializes the content of this PDBFile struct to a file.
    // Serialize is an alternative name for this function.
    bool writeFile(const char* path);

    // Returns a number (or index) which identifies the stream.
    // Returns -1 on failure.
    u32 createStream();
    // You can request a number from 1 - 4 (PDB, TPI, DBI, IPI).
    // Returns -1 if the number isn't available or because of some failure.
    u32 requestStream(u32 requestNumber);
    // ?
    void removeStream(u32 streamNumber);

    // The reason for the read and write interface is because
    // the memory range you are reading or writing may exist on two blocks.
    // A pointer can only be taken from a stream if it exists on one block.

    // Memory from src is copied to the offset inside the stream.
    bool write(u32 streamNumber, u32 offset, u32 bytes, void* src);
    // Memory from the offset in the stream is copied to dst.
    bool read(u32 streamNumber, u32 offset, u32 bytes, void* dst);
    template<typename T>
    bool writeValue(u32 streamNumber, u32 offset, const T& value) {
        return write(streamNumber, offset, sizeof(T), (void*)&value);
    }

    // Returns a pointer to the request memory range.
    // Returns nullptr if the memory range [offset, offset + bytes] exists within
    // more than one block.
    // DO NOT write to an address beyond the range. You may write to blocks of other streams.
    u8* takePointer(u32 streamNumber, u32 offset, u32 bytes);

    ~PDBFile() {
        cleanup();
    }
    // Makes the struct as good as new (frees all memory).
    void cleanup();
    // Called if the pdb came from Deconstruct.
    static void Destroy(PDBFile* pdb);
    // Call Destroy when you are done with the PDB.
    // Deserialize is an alternative name for this function.
    static PDBFile* Deconstruct(const char* path);
    static PDBFile* CreateEmpty();

    // PDB header
    PDBHeader* getPDB() { return (PDBHeader*)takePointer(PDB_NUMBER, 0, sizeof(PDBHeader)); }
    // type info
    TPIHeader* getTPI() { return (TPIHeader*)takePointer(TPI_NUMBER, 0, sizeof(TPIHeader)); }
    // id info
    TPIHeader* getIPI() { return (TPIHeader*)takePointer(IPI_NUMBER, 0, sizeof(TPIHeader)); }
    
    static const u32 PDB_NUMBER = 1;
    static const u32 TPI_NUMBER = 2;
    static const u32 DBI_NUMBER = 3;
    static const u32 IPI_NUMBER = 4;

    /*
        Implementation
    */
    static const int BLOCK_SIZE = 0x1000;
   
    // TODO: We can have two distinctions in the implementation.
    //   One which is based on Deconstruct which uses _baseData and free page map
    //   The other could use a bucket array of blocks (instead of _baseData) and nextFreeBlock isntead of free page map.
    //   The second can be optimized but in the first one we have some restrictions because of the file we read.

    u8* _baseData = nullptr; // allocation
    u32 _baseSize = 0; // size of data allocation
    u32 _blockCount = 0;

    u32 validFPM = 1; // PDB from disc may use 2

    struct Stream {
        static const u32 STREAM_AVAILABLE = (u32)-1;
        u32 streamNumber = STREAM_AVAILABLE; // -1 indicates available/unused
        u32 size = 0;
        DynamicArray<u32> blockIndices;
    };
    DynamicArray<Stream> streams;

    // Returns -1 on failure
    u32 requestBlock();

    static const u32 MIN_BLOCK_COUNT = 5 + 5;
    bool reserveTotalBlocks(u32 newBlockCount);
    bool setFreeBlock(i32 blockIndex, bool beFree);
    bool isBlockFree(i32 blockIndex);
};
/*
    Older unused code.
    It's important because it's kind of working while everything else isn't (at the time of writing atow).

    Other stuff, mostly for testing.
*/
struct PDBFile_old {
    // I had a hard time getting the format right. The code in this function
    // was the first that successfully created a PDB.
    static bool WriteEmpty(const char* path);
    static PDBFile_old* Deconstruct(const char* path);

    u8* rawData = nullptr;
    u32 rawSize = 0;

    u32 block_of_streamTable = -1;

    MSFHeader* msfHeader = nullptr;
    PDBHeader* pdbHeader = nullptr;
    TPIHeader* tpiHeader = nullptr;
    TPIHeader* ipiHeader = nullptr;

    struct StreamOld {
        u32 byteSize;
        QuickArray<u32> blockIndices;
    };
    DynamicArray<StreamOld*> streams2;
    DynamicArray<StreamOld*> streams_old;
};

const char* ToString(LeafType type, bool nullAsUnknown = false);
const char* ToString(SubSectionType type, bool nullAsUnknown = false);
const char* ToString(RecordType type, bool nullAsUnknown = false);
void DeconstructDebugSymbols(u8* buffer, u32 size);
void DeconstructDebugTypes(u8* buffer, u32 size, bool fromPDB = false);
