#pragma once

#include "Engone/PlatformLayer.h"
#include "Engone/Util/Array.h"

#include "Engone/Util/Stream.h"
// #include "BetBat/PDB.h"

#include "BetBat/Program.h"

namespace coff {
    // This information comes from this site:
    // https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
    // I will refer to this site as MSDOC

    enum Machine_Flags : u16 {
        MACHINE_ZERO=0,
        IMAGE_FILE_MACHINE_AMD64=0x8664, // x64
    };
    // MSDOC calls this Characteristics
    // This collides with some structs' member names.
    enum COFF_Header_Flags : u16 {
        CHARACTERISTICS_ZERO=0,
        IMAGE_FILE_RELOCS_STRIPPED = 0x0001, // Image only, Windows CE, and Microsoft Windows NT and later. This indicates that the file does not contain base relocations and must therefore be loaded at its preferred base address. If the base address is not available, the loader reports an error. The default behavior of the linker is to strip base relocations from executable (EXE) files.
        IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002, // Image only. This indicates that the image file is valid and can be run. If this flag is not set, it indicates a linker error.
        IMAGE_FILE_LINE_NUMS_STRIPPED = 0x0004, // COFF line numbers have been removed. This flag is deprecated and should be zero.
        IMAGE_FILE_LOCAL_SYMS_STRIPPED = 0x0008, // COFF symbol table entries for local symbols have been removed. This flag is deprecated and should be zero.
        IMAGE_FILE_AGGRESSIVE_WS_TRIM = 0x0010, // Obsolete. Aggressively trim working set. This flag is deprecated for Windows 2000 and later and must be zero.
        IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020, // Application can handle > 2-GB addresses. = 0x0040, // This flag is reserved for future use.
        IMAGE_FILE_BYTES_REVERSED_LO = 0x0080, // Little endian: the least significant bit (LSB) precedes the most significant bit (MSB) in memory. This flag is deprecated and should be zero.
        IMAGE_FILE_32BIT_MACHINE = 0x0100, // Machine is based on a 32-bit-word architecture.
        IMAGE_FILE_DEBUG_STRIPPED = 0x0200, // Debugging information is removed from the image file.
        IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP = 0x0400, // If the image is on removable media, fully load it and copy it to the swap file.
        IMAGE_FILE_NET_RUN_FROM_SWAP = 0x0800, // If the image is on network media, fully load it and copy it to the swap file.
        IMAGE_FILE_SYSTEM = 0x1000, // The image file is a system file, not a user program.
        IMAGE_FILE_DLL = 0x2000, // The image file is a dynamic-link library (DLL). Such files are considered executable files for almost all purposes, although they cannot be directly run.
        IMAGE_FILE_UP_SYSTEM_ONLY = 0x4000, // The file should be run only on a uniprocessor machine.
        IMAGE_FILE_BYTES_REVERSED_HI = 0x8000, // Big endian: the MSB precedes the LSB in memory. This flag is deprecated and should be zero.
    };
    #pragma pack(push, 1)
    struct COFF_File_Header {
        static const u32 SIZE = 20;
        Machine_Flags Machine=MACHINE_ZERO;
        u16 NumberOfSections=0;
        u32 TimeDateStamp=0;
        u32 PointerToSymbolTable=0;
        u32 NumberOfSymbols=0;
        u16 SizeOfOptionalHeader = 0; // always zero for object files (not executables)
        COFF_Header_Flags Characteristics=CHARACTERISTICS_ZERO;
    };
    #pragma pack(pop)
    enum Section_Flags : u32 {
        SECTION_FLAG_ZERO = 0,

