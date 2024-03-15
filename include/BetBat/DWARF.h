/*
    DWARF debug information which can be used by GDB
    This file (and this compiler) will use DWARF version 3 for now.
    It's kind of old but it should have all you need to debug some basic
    things. Other versions may be more complicated.

    https://dwarfstd.org/doc/Dwarf3.pdf
    
    What I know about DWARF format
    
    Data is stored in various sections of object files
    
    What the ObjectWriter wants is sections and the data of those sections separately.
    Additionaly, DWARF may need to provide symbols

    
*/
#pragma once

#include "Engone/Util/Stream.h"
#include "BetBat/COFF.h"
#include "BetBat/ELF.h"
#include "BetBat/ObjectFile.h"

enum DWARF_ObjectFileType {
    DWARF_OBJ_COFF,
    DWARF_OBJ_ELF,
};

namespace dwarf {
    // IMPORTANT: All structs assume 32-bit DWARF format
    #pragma pack(push, 1)
    struct CompilationUnitHeader {
        u32 unit_length;
        u16 version;
        u32 debug_abbrev_offset;
        u8 address_size;
    };
    struct LineNumberProgramHeader {
        // PAGE 97
        
        // static const u32 SIZE = 11;
        u32 unit_length;
        u16 version;
        u32 header_length;
        u8 minimum_instruction_length;
        u8 default_is_stmt;
        i8 line_base;
        u8 line_range;
        u8 opcode_base;
        // u8 standard_opcode_lengths[];
        // char include_directories[][];
        // u8 nuLL_end_of_dirs = 0;
        // ? file_entry[][];
        // u8 nuLL_end_of_files = 0;
    };
    struct ArangesHeader {
        u32 unit_length;
        u16 version;
        u32 debug_info_offset;
        u8 address_size;
        u8 segment_size;
    };
    // THE START OF THIS STRUCT MUST BE 8-BYTE ALIGNED
    struct CommonInformationEntry {
        u32 length; // must be a multiple of 8 (or the address of the target system)
        u32 CIE_id;
        u8 version;
        // augmentation, null terminated UTF-8 string
        // code_alignment_factor, ULEB-128
        // data_alignment_factor, SLEB-128
        // return_address_register, ULEB-128
        // initial_instructions, array of u8
        // padding of DW_CFA_nop to match the specified length
    };
    struct FrameDescriptionEntry {
        u32 length; // multiple of 8?
        u32 CIE_pointer; // points to common information entry this description belongs to?
        u64 initial_location;
        u64 address_range;
        // instructions, array of u8
        // padding, DW_CFA_nop untill the length is reached
    };
    #pragma pack(pop)
    

    struct DWARFInfo {
        // section numbers
        int number_debug_info = -1;
        int number_debug_abbrev = -1;
        int number_debug_line = -1;
        int number_debug_aranges = -1;
        int number_debug_frame = -1;
        
        union {
            struct {
                coff::Section_Header* coff_section_debug_info;
                coff::Section_Header* coff_section_debug_abbrev;
                coff::Section_Header* coff_section_debug_line;
                coff::Section_Header* coff_section_debug_aranges;
                coff::Section_Header* coff_section_debug_frame;
            };
            struct {
                elf::Elf64_Shdr* elf_section_debug_info;
                elf::Elf64_Shdr* elf_section_debug_abbrev;
                elf::Elf64_Shdr* elf_section_debug_line;
                elf::Elf64_Shdr* elf_section_debug_aranges;
                elf::Elf64_Shdr* elf_section_debug_frame;
            };
            void* _zero[5] {nullptr};
        };
        
        // input/output
        ByteStream* stream = nullptr;
        union {
            coff::COFF_File_Header* coff_header = nullptr;
            elf::Elf64_Ehdr* elf_header;
        };
        DebugInformation* debug = nullptr;
        
        DynamicArray<std::string>* stringTable = nullptr;
        u32* stringTableOffset = nullptr;

