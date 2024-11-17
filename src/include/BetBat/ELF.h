/*
    ELF format used by Unix systems (COFF doesn't work so well)

    https://refspecs.linuxfoundation.org/elf/elf.pdf
    https://uclibc.org/docs/elf-64-gen.pdf
    
    Credit:
    https://gist.github.com/x0nu11byt3/bcb35c3de461e5fb66173071a2379779
    
    Might be useful:
    https://gist.github.com/DhavalKapil/2243db1b732b211d0c16fd5d9140ab0b

    ARM relocations
    https://refspecs.linuxbase.org/elf/ARMELFA08.pdf
*/
#pragma once

#include "Engone/PlatformLayer.h"
#include "Engone/Util/Array.h"
#include "Engone/Util/Stream.h"
#include "BetBat/Program.h"

namespace elf {
    
    typedef u16 Elf64_Half;
    typedef u32 Elf64_Word;
    typedef i32 Elf64_Sword;
    typedef u64 Elf64_Xword;
    typedef i64 Elf64_Sxword;
    typedef u64 Elf64_Addr;
    typedef u64 Elf64_Off;
    
    typedef u32 Elf32_Addr;
    typedef u16 Elf32_Half;
    typedef u32 Elf32_Off;
    typedef i32 Elf32_Sword;
    typedef u32 Elf32_Word;
    
    #define EI_CLASS 4
    #define EI_DATA 5
    #define EI_VERSION 6
    // OS/ABI identification
    #define EI_OSABI 7
    // ABI version
    #define EI_ABIVERSION 8
    // Start of padding bytes
    #define EI_PAD 9
    #define EI_NIDENT (16)

    #define ET_NONE		0		/* No file type */
    #define ET_REL		1		/* Relocatable file */
    #define ET_EXEC		2		/* Executable file */
    #define ET_DYN		3		/* Shared object file */
    #define ET_CORE		4		/* Core file */
    #define	ET_NUM		5		/* Number of defined types */
    #define ET_LOOS		0xfe00		/* OS-specific range start */
    #define ET_HIOS		0xfeff		/* OS-specific range end */
    #define ET_LOPROC	0xff00		/* Processor-specific range start */
    #define ET_HIPROC	0xffff		/* Processor-specific range end */

    #define EM_NONE		 0	/* No machine */
    #define EM_386		 3	/* Intel 80386 */
    #define EM_ARM		40	/* ARM */
    #define EM_X86_64	62	/* AMD x86-64 architecture */
    #define EM_AARCH64	183	/* ARM AARCH64 */
    
    #define EV_NONE		0		/* Invalid ELF version */
    #define EV_CURRENT	1		/* Current version */
    #define EV_NUM		2
    
    typedef struct {
        unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
        Elf64_Half	e_type;			/* Object file type */
        Elf64_Half	e_machine;		/* Architecture */
        Elf64_Word	e_version;		/* Object file version */
        Elf64_Addr	e_entry;		/* Entry point virtual address */
        Elf64_Off	e_phoff;		/* Program header table file offset */
        Elf64_Off	e_shoff;		/* Section header table file offset */
        Elf64_Word	e_flags;		/* Processor-specific flags */
        Elf64_Half	e_ehsize;		/* ELF header size in bytes */
        Elf64_Half	e_phentsize;		/* Program header table entry size */
        Elf64_Half	e_phnum;		/* Program header table entry count */
        Elf64_Half	e_shentsize;		/* Section header table entry size */
        Elf64_Half	e_shnum;		/* Section header table entry count */
        Elf64_Half	e_shstrndx;		/* Section header string table index */
    } Elf64_Ehdr; // NOTE: The struct is perfectly aligned to 8 bytes
    typedef struct {
        unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
        Elf32_Half	e_type;			/* Object file type */
        Elf32_Half	e_machine;		/* Architecture */
        Elf32_Word	e_version;		/* Object file version */
        Elf32_Addr	e_entry;		/* Entry point virtual address */
        Elf32_Off	e_phoff;		/* Program header table file offset */
        Elf32_Off	e_shoff;		/* Section header table file offset */
        Elf32_Word	e_flags;		/* Processor-specific flags */
        Elf32_Half	e_ehsize;		/* ELF header size in bytes */
        Elf32_Half	e_phentsize;		/* Program header table entry size */
        Elf32_Half	e_phnum;		/* Program header table entry count */
        Elf32_Half	e_shentsize;		/* Section header table entry size */
        Elf32_Half	e_shnum;		/* Section header table entry count */
        Elf32_Half	e_shstrndx;		/* Section header string table index */
    } Elf32_Ehdr; // NOTE: The struct is perfectly aligned to 8 bytes
    
