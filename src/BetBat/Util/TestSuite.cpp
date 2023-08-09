#include "BetBat/Util/TestSuite.h"

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) { CONTENT }

#define ERR_HEAD_SUITE() log::out << log::RED << "TestSuite: "<<log::SILVER;
#define ERR_MSG_SUITE(MSG) log::out << log::SILVER << MSG << "\n";

void ParseTestCases(std::string path,  DynamicArray<TestOrigin>* outTestOrigins, DynamicArray<TestCase>* outTestCases){
    using namespace engone;
    Assert(outTestCases);
    Assert(outTestOrigins);
    u64 fileSize = 0;
    auto file = engone::FileOpen(path, &fileSize, FILE_ONLY_READ);
    if(!file) {
        ERR_SECTION(
            ERR_HEAD_SUITE()
            ERR_MSG_SUITE("Cannot open '"<<path<<"'.")
        )
        return;
    }
    char* buffer = (char*)engone::Allocate(fileSize);
    Assert(buffer);
    bool yes = engone::FileRead(file, buffer, fileSize);
    Assert(yes);
    engone::FileClose(file);

    TestOrigin origin{};
    origin.buffer = buffer;
    origin.size = fileSize;
    outTestOrigins->add(origin);

    // TODO: Not handling comments or quotes. Expect wierd things if @ is
    //   found in one.
    const char* const testKey = "TEST-CASE ";
    const int testKeyLen = strlen(testKey);
    // const char* const textKey = "text ";
    // const int textKeyLen = strlen(textKey);
    TestCase nextTest = {};
    #define FINALIZE_TEST(END_INDEX) if(nextTest.textBuffer.buffer) {\
        nextTest.textBuffer.size = (u64)buffer + END_INDEX - (u64)nextTest.textBuffer.buffer;\
        outTestCases->add(nextTest);\
        }
    int line = 1;
    int column = 1;
    int index = 0;
    while(index<(int)fileSize){
        char chr = buffer[index];
        index++;
        column++;
        if(chr == '\n') {
            line++;
            column = 1;
        }

        if(chr != '@')
            continue;

        if(fileSize - index >= testKeyLen && !strncmp(buffer + index, testKey, testKeyLen)) {
            FINALIZE_TEST(index-1)
            
            index += testKeyLen;
            column += testKeyLen;

            int startIndex = -1;
            int nameLen = 0;
            bool hadLineFeed = false;
            while (index<fileSize){
                char chr = buffer[index];
                index++;
                column++;
                if(chr == '\n') {
                    line++;
                    column = 1;
                }
                if(chr == '\n') {
                    hadLineFeed=true;
                    break;
                }
                if(chr == ' ' || chr == '\t' || chr == '\r') {
                    if (startIndex!=-1)
                        break;
                    continue;
                }
                if(startIndex==-1)
                    startIndex = index-1;
                nameLen++;
            }
            if(!hadLineFeed){
                while (index<fileSize){
                    char chr = buffer[index];
                    index++;
                    column++;
                    if(chr == '\n') {
                        line++;
                        column = 1;
                    }
                    if(chr == '\n')
                        break;
                }
            }
            if(startIndex==-1){
                ERR_SECTION(
                    ERR_HEAD_SUITE()
                    ERR_MSG_SUITE("Bad syntax.")
                    // TODO: Print file and line
                )
            } else {
                nextTest.testName = std::string(buffer + startIndex,nameLen);
                nextTest.textBuffer.origin = path;
                nextTest.textBuffer.buffer = buffer + index;
                nextTest.textBuffer.startLine = line;
                nextTest.textBuffer.startColumn = column;
            }
        } else {
            // TODO: text insertion
            ERR_SECTION(
                ERR_HEAD_SUITE()
                ERR_MSG_SUITE("Bad syntax.")
            )
        }
    }
    FINALIZE_TEST(index)
    #undef FINALIZE_TEST
}

void TestSuite(DynamicArray<std::string>& tests){
    using namespace engone;
    VerifyTests(tests);
}


void VerifyTests(DynamicArray<std::string>& filesToTest){
    using namespace engone;
    DynamicArray<TestCase> testCases{};
    DynamicArray<TestOrigin> testOrigins{};
    testOrigins.resize(filesToTest.size());

    for (auto& file : filesToTest) {
        ParseTestCases(file, &testOrigins, &testCases);
    }
    log::out << "Test cases ("<<testCases.size()<<"):\n";
    for(auto& testCase : testCases){
        log::out << " "<<testCase.testName << ": "<<testCase.textBuffer.size<<" bytes (origin: "<<testCase.textBuffer.origin<<")\n";
    }

    for(int i=0;i<testCases.size();i++) {
        auto& testcase = testCases[i];

        CompileOptions options{};
        options.silent=true;
        options.target = BYTECODE;
        // options.initialSourceFile = testcase.textBuffer.origin;
        options.initialSourceBuffer = testcase.textBuffer;
        // options.initialSourceBufferSize = testcase.size;
        Bytecode* bytecode = CompileSource(&options);
        // TODO: Compile source and run?
        // What happens in text?
        Bytecode::Destroy(bytecode);
    }

    for(auto& origin : testOrigins) {
        engone::Free(origin.buffer, origin.size);
    }
}