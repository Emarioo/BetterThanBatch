#include "BetBat/Compiler.h"

Path::Path(const char* path) : text(path), _type((Type)0) {
    ReplaceChar((char*)text.data(), text.length(), '\\', '/');
#ifdef OS_WINDOWS
    size_t colon = text.find(":/");
    size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    
    if(colon != std::string::npos) {
        _type |= (u32)ABSOLUTE;
    } else {
        // trim slashes at beginning of path
        for(int i=0;i<(int)text.length();i++){
            if(text[i] != '/') {
                text = text.substr(i,text.length()-i);
                break;
            }
        }
    }
#elif defined(OS_LINUX)
     size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    bool absolute = text[0] == '/' || text[0] == '~';
    if(absolute) {
        _type |= (u32)ABSOLUTE;
    }
#endif
}
Path::Path(const std::string& path) : text(path), _type((Type)0) {
    ReplaceChar((char*)text.data(), text.length(), '\\', '/');
#ifdef OS_WINDOWS
    size_t colon = text.find(":/");
    size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    
    if(colon != std::string::npos) {
        _type |= (u32)ABSOLUTE;
    } else {
        // trim slashes at beginning of path
        for(int i=0;i<(int)text.length();i++){
            if(text[i] != '/') {
                text = text.substr(i,text.length()-i);
                break;
            }
        }
    }
#elif defined(OS_LINUX)
     size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    bool absolute = text[0] == '/' || text[0] == '~';
    if(absolute) {
        _type |= (u32)ABSOLUTE;
    }
#endif
}
std::string Path::getFormat() const {
    int index = text.find(".");
    if(index == std::string::npos || index + 1 == text.length()) {
        return "";
    }
    return text.substr(index+1);
}
Path Path::getFileName(bool withoutFormat) const {
    std::string name = TrimDir(text);
    int index = name.find(".");
    if(index == std::string::npos || index + 1 == text.length()) {
        return name;
    }
    return name.substr(0,index);
}
Path Path::getAbsolute() const {
    if(isAbsolute()) return *this;

    std::string cwd = engone::GetWorkingDirectory();
    if(cwd.empty()) return {}; // error?
    if(cwd.back() == '/')
        return Path(cwd + text);
    else
        return Path(cwd + "/" + text);
}
Path Path::getDirectory() const {
    if(isDir()) return *this;

    size_t lastSlash = text.find_last_of("/");
    if(lastSlash == std::string::npos){
        if(isAbsolute()) return *this;
        return Path("/");
    }
    return text.substr(0,lastSlash+1);
}