    /* ################
          sh_type    
    ################ */
    #define SHT_NULL	  0		/* Section header table entry unused */
    #define SHT_PROGBITS	  1		/* Program data */
    #define SHT_SYMTAB	  2		/* Symbol table */
    #define SHT_STRTAB	  3		/* String table */
    #define SHT_RELA	  4		/* Relocation entries with addends */
    #define SHT_HASH	  5		/* Symbol hash table */
    #define SHT_DYNAMIC	  6		/* Dynamic linking information */
    #define SHT_NOTE	  7		/* Notes */
    #define SHT_NOBITS	  8		/* Program space with no data (bss) */
    #define SHT_REL		  9		/* Relocation entries, no addends */
    #define SHT_SHLIB	  10		/* Reserved */
    #define SHT_DYNSYM	  11		/* Dynamic linker symbol table */
    #define SHT_INIT_ARRAY	  14		/* Array of constructors */
    #define SHT_FINI_ARRAY	  15		/* Array of destructors */
    #define SHT_PREINIT_ARRAY 16		/* Array of pre-constructors */
    #define SHT_GROUP	  17		/* Section group */
    #define SHT_SYMTAB_SHNDX  18		/* Extended section indeces */
    #define	SHT_NUM		  19		/* Number of defined types.  */
    #define SHT_LOOS	  0x60000000	/* Start OS-specific.  */
    #define SHT_GNU_ATTRIBUTES 0x6ffffff5	/* Object attributes.  */
    #define SHT_GNU_HASH	  0x6ffffff6	/* GNU-style hash table.  */
    #define SHT_GNU_LIBLIST	  0x6ffffff7	/* Prelink library list */
    #define SHT_CHECKSUM	  0x6ffffff8	/* Checksum for DSO content.  */
    #define SHT_LOSUNW	  0x6ffffffa	/* Sun-specific low bound.  */
    #define SHT_SUNW_move	  0x6ffffffa
    #define SHT_SUNW_COMDAT   0x6ffffffb
    #define SHT_SUNW_syminfo  0x6ffffffc
    #define SHT_GNU_verdef	  0x6ffffffd	/* Version definition section.  */
    #define SHT_GNU_verneed	  0x6ffffffe	/* Version needs section.  */
    #define SHT_GNU_versym	  0x6fffffff	/* Version symbol table.  */
    #define SHT_HISUNW	  0x6fffffff	/* Sun-specific high bound.  */
    #define SHT_HIOS	  0x6fffffff	/* End OS-specific type */
    #define SHT_LOPROC	  0x70000000	/* Start of processor-specific */
    #define SHT_ARM_ATTRIBUTES 0x70000003
    #define SHT_HIPROC	  0x7fffffff	/* End of processor-specific */
    #define SHT_LOUSER	  0x80000000	/* Start of application-specific */
    #define SHT_HIUSER	  0x8fffffff	/* End of application-specific */
    
    
    #define SHF_WRITE	     (1 << 0)	/* Writable */
    #define SHF_ALLOC	     (1 << 1)	/* Occupies memory during execution */
    #define SHF_EXECINSTR	     (1 << 2)	/* Executable */
    #define SHF_MERGE	     (1 << 4)	/* Might be merged */
    #define SHF_STRINGS	     (1 << 5)	/* Contains nul-terminated strings */
    #define SHF_INFO_LINK	     (1 << 6)	/* `sh_info' contains SHT index */
    #define SHF_LINK_ORDER	     (1 << 7)	/* Preserve order after combining */
    #define SHF_OS_NONCONFORMING (1 << 8)	/* Non-standard OS specific handling
                        required */
    #define SHF_GROUP	     (1 << 9)	/* Section is member of a group.  */
    #define SHF_TLS		     (1 << 10)	/* Section hold thread-local data.  */
    #define SHF_COMPRESSED	     (1 << 11)	/* Section with compressed data. */
    #define SHF_MASKOS	     0x0ff00000	/* OS-specific.  */
    #define SHF_MASKPROC	     0xf0000000	/* Processor-specific */
    #define SHF_ORDERED	     (1 << 30)	/* Special ordering requirement
                        (Solaris).  */
    #define SHF_EXCLUDE	     (1U << 31)	/* Section is excluded unless
                        referenced or allocated (Solaris).*/
    
