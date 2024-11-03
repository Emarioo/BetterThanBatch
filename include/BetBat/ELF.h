/*
    ELF format used by Unix systems (COFF doesn't work so well)

    https://refspecs.linuxfoundation.org/elf/elf.pdf
    https://uclibc.org/docs/elf-64-gen.pdf
    
    Credit:
    https://gist.github.com/x0nu11byt3/bcb35c3de461e5fb66173071a2379779
    
    Might be useful:
    https://gist.github.com/DhavalKapil/2243db1b732b211d0c16fd5d9140ab0b
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

    elf::Elf64_Ehdr* header = nullptr;
    elf::Elf64_Shdr* sections = nullptr;
    int sections_count = 0;
    char* section_names = nullptr;
    int section_names_size = 0;
    
    // returns 0 if not found or invalid section
    int sectionIndexByName(const std::string& name) const;
    const char* nameOfSection(int sectionIndex) const;
    const u8* dataOfSection(int sectionIndex, int* out_size) const;
    
    const elf::Elf64_Rela* relaOfSection(int sectionIndex, int* out_count) const;

    // QuickArray<elf::Elf64_Shdr*> sections{};
    // QuickArray<elf::Elf64_Sym*> symbols{};

    // u32 stringTableSize = 0;
    // char* stringTableData = nullptr;

    void writeFile(const std::string& path);

    static FileELF* DeconstructFile(const std::string& path, bool silent = true);
    static void Destroy(FileELF* elfFile);

    static bool WriteFile(const std::string& name, Program* program, u32 from = 0, u32 to = (u32)-1);
};