        IMAGE_SCN_TYPE_NO_PAD = 0x00000008, // The section should not be padded to the next boundary. This flag is obsolete and is replaced by IMAGE_SCN_ALIGN_1BYTES. This is valid only for object files.
        IMAGE_SCN_CNT_CODE = 0x00000020, // The section contains executable code.
        IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040, // The section contains initialized data.
        IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080, // The section contains uninitialized data.
        IMAGE_SCN_LNK_OTHER = 0x00000100, // Reserved for future use.
        IMAGE_SCN_LNK_INFO = 0x00000200, // The section contains comments or other information. The .drectve section has this type. This is valid for object files only.
        IMAGE_SCN_LNK_REMOVE = 0x00000800, // The section will not become part of the image. This is valid only for object files.
        IMAGE_SCN_LNK_COMDAT = 0x00001000, // The section contains COMDAT data. For more information, see COMDAT Sections (Object Only). This is valid only for object files.
        IMAGE_SCN_GPREL = 0x00008000, // The section contains data referenced through the global pointer (GP).
        IMAGE_SCN_MEM_PURGEABLE = 0x00020000, // Reserved for future use.
        IMAGE_SCN_MEM_16BIT = 0x00020000, // Reserved for future use.
        IMAGE_SCN_MEM_LOCKED = 0x00040000, // Reserved for future use.
        IMAGE_SCN_MEM_PRELOAD = 0x00080000, // Reserved for future use.
        IMAGE_SCN_ALIGN_1BYTES = 0x00100000, // Align data on a 1-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_2BYTES = 0x00200000, // Align data on a 2-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_4BYTES = 0x00300000, // Align data on a 4-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_8BYTES = 0x00400000, // Align data on an 8-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_16BYTES = 0x00500000, // Align data on a 16-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_32BYTES = 0x00600000, // Align data on a 32-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_64BYTES = 0x00700000, // Align data on a 64-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_128BYTES = 0x00800000, // Align data on a 128-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_256BYTES = 0x00900000, // Align data on a 256-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_512BYTES = 0x00A00000, // Align data on a 512-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_1024BYTES = 0x00B00000, // Align data on a 1024-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_2048BYTES = 0x00C00000, // Align data on a 2048-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_4096BYTES = 0x00D00000, // Align data on a 4096-byte boundary. Valid only for object files.
        IMAGE_SCN_ALIGN_8192BYTES = 0x00E00000, // Align data on an 8192-byte boundary. Valid only for object files.
        IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000, // The section contains extended relocations.
        IMAGE_SCN_MEM_DISCARDABLE = 0x02000000, // The section can be discarded as needed.
        IMAGE_SCN_MEM_NOT_CACHED = 0x04000000, // The section cannot be cached.
        IMAGE_SCN_MEM_NOT_PAGED = 0x08000000, // The section is not pageable.
        IMAGE_SCN_MEM_SHARED = 0x10000000, // The section can be shared in memory.
        IMAGE_SCN_MEM_EXECUTE = 0x20000000, // The section can be executed as code.
        IMAGE_SCN_MEM_READ = 0x40000000, // The section can be read.
        IMAGE_SCN_MEM_WRITE = 0x80000000, // The section can be written to.
    };
    
