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
    
    if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        
        _VLOG(log::out <<log::BLUE<< "Preprocess (imports): "<<BriefPath(path.text)<<"\n";)
        PreprocessImports(&info, tokenStream);
    }
    
    Path dir = path.getDirectory();
    for(auto& item : tokenStream->importList){
        std::string importName = "";
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
        if(importName.find("./")==0) {
            fullPath = dir.text + importName.substr(2);
        } 
        //-- Search cwd or absolute path
        else if(FileExist(importName)){
            fullPath = importName;
            if(!fullPath.isAbsolute())
                fullPath = fullPath.getAbsolute();
        }
        //-- Search additional import directories.
        // TODO: DO THIS WITH #INCLUDE TOO!
        else {
            for(int i=0;i<(int)info.importDirectories.size();i++){
                const Path& dir = info.importDirectories[i];
                Assert(dir.isDir() && dir.isAbsolute());
                Path path = dir.text + importName;
                bool yes = FileExist(path.text);
                if(yes) {
                    fullPath = path;
                    break;
                }
            }
        }
        
        if(fullPath.text.empty()){
            log::out << log::RED << "Could not find import '"<<importName<<"' (import from '"<<BriefPath(path.text,20)<<"'\n";
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
            info.ast->mainBody->add(body, info.ast);
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
    compileInfo.nativeRegistry.initNativeContent();
    compileInfo.ast = AST::Create();
    defer { AST::Destroy(compileInfo.ast); };
    // compileInfo.compilerDir = TrimLastFile(compilerPath);
    compileInfo.importDirectories.push_back(options.compilerDirectory.text + "modules/");
    for(auto& path : options.importDirectories){
        compileInfo.importDirectories.push_back(path);
    }

    const char* essentialStructs = 
    "struct Slice<T> {"
    // "struct @hide Slice<T> {"
        "ptr: T*;"
        "len: u64;"
    "}\n"
    "struct Range {" 
    // "struct @hide Range {" 
        "beg: i32;"
        "end: i32;"
    "}\n"
    ;
    ParseFile(compileInfo, "<base-structs>","",(char*)essentialStructs, strlen(essentialStructs));

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
            log::out << "Compiled " << FormatUnit((u64)compileInfo.lines) << " lines ("<<FormatUnit((u64)compileInfo.blankLines)<<" blanks, "<<FormatBytes(compileInfo.readBytes)<<" in source files) in " << FormatTime(seconds) << "\n (" << FormatUnit(compileInfo.lines / seconds) << " lines/s)\n";
        }
    // )
    }
    AST::Destroy(compileInfo.ast);
    compileInfo.ast = nullptr;
    if(compileInfo.errors!=0){
        log::out << log::RED<<"Compiler failed with "<<compileInfo.errors<<" error(s) ("<<FormatTime(seconds)<<", "<<FormatUnit((u64)compileInfo.lines)<<" line(s), " << FormatUnit(compileInfo.lines / seconds) << " loc/s)\n";
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    }
    if(compileInfo.warnings!=0){
        log::out << log::YELLOW<<"Compiler had "<<compileInfo.warnings<<" warning(s)\n";
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
    if(err!=-1)
        err = FileWrite(file, bytecode->codeSegment.data, header.codeSize);
    if(err!=-1)
        err = FileWrite(file, bytecode->dataSegment.data, header.dataSize);

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