const char* ToString(TargetPlatform target){
    #define CASE(X,N) case X: return N;
    switch(target){
        CASE(WINDOWS_x64,"win-x64")
        CASE(LINUX_x64,"linux-x64")
        CASE(BYTECODE,"bytecode")
    }
    return "unknown";
    #undef CASE
}
engone::Logger& operator<<(engone::Logger& logger,TargetPlatform target){
    return logger << ToString(target);
}
TargetPlatform ToTarget(const std::string& str){
    #define CASE(N,X) if (str==X) return N;
    CASE(WINDOWS_x64,"win-x64")
    CASE(LINUX_x64,"linux-x64")
    CASE(BYTECODE,"bytecode")
    return UNKNOWN_TARGET;
    #undef CASE
}
void CompileInfo::addLinkDirective(const std::string& name){
    for(int i=0;i<linkDirectives.size();i++){
        if(name == linkDirectives[i])
            return;
    }
    linkDirectives.add(name);
}
void CompileInfo::cleanup(){
    for(auto& pair : tokenStreams){
        TokenStream::Destroy(pair.second.stream);
    }
    tokenStreams.clear();
    for(auto& pair : includeStreams){
        TokenStream::Destroy(pair.second);
    }
    includeStreams.clear();
    for(auto& stream : streamsToClean){
        TokenStream::Destroy(stream);
    }
    streamsToClean.resize(0);
    if(nativeRegistry){
        NativeRegistry::Destroy(nativeRegistry);
        nativeRegistry = nullptr;
    }
}
bool CompileInfo::addStream(TokenStream* stream){
    auto pair = tokenStreams.find(stream->streamName);
    // stream should never be added again.
    Assert(pair==tokenStreams.end());
    if(pair!=tokenStreams.end()) return false;
    auto ptr = &(tokenStreams[stream->streamName] = {});
    ptr->stream = stream;
    lines += stream->lines;
    blankLines += stream->blankLines;
    commentCount += stream->commentCount;
    readBytes += stream->readBytes;
    return true;
}
CompileInfo::FileInfo* CompileInfo::getStream(const Path& name){
    auto pair = tokenStreams.find(name.text);
    if(pair == tokenStreams.end())
        return nullptr;
    return &pair->second;
}
// does not handle backslash
bool ParseFile(CompileInfo& info, const Path& path, std::string as = "", const char* textData = nullptr, u64 textLength = 0){
    using namespace engone;

    if(!textData){ // path can be anything if textData is present.
        Assert(path.isAbsolute()); // A bug at the call site if not absolute
    }
    _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefPath(path.text)<<"\n";)
    TokenStream* tokenStream = nullptr;
    if(textData) {
        tokenStream = TokenStream::Tokenize(textData,textLength);
        tokenStream->streamName = path.text;
    } else {
        tokenStream = TokenStream::Tokenize(path.text);
    }
    if(!tokenStream){
        log::out << log::RED << "Failed tokenization: " << BriefPath(path.text) <<"\n";
        info.errors++;
        return false;
    }
    info.addStream(tokenStream);

    // tokenStream->printData();
    
    if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        
        _VLOG(log::out <<log::BLUE<< "Preprocess (imports): "<<BriefPath(path.text)<<"\n";)
        PreprocessImports(&info, tokenStream);
    }
    
    Path dir = path.getDirectory();
    for(auto& item : tokenStream->importList){
        Path importName = "";
        int dotindex = item.name.find_last_of(".");
        int slashindex = item.name.find_last_of("/");
        // log::out << "dot "<<dotindex << " slash "<<slashindex<<"\n";
        if(dotindex==-1 || dotindex<slashindex){
            importName = item.name+".btb";
        } else {
            importName = item.name;
        }
        // TODO: import AS
        
        Path fullPath = {};
        // TODO: Test "/src.btb", "ok./hum" and other unusual paths
        
        //-- Search directory of current source file
        if(importName.text.find("./")==0) {
            fullPath = dir.text + importName.text.substr(2);
        } 
        //-- Search cwd or absolute path
        else if(FileExist(importName.text)){
            fullPath = importName;
            if(!fullPath.isAbsolute())
                fullPath = fullPath.getAbsolute();
        }
        else if(fullPath = dir.text + importName.text, FileExist(fullPath.text)){
            // fullpath =  dir.text + importName.text;
        }
        //-- Search additional import directories.
        // TODO: DO THIS WITH #INCLUDE TOO!
        else {
            for(int i=0;i<(int)info.importDirectories.size();i++){
                const Path& dir = info.importDirectories[i];
                Assert(dir.isDir() && dir.isAbsolute());
                Path path = dir.text + importName.text;
                bool yes = FileExist(path.text);
                if(yes) {
                    fullPath = path;
                    break;
                }
            }
        }
        
        if(fullPath.text.empty()){
            log::out << log::RED << "Could not find import '"<<importName.text<<"' (import from '"<<BriefPath(path.text,20)<<"'\n";
        }else{
            auto fileInfo = info.getStream(fullPath);
            if(fileInfo){   
                log::out << log::LIME << "Already imported "<<BriefPath(fullPath.text,20)<<"\n";
            }else{
                bool yes = ParseFile(info, fullPath, item.as);
                // ASTScope* body = ParseFile(info, fullPath, item.as);
                // if(body){
                //     if(item.as.empty()){
                //         info.ast->appendToMainBody(body);
                //     } else {
                //         info.ast->mainBody->add(body, info.ast);
                //     }
                // }
            }
        }
    }

    bool macroBenchmark = tokenStream->length() && Equal(tokenStream->get(0),"@macro-benchmark");

    // second preprocess in case imports defined macros which
    // are used in this source file
    TokenStream* finalStream = tokenStream;
    if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(path.text)<<"\n";)
        finalStream = Preprocess(&info, tokenStream);
        if(macroBenchmark){
            log::out << log::LIME<<"Finished with " << finalStream->length()<<" token(s)\n";
            #ifndef LOG_MEASURES
            if(finalStream->length()<50){
                finalStream->print();
                log::out<<"\n";
            }
            #endif
        }
        info.streamsToClean.add(finalStream);
        // tokenStream->print();
    }
    if(macroBenchmark)
        return true;
    
    _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefPath(path.text)<<"\n";)
    ASTScope* body = ParseTokenStream(finalStream,info.ast,&info, as);
    if(body){
        if(as.empty()){
            info.ast->appendToMainBody(body);
        } else {
            info.ast->mainBody->add(info.ast, body);
        }
    }
    return true;
}