        DynamicArray<coff::SectionSymbol>* sectionSymbols = nullptr;
        std::function<int(const std::string&, int, int, int, int)> elf_addSymbol = nullptr;
        //  = [&](const std::string& name, int section, int value, int bind, int type) {

        int symindex_text = -1;
        int symindex_debug_info = -1;
        int symindex_debug_line = -1;
        int symindex_debug_abbrev = -1;
        int symindex_debug_frame = -1;

        X64Program* program = nullptr; // DWARF needs to know the size of the program code
    };

    void ProvideSections(ObjectFile* objectFile, X64Program* program);
    
    // returns number of written bytes
    // return a negative number indicating how many bytes are missing
    int ULEB128_encode(u8* buffer, u32 buffer_space, u64 value);
    u64 ULEB128_decode(u8* buffer, u32 buffer_space);
    int SLEB128_encode(u8* buffer, u32 buffer_space, i64 value);
    i64 SLEB128_decode(u8* buffer, u32 buffer_space);
    void LEB128_test();
    
    /*#######################
        ATTRIBUTE ENCODINGS
    ########################*/
    #define DW_AT_sibling                       0x01     // reference
    #define DW_AT_location                      0x02     // block, loclistptr
    #define DW_AT_name                          0x03     // string
    #define DW_AT_ordering                      0x09     // constant
    #define DW_AT_byte_size                     0x0b     // block, constant, reference
    #define DW_AT_bit_offset                    0x0c     // block, constant, reference
    #define DW_AT_bit_size                      0x0d     // block, constant, reference 
    #define DW_AT_stmt_list                     0x10     // lineptr
    #define DW_AT_low_pc                        0x11     // address
    #define DW_AT_high_pc                       0x12     // address
    #define DW_AT_language                      0x13     // constant
    #define DW_AT_discr                         0x15     // reference
    #define DW_AT_discr_value                   0x16     // constant
    #define DW_AT_visibility                    0x17     // constant
    #define DW_AT_import                        0x18     // reference
    #define DW_AT_string_length                 0x19     // block, loclistptr
    #define DW_AT_common_reference              0x1a     // reference
    #define DW_AT_comp_dir                      0x1b     // string
    #define DW_AT_const_value                   0x1c     // block, constant, string
    #define DW_AT_containing_type               0x1d     // reference
    #define DW_AT_default_value                 0x1e     // reference
    #define DW_AT_inline                        0x20     // constant
    #define DW_AT_is_optional                   0x21     // flag
    #define DW_AT_lower_bound                   0x22     // block, constant, reference
    #define DW_AT_producer                      0x25     // string
    #define DW_AT_prototyped                    0x27     // flag
    #define DW_AT_return_addr                   0x2a     // block, loclistptr
    #define DW_AT_start_scope                   0x2c     // constant
    #define DW_AT_bit_stride                    0x2e     // constant
    #define DW_AT_upper_bound                   0x2f     // block, constant, reference
    #define DW_AT_abstract_origin               0x31     // reference
    #define DW_AT_accessibility                 0x32     // constant
    #define DW_AT_address_class                 0x33     // constant
    #define DW_AT_artificial                    0x34     // flag
    #define DW_AT_base_types                    0x35     // reference
    #define DW_AT_calling_convention            0x36     // constant
    #define DW_AT_count                         0x37     // block, constant, reference
    #define DW_AT_data_member_location          0x38     // block, constant, loclistptr
    #define DW_AT_decl_column                   0x39     // constant
    #define DW_AT_decl_file                     0x3a     // constant
    #define DW_AT_decl_line                     0x3b     // constant
    #define DW_AT_declaration                   0x3c     // flag
    #define DW_AT_discr_list                    0x3d     // block
    #define DW_AT_encoding                      0x3e     // constant
    #define DW_AT_external                      0x3f     // flag
    #define DW_AT_frame_base                    0x40     // block, loclistptr
    #define DW_AT_friend                        0x41     // reference
    #define DW_AT_identifier_case               0x42     // constant
    #define DW_AT_macro_info                    0x43     // macptr
    #define DW_AT_namelist_item                 0x44     // block
    #define DW_AT_priority                      0x45     // reference
    #define DW_AT_segment                       0x46     // block, loclistptr
    #define DW_AT_specification                 0x47     // reference
    #define DW_AT_static_link                   0x48     // block, loclistptr
    #define DW_AT_type                          0x49     // reference
    #define DW_AT_use_location                  0x4a     // block, loclistptr
    #define DW_AT_variable_parameter            0x4b     // flag
    #define DW_AT_virtuality                    0x4c     // constant
    #define DW_AT_vtable_elem_location          0x4d     // block, loclistptr
    #define DW_AT_allocated                     0x4e     // block, constant, reference
    #define DW_AT_associated                    0x4f     // block, constant, reference
    #define DW_AT_data_location                 0x50     // block
    #define DW_AT_byte_stride                   0x51     // block, constant, reference
    #define DW_AT_entry_pc                      0x52     // address
    #define DW_AT_use_UTF8                      0x53     // flag
    #define DW_AT_extension                     0x54     // reference
    #define DW_AT_ranges                        0x55     // rangelistptr 
    #define DW_AT_trampoline                    0x56     // address, flag, reference, string
    #define DW_AT_call_column                   0x57     // constant
    #define DW_AT_call_file                     0x58     // constant
    #define DW_AT_call_line                     0x59     // constant
    #define DW_AT_description                   0x5a     // string
    #define DW_AT_binary_scale                  0x5b     // constant
    #define DW_AT_decimal_scale                 0x5c     // constant
    #define DW_AT_small                         0x5d     // reference
    #define DW_AT_decimal_sign                  0x5e     // constant
    #define DW_AT_digit_count                   0x5f     // constant
    #define DW_AT_picture_string                0x60     // string
    #define DW_AT_mutable                       0x61     // flag
    #define DW_AT_threads_scaled                0x62     // flag
    #define DW_AT_explicit                      0x63     // flag
    #define DW_AT_object_pointer                0x64     // reference
    #define DW_AT_endianity                     0x65     // constant
    #define DW_AT_elemental                     0x66     // flag
    #define DW_AT_pure                          0x67     // flag 
    #define DW_AT_recursive                     0x68     // flag
    #define DW_AT_lo_user                       0x2000   // ---
    #define DW_AT_hi_user                       0x3fff   // ---
    
    
    /*##############
        TAG NAMES
    ###############*/
    #define DW_TAG_array_type                   0x01
    #define DW_TAG_class_type                   0x02
    #define DW_TAG_entry_point                  0x03
    #define DW_TAG_enumeration_type             0x04
    #define DW_TAG_formal_parameter             0x05
    #define DW_TAG_imported_declaration         0x08
    #define DW_TAG_label                        0x0a
    #define DW_TAG_lexical_block                0x0b
    #define DW_TAG_member                       0x0d
    #define DW_TAG_pointer_type                 0x0f
    #define DW_TAG_reference_type               0x10
    #define DW_TAG_compile_unit                 0x11
    #define DW_TAG_string_type                  0x12
    #define DW_TAG_structure_type               0x13
    #define DW_TAG_subroutine_type              0x15
    #define DW_TAG_typedef                      0x16
    #define DW_TAG_union_type                   0x17
    #define DW_TAG_unspecified_parameters       0x18
    #define DW_TAG_variant                      0x19
    #define DW_TAG_common_block                 0x1a
    #define DW_TAG_common_inclusion             0x1b
    #define DW_TAG_inheritance                  0x1c
    #define DW_TAG_inlined_subroutine           0x1d
    #define DW_TAG_module                       0x1e
    #define DW_TAG_ptr_to_member_type           0x1f
    #define DW_TAG_set_type                     0x20
    #define DW_TAG_subrange_type                0x21
    #define DW_TAG_with_stmt                    0x22
    #define DW_TAG_access_declaration           0x23
    #define DW_TAG_base_type                    0x24
    #define DW_TAG_catch_block                  0x25
    #define DW_TAG_const_type                   0x26
    #define DW_TAG_constant                     0x27
    #define DW_TAG_enumerator                   0x28
    #define DW_TAG_file_type                    0x29
    #define DW_TAG_friend                       0x2a
    #define DW_TAG_namelist                     0x2b
    #define DW_TAG_namelist_item                0x2c
    #define DW_TAG_packed_type                  0x2d
    #define DW_TAG_subprogram                   0x2e
    #define DW_TAG_template_type_parameter      0x2f
    #define DW_TAG_template_value_parameter     0x30
    #define DW_TAG_thrown_type                  0x31
    #define DW_TAG_try_block                    0x32
    #define DW_TAG_variant_part                 0x33
    #define DW_TAG_variable                     0x34
    #define DW_TAG_volatile_type                0x35
    #define DW_TAG_dwarf_procedure              0x36
    #define DW_TAG_restrict_type                0x37
    #define DW_TAG_interface_type               0x38
    #define DW_TAG_namespace                    0x39
    #define DW_TAG_imported_module              0x3a
    #define DW_TAG_unspecified_type             0x3b
    #define DW_TAG_partial_unit                 0x3c
    #define DW_TAG_imported_unit                0x3d
    #define DW_TAG_condition                    0x3f 
    #define DW_TAG_shared_type                  0x40
    #define DW_TAG_lo_user                      0x4080
    #define DW_TAG_hi_user                      0xffff
    