    typedef struct {
        Elf64_Word	sh_name;		/* Section name (string tbl index) */
        Elf64_Word	sh_type;		/* Section type */
        Elf64_Xword	sh_flags;		/* Section flags */
        Elf64_Addr	sh_addr;		/* Section virtual addr at execution */
        Elf64_Off	sh_offset;		/* Section file offset */
        Elf64_Xword	sh_size;		/* Section size in bytes */
        Elf64_Word	sh_link;		/* Link to another section */
        Elf64_Word	sh_info;		/* Additional section information */
        Elf64_Xword	sh_addralign;		/* Section alignment */
        Elf64_Xword	sh_entsize;		/* Entry size if section holds table */
    } Elf64_Shdr;
    typedef struct {
        Elf32_Word	sh_name;		/* Section name (string tbl index) */
        Elf32_Word	sh_type;		/* Section type */
        Elf32_Word	sh_flags;		/* Section flags */
        Elf32_Addr	sh_addr;		/* Section virtual addr at execution */
        Elf32_Off	sh_offset;		/* Section file offset */
        Elf32_Word	sh_size;		/* Section size in bytes */
        Elf32_Word	sh_link;		/* Link to another section */
        Elf32_Word	sh_info;		/* Additional section information */
        Elf32_Word	sh_addralign;		/* Section alignment */
        Elf32_Word	sh_entsize;		/* Entry size if section holds table */
    } Elf32_Shdr;
    
    #define ELF32_ST_BIND(val)		(((unsigned char) (val)) >> 4)
    #define ELF32_ST_TYPE(val)		((val) & 0xf)
    #define ELF32_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

    #define ELF64_ST_BIND(val)		ELF32_ST_BIND (val)
    #define ELF64_ST_TYPE(val)		ELF32_ST_TYPE (val)
    #define ELF64_ST_INFO(bind, type)	ELF32_ST_INFO ((bind), (type))
    
    #define SHN_UNDEF       0
    #define SHN_LORESERVE   0xff00
    #define SHN_LOPROC      0xff00
    #define SHN_HIPROC      0xff1f
    #define SHN_ABS         0xfff1
    #define SHN_COMMON      0xfff2
    #define SHN_HIRESERVE   0xffff
    
    #define STB_LOCAL	0		/* Local symbol */
    #define STB_GLOBAL	1		/* Global symbol */
    #define STB_WEAK	2		/* Weak symbol */
    #define	STB_NUM		3		/* Number of defined types.  */
    #define STB_LOOS	10		/* Start of OS-specific */
    #define STB_GNU_UNIQUE	10		/* Unique symbol.  */
    #define STB_HIOS	12		/* End of OS-specific */
    #define STB_LOPROC	13		/* Start of processor-specific */
    #define STB_HIPROC	15		/* End of processor-specific */
    