Bytecode* CompileSource(CompileOptions options) {
    using namespace engone;
    Bytecode* bytecode=0;
    double seconds = 0;
    
    // NOTE: Parser and generator uses tokens. Do not free tokens before compilation is complete.

    auto startCompileTime = engone::MeasureTime();
    
    CompileInfo compileInfo{};
    compileInfo.nativeRegistry = NativeRegistry::Create();
    compileInfo.nativeRegistry->initNativeContent();
    compileInfo.targetPlatform = options.target;
    
    compileInfo.ast = AST::Create();
    defer { AST::Destroy(compileInfo.ast); };
    // compileInfo.compilerDir = TrimLastFile(compilerPath);
    options.compilerDirectory = options.compilerDirectory.getAbsolute();
    compileInfo.importDirectories.push_back(options.compilerDirectory.text + "modules/");
    for(auto& path : options.importDirectories){
        compileInfo.importDirectories.push_back(path);
    }
    #ifndef DISABLE_BASE_IMPORT
    StringBuilder essentialStructs{};
    essentialStructs +=
    // "struct Slice<T> {"
    "struct @hide Slice<T> {"
        "ptr: T*;"
        "len: u64;"
    "}\n"
    // "struct Range {" 
    "struct @hide Range {" 
        "beg: i32;"
        "end: i32;"
    "}\n"
    #ifdef OS_WINDOWS
    "#define OS_WINDOWS\n"
    #elif defined(OS_LINUX)
    "#define OS_LINUX\n"
    #endif
    ;
    essentialStructs += (options.target == BYTECODE ? "#define LINK_BYTECODE\n" : "");
    ParseFile(compileInfo, "<base>","",essentialStructs.data(), strlen(essentialStructs));
    #endif

    if(options.rawSource.data){
        ParseFile(compileInfo, "<raw-data>","",(char*)options.rawSource.data, options.rawSource.used);
        // ASTScope* body = ParseFile(compileInfo, std::string("<raw-data>"),"",(char*)options.rawSource.data, options.rawSource.used);
        // if(body) {
        //     compileInfo.ast->appendToMainBody(body);
        // }
    } else {
        ParseFile(compileInfo, options.initialSourceFile.getAbsolute());
        // ASTScope* body = ParseFile(compileInfo, options.initialSourceFile.getAbsolute());
        // if(body) {
        //     compileInfo.ast->appendToMainBody(body);
        // }
    }
    
    _VLOG(log::out << log::BLUE<< "Final "; compileInfo.ast->print();)

    TypeCheck(compileInfo.ast, compileInfo.ast->mainBody, &compileInfo);
    
    // if(compileInfo.errors==0){
    // }
    // if (tokens.enabled & LAYER_GENERATOR){

    if(compileInfo.ast
        // && compileInfo.errors==0 /* commented out so we keep going and catch more errors */
        ){
        _VLOG(log::out <<log::BLUE<< "Generating code:\n";)
        bytecode = Generate(compileInfo.ast, &compileInfo);
    // }
    }

    // _VLOG(log::out << "\n";)
    seconds = engone::StopMeasure(startCompileTime);
    if(bytecode && compileInfo.errors==0){
        int bytes = bytecode->getMemoryUsage();
    // _VLOG(
        if(!options.silent){
            log::out << "Compiled " << FormatUnit((u64)compileInfo.lines) << " lines ("<<FormatUnit((u64)compileInfo.blankLines)<<" blanks, "<<FormatBytes(compileInfo.readBytes)<<" in source files) in " <<log::LIME<< FormatTime(seconds)<<log::SILVER << "\n (" << FormatUnit(compileInfo.lines / seconds) << " lines/s)\n";
        }
    // )
    }
    AST::Destroy(compileInfo.ast);
    compileInfo.ast = nullptr;
    if(compileInfo.errors!=0){
        log::out << log::RED<<"Compiler failed with "<<compileInfo.errors<<" error(s) ("<<FormatTime(seconds)<<", "<<FormatUnit((u64)compileInfo.lines)<<" line(s), " << FormatUnit(compileInfo.lines / seconds) << " loc/s)\n";
        if(compileInfo.nativeRegistry == bytecode->nativeRegistry) {
            bytecode->nativeRegistry = nullptr;
        }
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    }
    if(compileInfo.warnings!=0){
        log::out << log::YELLOW<<"Compiler had "<<compileInfo.warnings<<" warning(s)\n";
    }
    if(bytecode) {
        if(!bytecode->nativeRegistry)
            bytecode->nativeRegistry = compileInfo.nativeRegistry;
        compileInfo.nativeRegistry = nullptr;
        bytecode->linkDirectives.stealFrom(compileInfo.linkDirectives);
    }
    compileInfo.cleanup();
    return bytecode;
}