    #define DW_CHILDREN_no                      0x00
    #define DW_CHILDREN_yes                     0x01
    
    /*############################
        ATTRIBUTE FORM ENCODINGS
    ###############################*/
    #define DW_FORM_addr                        0x01    // address
    #define DW_FORM_block2                      0x03    // block
    #define DW_FORM_block4                      0x04    // block
    #define DW_FORM_data2                       0x05    // constant, 2 bytes
    #define DW_FORM_data4                       0x06    // constant, 4 bytes, lineptr, loclistptr, macptr, rangelistptr
    #define DW_FORM_data8                       0x07    // constant, 8 bytes, lineptr, loclistptr, macptr, rangelistptr
    #define DW_FORM_string                      0x08    // string
    #define DW_FORM_block                       0x09    // block
    #define DW_FORM_block1                      0x0a    // block
    #define DW_FORM_data1                       0x0b    // constant, 1 byte
    #define DW_FORM_flag                        0x0c    // flag
    #define DW_FORM_sdata                       0x0d    // constant
    #define DW_FORM_strp                        0x0e    // string
    #define DW_FORM_udata                       0x0f    // constant
    #define DW_FORM_ref_addr                    0x10    // reference
    #define DW_FORM_ref1                        0x11    // reference
    #define DW_FORM_ref2                        0x12    // reference
    #define DW_FORM_ref4                        0x13    // reference
    #define DW_FORM_ref8                        0x14    // reference
    #define DW_FORM_ref_udata                   0x15    // reference
    #define DW_FORM_indirect_THIS_IS_SPECIAL_PAGE_125    0x16    //
    