    #pragma pack(push, 1)
    // section table entry
    struct Section_Header {
        static const u32 SIZE = 40;
        char Name[8]; // null terminated unless name is exactly 8 bytes. Longer names exist in the string table. They are referred to like this "/index\0" (ex, "/23\0")
        u32 VirtualSize=0; // 0 for object files
        u32 VirtualAddress=0; // should be 0 for simplicity, it offsets relocations but isn't necessary.
        u32 SizeOfRawData=0; // size of section data
        u32 PointerToRawData=0; // 4-byte aligned for best performance
        u32 PointerToRelocations=0;
        u32 PointerToLineNumbers=0;
        u16 NumberOfRelocations=0;
        u16 NumberOfLineNumbers=0;
        Section_Flags Characteristics=SECTION_FLAG_ZERO;
    };
    #pragma pack(pop)
    enum Type_Indicator : u16 {
        TYPE_INDICATOR_ZERO = 0,
        IMAGE_REL_AMD64_ABSOLUTE = 0x0000, // The relocation is ignored.
        IMAGE_REL_AMD64_ADDR64 = 0x0001, // The 64-bit VA of the relocation target.
        IMAGE_REL_AMD64_ADDR32 = 0x0002, // The 32-bit VA of the relocation target.
        IMAGE_REL_AMD64_ADDR32NB = 0x0003, // The 32-bit address without an image base (RVA).
        IMAGE_REL_AMD64_REL32 = 0x0004, // The 32-bit relative address from the byte following the relocation.
        IMAGE_REL_AMD64_REL32_1 = 0x0005, // The 32-bit address relative to byte distance 1 from the relocation.
        IMAGE_REL_AMD64_REL32_2 = 0x0006, // The 32-bit address relative to byte distance 2 from the relocation.
        IMAGE_REL_AMD64_REL32_3 = 0x0007, // The 32-bit address relative to byte distance 3 from the relocation.
        IMAGE_REL_AMD64_REL32_4 = 0x0008, // The 32-bit address relative to byte distance 4 from the relocation.
        IMAGE_REL_AMD64_REL32_5 = 0x0009, // The 32-bit address relative to byte distance 5 from the relocation.
        IMAGE_REL_AMD64_SECTION = 0x000A, // The 16-bit section index of the section that contains the target. This is used to support debugging information.
        IMAGE_REL_AMD64_SECREL = 0x000B, // The 32-bit offset of the target from the beginning of its section. This is used to support debugging information and static thread local storage.
        IMAGE_REL_AMD64_SECREL7 = 0x000C, // A 7-bit unsigned offset from the base of the section that contains the target.
        IMAGE_REL_AMD64_TOKEN = 0x000D, // CLR tokens.
        IMAGE_REL_AMD64_SREL32 = 0x000E, // A 32-bit signed span-dependent value emitted into the object.
        IMAGE_REL_AMD64_PAIR = 0x000F, // A pair that must immediately follow every span-dependent value.
        IMAGE_REL_AMD64_SSPAN32 = 0x0010, // A 32-bit signed span-dependent value that is applied at link time.
    };
    
