#include "BetBat/PDBWriter.h"
#include "Engone/Logger.h"
#include "Engone/PlatformLayer.h"
#include "BetBat/Asserts.h"
#include "BetBat/Util/Array.h"


extern const char* registerNames_x64[];
extern const char* registerNames_AMD64[];

void DeconstructPDB(const char* path) {
    using namespace engone;
    
    u64 fileSize = 0;
    auto pdbFile = FileOpen(path, &fileSize, FILE_ONLY_READ);
    Assert(pdbFile);
    u8* fileData = (u8*)engone::Allocate(fileSize);
    auto rb = FileRead(pdbFile, fileData, fileSize);
    Assert(rb == fileSize);
    FileClose(pdbFile);
    
    log::out << "PDB["<<fileSize<<"]:\n";

    u32 offset = 0;

    // BIGMSF_HDR
    const char magic[] = "Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53";
    if(fileSize < sizeof(magic) || strncmp((char*)fileData + offset, magic, sizeof(magic))) {
        log::out << log::RED << "MSF signature is wrong\n";
        return;
    }
    offset += sizeof(magic);

    // 4-byte alignment
    if(offset & 3)  offset += 4 - (offset&3);

    u32 blockSize = *(i32*)(fileData + offset);
    offset += 4;
    u32 blockNumberOfValidFPM = *(u32*)(fileData + offset); // page no. of valid FPM
    offset += 4;
    u32 blockCount = *(u32*)(fileData + offset);
    offset += 4;
    Assert(fileSize == blockSize * blockCount);
    // SI_PERSIST, stream info stream table?
    u32 bytesOfStreamDirectory = *(i32*)(fileData + offset);
    offset += 4;
    u32 mpspnpn_SI_PERSIST = *(i32*)(fileData + offset);
    offset += 4; // padding

    log::out << "Block size "<<blockSize <<"\n";
    log::out << "Block number of valid FPM "<<blockNumberOfValidFPM <<"\n";
    log::out << "Block count "<<blockCount <<"\n";
    log::out << "Bytes of stream directory "<<bytesOfStreamDirectory <<"\n";
    // log::out << "SI_PERSIST.mpspnpn "<<mpspnpn_SI_PERSIST <<"\n";

    // DUDE! this doesn't point to stream directory blocks!
    // it points to an index that points to the stream directory/table.
    u32* streamDirectoryBlocks = (u32*)(fileData + (*(u32*)(fileData + offset)) * blockSize);
    int n = ceil((double)bytesOfStreamDirectory / blockSize);
    offset += 4 * n;

    u32 firstStreamDirectory = *streamDirectoryBlocks;
    log::out << "First stream dir "<<firstStreamDirectory<<"\n";
    offset = firstStreamDirectory * blockSize; // PDB Header location
    u32 streamCount = *(i32*)(fileData + offset);
    offset += 4;
    u32* streamSizes = (u32*)(fileData + offset); // page no. of valid FPM
    offset += 4 * streamCount;
    Assert(0==(offset&3));
    u8* streamBlocks = fileData + offset;
    
    log::out << "Stream count " << streamCount << "\n";

    struct Stream {
        u32 byteSize;
        DynamicArray<u32> blockIndices;
    };
    DynamicArray<Stream> streams;

    int streamIndex = 0;
    int streamBlocksOffset = 0;
    while (streamIndex < streamCount) {
        int blocks = ceil((double)streamSizes[streamIndex] / blockSize);
        int size = streamSizes[streamIndex];
        streams.add({});
        streams.last().blockIndices._reserve(blocks);
        streams.last().byteSize = size;
        log::out << "stream " << streamIndex << ", size "<<size<<"\n";
        log::out << " blocks["<<blocks<<"]:";
        while(blocks > 0){
            blocks--;
            u32 blockIndex = *(u32*)(streamBlocks + streamBlocksOffset);
            streams.last().blockIndices.add(blockIndex);
            streamBlocksOffset+=4;
            log::out << " " <<blockIndex;
            if(blocks !=0)
                log::out << ",";
        }
        log::out << "\n";
        streamIndex++;
    }
    u32 names_stream = -1;
    // IMPORTANT: A stream can consist of multiple blocks.
    //  We ignore this at the moment when reading data and increasing offset.
    //  We do this because this is just an experiment to understanding
    //  the format of PDB.
    {
        Stream& pdbStream = streams[1];
        Assert(pdbStream.blockIndices.size() > 0);
        offset = pdbStream.blockIndices[0] * blockSize;

        PDBStreamHeader pdbHeader;
        memcpy(&pdbHeader, fileData + offset, sizeof(PDBStreamHeader));
        offset += sizeof(PDBStreamHeader);

        Assert(pdbHeader.impv == PDBImpvVC70);

        log::out << "impv "<<pdbHeader.impv <<"\n";
        log::out << "sig "<<pdbHeader.sig <<"\n";
        log::out << "age "<<pdbHeader.age <<"\n";
        
        u32 stringDataSize = *(u32*)(fileData + offset);
        offset += 4;
        char* stringData = (char*)(fileData + offset);
        offset += stringDataSize;
        log::out << "Strings size: "<<stringDataSize<<"\n";

        u32 valueCount = *(u32*)(fileData + offset);
        offset += 4;
        u32 bucketCount = *(u32*)(fileData + offset);
        offset += 4;

        u32 wordCount = *(u32*)(fileData + offset);
        offset += 4;
        u32* presentArray = (u32*)(fileData + offset);
        offset += wordCount * 4;
        
        u32 wordCount2 = *(u32*)(fileData + offset);
        offset += 4;
        u32* deletedArray = (u32*)(fileData + offset);
        offset += wordCount2 * 4;

        for(int i=0;i<wordCount;i++){
            for(int j=0;j<32;j++){
                u8 bit = ((presentArray[i]>>j)& 0x1);
                // log::out << i<<"."<<j<<"\n";
                if(bit) {
                    u32 key = *(u32*)(fileData + offset);
                    offset += 4;
                    char* str = stringData + key;
                    u32 value = *(u32*)(fileData + offset);
                    offset += 4;
                    log::out << " kv: "<<(str) << " "<<value<<"\n";
                    if(!strcmp(str, "/names")){
                        names_stream = value;
                    }
                }
            }
            
        }
    }
    if (names_stream != -1){
        Stream& namesStream = streams[names_stream];
        Assert(namesStream.blockIndices.size() > 0);
        offset = namesStream.blockIndices[0] * blockSize;

        // what format?
    }
    {
        Stream& tpiStream = streams[2];
        Assert(tpiStream.blockIndices.size() > 0);
        offset = tpiStream.blockIndices[0] * blockSize;

        TPIStreamHeader tpiHeader;
        memcpy(&tpiHeader, fileData + offset, sizeof(TPIStreamHeader));
        offset += sizeof(TPIStreamHeader);

        log::out << "TPI hash stream: "<<tpiHeader.HashStreamIndex<<"\n";

        Assert(tpiHeader.HeaderSize == sizeof(TPIStreamHeader));
        Assert(tpiHeader.Version == (u32)TPIStreamVersion::V80);
        // llvm docs says that it's rare to see a version other than V80 and
        // if you do then the way you deserialize that version might be different from V80.

        // TODO: Hash values/buckets...?

        DeconstructDebugTypes(fileData + offset, tpiHeader.TypeRecordBytes, true);
    }
    {
        Stream& ipiStream = streams[4];
        Assert(ipiStream.blockIndices.size() > 0);
        offset = ipiStream.blockIndices[0] * blockSize;

        TPIStreamHeader ipiHeader; // IPI stream uses the same header as TPI
        memcpy(&ipiHeader, fileData + offset, sizeof(TPIStreamHeader));
        offset += sizeof(TPIStreamHeader);
        log::out << "IPI hash stream: "<<ipiHeader.HashStreamIndex<<"\n";

        Assert(ipiHeader.HeaderSize == sizeof(TPIStreamHeader));
        Assert(ipiHeader.Version == (u32)TPIStreamVersion::V80);
        // llvm docs says that it's rare to see a version other than V80 and
        // if you do then the way you deserialize that version might be different from V80.

        // TODO: Hash values/buckets...?

        DeconstructDebugTypes(fileData + offset, ipiHeader.TypeRecordBytes, true);
    }
    {
        Stream& dbiStream = streams[3];
        if(dbiStream.blockIndices.size() == 0) {
            log::out << "No DBI stream\n";
        } else {
            Assert(dbiStream.blockIndices.size() > 0);
            offset = dbiStream.blockIndices[0] * blockSize;

            DBIStreamHeader dbiHeader;
            memcpy(&dbiHeader, fileData + offset, sizeof(DBIStreamHeader));
            offset += sizeof(DBIStreamHeader);

            Assert(dbiHeader.VersionHeader == (u32)DbiStreamVersion::V70);

            // DeconstructDebugTypes(fileData + offset, ipiHeader.TypeRecordBytes, true);
        }
    }

    engone::Free(fileData, fileSize);
}