    /*#########################################
        LINE NUMBER STANDARD OPCODE ENCODINGS
    ###########################################*/
    #define DW_LNS_copy                                 0x01 // 0 operand
    #define DW_LNS_advance_pc                           0x02 // 1 operand
    #define DW_LNS_advance_line                         0x03 // 1 operand
    #define DW_LNS_set_file                             0x04 // 1 operand
    #define DW_LNS_set_column                           0x05 // 1 operand
    #define DW_LNS_negate_stmt                          0x06 // 0 operand
    #define DW_LNS_set_basic_block                      0x07 // 0 operand
    #define DW_LNS_const_add_pc                         0x08 // 0 operand
    #define DW_LNS_fixed_advance_pc                     0x09 // 1 operand
    #define DW_LNS_set_prologue_end                     0x0a // 0 operand
    #define DW_LNS_set_epilogue_begin                   0x0b // 0 operand
    #define DW_LNS_set_isa                              0x0c // 1 operand
    
    /*#########################################
        LINE NUMBER EXTENDED OPCODE ENCODINGS
    ###########################################*/
    #define DW_LNE_end_sequence                         0x01
    #define DW_LNE_set_address                          0x02
    #define DW_LNE_define_file                          0x03
    #define DW_LNE_lo_user                              0x80
    #define DW_LNE_hi_user                              0xff 