    #pragma pack(push, 1)
    // relocation entry/record
    struct COFF_Relocation {
        static const u32 SIZE = 10;
        u32 VirtualAddress=0;
        u32 SymbolTableIndex=0;
        Type_Indicator Type=TYPE_INDICATOR_ZERO;
    };
    #pragma pack(pop)
    enum Storage_Class : u8 {
        IMAGE_SYM_CLASS_END_OF_FUNCTION = (u8)-1, // A special symbol that represents the end of function, for debugging purposes.
        IMAGE_SYM_CLASS_NULL = 0, // No assigned storage class.
        IMAGE_SYM_CLASS_AUTOMATIC = 1, // The automatic (stack) variable. The Value field specifies the stack frame offset.
        IMAGE_SYM_CLASS_EXTERNAL = 2, // A value that Microsoft tools use for external symbols. The Value field indicates the size if the section number is IMAGE_SYM_UNDEFINED = 0, //  If the section number is not zero, then the Value field specifies the offset within the section.
        IMAGE_SYM_CLASS_STATIC = 3, // The offset of the symbol within the section. If the Value field is zero, then the symbol represents a section name.
        IMAGE_SYM_CLASS_REGISTER = 4, // A register variable. The Value field specifies the register number.
        IMAGE_SYM_CLASS_EXTERNAL_DEF = 5, // A symbol that is defined externally.
        IMAGE_SYM_CLASS_LABEL = 6, // A code label that is defined within the module. The Value field specifies the offset of the symbol within the section.
        IMAGE_SYM_CLASS_UNDEFINED_LABEL = 7, // A reference to a code label that is not defined.
        IMAGE_SYM_CLASS_MEMBER_OF_STRUCT = 8, // The structure member. The Value field specifies the n th member.
        IMAGE_SYM_CLASS_ARGUMENT = 9, // A formal argument (parameter) of a function. The Value field specifies the n th argument.
        IMAGE_SYM_CLASS_STRUCT_TAG = 10, // The structure tag-name entry.
        IMAGE_SYM_CLASS_MEMBER_OF_UNION = 11, // A union member. The Value field specifies the n th member.
        IMAGE_SYM_CLASS_UNION_TAG = 12, // The Union tag-name entry.
        IMAGE_SYM_CLASS_TYPE_DEFINITION = 13, // A Typedef entry.
        IMAGE_SYM_CLASS_UNDEFINED_STATIC = 14, // A static data declaration.
        IMAGE_SYM_CLASS_ENUM_TAG = 15, // An enumerated type tagname entry.
        IMAGE_SYM_CLASS_MEMBER_OF_ENUM = 16, // A member of an enumeration. The Value field specifies the n th member.
        IMAGE_SYM_CLASS_REGISTER_PARAM = 17, // A register parameter.
        IMAGE_SYM_CLASS_BIT_FIELD = 18, // A bit-field reference. The Value field specifies the n th bit in the bit field.
        IMAGE_SYM_CLASS_BLOCK = 100, // A .bb (beginning of block) or .eb (end of block) record. The Value field is the relocatable address of the code location.
        IMAGE_SYM_CLASS_FUNCTION = 101, // A value that Microsoft tools use for symbol records that define the extent of a function: begin function (.bf ), end function ( .ef ), and lines in function ( .lf ). For .lf records, the Value field gives the number of source lines in the function. For .ef records, the Value field gives the size of the function code.
        IMAGE_SYM_CLASS_END_OF_STRUCT = 102, // An end-of-structure entry.
        IMAGE_SYM_CLASS_FILE = 103, // A value that Microsoft tools, as well as traditional COFF format, use for the source-file symbol record. The symbol is followed by auxiliary records that name the file.
        IMAGE_SYM_CLASS_SECTION = 104, // A definition of a section (Microsoft tools use STATIC storage class instead).
        IMAGE_SYM_CLASS_WEAK_EXTERNAL = 105, // A weak external. For more information, see Auxiliary Format 3: Weak Externals.
        IMAGE_SYM_CLASS_CLR_TOKEN = 107, // A CLR token symbol. The name is an ASCII string that consists of the hexadecimal value of the token. For more information, see CLR Token Definition (Object Only).
    };
    enum Type_Representation_LSB : u8 { /* TODO: Add types */ };
    enum Type_Representation_MSB : u8 { 
        // These may be wrong but the object file has 32 as value for main symbol
        // main is supposed to be a function. That would be MSB. MSB is the second byte.
        // 256*2 which is 512. Is that not the value it should and not 32?
        // I am missing something. I guess I don't understand Little endian, MSB and LSB
        IMAGE_SYM_DTYPE_NULL = 0, // No derived type; the symbol is a simple scalar variable.
        IMAGE_SYM_DTYPE_POINTER = 0x10, // The symbol is a pointer to base type.
        IMAGE_SYM_DTYPE_FUNCTION = 0x20, // The symbol is a function that returns a base type.
        IMAGE_SYM_DTYPE_ARRAY = 0x30, // The symbol is an array of base type.
    };
    #pragma pack(push,1)
    struct Symbol_Record {
        static const u32 SIZE = 18;
        union {
            char ShortName[8];
            struct {
                u32 zero; // if zero then the name is longer than 8 bytes
                u32 offset; // offset into string table
            };
        } Name;
        u32 Value;
        i16 SectionNumber; // Starts from 1. A number less than that has special meaning like unknown/undefined
        // struct {
        //     Type_Representation_MSB complex;
        //     Type_Representation_LSB base;
        // }
        u16 Type;
        Storage_Class StorageClass;
        u8 NumberOfAuxSymbols;
    };
    struct Aux_Format_5 {
        static const u32 SIZE = Symbol_Record::SIZE;
        u32 Length=0;
        u16 NumberOfRelocations;
        u16 NumberOfLineNumbers;
        u32 CheckSum=0;
        u16 Number=0;
        u8 Selection=0;
        u8 Unused[3]{0};
    };
    #pragma pack(pop)