void DeconstructDebugSymbols(u8* buffer, u32 size) {
    using namespace engone;
    // return;
    log::out << "Debug symbols["<<size<<"]:\n";

    // TODO: Assert if offset is more than size
    // #define READ(T)
    u32 offset = 0;

    u32 signature = *(u32*)(buffer + offset);
    log::out << "Signature: "<<signature << "\n";
    offset += 4;

    Assert(signature == CV_SIGNATURE_C13);

    int recordCount = -1;
    int subSectionCount = -1;
    while(offset < size) {
        subSectionCount++;

        Assert(size % 4 == 0);
        
        SubSectionType subSectionType = *(SubSectionType*)(buffer + offset);
        offset += 4;
        u32 sectionLength = *(u32*)(buffer + offset);
        offset += 4;

        u32 nextOffset = offset + sectionLength;
        
        const char* subName = ToString(subSectionType, true);
        log::out <<  "Sub section "<<subSectionCount<<", ";
        if(subName)
            log::out << log::LIME << subName << log::SILVER;
        else
            log::out <<log::RED <<  "{"<<(u32)subSectionType<<"}" << log::SILVER;
        log::out << ", len: "<<sectionLength<<"\n";
        
        Assert(nextOffset <= size);

        switch(subSectionType) {
            case DEBUG_S_SYMBOLS: {
                while(offset < nextOffset) {
                    recordCount++;
                    // length does not include the length integer itself (See NextSym)
                    u16 length = *(u16*)(buffer + offset);
                    offset += 2;
                    u32 nextOffset = offset + length;
                    RecordType recordType = *(RecordType*)(buffer + offset);
                    offset += 2;

                    const char* recordName = ToString(recordType, true);
                    log::out << "record "<<recordCount<<", ";
                    if(recordName)
                        log:: out << log::LIME <<  recordName << log::SILVER;
                    else
                        log::out << log::RED << "{"<<(u16)recordType<<"}" << log::SILVER;
                    log::out << ", len: "<<length<<"\n";

                    Assert(nextOffset <= size);

                    switch(recordType) {
                        case S_OBJNAME: {
                            u32 signature = *(u32*)(buffer + offset);
                            offset += 4;
                            log::out << " signature: "<<signature<<"\n";

                            char* name = (char*)(buffer + offset);
                            u8 nameLength = strlen(name);
                            offset += nameLength + 1;

                            log::out << " " << name << "\n";
                            break;
                        }
                        // case S_COMPILE3: {
                        //     log::out << " printing not implemented\n";
                            
                        //     break;
                        // }
                        case S_BUILDINFO: {
                            u32 id = *(u32*)(buffer + offset);
                            offset += 4;
                            log::out << " "<<id <<"\n";
                            break;
                        }
                        case S_GPROC32_ID: {
                            struct GPROC32 {
                                u32 pParent;    // pointer to the parent
                                u32 pEnd;       // pointer to this blocks end
                                u32 pNext;      // pointer to next symbol
                                u32 len;        // Proc length
                                u32 DbgStart;   // Debug start offset
                                u32 DbgEnd;     // Debug end offset
                                u32 typind;     // Type index or ID
                                u32 off;
                                u16 seg;
                                u8 flags;      // Proc flags, CV_PROCFLAGS
                                // 1 byte padding
                            };
                            GPROC32 proc;
                            Assert((offset & 3) == 0);
                            proc = *(GPROC32*)(buffer + offset);
                            offset += sizeof(GPROC32) - 1; // -1 because the struct is padded with one byte
                            char* name = (char*)(buffer + offset);
                            int namelen = strlen(name);
                            offset += namelen + 1;
                            log::out << " "<<name<<", ["<<proc.seg<<":"<<proc.off<<"], len "<<proc.len << ", typeindex "<<proc.typind<<"\n";
                            log::out << " parent "<<proc.pParent<<", end "<<proc.pEnd<<", next "<<proc.pNext<<"\n";
                            log::out << " debug start-end: "<<proc.DbgStart<<"-"<<proc.DbgEnd<<", flags "<<proc.flags<<"\n";

                            break;
                        }
                        case S_FRAMEPROC: {
                            FRAMEPROCSYM frame;
                            // THIS IS BROKEN WITH FLAGS BECAUSE OF PADDING IN STRUCT
                            frame = *(FRAMEPROCSYM*)(buffer + offset);
                            offset += sizeof(FRAMEPROCSYM);

                            log::out << " frame size " <<frame.cbFrame<<", pad size " << frame.cbPad << ", offset pad " << frame.offPad << "\n";
                            log::out << " size of callee save registers " << frame.cbSaveRegs << "\n";
                            log::out << " Address of exception handler " << frame.sectExHdlr<< ":"<<frame.offExHdlr<<"\n";
                            log::out << " flags "<<*(u32*)&frame.flags<<"\n";
                            break;
                        }
                        case S_REGREL32: {
                            u32 off = *(u32*)(buffer + offset);
                            offset += 4;
                            u32 typind = *(u32*)(buffer + offset);
                            offset += 4;
                            u16 reg = *(u16*)(buffer + offset);
                            offset += 2;
                            char* name = (char*)(buffer + offset);
                            u32 len = strlen(name);
                            offset += len + 1;
                            // Even though I compiled with x64, it seems to use AMD64.
                            log::out << " "<<name << ", off " << registerNames_AMD64[reg]<<"+"<< off << ", typeind " << typind << "\n";
                            // log::out << " "<<name << ", off " << registerNames_x64[reg]<<"+"<< off << ", typeind " << typind << "\n";
                            break;
                        }
                        case S_PROC_ID_END: {
                            // nothing
                            break;
                        }
                        default: {
                            log::out <<log::GRAY<< " (record format not implemented)\n";
                            break;
                        }
                    }

                    offset = nextOffset;
                    // // 4-byte alignment
                    // if((offset & 3) != 0)
                    //     offset += 4 - offset & 3;
                }
                // log::out << " (nextOffset: "<<nextOffset<<", tempOffset: "<<offset<<")\n";
                break;
            }
            case DEBUG_S_LINES: {
                u32 offCon = *(u32*)(buffer + offset);
                offset += 4;
                u16 segCon = *(u16*)(buffer + offset);
                offset += 2;
                u16 flags = *(u16*)(buffer + offset);
                offset += 2;
                u32 cbCon = *(u32*)(buffer + offset);
                offset += 4;
                #define CV_LINES_HAVE_COLUMNS 1
                bool hasColumns = (flags & CV_LINES_HAVE_COLUMNS);

                log::out << " Header "<<segCon <<":"<<offCon << "-"<<(offCon + cbCon)<<", flags: "<<flags <<"\n";

                while(offset < nextOffset) {
                    u32 fileid = *(u32*)(buffer + offset);
                    offset += 4;
                    u32 nLines = *(u32*)(buffer + offset);
                    offset += 4;
                    u32 cbFileBlock = *(u32*)(buffer + offset);
                    offset += 4;

                    log::out << " fileid "<<fileid <<"\n";

                    DebugLine* lines = (DebugLine*)(buffer + offset);
                    offset += nLines * sizeof(DebugLine);

                    DebugColumn* columns = nullptr;
                    if(hasColumns) {
                        columns = (DebugColumn*)(buffer + offset);
                        offset += nLines * sizeof(DebugColumn);
                    }

                    for(int i=0;i<nLines;i++) {
                        DebugLine line = lines[i];

                        if(hasColumns) {
                            log::out << "  column special? ";
                        } else {
                        }
                        log::out << "  LN:"<<line.linenumStart<<" -> code:"<<(line.offset + offCon)<<"\n";
                    }
                }

                break;
            }
            case DEBUG_S_FILECHKSMS: {
                u32 offstFileName;
                u8 cbChecksum;
                u8 ChecksumType;
                while(offset < nextOffset) {
                    offstFileName = *(u32*)(buffer + offset);
                    offset += 4;
                    cbChecksum = *(u8*)(buffer + offset);
                    offset += 1;
                    ChecksumType = *(u8*)(buffer + offset);
                    offset += 1;

                    log::out << "stfile " << offstFileName << ", checksum size "<<cbChecksum<<", chktype ";

                    switch(ChecksumType) {
                        case CHKSUM_TYPE_NONE :
                            log::out << "None";
                            break;
                        case CHKSUM_TYPE_MD5 :
                            log::out << "MD5";
                            break;
                        case CHKSUM_TYPE_SHA1 :
                            log::out << "SHA1";
                            break;
                        case CHKSUM_TYPE_SHA_256:
                           log::out << "SHA_256";
                           break;
                        default:
                            log::out << ChecksumType;
                            break;
                    }
                    log::out<<"\n";

                    Assert(cbChecksum < 256);
                    offset += cbChecksum;

                    // align for next record
                    if(offset&3) offset += 4 - (offset&3);
                }
                break;
            }
            case DEBUG_S_STRINGTABLE: {
                u32 offsetStart = offset;
                while(offset < nextOffset){
                    char* str = (char*)(buffer + offset);
                    int len = strlen(str);
                    log::out << (offset - offsetStart) << ": "<<str << "\n";

                    offset += len + 1;
                }
                break;
            }
            default: {
                log::out <<log::GRAY<< " (sub section not implemented)\n";
                break;
            }
        }

        offset = nextOffset;
        // 4-byte alignment
        if((offset & 3) != 0)
            offset += 4 - offset & 3;
    }
}
void DeconstructDebugTypes(u8* buffer, u32 size, bool fromPDB) {
    using namespace engone;
    log::out << "Debug types["<<size<<"]:\n";

    // TODO: Assert if offset is more than size
    // #define READ(T)
    u32 offset = 0;

    if(!fromPDB) {
        u32 signature = *(u32*)(buffer + offset);
        log::out << "Signature: "<<signature << "\n";
        offset += 4;

        Assert(signature == CV_SIGNATURE_C13);
    }

    int recordCount = -1;
    int typeCount = -1;
    while(offset < size) {
        typeCount++;

        Assert(size % 4 == 0);
        
        u16 length = *(u16*)(buffer + offset);
        offset += 2;
        u32 nextOffset = offset + length;
        LeafType leaf = *(LeafType*)(buffer + offset);
        offset += 2;

        
        const char* name = ToString(leaf, true);
        log::out <<  "Type index "<<typeCount<<", ";
        if(name)
            log::out << log::LIME << name << log::SILVER;
        else
            log::out <<log::RED <<  "{"<<(u32)leaf<<"}" << log::SILVER;
        log::out << ", len: "<<length<<"\n";
        
        Assert(nextOffset <= size);

        // the leaf formats come from the structs in cvinfo.h
        switch(leaf) {
            case LF_TYPESERVER2: {
                offset += 16; // guid
                u32 age = *(u32*)(buffer + offset);
                offset += 4;
                char* pdbpath = (char*)(buffer + offset);
                u32 pdblen = strlen(pdbpath);
                offset += pdblen + 1;
                log::out << " guid, age: "<<age << ", pdb: "<< pdbpath<<"\n";
                break;
            }
            case LF_STRING_ID: {
                u32 id = *(u32*)(buffer + offset);
                offset += 4;
                char* str = (char*)(buffer + offset);
                u32 len = strlen(str);
                offset += len;
                log::out << " id "<<id << ", "<<str << "\n";
                break;
            }
            case LF_FUNC_ID: {
                u32 scopeId = *(u32*)(buffer + offset);
                offset += 4;
                u32 type = *(u32*)(buffer + offset);
                offset += 4;
                char* str = (char*)(buffer + offset);
                u32 len = strlen(str);
                offset += len;
                log::out << " scopeid "<<scopeId << ", type "<<type << ", "<<str << "\n";
                break;
            }
            case LF_UDT_SRC_LINE: {
                u32 type = *(u32*)(buffer + offset); // UDT's type index
                offset += 4;
                u32 src = *(u32*)(buffer + offset); // index to LF_STRING_ID record where source file name is saved
                offset += 4;
                u16 line = *(u16*)(buffer + offset);
                offset += 2;
                log::out << " type "<< type << ", src "<< src << ", line "<< line << "\n";
                break;
            }
            case LF_BUILDINFO: {
                u16 argCount = *(u16*)(buffer + offset); // UDT's type index
                offset += 2;
                // UNALIGNED! but it should be
                u32* args = (u32*)(buffer + offset); // index to LF_STRING_ID record where source file name is saved
                offset += 4 * argCount;
                log::out << " args["<<argCount<<"]: ";
                for(int i=0;i<argCount;i++){
                    if(i != 0){
                        log::out << ", ";
                    }
                    log::out << args[i];
                }
                log::out << "\n";
                break;
            }
            case LF_SUBSTR_LIST: {
                u32 argCount = *(u32*)(buffer + offset); // UDT's type index
                offset += 4;
                u32* args = (u32*)(buffer + offset); // index to LF_STRING_ID record where source file name is saved
                offset += 4 * argCount;
                log::out << " args["<<argCount<<"]: ";
                for(int i=0;i<argCount;i++){
                    if(i != 0){
                        log::out << ", ";
                    }
                    log::out << args[i];
                }
                log::out << "\n";
                break;
            }
        }

        offset = nextOffset;
        // 4-byte alignment
        if((offset & 3) != 0)
            offset += 4 - offset & 3;
    }
}