    /* ###################3
        OPS of some sort
    ###########*/
    #define DW_OP_addr                  0x03 // constant address (size target specific)
    #define DW_OP_deref                 0x06 // 
    #define DW_OP_const1u               0x08 // 1-byte constant
    #define DW_OP_const1s               0x09 // 1-byte constant
    #define DW_OP_const2u               0x0a // 2-byte constant
    #define DW_OP_const2s               0x0b // 2-byte constant
    #define DW_OP_const4u               0x0c // 4-byte constant
    #define DW_OP_const4s               0x0d // 4-byte constant
    #define DW_OP_const8u               0x0e // 8-byte constant
    #define DW_OP_const8s               0x0f // 8-byte constant
    #define DW_OP_constu                0x10 // ULEB128 constant
    #define DW_OP_consts                0x11 // SLEB128 constant
    #define DW_OP_dup                   0x12 // 
    #define DW_OP_drop                  0x13 // 
    #define DW_OP_over                  0x14 // 
    #define DW_OP_pick                  0x15 // 1-byte stack index
    #define DW_OP_swap                  0x16 // 
    #define DW_OP_rot                   0x17 // 
    #define DW_OP_xderef                0x18 // 
    #define DW_OP_abs                   0x19 // 
    #define DW_OP_and                   0x1a // 
    #define DW_OP_div                   0x1b // 
    #define DW_OP_minus                 0x1c // 
    #define DW_OP_mod                   0x1d // 
    #define DW_OP_mul                   0x1e // 
    #define DW_OP_neg                   0x1f // 
    #define DW_OP_not                   0x20 // 
    #define DW_OP_or                    0x21 // 
    #define DW_OP_plus                  0x22 // 
    #define DW_OP_plus_uconst           0x23 // ULEB128 addend
    #define DW_OP_shl                   0x24 // 
    #define DW_OP_shr                   0x25 // 
    #define DW_OP_shra                  0x26 // 
    #define DW_OP_xor                   0x27 // 
    #define DW_OP_skip                  0x2f // signed 2-byte constant
    #define DW_OP_bra                   0x28 // signed 2-byte constant
    #define DW_OP_eq                    0x29 // 
    #define DW_OP_ge                    0x2a // 
    #define DW_OP_gt                    0x2b // 
    #define DW_OP_le                    0x2c // 
    #define DW_OP_lt                    0x2d // 
    #define DW_OP_ne                    0x2e // 
    #define DW_OP_lit0                  0x30 // 
    #define DW_OP_lit1                  0x31 // 
    #define DW_OP_lit31                 0x4f // literals 0..31 = (DW_OP_lit0 + literal)
    #define DW_OP_reg0                  0x50 // 
    #define DW_OP_reg1                  0x51 // 
    #define DW_OP_reg31                 0x6f // reg 0..31 = (DW_OP_reg0 + regnum)
    #define DW_OP_breg0                 0x70 // 
    #define DW_OP_breg1                 0x71 // 
    #define DW_OP_breg31                0x8f // SLEB128 offset base register 0..31 = (DW_OP_breg0 + regnum)
    #define DW_OP_regx                  0x90 // ULEB128 register
    #define DW_OP_fbreg                 0x91 // SLEB128 offset
    #define DW_OP_bregx                 0x92 // ULEB128 register followed by SLEB128 offset
    #define DW_OP_piece                 0x93 // ULEB128 size of piece addressed
    #define DW_OP_deref_size            0x94 // 1-byte size of data retrieved
    #define DW_OP_xderef_size           0x95 // 1-byte size of data retrieved
    #define DW_OP_nop                   0x96 // 
    #define DW_OP_push_object_address   0x97 // 
    #define DW_OP_call2                 0x98 // 2-byte offset of DIE
    #define DW_OP_call4                 0x99 // 4-byte offset of DIE
    #define DW_OP_call_ref              0x9a // 4- or 8-byte offset of DIE
    #define DW_OP_form_tls_address      0x9b // 
    #define DW_OP_call_frame_cfa        0x9c // 
    #define DW_OP_bit_piece             0x9d // 
    #define DW_OP_lo_user               0xe0 // 
    #define DW_OP_hi_user               0xff // 