void CompileAndRun(CompileOptions options) {
    using namespace engone;
    Bytecode* bytecode = CompileSource(options);
    if(bytecode){
        RunBytecode(bytecode, options.userArgs);
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    }
    #ifdef LOG_MEASURES
    PrintMeasures();
    #endif
}
void RunBytecode(Bytecode* bytecode, const std::vector<std::string>& userArgs){
    Assert(bytecode);
        
    Interpreter interpreter{};
    interpreter.setCmdArgs(userArgs);
    // interpreter.silent = true;
    interpreter.execute(bytecode);
    interpreter.cleanup();
}
void CompileAndExport(CompileOptions options, Path outPath){
    using namespace engone;
    Bytecode* bytecode = nullptr;
    // doing switch statement directly to see if the target is implemented
    switch(options.target){
        case BYTECODE: {
            bytecode = CompileSource(options);
            bool yes = ExportBytecode(outPath, bytecode);
            Bytecode::Destroy(bytecode);
            if(yes)
                log::out << log::LIME<<"Exported "<<options.initialSourceFile.text << " into bytecode in "<<outPath.text<<"\n";
            else
                log::out <<log::RED <<"Failed exporting "<<options.initialSourceFile.text << " into bytecode in "<<outPath.text<<"\n";
        }
        break; case WINDOWS_x64: {
            // TODO: The user may not want the temporary object files to end up in bin

            bytecode = CompileSource(options);
            Program_x64* program = nullptr;
            
            if(bytecode)
                program = ConvertTox64(bytecode);

            if(program){
                auto format = outPath.getFormat();
                bool outputIsObject = format == "o" || format == "obj";

                if(outputIsObject){
                    WriteObjectFile(outPath.text,program);
                    log::out << log::LIME<<"Exported "<<options.initialSourceFile.text << " into an object file: "<<outPath.text<<".\n";
                } else {
                    std::string objPath = "bin/" + outPath.getFileName(true).text + ".obj";
                    WriteObjectFile(objPath,program);

                    std::string cmd = "link /nologo ";
                    cmd += objPath + " ";
                    // cmd += "bin/NativeLayer.lib ";
                    // cmd += "uuid.lib ";
                    for (int i = 0;i<bytecode->linkDirectives.size();i++) {
                        auto& dir = bytecode->linkDirectives[i];
                        cmd += dir + " ";
                    }
                    cmd += "/DEFAULTLIB:LIBCMT ";
                    cmd += "/OUT:" + outPath.text+" ";

                    int exitCode = 0;
                    engone::StartProgram("",(char*)cmd.c_str(),PROGRAM_WAIT, &exitCode);
                    // msvc linker returns a fatal error as exit code
                    if(exitCode == 0)
                        log::out << log::LIME<<"Exported "<<options.initialSourceFile.text << " into an executable: "<<outPath.text<<".\n";
                }
            } else {
                log::out <<log::RED <<"Failed converting "<<options.initialSourceFile.text << " to x64 program.\n";
            }
            if(bytecode)
                Bytecode::Destroy(bytecode);
            if(program)
                Program_x64::Destroy(program); 
        }
        break; default: {
            log::out << log::RED << "Target " << options.target << " is missing some implementations.\n";
        }
    }
}
struct BTBCHeader{
    u32 magicNumber = 0x13579BDF;
    char humanChars[4] = {'B','T','B','C'};
    // TODO: Version number for interpreter?
    // TODO: Version number for bytecode format?
    u32 codeSize = 0;
    u32 dataSize = 0;
};