    #define STT_NOTYPE	0		/* Symbol type is unspecified */
    #define STT_OBJECT	1		/* Symbol is a data object */
    #define STT_FUNC	2		/* Symbol is a code object */
    #define STT_SECTION	3		/* Symbol associated with a section */
    #define STT_FILE	4		/* Symbol's name is file name */
    #define STT_COMMON	5		/* Symbol is a common data object */
    #define STT_TLS		6		/* Symbol is thread-local data object*/
    #define	STT_NUM		7		/* Number of defined types.  */
    #define STT_LOOS	10		/* Start of OS-specific */
    #define STT_GNU_IFUNC	10		/* Symbol is indirect code object */
    #define STT_HIOS	12		/* End of OS-specific */
    #define STT_LOPROC	13		/* Start of processor-specific */
    #define STT_HIPROC	15		/* End of processor-specific */
    
    typedef struct {
        Elf64_Word	st_name;		/* Symbol name (string tbl index) */
        unsigned char	st_info;		/* Symbol type and binding */
        unsigned char st_other;		/* Symbol visibility */
        Elf64_Half	st_shndx;		/* Section index */
        Elf64_Addr	st_value;		/* Symbol value */
        Elf64_Xword	st_size;		/* Symbol size */
    } Elf64_Sym;
    typedef struct {
        Elf32_Word	st_name;		/* Symbol name (string tbl index) */
        Elf32_Addr	st_value;		/* Symbol value */
        Elf32_Word	st_size;		/* Symbol size */
        unsigned char st_info;		/* Symbol type and binding */
        unsigned char st_other;		/* Symbol visibility */
        Elf32_Half	st_shndx;		/* Section index */
    } Elf32_Sym;
    
    #define R_X86_64_PC64 24 // field: dword, calc: S + A - P
    #define R_X86_64_PC32 2 // field: dword, calc: S + A - P
    #define R_X86_64_PLT32 4 // field: dword, calc: L + A - P
    #define R_X86_64_32 10 // field: dword, calc: S + A
    #define R_X86_64_64 1 // field: qword, calc: S + A

    #define ELF32_R_SYM(i)	((i)>>8)
	#define ELF32_R_TYPE(i)   ((unsigned char)(i))
	#define ELF32_R_INFO(s,t) (((s)<<8)+(unsigned char)(t))

	#define ELF64_R_SYM(i)    ((i)>>32)
	#define ELF64_R_TYPE(i)   ((i)&0xffffffffL)
	#define ELF64_R_INFO(s,t) (((u64)(s)<<32)+((u64)(t)&0xffffffffL))
    
    typedef struct {
        Elf64_Addr	r_offset;		/* Address */
        Elf64_Xword	r_info;			/* Relocation type and symbol index */
    } Elf64_Rel;
    typedef struct {
        Elf32_Addr	r_offset;		/* Address */
        Elf32_Word	r_info;			/* Relocation type and symbol index */
    } Elf32_Rel;
    
    typedef struct {
        Elf64_Addr	r_offset;		/* Address */
        Elf64_Xword	r_info;			/* Relocation type and symbol index */
        Elf64_Sxword	r_addend;		/* Addend */
    } Elf64_Rela;
    typedef struct {
        Elf32_Addr	r_offset;		/* Address */
        Elf32_Word	r_info;			/* Relocation type and symbol index */
        Elf32_Sword	r_addend;		/* Addend */
    } Elf32_Rela;

    // #define	PT_NULL		0		/* Program header table entry unused */
    // #define PT_LOAD		1		/* Loadable program segment */
    // #define PT_DYNAMIC	2		/* Dynamic linking information */
    // #define PT_INTERP	3		/* Program interpreter */
    // #define PT_NOTE		4		/* Auxiliary information */
    // #define PT_SHLIB	5		/* Reserved */
    // #define PT_PHDR		6		/* Entry for header table itself */
    // #define PT_TLS		7		/* Thread-local storage segment */
    // #define	PT_NUM		8		/* Number of defined types */
    // #define PT_LOOS		0x60000000	/* Start of OS-specific */
    // #define PT_GNU_EH_FRAME	0x6474e550	/* GCC .eh_frame_hdr segment */
    // #define PT_GNU_STACK	0x6474e551	/* Indicates stack executability */
    // #define PT_GNU_RELRO	0x6474e552	/* Read-only after relocation */
    // #define PT_LOSUNW	0x6ffffffa
    // #define PT_SUNWBSS	0x6ffffffa	/* Sun Specific segment */
    // #define PT_SUNWSTACK	0x6ffffffb	/* Stack segment */
    // #define PT_HISUNW	0x6fffffff
    // #define PT_HIOS		0x6fffffff	/* End of OS-specific */
    // #define PT_LOPROC	0x70000000	/* Start of processor-specific */
    // #define PT_HIPROC	0x7fffffff	/* End of processor-specific */
    