const char* ToString(SubSectionType type, bool nullAsUnknown) {
    if(type & DEBUG_S_IGNORE) return "DEBUG_S_IGNORE";
    #define CASE(X) case X: return #X;
    switch(type){
        CASE(DEBUG_S_SYMBOLS)
        CASE(DEBUG_S_LINES)
        CASE(DEBUG_S_STRINGTABLE)
        CASE(DEBUG_S_FILECHKSMS)
        CASE(DEBUG_S_FRAMEDATA)
        CASE(DEBUG_S_INLINEELINES)
        CASE(DEBUG_S_CROSSSCOPEIMPORTS)
        CASE(DEBUG_S_CROSSSCOPEEXPORTS)

        CASE(DEBUG_S_IL_LINES)
        CASE(DEBUG_S_FUNC_MDTOKEN_MAP)
        CASE(DEBUG_S_TYPE_MDTOKEN_MAP)
        CASE(DEBUG_S_MERGED_ASSEMBLYINPUT)

        CASE(DEBUG_S_COFF_SYMBOL_RVA)
    }
    if(nullAsUnknown)
        return nullptr;
    else
        return "UNKNOWN_SECTION";
    #undef CASE
}
const char* ToString(RecordType type, bool nullAsUnknown) {
    #define CASE(X) case X: return #X;
    switch(type){
        CASE(S_COMPILE) 
        CASE(S_REGISTER_16t)
        CASE(S_CONSTANT_16t)
        CASE(S_UDT_16t) 
        CASE(S_SSEARCH) 
        CASE(S_END)       
        CASE(S_SKIP)  
        CASE(S_CVRESERVE) 
        CASE(S_OBJNAME_ST)
        CASE(S_ENDARG)
        CASE(S_COBOLUDT_16t)
        CASE(S_MANYREG_16t) 
        CASE(S_RETURN)   
        CASE(S_ENTRYTHIS)
        CASE(S_BPREL16)
        CASE(S_LDATA16)
        CASE(S_GDATA16)
        CASE(S_PUB16) 
        CASE(S_LPROC16)
        CASE(S_GPROC16)
        CASE(S_THUNK16)
        CASE(S_BLOCK16)     
        CASE(S_WITH16)      
        CASE(S_LABEL16)     
        CASE(S_CEXMODEL16)  
        CASE(S_VFTABLE16)   
        CASE(S_REGREL16)    

        CASE(S_BPREL32_16t)
        CASE(S_LDATA32_16t)
        CASE(S_GDATA32_16t)
        CASE(S_PUB32_16t)
        CASE(S_LPROC32_16t)
        CASE(S_GPROC32_16t)
        CASE(S_THUNK32_ST)
        CASE(S_BLOCK32_ST)
        CASE(S_WITH32_ST)
        CASE(S_LABEL32_ST)
        CASE(S_CEXMODEL32)
        CASE(S_VFTABLE32_16t)
        CASE(S_REGREL32_16t)
        CASE(S_LTHREAD32_16t)
        CASE(S_GTHREAD32_16t)
        CASE(S_SLINK32)

        CASE(S_LPROCMIPS_16t)
        CASE(S_GPROCMIPS_16t)

        CASE(S_PROCREF_ST)
        CASE(S_DATAREF_ST)
        CASE(S_ALIGN)
        
        CASE(S_LPROCREF_ST)
        CASE(S_OEM)

        CASE(S_TI16_MAX)

        CASE(S_REGISTER_ST)   //=  0x1001,  // Register variable
        CASE(S_CONSTANT_ST)   //=  0x1002,  // constant symbol
        CASE(S_UDT_ST)        //=  0x1003,  // User defined type
        CASE(S_COBOLUDT_ST)   //=  0x1004,  // special UDT for cobol that does not symbol pack
        CASE(S_MANYREG_ST)    //=  0x1005,  // multiple register variable
        CASE(S_BPREL32_ST)    //=  0x1006,  // BP-relative
        CASE(S_LDATA32_ST)    //=  0x1007,  // Module-local symbol
        CASE(S_GDATA32_ST)    //=  0x1008,  // Global data symbol
        CASE(S_PUB32_ST)      //=  0x1009,  // a public symbol (CV internal reserved)
        CASE(S_LPROC32_ST)    //=  0x100a,  // Local procedure start
        CASE(S_GPROC32_ST)    //=  0x100b,  // Global procedure start
        CASE(S_VFTABLE32)     //=  0x100c,  // address of virtual function table
        CASE(S_REGREL32_ST)   //=  0x100d,  // register relative address
        CASE(S_LTHREAD32_ST)  //=  0x100e,  // local thread storage
        CASE(S_GTHREAD32_ST)  //=  0x100f,  // global thread storage

        CASE(S_LPROCMIPS_ST) // =  0x1010,  // Local procedure start
        CASE(S_GPROCMIPS_ST)  //=  0x1011,  // Global procedure start

        CASE(S_FRAMEPROC)    // =  0x1012,  // extra frame and proc information
        CASE(S_COMPILE2_ST)   //=  0x1013,  // extended compile flags and info

        CASE(S_MANYREG2_ST)  // =  0x1014,  // multiple register variable
        CASE(S_LPROCIA64_ST)  //=  0x1015,  // Local procedure start (IA64)
        CASE(S_GPROCIA64_ST)  //=  0x1016,  // Global procedure start (IA64)
        CASE(S_LOCALSLOT_ST)  //=  0x1017,  // local IL sym with field for local slot index
        CASE(S_PARAMSLOT_ST)  //=  0x1018,  // local IL sym with field for parameter slot index
        CASE(S_ANNOTATION)   // =  0x1019,  // Annotation string literals
        CASE(S_GMANPROC_ST)   //=  0x101a,  // Global proc
        CASE(S_LMANPROC_ST)   //=  0x101b,  // Local proc
        CASE(S_RESERVED1)     //=  0x101c,  // reserved
        CASE(S_RESERVED2)     //=  0x101d,  // reserved
        CASE(S_RESERVED3)     //=  0x101e,  // reserved
        CASE(S_RESERVED4)     //=  0x101f,  // reserved
        CASE(S_LMANDATA_ST)   //=  0x1020,
        CASE(S_GMANDATA_ST)   //=  0x1021,
        CASE(S_MANFRAMEREL_ST)//=  0x1022,
        CASE(S_MANREGISTER_ST)//=  0x1023,
        CASE(S_MANSLOT_ST)    //=  0x1024,
        CASE(S_MANMANYREG_ST) //=  0x1025,
        CASE(S_MANREGREL_ST)  //=  0x1026,
        CASE(S_MANMANYREG2_ST)//=  0x1027,
        CASE(S_MANTYPREF)     //=  0x1028,  // Index for type referenced by name from metadata
        CASE(S_UNAMESPACE_ST) //=  0x1029,  // Using namespace
        CASE(S_ST_MAX)      //  =  0x1100,  // starting point for SZ name symbols
        CASE(S_OBJNAME)    //   =  0x1101,  // path to object file name
        CASE(S_THUNK32)     //  =  0x1102,  // Thunk Start
        CASE(S_BLOCK32)     //  =  0x1103,  // block start
        CASE(S_WITH32)      //  =  0x1104,  // with start
        CASE(S_LABEL32)     //  =  0x1105,  // code label
        CASE(S_REGISTER)    //  =  0x1106,  // Register variable
        CASE(S_CONSTANT)    //  =  0x1107,  // constant symbol
        CASE(S_UDT)         //  =  0x1108,  // User defined type
        CASE(S_COBOLUDT)    //  =  0x1109,  // special UDT for cobol that does not symbol pack
        CASE(S_MANYREG)     //  =  0x110a,  // multiple register variable
        CASE(S_BPREL32)     //  =  0x110b,  // BP-relative
        CASE(S_LDATA32)     //  =  0x110c,  // Module-local symbol
        CASE(S_GDATA32)     //  =  0x110d,  // Global data symbol
        CASE(S_PUB32)       //  =  0x110e,  // a public symbol (CV internal reserved)
        CASE(S_LPROC32)     //  =  0x110f,  // Local procedure start
        CASE(S_GPROC32)     //  =  0x1110,  // Global procedure start
        CASE(S_REGREL32)    //  =  0x1111,  // register relative address
        CASE(S_LTHREAD32)   //  =  0x1112,  // local thread storage
        CASE(S_GTHREAD32)   //  =  0x1113,  // global thread storage
        CASE(S_LPROCMIPS)    // =  0x1114,  // Local procedure start
        CASE(S_GPROCMIPS)     //=  0x1115,  // Global procedure start
        CASE(S_COMPILE2)      //=  0x1116,  // extended compile flags and info
        CASE(S_MANYREG2)      //=  0x1117,  // multiple register variable
        CASE(S_LPROCIA64)     //=  0x1118,  // Local procedure start (IA64)
        CASE(S_GPROCIA64)     //=  0x1119,  // Global procedure start (IA64)
        CASE(S_LOCALSLOT)     //=  0x111a,  // local IL sym with field for local slot index
        // CASE(S_SLOT)          //= S_LOCALSLOT,  // alias for LOCALSLOT
        CASE(S_PARAMSLOT)     //=  0x111b,  // local IL sym with field for parameter slot index
        CASE(S_LMANDATA)     // =  0x111c,
        CASE(S_GMANDATA)     // =  0x111d,
        CASE(S_MANFRAMEREL)  // =  0x111e,
        CASE(S_MANREGISTER)  // =  0x111f,
        CASE(S_MANSLOT)      // =  0x1120,
        CASE(S_MANMANYREG)   // =  0x1121,
        CASE(S_MANREGREL)    // =  0x1122,
        CASE(S_MANMANYREG2)  // =  0x1123,
        CASE(S_UNAMESPACE)   // =  0x1124,  // Using namespace
        CASE(S_PROCREF)       //=  0x1125,  // Reference to a procedure
        CASE(S_DATAREF)       //=  0x1126,  // Reference to data
        CASE(S_LPROCREF)      //=  0x1127,  // Local Reference to a procedure
        CASE(S_ANNOTATIONREF) //=  0x1128,  // Reference to an S_ANNOTATION symbol
        CASE(S_TOKENREF)      //=  0x1129,  // Reference to one of the many MANPROCSYM's
        CASE(S_GMANPROC)      // =  0x112a,  // Global proc
        CASE(S_LMANPROC)      // =  0x112b,  // Local proc
        CASE(S_TRAMPOLINE)    //=  0x112c,  // trampoline thunks
        CASE(S_MANCONSTANT)   //=  0x112d,  // constants with metadata type info
        CASE(S_ATTR_FRAMEREL) //=  0x112e,  // relative to virtual frame ptr
        CASE(S_ATTR_REGISTER) //=  0x112f,  // stored in a register
        CASE(S_ATTR_REGREL)   //=  0x1130,  // relative to register (alternate frame ptr)
        CASE(S_ATTR_MANYREG)  //=  0x1131,  // stored in >1 register
        CASE(S_SEPCODE)      // =  0x1132,
        CASE(S_LOCAL_2005)   // =  0x1133,  // defines a local symbol in optimized code
        CASE(S_DEFRANGE_2005) //=  0x1134,  // defines a single range of addresses in which symbol can be evaluated
        CASE(S_DEFRANGE2_2005)// =  0x1135,  // defines ranges of addresses in which symbol can be evaluated
        CASE(S_SECTION)      // =  0x1136,  // A COFF section in a PE executable
        CASE(S_COFFGROUP)    // =  0x1137,  // A COFF group
        CASE(S_EXPORT)       // =  0x1138,  // A export
        CASE(S_CALLSITEINFO) // =  0x1139,  // Indirect call site information
        CASE(S_FRAMECOOKIE)   //=  0x113a,  // Security cookie information
        CASE(S_DISCARDED)   //  =  0x113b,  // Discarded by LINK /OPT:REF (experimental, see richards)
        CASE(S_COMPILE3)   //   =  0x113c,  // Replacement for S_COMPILE2
        CASE(S_ENVBLOCK)     // =  0x113d,  // Environment block split off from S_COMPILE2
        CASE(S_LOCAL)        // =  0x113e,  // defines a local symbol in optimized code
        CASE(S_DEFRANGE)    //  =  0x113f,  // defines a single range of addresses in which symbol can be evaluated
        CASE(S_DEFRANGE_SUBFIELD) //=  0x1140,           // ranges for a subfield
        CASE(S_DEFRANGE_REGISTER) //=  0x1141,           // ranges for en-registered symbol
        CASE(S_DEFRANGE_FRAMEPOINTER_REL) //=  0x1142,   // range for stack symbol.
        CASE(S_DEFRANGE_SUBFIELD_REGISTER) //=  0x1143,  // ranges for en-registered field of symbol
        CASE(S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE)// =  0x1144, // range for stack symbol span valid full scope of function body, gap might apply.
        CASE(S_DEFRANGE_REGISTER_REL)// =  0x1145, // range for symbol address as register + offset.
        CASE(S_LPROC32_ID)     //=  0x1146,
        CASE(S_GPROC32_ID)     //=  0x1147,
        CASE(S_LPROCMIPS_ID)   //=  0x1148,
        CASE(S_GPROCMIPS_ID)   //=  0x1149,
        CASE(S_LPROCIA64_ID)   //=  0x114a,
        CASE(S_GPROCIA64_ID)   //=  0x114b,

        CASE(S_BUILDINFO)      //= 0x114c, // build information.
        CASE(S_INLINESITE)     //= 0x114d, // inlined function callsite.
        CASE(S_INLINESITE_END) //= 0x114e,
        CASE(S_PROC_ID_END)    //= 0x114f,
        CASE(S_DEFRANGE_HLSL)  //= 0x1150,
        CASE(S_GDATA_HLSL)     //= 0x1151,
        CASE(S_LDATA_HLSL)     //= 0x1152,
        CASE(S_FILESTATIC)     //= 0x1153,

        CASE(S_ARMSWITCHTABLE) //  = 0x1159,
        CASE(S_CALLEES) // = 0x115a,
        CASE(S_CALLERS) // = 0x115b,
        CASE(S_POGODATA) // = 0x115c,
        CASE(S_INLINESITE2) // = 0x115d,      // extended inline site information
        CASE(S_HEAPALLOCSITE) // = 0x115e,    // heap allocation site
        CASE(S_MOD_TYPEREF) // = 0x115f,      // only generated at link time
        CASE(S_REF_MINIPDB) // = 0x1160,      // only generated at link time for mini PDB
        CASE(S_PDBMAP) //      = 0x1161,      // only generated at link time for mini PDB

        CASE(S_GDATA_HLSL32) // = 0x1162,
        CASE(S_LDATA_HLSL32) // = 0x1163,

        CASE(S_GDATA_HLSL32_EX) // = 0x1164,
        CASE(S_LDATA_HLSL32_EX) // = 0x1165,
    }
    if(nullAsUnknown)
        return nullptr;
    else
        return "UNKNOWN_RECORD";
    #undef CASE
}
const char* ToString(LeafType type, bool nullAsUnknown){
    #define CASE(X) case X: return #X;
    switch(type){
        CASE(LF_MODIFIER_16t     )
        CASE(LF_POINTER_16t      )
        CASE(LF_ARRAY_16t        )
        CASE(LF_CLASS_16t        )
        CASE(LF_STRUCTURE_16t    )
        CASE(LF_UNION_16t        )
        CASE(LF_ENUM_16t         )
        CASE(LF_PROCEDURE_16t    )
        CASE(LF_MFUNCTION_16t    )
        CASE(LF_VTSHAPE          )
        CASE(LF_COBOL0_16t       )
        CASE(LF_COBOL1           )
        CASE(LF_BARRAY_16t       )
        CASE(LF_LABEL            )
        CASE(LF_NULL             )
        CASE(LF_NOTTRAN          )
        CASE(LF_DIMARRAY_16t     )
        CASE(LF_VFTPATH_16t      )
        CASE(LF_PRECOMP_16t      )
        CASE(LF_ENDPRECOMP       )
        CASE(LF_OEM_16t          )
        CASE(LF_TYPESERVER_ST    )

        CASE(LF_SKIP_16t         )
        CASE(LF_ARGLIST_16t      )
        CASE(LF_DEFARG_16t       )
        CASE(LF_LIST             )
        CASE(LF_FIELDLIST_16t    )
        CASE(LF_DERIVED_16t      )
        CASE(LF_BITFIELD_16t     )
        CASE(LF_METHODLIST_16t   )
        CASE(LF_DIMCONU_16t      )
        CASE(LF_DIMCONLU_16t     )
        CASE(LF_DIMVARU_16t      )
        CASE(LF_DIMVARLU_16t     )
        CASE(LF_REFSYM           )

        CASE(LF_BCLASS_16t       )
        CASE(LF_VBCLASS_16t      )
        CASE(LF_IVBCLASS_16t     )
        CASE(LF_ENUMERATE_ST     )
        CASE(LF_FRIENDFCN_16t    )
        CASE(LF_INDEX_16t        )
        CASE(LF_MEMBER_16t       )
        CASE(LF_STMEMBER_16t     )
        CASE(LF_METHOD_16t       )
        CASE(LF_NESTTYPE_16t     )
        CASE(LF_VFUNCTAB_16t     )
        CASE(LF_FRIENDCLS_16t    )
        CASE(LF_ONEMETHOD_16t    )
        CASE(LF_VFUNCOFF_16t     )

        CASE(LF_TI16_MAX        )

        CASE(LF_MODIFIER         )
        CASE(LF_POINTER          )
        CASE(LF_ARRAY_ST         )
        CASE(LF_CLASS_ST         )
        CASE(LF_STRUCTURE_ST     )
        CASE(LF_UNION_ST         )
        CASE(LF_ENUM_ST          )
        CASE(LF_PROCEDURE        )
        CASE(LF_MFUNCTION        )
        CASE(LF_COBOL0           )
        CASE(LF_BARRAY           )
        CASE(LF_DIMARRAY_ST      )
        CASE(LF_VFTPATH          )
        CASE(LF_PRECOMP_ST       )
        CASE(LF_OEM              )
        CASE(LF_ALIAS_ST         )
        CASE(LF_OEM2             )

        CASE(LF_SKIP             )
        CASE(LF_ARGLIST          )
        CASE(LF_DEFARG_ST        )
        CASE(LF_FIELDLIST        )
        CASE(LF_DERIVED          )
        CASE(LF_BITFIELD         )
        CASE(LF_METHODLIST       )
        CASE(LF_DIMCONU          )
        CASE(LF_DIMCONLU         )
        CASE(LF_DIMVARU          )
        CASE(LF_DIMVARLU         )

        CASE(LF_BCLASS          )
        CASE(LF_VBCLASS         )
        CASE(LF_IVBCLASS        )
        CASE(LF_FRIENDFCN_ST    )
        CASE(LF_INDEX           )
        CASE(LF_MEMBER_ST       )
        CASE(LF_STMEMBER_ST     )
        CASE(LF_METHOD_ST       )
        CASE(LF_NESTTYPE_ST     )
        CASE(LF_VFUNCTAB        )
        CASE(LF_FRIENDCLS       )
        CASE(LF_ONEMETHOD_ST    )
        CASE(LF_VFUNCOFF        )
        CASE(LF_NESTTYPEEX_ST   )
        CASE(LF_MEMBERMODIFY_ST )
        CASE(LF_MANAGED_ST      )

        CASE(LF_ST_MAX          )

        CASE(LF_TYPESERVER       )
        CASE(LF_ENUMERATE        )
        CASE(LF_ARRAY            )
        CASE(LF_CLASS            )
        CASE(LF_STRUCTURE        )
        CASE(LF_UNION            )
        CASE(LF_ENUM             )
        CASE(LF_DIMARRAY         )
        CASE(LF_PRECOMP          )
        CASE(LF_ALIAS            )
        CASE(LF_DEFARG           )
        CASE(LF_FRIENDFCN        )
        CASE(LF_MEMBER           )
        CASE(LF_STMEMBER         )
        CASE(LF_METHOD           )
        CASE(LF_NESTTYPE         )
        CASE(LF_ONEMETHOD        )
        CASE(LF_NESTTYPEEX       )
        CASE(LF_MEMBERMODIFY     )
        CASE(LF_MANAGED          )
        CASE(LF_TYPESERVER2      )

        CASE(LF_STRIDED_ARRAY   )
        CASE(LF_HLSL            )
        CASE(LF_MODIFIER_EX     )
        CASE(LF_INTERFACE       )
        CASE(LF_BINTERFACE      )
        CASE(LF_VECTOR          )
        CASE(LF_MATRIX          )

        CASE(LF_VFTABLE          )

        CASE(LF_TYPE_LAST     )

        CASE(LF_FUNC_ID         )
        CASE(LF_MFUNC_ID        )
        CASE(LF_BUILDINFO       )
        CASE(LF_SUBSTR_LIST     )
        CASE(LF_STRING_ID       )

        CASE(LF_UDT_SRC_LINE    )

        CASE(LF_UDT_MOD_SRC_LINE )

        CASE(LF_ID_LAST         )

        CASE(LF_CHAR             )
        CASE(LF_SHORT            )
        CASE(LF_USHORT           )
        CASE(LF_LONG             )
        CASE(LF_ULONG            )
        CASE(LF_REAL32           )
        CASE(LF_REAL64           )
        CASE(LF_REAL80           )
        CASE(LF_REAL128          )
        CASE(LF_QUADWORD         )
        CASE(LF_UQUADWORD        )
        CASE(LF_REAL48           )
        CASE(LF_COMPLEX32        )
        CASE(LF_COMPLEX64        )
        CASE(LF_COMPLEX80        )
        CASE(LF_COMPLEX128       )
        CASE(LF_VARSTRING        )

        CASE(LF_OCTWORD         )
        CASE(LF_UOCTWORD        )

        CASE(LF_DECIMAL         )
        CASE(LF_DATE            )
        CASE(LF_UTF8STRING      )

        CASE(LF_REAL16          )
        
        CASE(LF_PAD0           )
        CASE(LF_PAD1           )
        CASE(LF_PAD2           )
        CASE(LF_PAD3           )
        CASE(LF_PAD4           )
        CASE(LF_PAD5           )
        CASE(LF_PAD6           )
        CASE(LF_PAD7           )
        CASE(LF_PAD8           )
        CASE(LF_PAD9           )
        CASE(LF_PAD10          )
        CASE(LF_PAD11          )
        CASE(LF_PAD12          )
        CASE(LF_PAD13          )
        CASE(LF_PAD14          )
        CASE(LF_PAD15          )
    }
    if(nullAsUnknown)
        return nullptr;
    else
        return "UNKNOWN_RECORD";
    #undef CASE
}

