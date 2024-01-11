/*
Implementation:
    
*/

#include "BetBat/Fuzzer.h"
#include "Engone/Util/RandomUtility.h"
#include "Engone/Util/Array.h"

enum GenType {
    GEN_ENUM,
    GEN_SWITCH,
    GEN_EXPR,
    
};

bool GenerateFuzz(FuzzerOptions& options, FuzzedText* out_text) {
    using namespace engone;
    Assert(out_text);
    
    if(!options.allocator)
        options.allocator = GlobalHeapAllocator();
    
    
    u64 text_max = options.requested_size;
    char* textbuffer = (char*)options.allocator->allocate(text_max);
    u64 written = 0;
    out_text->buffer = textbuffer;
    out_text->buffer_max = text_max;
    
    #define WRITES(P,L) if (written+L<=text_max) {memcpy(textbuffer+written,P,L); written += L;}
    #define WRITE(C) if (written+1<=text_max) textbuffer[written++] = C;
    
    enum EnvType {
        ENV_BODY,
        ENV_EXPR,
        ENV_STRUCT,
        ENV_ENUM,
    };
    
    DynamicArray<EnvType> env_stack{};
    env_stack.add(ENV_BODY);
    
    while (written+50 < text_max) {
        
        double factor = GetRandom();
        
        switch(env_stack.last()){
        case ENV_BODY: {
            env_stack.add(ENV_EXPR);
            break;
        }
        case ENV_EXPR: {
            u32 random = Random32();
            int type = random & 0x8000'0000;
            if(type == 0) {
                int len = random & 15;
                for(int i=0;i<len;i++) {
                    char chr = 'A' + Random32() % 26;
                    if (((random >> (5+i)) & 0x1) == 0) {
                        chr |= 32;
                    }
                    WRITE(chr)
                }
                WRITE(' ')
                // if (random & 0x4000'0000) {
                // }
                if ((random & 0x0730'0000) == 0) {
                    WRITE('\n')
                }
            } else {
                char random = 32 + Random32()%(127-32);
                WRITE(random)
                if(random == ';') {
                    WRITE('\n')
                }
            }
            break;
        }
        }
    }
    out_text->written = written;
    return true;
}

bool GenerateFuzzedFile(FuzzerOptions& options, const std::string& path) {
    FuzzedText text{};
    bool yes = GenerateFuzz(options, &text);
    Assert(yes);
    
    auto file = engone::FileOpen(path, engone::FILE_CLEAR_AND_WRITE);
    engone::FileWrite(file,text.buffer, text.written);
    engone::FileClose(file);
    
    options.allocator->allocate(0,text.buffer,text.buffer_max);
    
    return true;
}