    // high 2 bits are constant, low 6 bits are special
    #define DW_CFA_advance_loc(DELTA) ((0x1<<6)|(delta))
    #define DW_CFA_offset(REG) ((0x2<<6)|(REG)) // ULEB128 offset
    #define DW_CFA_restore(REG) ((0x3<<6)|(REG))

    // the comments indicate the operands, 0, 1 or 2
    #define DW_CFA_nop                  0x00 //
    #define DW_CFA_set_loc              0x01 // address
    #define DW_CFA_advance_loc1         0x02 // 1-byte delta
    #define DW_CFA_advance_loc2         0x03 // 2-byte delta
    #define DW_CFA_advance_loc4         0x04 // 4-byte delta
    #define DW_CFA_offset_extended      0x05 // ULEB128 register ULEB128 offset
    #define DW_CFA_restore_extended     0x06 // ULEB128 register
    #define DW_CFA_undefined            0x07 // ULEB128 register
    #define DW_CFA_same_value           0x08 // ULEB128 register
    #define DW_CFA_register             0x09 // ULEB128 register ULEB128 register
    #define DW_CFA_remember_state       0x0a //
    #define DW_CFA_restore_state        0x0b //
    #define DW_CFA_def_cfa              0x0c // ULEB128 register ULEB128 offset
    #define DW_CFA_def_cfa_register     0x0d // ULEB128 register
    #define DW_CFA_def_cfa_offset       0x0e // ULEB128 offset
    #define DW_CFA_def_cfa_expression0  0x0f // BLOCK
    #define DW_CFA_expression0          0x10 // ULEB128 register BLOCK
    #define DW_CFA_offset_extended_sf0  0x11 // ULEB128 register SLEB128 offset
    #define DW_CFA_def_cfa_sf0          0x12 // ULEB128 register SLEB128 offset
    #define DW_CFA_def_cfa_offset_sf0   0x13 // SLEB128 offset
    #define DW_CFA_val_offset0          0x14 // ULEB128 ULEB128
    #define DW_CFA_val_offset_sf0       0x15 // ULEB128 SLEB128
    #define DW_CFA_val_expression0      0x16 // ULEB128 BLOCK
    #define DW_CFA_lo_user              0x1c //
    #define DW_CFA_hi_user              0x3f //

    #define DW_ATE_address              0x01
    #define DW_ATE_boolean              0x02
    #define DW_ATE_complex_float        0x03
    #define DW_ATE_float                0x04
    #define DW_ATE_signed               0x05
    #define DW_ATE_signed_char          0x06
    #define DW_ATE_unsigned             0x07
    #define DW_ATE_unsigned_char        0x08
    #define DW_ATE_imaginary_float      0x09
    #define DW_ATE_packed_decimal       0x0a
    #define DW_ATE_numeric_string       0x0b
    #define DW_ATE_edited               0x0c
    #define DW_ATE_signed_fixed         0x0d
    #define DW_ATE_unsigned_fixed       0x0e
    #define DW_ATE_decimal_float        0x0f
    #define DW_ATE_lo_user              0x80
    #define DW_ATE_hi_user              0xff
}