    // #define PF_X		(1 << 0)	/* Segment is executable */
    // #define PF_W		(1 << 1)	/* Segment is writable */
    // #define PF_R		(1 << 2)	/* Segment is readable */
    // #define PF_MASKOS	0x0ff00000	/* OS-specific */
    // #define PF_MASKPROC	0xf0000000	/* Processor-specific */
    
    // typedef struct {
    //     Elf64_Word	p_type;			/* Segment type */
    //     Elf64_Word	p_flags;		/* Segment flags */
    //     Elf64_Off	p_offset;		/* Segment file offset */
    //     Elf64_Addr	p_vaddr;		/* Segment virtual address */
    //     Elf64_Addr	p_paddr;		/* Segment physical address */
    //     Elf64_Xword	p_filesz;		/* Segment size in file */
    //     Elf64_Xword	p_memsz;		/* Segment size in memory */
    //     Elf64_Xword	p_align;		/* Segment alignment */
    // } Elf64_Phdr;

    #define R_ARM_NONE        0          //  Any No relocation. Encodes dependencies between sections.
    #define R_ARM_PC24        1          //  ARM B/BL S – P + A
    #define R_ARM_ABS32       2         //  32-bit word S + A
    #define R_ARM_REL32       3         //  32-bit word S – P + A
    #define R_ARM_PC13        4          //  ARM LDR r, [pc,…] S–P+A
    #define R_ARM_ABS16       5         //  16-bit half-word S + A
    #define R_ARM_ABS12       6         //  ARM LDR/STR S+A
    #define R_ARM_THM_ABS5    7      //  Thumb LDR/STR S+A
    #define R_ARM_ABS8        8          //  8-bit byte S + A
    #define R_ARM_SBREL32     9       //  32-bit word S – B + A
    #define R_ARM_THM_PC22    10     // Thumb BL pair S – P+ A
    #define R_ARM_THM_PC8     11      // Thumb LDR r, [pc,…] S–P+A
    #define R_ARM_AMP_VCALL9  12   // AMP VCALL S – B + A
    #define R_ARM_SWI24       13        // ARM SWI S + A
    #define R_ARM_THM_SWI8    14     // Thumb SWI S + A
    #define R_ARM_XPC25       15        // ARM BLX S – P+ A
    #define R_ARM_THM_XPC22   16    // Thumb BLX pair S – P+ A

    #define R_ARM_V4BX        40
}

struct FileELF {
    FileELF() = default;
    ~FileELF() {
        TRACK_ARRAY_FREE(_rawFileData, u8, fileSize);
        // engone::Free(_rawFileData,fileSize);
        _rawFileData=nullptr;
        fileSize=0;
    }
    u8* _rawFileData = nullptr;
    u64 fileSize = 0;
    bool is_32bit = false;

    elf::Elf64_Ehdr* header = nullptr;
    elf::Elf64_Shdr* sections = nullptr;
    elf::Elf32_Ehdr* header32 = nullptr;
    elf::Elf32_Shdr* sections32 = nullptr;
    int sections_count = 0;
    char* section_names = nullptr;
    int section_names_size = 0;
    
    // returns 0 if not found or invalid section
    int sectionIndexByName(const std::string& name) const;
    const char* nameOfSection(int sectionIndex) const;
    const u8* dataOfSection(int sectionIndex, int* out_size) const;
    
    const elf::Elf64_Rela* relaOfSection(int sectionIndex, int* out_count) const;
    const elf::Elf32_Rela* relaOfSection32(int sectionIndex, int* out_count) const;

