#include "BetBat/UnitTesting.h"

struct Test {
    std::string name="";
    void* start=0;
    u32 length=0;
};
void SplitTests(std::string path, engone::Memory& baseMemory, std::vector<Test>& outSources){
    baseMemory = ReadFile(path.c_str());
    if(!baseMemory.data) return;
    outSources.clear();
    // TODO: Not handling comments or quotes. Expect wierd things if @ is
    //   found in one.
    const char* const sectionKey = "@test ";
    const int sectionKeyLen = strlen(sectionKey);
    Test nextTest = {};
    for(int i=0;i<(int)baseMemory.max;i++){
        char chr = *((char*)baseMemory.data + i);
        if(i+1 == (int)baseMemory.max || 
            (baseMemory.max - (i-1) >= sectionKeyLen && 
            strncmp((char*)baseMemory.data + i, sectionKey, sectionKeyLen) == 0))
        {
            if(nextTest.start) {
                nextTest.length = i - ((u64)nextTest.start - (u64)baseMemory.data);
                outSources.push_back(nextTest);
                nextTest = {};
            }
            i+=sectionKeyLen;
            while(i < (int)baseMemory.max){
                char chr = *((char*)baseMemory.data + i);
                if(chr == ' '||chr == '\t' || chr=='\r' || chr=='\n'){
                    break;
                }
                nextTest.name += chr;
                i++;
            }
        }
    }
}

void RunUnitTests(std::string testsString){
    /*
    struct A {
        a: i32
        b: i64
    }
    a: A;
    */
    std::string unitpath = "test/unittests/struct.btb";

    std::vector<std::string> testsToRun;
    int lastIndex=0;
    for(int i=0;i<(int)testsString.length();i++){
        if(testsString[i] == ' '){
            if(i-lastIndex!=0){
                testsToRun.push_back(testsString.substr(lastIndex, i-lastIndex));
            }
            lastIndex = i+1;
        }
    }

    engone::Memory baseMemory = {1};
    std::vector<Test> tests;
    SplitTests(unitpath,baseMemory,tests);
    defer { baseMemory.resize(0); };

    for(int i=0;i<(int)tests.size();i++){
        bool found = false;
        for(int j=0;j<(int)testsToRun.size();j++){
            if(tests[i].name == testsToRun[j]){
                found = true;
            }
        }
        if(!found) continue;
        engone::Memory mem{1};
        mem.data = (void*)tests[i].start;
        mem.max = tests[i].length;
        auto bc = CompileSource({mem});
        
        // Interpreter inter{};
        // inter.silent = true;
        // inter.execute(bc);
        // inter.cleanup();
        Bytecode::Destroy(bc);
    }
}