bool ExportBytecode(Path filePath, const Bytecode* bytecode){
    using namespace engone;
    Assert(bytecode);
    auto file = FileOpen(filePath.text, 0, engone::FILE_WILL_CREATE);
    if(!file)
        return false;
    BTBCHeader header{};

    Assert(bytecode->codeSegment.used*bytecode->codeSegment.getTypeSize() < ((u64)1<<32));
    header.codeSize = bytecode->codeSegment.used*bytecode->codeSegment.getTypeSize();
    Assert(bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize() < ((u64)1<<32));
    header.dataSize = bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize();

    // TOOD: Check error when writing files
    int err = FileWrite(file, &header, sizeof(header));
    if(err!=-1 && bytecode->codeSegment.data){
        err = FileWrite(file, bytecode->codeSegment.data, header.codeSize);
    }
    if(err!=-1 && bytecode->dataSegment.data) {
        err = FileWrite(file, bytecode->dataSegment.data, header.dataSize);
    }
    // debugSegment is ignored

    FileClose(file);
    return true;
}
Bytecode* ImportBytecode(Path filePath){
    using namespace engone;
    u64 fileSize = 0;
    auto file = FileOpen(filePath.text, &fileSize, engone::FILE_ONLY_READ);

    if(fileSize < sizeof(BTBCHeader))
        return nullptr; // File is to small for a bytecode file

    BTBCHeader baseHeader{};
    BTBCHeader header{};

    // TOOD: Check error when writing files
    FileRead(file, &header, sizeof(header));
    
    if(strncmp((char*)&baseHeader,(char*)&header,8)) // TODO: Don't use hardcoded 8?
        return nullptr; // Magic number and human characters is incorrect. Meaning not a bytecode file.

    if(fileSize != sizeof(BTBCHeader) + header.codeSize + header.dataSize)
        return nullptr; // Corrupt file. Sizes does not match.
    
    Bytecode* bc = Bytecode::Create();
    bc->codeSegment.resize(header.codeSize/bc->codeSegment.getTypeSize());
    bc->codeSegment.used = header.codeSize/bc->codeSegment.getTypeSize();
    bc->dataSegment.resize(header.dataSize/bc->dataSegment.getTypeSize());
    bc->dataSegment.used = header.dataSize/bc->dataSegment.getTypeSize();
    FileRead(file, bc->codeSegment.data, header.codeSize);
    FileRead(file, bc->dataSegment.data, header.dataSize);

    // debugSegment is ignored

    FileClose(file);
    return bc;
}