    // QuickArray<elf::Elf64_Shdr*> sections{};
    // QuickArray<elf::Elf64_Sym*> symbols{};

    // u32 stringTableSize = 0;
    // char* stringTableData = nullptr;

    void writeFile(const std::string& path);

    static FileELF* DeconstructFile(const std::string& path, bool silent = true);
    static void Destroy(FileELF* elfFile);

    static bool WriteFile(const std::string& name, Program* program, u32 from = 0, u32 to = (u32)-1);
};

// Constants for .ARM.attributes
#define Tag_File                       1  // u32
// #define Tag_Section                    2  // u32
// #define Tag_Symbol                     3  // u32
#define Tag_CPU_raw_name               4  // NTBS         r2.0	 
#define Tag_CPU_name                   5  // NTBS         r2.0	 
#define Tag_CPU_arch                   6  // uleb128      r2.0	r2.06: Added enum values for v6-M, v6S-M; r2.08: Added enum value for v7E-M. r2.09: Added enum value for v8.
#define Tag_CPU_arch_profile           7  // uleb128      r2.0	r2.05: Added ‘S’ denoting ‘A’ or ‘R’ (don’t care)
#define Tag_ARM_ISA_use                8  // uleb128      r2.0	 
#define Tag_THUMB_ISA_use              9  // uleb128      r2.0	 
#define Tag_FP_arch                    10 // uleb128      r2.0	r2.04: Added enum value for VFPv3 r2.06: Added enum value for VFPv3 restricted to D0-D15 r2.08: Renamed; added enum value for VFPv4 (adds fused MAC + 16-bit FP data in memory). r2.09: Added enum value for v8-A FP
#define Tag_WMMX_arch                  11 // uleb128      r2.0	r2.02: Added enum value for wireless MMX v2.
#define Tag_Advanced_SIMD_arch         12 // uleb128      r2.0	r2.08: Added enum value for Neon with fused MAC
#define Tag_PCS_config                 13 // uleb128      r2.0	 
#define Tag_ABI_PCS_R9_use             14 // uleb128      r2.0	r2.08: Clarified that value = 2 denotes using R9 to emulate one of the architecturally defined thread ID registers in CP15 c13.
#define Tag_ABI_PCS_RW_data            15 // uleb128      r2.0	 
#define Tag_ABI_PCS_RO_data            16 // uleb128      r2.0	 
#define Tag_ABI_PCS_GOT_use            17 // uleb128      r2.0	 
#define Tag_ABI_PCS_wchar_t            18 // uleb128      r2.0	 
#define Tag_ABI_FP_rounding            19 // uleb128      r2.0	 
#define Tag_ABI_FP_denormal            20 // uleb128      r2.0	 
#define Tag_ABI_FP_exceptions          21 // uleb128      r2.0	r2.09: fixed typo (missing ‘permitted’)
#define Tag_ABI_FP_user_exceptions     22 // uleb128      r2.0	 
#define Tag_ABI_FP_number_model        23 // uleb128      r2.0	 
#define Tag_ABI_align_needed           24 // uleb128      r2.0	r2.08: Generalized to extended alignment of 2n bytes for n in 4..12
#define Tag_ABI_align8_preserved       25 // uleb128      r2.0	r2.08: Generalized to extended alignment of 2n bytes for n in 4..12
#define Tag_ABI_enum_size              26 // uleb128      r2.0	 
#define Tag_ABI_HardFP_use             27 // uleb128      r2.0	r2.09: Clarified use of existing values and removed DP-only option
#define Tag_ABI_VFP_args               28 // uleb128      r2.0	r2.09: Added enum value for code compatible with both the base and VFP variants
#define Tag_ABI_WMMX_args              29 // uleb128      r2.0	 
#define Tag_ABI_optimization_goals     30 // uleb128      r2.0	 
#define Tag_ABI_FP_optimization_goals  31 // uleb128      r2.0	 
#define Tag_compatibility              32 // NTBS         r2.0	r2.05: Revised the ineffective definition. The new one is compatible with the old one if a nonsensical clause in the old one is ignored.
#define Tag_CPU_unaligned_access       34 // uleb128      r2.02	 
#define Tag_FP_HP_extension            36 // uleb128      r2.06	r2.08: Renamed (VFP → FP) r2.09: Clarified use of existing values.
#define Tag_ABI_FP_16bit_format        38 // uleb128      r2.06	 
#define Tag_MPextension_use            42 // uleb128      r2.08	 
#define Tag_DIV_use                    44 // uleb128      r2.08	r2.09: Changed wording to clarify meaning.
#define Tag_DSP_extension              46 // uleb128      2018q4	 
#define Tag_MVE_arch                   48 // uleb128      2019q4	 
#define Tag_PAC_extension              50 // uleb128      2021q1	 
#define Tag_BTI_extension              52 // uleb128      2021q1	 
#define Tag_nodefaults                 64 // uleb128      r2.06	r2.07: Re-specified tag value inheritance more precisely (Inheritance of public tag values and No defaults tag). In the absence of Tag_nodefaults the specification reduces to that used before its introduction. We believe that only Arm tools are affected. r2.09: Use deprecated as Tag_Section and Tag_Symbol are deprecated.
#define Tag_also_compatible_with       65 // NTBS         r2.05	r2.06: Restricted usage as noted in Secondary compatibility tag. r2.09: Clarified data format.
#define Tag_conformance                67 // NTBS         r2.05	 
// #define Tag_T2EE_use                   66 // uleb128      r2.07	r2.09: Deprecated as not useful
#define Tag_Virtualization_use         68 // uleb128      r2.07	r2.08: Added two enumeration values to support the 2009 virtualization extensions.
// #define Tag_MPextension_use            70 // uleb128      r2.07	r2.08: Recoded to 42 (must be recognized by toolchains). PLDW is a user-mode instruction, undefined in architecture v7 w/o MP extensions.
#define Tag_FramePointer_use           72 // uleb128      2019q2	 
#define Tag_BTI_use                    74 // uleb128      2021q1	 
#define Tag_PACRET_use                 76 // uleb128      2021q1