    // Below are some structs for exceptions on Windows.
    // Definitions can be found here: https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170
    struct RUNTIME_FUNCTION {
        u32 StartAddress;
        u32 EndAddress;
        u32 UnwindInfoAddress;
    };
    enum UnwindInfoFlags {
        UNW_FLAG_NHANDLER = 0x0,
        UNW_FLAG_EHANDLER = 0x1, // The function has an exception handler that should be called when looking for functions that need to examine exceptions.
        UNW_FLAG_UHANDLER = 0x2, // The function has a termination handler that should be called when unwinding an exception.
        UNW_FLAG_CHAININFO = 0x4, //This unwind info structure is not the primary one for the procedure. Instead, the chained unwind info entry is the contents of a previous RUNTIME_FUNCTION entry. For information, see Chained unwind info structures. If this flag is set, then the UNW_FLAG_EHANDLER and UNW_FLAG_UHANDLER flags must be cleared. Also, the frame register and fixed-stack allocation fields must have the same values as in the primary unwind info.
    };
    struct UNWIND_INFO {
        u8 Version : 3;
        u8 Flags : 5;
        u8 SizeOfProlog;
        u8 CountOfUnwindCodes;
        u8 FrameRegister : 4;
        u8 FrameRegisterOffset : 4; // (scaled)
        // u16 UnwindCodesArray[CountOfUnwindCodes];

        // variable	Can either be of form (1):
            // u32 AddressOfExceptionHandler;
            // variable	Language-specific handler data (optional)
        // or (2):
            // RUNTIME_FUNCTION ChainedUnwindInfo;
    };
    struct UNWIND_CODE {
        u8 OffsetInProlog;
        u8 UnwindOperationCode : 4;
        u8 OperationInfo : 4;
    };
    // Read about the operations here: https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170#unwind-operation-code
    enum UnwindOperation {
        UWOP_PUSH_NONVOL = 0, // 1 node
        UWOP_ALLOC_LARGE = 1, // 2 or 3 nodes
        UWOP_ALLOC_SMALL = 2, // 1 node
        UWOP_SET_FPREG = 3, // 1 node
        UWOP_SAVE_NONVOL = 4, // 2 nodes
        UWOP_SAVE_NONVOL_FAR = 5, // 3 nodes
        UWOP_SAVE_XMM128 = 8, // 2 nodes
        UWOP_SAVE_XMM128_FAR = 9, // 3 nodes
        UWOP_PUSH_MACHFRAME = 10, // 1 node
    };
    enum UnwindOpRegister {
    	UWOP_RAX = 0,
    	UWOP_RCX = 1,
    	UWOP_RDX = 2,
    	UWOP_RBX = 3,
    	UWOP_RSP = 4,
    	UWOP_RBP = 5,
    	UWOP_RSI = 6,
    	UWOP_RDI = 7,
        UWOP_R8 = 8, // To get R13 do: UWOP_R8 + (8 - N), where N = 13
        // 8 to 15	R8 to R15
    };

    // #######################
    //   NOT DEFINED BY COFF
    // ######################
    struct SectionSymbol {
        std::string name;
        int sectionNumber = 0;
        u32 symbolIndex = 0;
    };
}
struct FileCOFF {
    FileCOFF() = default;
    ~FileCOFF() {
        TRACK_ARRAY_FREE(_rawFileData, u8, fileSize);
        // engone::Free(_rawFileData,fileSize);
        _rawFileData=nullptr;
        fileSize=0;
    }
    u8* _rawFileData = nullptr;
    u64 fileSize = 0;

    coff::COFF_File_Header* header = nullptr;

    std::string getSectionName(int sectionIndex);
    std::string getSymbolName(int symbolIndex);
    QuickArray<coff::Section_Header*> sections{};

    QuickArray<coff::Symbol_Record*> symbols{};
    
    QuickArray<coff::COFF_Relocation*> text_relocations{};

    u32 stringTableSize = 0;
    char* stringTableData = nullptr;

    void writeFile(const std::string& path);

    static FileCOFF* DeconstructFile(const std::string& path, bool silent = true);

    static void Destroy(FileCOFF* objectFile);
    static bool WriteFile(const std::string& name, Program* program, u32 from = 0, u32 to = (u32)-1);
};

void DeconstructPData(u8* buffer, u32 size);
void DeconstructXData(u8* buffer, u32 size);