const char* registerNames_x64[] = {
    "None",         // 0   CV_REG_NONE
    "al",           // 1   CV_REG_AL
    "cl",           // 2   CV_REG_CL
    "dl",           // 3   CV_REG_DL
    "bl",           // 4   CV_REG_BL
    "ah",           // 5   CV_REG_AH
    "ch",           // 6   CV_REG_CH
    "dh",           // 7   CV_REG_DH
    "bh",           // 8   CV_REG_BH
    "ax",           // 9   CV_REG_AX
    "cx",           // 10  CV_REG_CX
    "dx",           // 11  CV_REG_DX
    "bx",           // 12  CV_REG_BX
    "sp",           // 13  CV_REG_SP
    "bp",           // 14  CV_REG_BP
    "si",           // 15  CV_REG_SI
    "di",           // 16  CV_REG_DI
    "eax",          // 17  CV_REG_EAX
    "ecx",          // 18  CV_REG_ECX
    "edx",          // 19  CV_REG_EDX
    "ebx",          // 20  CV_REG_EBX
    "esp",          // 21  CV_REG_ESP
    "ebp",          // 22  CV_REG_EBP
    "esi",          // 23  CV_REG_ESI
    "edi",          // 24  CV_REG_EDI
    "es",           // 25  CV_REG_ES
    "cs",           // 26  CV_REG_CS
    "ss",           // 27  CV_REG_SS
    "ds",           // 28  CV_REG_DS
    "fs",           // 29  CV_REG_FS
    "gs",           // 30  CV_REG_GS
    "ip",           // 31  CV_REG_IP
    "flags",        // 32  CV_REG_FLAGS
    "eip",          // 33  CV_REG_EIP
    "eflags",       // 34  CV_REG_EFLAG
    "???",          // 35
    "???",          // 36
    "???",          // 37
    "???",          // 38
    "???",          // 39
    "temp",         // 40  CV_REG_TEMP
    "temph",        // 41  CV_REG_TEMPH
    "quote",        // 42  CV_REG_QUOTE
    "pcdr3",        // 43  CV_REG_PCDR3
    "pcdr4",        // 44  CV_REG_PCDR4
    "pcdr5",        // 45  CV_REG_PCDR5
    "pcdr6",        // 46  CV_REG_PCDR6
    "pcdr7",        // 47  CV_REG_PCDR7
    "???",          // 48
    "???",          // 49
    "???",          // 50
    "???",          // 51
    "???",          // 52
    "???",          // 53
    "???",          // 54
    "???",          // 55
    "???",          // 56
    "???",          // 57
    "???",          // 58
    "???",          // 59
    "???",          // 60
    "???",          // 61
    "???",          // 62
    "???",          // 63
    "???",          // 64
    "???",          // 65
    "???",          // 66
    "???",          // 67
    "???",          // 68
    "???",          // 69
    "???",          // 70
    "???",          // 71
    "???",          // 72
    "???",          // 73
    "???",          // 74
    "???",          // 75
    "???",          // 76
    "???",          // 77
    "???",          // 78
    "???",          // 79
    "cr0",          // 80  CV_REG_CR0
    "cr1",          // 81  CV_REG_CR1
    "cr2",          // 82  CV_REG_CR2
    "cr3",          // 83  CV_REG_CR3
    "cr4",          // 84  CV_REG_CR4
    "???",          // 85
    "???",          // 86
    "???",          // 87
    "???",          // 88
    "???",          // 89
    "dr0",          // 90  CV_REG_DR0
    "dr1",          // 91  CV_REG_DR1
    "dr2",          // 92  CV_REG_DR2
    "dr3",          // 93  CV_REG_DR3
    "dr4",          // 94  CV_REG_DR4
    "dr5",          // 95  CV_REG_DR5
    "dr6",          // 96  CV_REG_DR6
    "dr7",          // 97  CV_REG_DR7
    "???",          // 98
    "???",          // 99
    "???",          // 100
    "???",          // 101
    "???",          // 102
    "???",          // 103
    "???",          // 104
    "???",          // 105
    "???",          // 106
    "???",          // 107
    "???",          // 108
    "???",          // 109
    "gdtr",         // 110 CV_REG_GDTR
    "gdtl",         // 111 CV_REG_GDTL
    "idtr",         // 112 CV_REG_IDTR
    "idtl",         // 113 CV_REG_IDTL
    "ldtr",         // 114 CV_REG_LDTR
    "tr",           // 115 CV_REG_TR
    "???",          // 116
    "???",          // 117
    "???",          // 118
    "???",          // 119
    "???",          // 120
    "???",          // 121
    "???",          // 122
    "???",          // 123
    "???",          // 124
    "???",          // 125
    "???",          // 126
    "???",          // 127
    "st(0)",        // 128 CV_REG_ST0
    "st(1)",        // 129 CV_REG_ST1
    "st(2)",        // 130 CV_REG_ST2
    "st(3)",        // 131 CV_REG_ST3
    "st(4)",        // 132 CV_REG_ST4
    "st(5)",        // 133 CV_REG_ST5
    "st(6)",        // 134 CV_REG_ST6
    "st(7)",        // 135 CV_REG_ST7
    "ctrl",         // 136 CV_REG_CTRL
    "stat",         // 137 CV_REG_STAT
    "tag",          // 138 CV_REG_TAG
    "fpip",         // 139 CV_REG_FPIP
    "fpcs",         // 140 CV_REG_FPCS
    "fpdo",         // 141 CV_REG_FPDO
    "fpds",         // 142 CV_REG_FPDS
    "isem",         // 143 CV_REG_ISEM
    "fpeip",        // 144 CV_REG_FPEIP
    "fped0"         // 145 CV_REG_FPEDO
};
const char * registerNames_AMD64[] = {
    "None",         // 0   CV_REG_NONE
    "al",           // 1   CV_AMD64_AL
    "cl",           // 2   CV_AMD64_CL
    "dl",           // 3   CV_AMD64_DL
    "bl",           // 4   CV_AMD64_BL
    "ah",           // 5   CV_AMD64_AH
    "ch",           // 6   CV_AMD64_CH
    "dh",           // 7   CV_AMD64_DH
    "bh",           // 8   CV_AMD64_BH
    "ax",           // 9   CV_AMD64_AX
    "cx",           // 10  CV_AMD64_CX
    "dx",           // 11  CV_AMD64_DX
    "bx",           // 12  CV_AMD64_BX
    "sp",           // 13  CV_AMD64_SP
    "bp",           // 14  CV_AMD64_BP
    "si",           // 15  CV_AMD64_SI
    "di",           // 16  CV_AMD64_DI
    "eax",          // 17  CV_AMD64_EAX
    "ecx",          // 18  CV_AMD64_ECX
    "edx",          // 19  CV_AMD64_EDX
    "ebx",          // 20  CV_AMD64_EBX
    "esp",          // 21  CV_AMD64_ESP
    "ebp",          // 22  CV_AMD64_EBP
    "esi",          // 23  CV_AMD64_ESI
    "edi",          // 24  CV_AMD64_EDI
    "es",           // 25  CV_AMD64_ES
    "cs",           // 26  CV_AMD64_CS
    "ss",           // 27  CV_AMD64_SS
    "ds",           // 28  CV_AMD64_DS
    "fs",           // 29  CV_AMD64_FS
    "gs",           // 30  CV_AMD64_GS
    "flags",        // 31  CV_AMD64_FLAGS
    "rip",          // 32  CV_AMD64_RIP
    "eflags",       // 33  CV_AMD64_EFLAGS
    "???",          // 34
    "???",          // 35
    "???",          // 36
    "???",          // 37
    "???",          // 38
    "???",          // 39
    "???",          // 40
    "???",          // 41
    "???",          // 42
    "???",          // 43
    "???",          // 44
    "???",          // 45
    "???",          // 46
    "???",          // 47
    "???",          // 48
    "???",          // 49
    "???",          // 50
    "???",          // 51
    "???",          // 52
    "???",          // 53
    "???",          // 54
    "???",          // 55
    "???",          // 56
    "???",          // 57
    "???",          // 58
    "???",          // 59
    "???",          // 60
    "???",          // 61
    "???",          // 62
    "???",          // 63
    "???",          // 64
    "???",          // 65
    "???",          // 66
    "???",          // 67
    "???",          // 68
    "???",          // 69
    "???",          // 70
    "???",          // 71
    "???",          // 72
    "???",          // 73
    "???",          // 74
    "???",          // 75
    "???",          // 76
    "???",          // 77
    "???",          // 78
    "???",          // 79
    "cr0",          // 80  CV_AMD64_CR0
    "cr1",          // 81  CV_AMD64_CR1
    "cr2",          // 82  CV_AMD64_CR2
    "cr3",          // 83  CV_AMD64_CR3
    "cr4",          // 84  CV_AMD64_CR4
    "???",          // 85
    "???",          // 86
    "???",          // 87
    "cr8",          // 88  CV_AMD64_CR8
    "???",          // 89
    "dr0",          // 90  CV_AMD64_DR0
    "dr1",          // 91  CV_AMD64_DR1
    "dr2",          // 92  CV_AMD64_DR2
    "dr3",          // 93  CV_AMD64_DR3
    "dr4",          // 94  CV_AMD64_DR4
    "dr5",          // 95  CV_AMD64_DR5
    "dr6",          // 96  CV_AMD64_DR6
    "dr7",          // 97  CV_AMD64_DR7
    "dr8",          // 98  CV_AMD64_DR8
    "dr9",          // 99  CV_AMD64_DR9
    "dr10",         // 100 CV_AMD64_DR10
    "dr11",         // 101 CV_AMD64_DR11
    "dr12",         // 102 CV_AMD64_DR12
    "dr13",         // 103 CV_AMD64_DR13
    "dr14",         // 104 CV_AMD64_DR14
    "dr15",         // 105 CV_AMD64_DR15
    "???",          // 106
    "???",          // 107
    "???",          // 108
    "???",          // 109
    "gdtr",         // 110 CV_AMD64_GDTR
    "gdtl",         // 111 CV_AMD64_GDTL
    "idtr",         // 112 CV_AMD64_IDTR
    "idtl",         // 113 CV_AMD64_IDTL
    "ldtr",         // 114 CV_AMD64_LDTR
    "tr",           // 115 CV_AMD64_TR
    "???",          // 116
    "???",          // 117
    "???",          // 118
    "???",          // 119
    "???",          // 120
    "???",          // 121
    "???",          // 122
    "???",          // 123
    "???",          // 124
    "???",          // 125
    "???",          // 126
    "???",          // 127
    "st(0)",        // 128 CV_AMD64_ST0
    "st(1)",        // 129 CV_AMD64_ST1
    "st(2)",        // 130 CV_AMD64_ST2
    "st(3)",        // 131 CV_AMD64_ST3
    "st(4)",        // 132 CV_AMD64_ST4
    "st(5)",        // 133 CV_AMD64_ST5
    "st(6)",        // 134 CV_AMD64_ST6
    "st(7)",        // 135 CV_AMD64_ST7
    "ctrl",         // 136 CV_AMD64_CTRL
    "stat",         // 137 CV_AMD64_STAT
    "tag",          // 138 CV_AMD64_TAG
    "fpip",         // 139 CV_AMD64_FPIP
    "fpcs",         // 140 CV_AMD64_FPCS
    "fpdo",         // 141 CV_AMD64_FPDO
    "fpds",         // 142 CV_AMD64_FPDS
    "isem",         // 143 CV_AMD64_ISEM
    "fpeip",        // 144 CV_AMD64_FPEIP
    "fped0",        // 145 CV_AMD64_FPEDO
    "mm0",          // 146 CV_AMD64_MM0
    "mm1",          // 147 CV_AMD64_MM1
    "mm2",          // 148 CV_AMD64_MM2
    "mm3",          // 149 CV_AMD64_MM3
    "mm4",          // 150 CV_AMD64_MM4
    "mm5",          // 151 CV_AMD64_MM5
    "mm6",          // 152 CV_AMD64_MM6
    "mm7",          // 153 CV_AMD64_MM7
    "xmm0",         // 154 CV_AMD64_XMM0
    "xmm1",         // 155 CV_AMD64_XMM1
    "xmm2",         // 156 CV_AMD64_XMM2
    "xmm3",         // 157 CV_AMD64_XMM3
    "xmm4",         // 158 CV_AMD64_XMM4
    "xmm5",         // 159 CV_AMD64_XMM5
    "xmm6",         // 160 CV_AMD64_XMM6
    "xmm7",         // 161 CV_AMD64_XMM7
    "xmm0_0",       // 162 CV_AMD64_XMM0_0
    "xmm0_1",       // 163 CV_AMD64_XMM0_1
    "xmm0_2",       // 164 CV_AMD64_XMM0_2
    "xmm0_3",       // 165 CV_AMD64_XMM0_3
    "xmm1_0",       // 166 CV_AMD64_XMM1_0
    "xmm1_1",       // 167 CV_AMD64_XMM1_1
    "xmm1_2",       // 168 CV_AMD64_XMM1_2
    "xmm1_3",       // 169 CV_AMD64_XMM1_3
    "xmm2_0",       // 170 CV_AMD64_XMM2_0
    "xmm2_1",       // 171 CV_AMD64_XMM2_1
    "xmm2_2",       // 172 CV_AMD64_XMM2_2
    "xmm2_3",       // 173 CV_AMD64_XMM2_3
    "xmm3_0",       // 174 CV_AMD64_XMM3_0
    "xmm3_1",       // 175 CV_AMD64_XMM3_1
    "xmm3_2",       // 176 CV_AMD64_XMM3_2
    "xmm3_3",       // 177 CV_AMD64_XMM3_3
    "xmm4_0",       // 178 CV_AMD64_XMM4_0
    "xmm4_1",       // 179 CV_AMD64_XMM4_1
    "xmm4_2",       // 180 CV_AMD64_XMM4_2
    "xmm4_3",       // 181 CV_AMD64_XMM4_3
    "xmm5_0",       // 182 CV_AMD64_XMM5_0
    "xmm5_1",       // 183 CV_AMD64_XMM5_1
    "xmm5_2",       // 184 CV_AMD64_XMM5_2
    "xmm5_3",       // 185 CV_AMD64_XMM5_3
    "xmm6_0",       // 186 CV_AMD64_XMM6_0
    "xmm6_1",       // 187 CV_AMD64_XMM6_1
    "xmm6_2",       // 188 CV_AMD64_XMM6_2
    "xmm6_3",       // 189 CV_AMD64_XMM6_3
    "xmm7_0",       // 190 CV_AMD64_XMM7_0
    "xmm7_1",       // 191 CV_AMD64_XMM7_1
    "xmm7_2",       // 192 CV_AMD64_XMM7_2
    "xmm7_3",       // 193 CV_AMD64_XMM7_3
    "xmm0l",        // 194 CV_AMD64_XMM0L
    "xmm1l",        // 195 CV_AMD64_XMM1L
    "xmm2l",        // 196 CV_AMD64_XMM2L
    "xmm3l",        // 197 CV_AMD64_XMM3L
    "xmm4l",        // 198 CV_AMD64_XMM4L
    "xmm5l",        // 199 CV_AMD64_XMM5L
    "xmm6l",        // 200 CV_AMD64_XMM6L
    "xmm7l",        // 201 CV_AMD64_XMM7L
    "xmm0h",        // 202 CV_AMD64_XMM0H
    "xmm1h",        // 203 CV_AMD64_XMM1H
    "xmm2h",        // 204 CV_AMD64_XMM2H
    "xmm3h",        // 205 CV_AMD64_XMM3H
    "xmm4h",        // 206 CV_AMD64_XMM4H
    "xmm5h",        // 207 CV_AMD64_XMM5H
    "xmm6h",        // 208 CV_AMD64_XMM6H
    "xmm7h",        // 209 CV_AMD64_XMM7H
    "???",          // 210
    "mxcsr",        // 211 CV_AMD64_MXCSR
    "???",          // 212
    "???",          // 213
    "???",          // 214
    "???",          // 215
    "???",          // 216
    "???",          // 217
    "???",          // 218
    "???",          // 219
    "emm0l",        // 220 CV_AMD64_EMM0L
    "emm1l",        // 221 CV_AMD64_EMM1L
    "emm2l",        // 222 CV_AMD64_EMM2L
    "emm3l",        // 223 CV_AMD64_EMM3L
    "emm4l",        // 224 CV_AMD64_EMM4L
    "emm5l",        // 225 CV_AMD64_EMM5L
    "emm6l",        // 226 CV_AMD64_EMM6L
    "emm7l",        // 227 CV_AMD64_EMM7L
    "emm0h",        // 228 CV_AMD64_EMM0H
    "emm1h",        // 229 CV_AMD64_EMM1H
    "emm2h",        // 230 CV_AMD64_EMM2H
    "emm3h",        // 231 CV_AMD64_EMM3H
    "emm4h",        // 232 CV_AMD64_EMM4H
    "emm5h",        // 233 CV_AMD64_EMM5H
    "emm6h",        // 234 CV_AMD64_EMM6H
    "emm7h",        // 235 CV_AMD64_EMM7H
    "mm00",         // 236 CV_AMD64_MM00
    "mm01",         // 237 CV_AMD64_MM01
    "mm10",         // 238 CV_AMD64_MM10
    "mm11",         // 239 CV_AMD64_MM11
    "mm20",         // 240 CV_AMD64_MM20
    "mm21",         // 241 CV_AMD64_MM21
    "mm30",         // 242 CV_AMD64_MM30
    "mm31",         // 243 CV_AMD64_MM31
    "mm40",         // 244 CV_AMD64_MM40
    "mm41",         // 245 CV_AMD64_MM41
    "mm50",         // 246 CV_AMD64_MM50
    "mm51",         // 247 CV_AMD64_MM51
    "mm60",         // 248 CV_AMD64_MM60
    "mm61",         // 249 CV_AMD64_MM61
    "mm70",         // 250 CV_AMD64_MM70
    "mm71",         // 251 CV_AMD64_MM71
    "xmm8",         // 252 CV_AMD64_XMM8
    "xmm9",         // 253 CV_AMD64_XMM9
    "xmm10",        // 254 CV_AMD64_XMM10
    "xmm11",        // 255 CV_AMD64_XMM11
    "xmm12",        // 256 CV_AMD64_XMM12
    "xmm13",        // 257 CV_AMD64_XMM13
    "xmm14",        // 258 CV_AMD64_XMM14
    "xmm15",        // 259 CV_AMD64_XMM15
    "xmm8_0",       // 260 CV_AMD64_XMM8_0
    "xmm8_1",       // 261 CV_AMD64_XMM8_1
    "xmm8_2",       // 262 CV_AMD64_XMM8_2
    "xmm8_3",       // 263 CV_AMD64_XMM8_3
    "xmm9_0",       // 264 CV_AMD64_XMM9_0
    "xmm9_1",       // 265 CV_AMD64_XMM9_1
    "xmm9_2",       // 266 CV_AMD64_XMM9_2
    "xmm9_3",       // 267 CV_AMD64_XMM9_3
    "xmm10_0",      // 268 CV_AMD64_XMM10_0
    "xmm10_1",      // 269 CV_AMD64_XMM10_1
    "xmm10_2",      // 270 CV_AMD64_XMM10_2
    "xmm10_3",      // 271 CV_AMD64_XMM10_3
    "xmm11_0",      // 272 CV_AMD64_XMM11_0
    "xmm11_1",      // 273 CV_AMD64_XMM11_1
    "xmm11_2",      // 274 CV_AMD64_XMM11_2
    "xmm11_3",      // 275 CV_AMD64_XMM11_3
    "xmm12_0",      // 276 CV_AMD64_XMM12_0
    "xmm12_1",      // 277 CV_AMD64_XMM12_1
    "xmm12_2",      // 278 CV_AMD64_XMM12_2
    "xmm12_3",      // 279 CV_AMD64_XMM12_3
    "xmm13_0",      // 280 CV_AMD64_XMM13_0
    "xmm13_1",      // 281 CV_AMD64_XMM13_1
    "xmm13_2",      // 282 CV_AMD64_XMM13_2
    "xmm13_3",      // 283 CV_AMD64_XMM13_3
    "xmm14_0",      // 284 CV_AMD64_XMM14_0
    "xmm14_1",      // 285 CV_AMD64_XMM14_1
    "xmm14_2",      // 286 CV_AMD64_XMM14_2
    "xmm14_3",      // 287 CV_AMD64_XMM14_3
    "xmm15_0",      // 288 CV_AMD64_XMM15_0
    "xmm15_1",      // 289 CV_AMD64_XMM15_1
    "xmm15_2",      // 290 CV_AMD64_XMM15_2
    "xmm15_3",      // 291 CV_AMD64_XMM15_3
    "xmm8l",        // 292 CV_AMD64_XMM8L
    "xmm9l",        // 293 CV_AMD64_XMM9L
    "xmm10l",       // 294 CV_AMD64_XMM10L
    "xmm11l",       // 295 CV_AMD64_XMM11L
    "xmm12l",       // 296 CV_AMD64_XMM12L
    "xmm13l",       // 297 CV_AMD64_XMM13L
    "xmm14l",       // 298 CV_AMD64_XMM14L
    "xmm15l",       // 299 CV_AMD64_XMM15L
    "xmm8h",        // 300 CV_AMD64_XMM8H
    "xmm9h",        // 301 CV_AMD64_XMM9H
    "xmm10h",       // 302 CV_AMD64_XMM10H
    "xmm11h",       // 303 CV_AMD64_XMM11H
    "xmm12h",       // 304 CV_AMD64_XMM12H
    "xmm13h",       // 305 CV_AMD64_XMM13H
    "xmm14h",       // 306 CV_AMD64_XMM14H
    "xmm15h",       // 307 CV_AMD64_XMM15H
    "emm8l",        // 308 CV_AMD64_EMM8L
    "emm9l",        // 309 CV_AMD64_EMM9L
    "emm10l",       // 310 CV_AMD64_EMM10L
    "emm11l",       // 311 CV_AMD64_EMM11L
    "emm12l",       // 312 CV_AMD64_EMM12L
    "emm13l",       // 313 CV_AMD64_EMM13L
    "emm14l",       // 314 CV_AMD64_EMM14L
    "emm15l",       // 315 CV_AMD64_EMM15L
    "emm8h",        // 316 CV_AMD64_EMM8H
    "emm9h",        // 317 CV_AMD64_EMM9H
    "emm10h",       // 318 CV_AMD64_EMM10H
    "emm11h",       // 319 CV_AMD64_EMM11H
    "emm12h",       // 320 CV_AMD64_EMM12H
    "emm13h",       // 321 CV_AMD64_EMM13H
    "emm14h",       // 322 CV_AMD64_EMM14H
    "emm15h",       // 323 CV_AMD64_EMM15H
    "sil",          // 324 CV_AMD64_SIL
    "dil",          // 325 CV_AMD64_DIL
    "bpl",          // 326 CV_AMD64_BPL
    "spl",          // 327 CV_AMD64_SPL
    "rax",          // 328 CV_AMD64_RAX
    "rbx",          // 329 CV_AMD64_RBX
    "rcx",          // 330 CV_AMD64_RCX
    "rdx",          // 331 CV_AMD64_RDX
    "rsi",          // 332 CV_AMD64_RSI
    "rdi",          // 333 CV_AMD64_RDI
    "rbp",          // 334 CV_AMD64_RBP
    "rsp",          // 335 CV_AMD64_RSP
    "r8",           // 336 CV_AMD64_R8
    "r9",           // 337 CV_AMD64_R9
    "r10",          // 338 CV_AMD64_R10
    "r11",          // 339 CV_AMD64_R11
    "r12",          // 340 CV_AMD64_R12
    "r13",          // 341 CV_AMD64_R13
    "r14",          // 342 CV_AMD64_R14
    "r15",          // 343 CV_AMD64_R15
    "r8b",          // 344 CV_AMD64_R8B
    "r9b",          // 345 CV_AMD64_R9B
    "r10b",         // 346 CV_AMD64_R10B
    "r11b",         // 347 CV_AMD64_R11B
    "r12b",         // 348 CV_AMD64_R12B
    "r13b",         // 349 CV_AMD64_R13B
    "r14b",         // 350 CV_AMD64_R14B
    "r15b",         // 351 CV_AMD64_R15B
    "r8w",          // 352 CV_AMD64_R8W
    "r9w",          // 353 CV_AMD64_R9W
    "r10w",         // 354 CV_AMD64_R10W
    "r11w",         // 355 CV_AMD64_R11W
    "r12w",         // 356 CV_AMD64_R12W
    "r13w",         // 357 CV_AMD64_R13W
    "r14w",         // 358 CV_AMD64_R14W
    "r15w",         // 359 CV_AMD64_R15W
    "r8d",          // 360 CV_AMD64_R8D
    "r9d",          // 361 CV_AMD64_R9D
    "r10d",         // 362 CV_AMD64_R10D
    "r11d",         // 363 CV_AMD64_R11D
    "r12d",         // 364 CV_AMD64_R12D
    "r13d",         // 365 CV_AMD64_R13D
    "r14d",         // 366 CV_AMD64_R14D
    "r15d",         // 367 CV_AMD64_R15D
};