// values for Tag_CPU_arch
#define Pre_v4                0 // Pre-v4
#define Arm_v4                1 // Arm v4     // e.g. SA110
#define Arm_v4T               2 // Arm v4T    // e.g. Arm7TDMI
#define Arm_v5T               3 // Arm v5T    // e.g. Arm9TDMI
#define Arm_v5TE              4 // Arm v5TE   // e.g. Arm946E-S
#define Arm_v5TEJ             5 // Arm v5TEJ  // e.g. Arm926EJ-S
#define Arm_v6                6 // Arm v6     // e.g. Arm1136J-S
#define Arm_v6KZ              7 // Arm v6KZ   // e.g. Arm1176JZ-S
#define Arm_v6T2              8 // Arm v6T2   // e.g. Arm1156T2F-S
#define Arm_v6K               9 // Arm v6K    // e.g. Arm1136J-S
#define Arm_v7               10 // Arm v7     // e.g. Cortex-A8, Cortex-M3
#define Arm_v6_M             11 // Arm v6-M   // e.g. Cortex-M1
#define Arm_v6S_M            12 // Arm v6S-M  // v6-M with the System extensions
#define Arm_v7E_M            13 // Arm v7E-M  // v7-M with DSP extensions
#define Arm_v8_A             14 // Arm v8-A
#define Arm_v8_R             15 // Arm v8-R
#define Arm_v8_M_baseline    16 // Arm v8-M.baseline
#define Arm_v8_M_mainline    17 // Arm v8-M.mainline
#define Arm_v8_1_A           18 // Arm v8.1-A
#define Arm_v8_2_A           19 // Arm v8.2-A
#define Arm_v8_3_A           20 // Arm v8.3-A
#define Arm_v8_1_M_mainline  21 // Arm v8.1-M.mainline
#define Arm_v9               22 // Arm v9-A
