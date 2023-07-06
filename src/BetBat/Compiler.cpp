#include "BetBat/Compiler.h"

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

ASTScope* ParseFile(CompileInfo& info, const Path& path, std::string as = "");
ASTScope* ParseFile(CompileInfo& info, const engone::Memory& data, std::string as = ""){
    using namespace engone;
    // Assert(path.isAbsolute()); // A bug at the call site if not absolute
    Path path = Path("<engone::Memory>");
    _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefPath(path.text)<<"\n";)
    TokenStream* tokenStream = TokenStream::Tokenize((char*)data.data, data.max);
    if(!tokenStream){
        log::out << log::RED << " Failed tokenization: " << BriefPath(path.text) <<"\n";
        return nullptr;
    }
    info.addStream(tokenStream);
    
    if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        TokenStream* old = tokenStream;
        
        _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(path.text)<<"\n";)
        Preprocess(&info, tokenStream);
        if (tokenStream->enabled == LAYER_PREPROCESSOR)
            tokenStream->print();
        // TODO: quit if errors? unfortunately we don't have that information
        //  since errors are added to compileInfo's errors
    }
    
    // Path dir = path.getDirectory();
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
        // if(importName.find("./")==0) {
        //     fullPath = dir.text + importName.substr(2);
        // } 
        //-- Search cwd or absolute path
        // else
         if(FileExist(importName)){
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
                ASTScope* body = ParseFile(info, fullPath, item.as);
                if(body){
                    if(item.as.empty()){
                        info.ast->appendToMainBody(body);
                    } else {
                        info.ast->mainBody->add(body, info.ast);
                    }
                }
            }
        }
    }
    
    _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefPath(path.text)<<"\n";)
    ASTScope* body = ParseTokens(tokenStream,info.ast,&info, as);
    return body;
}
// does not handle backslash
ASTScope* ParseFile(CompileInfo& info, const Path& path, std::string as){
    using namespace engone;

    Assert(path.isAbsolute()); // A bug at the call site if not absolute
    
    _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefPath(path.text)<<"\n";)
    TokenStream* tokenStream = TokenStream::Tokenize(path.text);
    if(!tokenStream){
        log::out << log::RED << " Failed tokenization: " << BriefPath(path.text) <<"\n";
        return nullptr;
    }
    info.addStream(tokenStream);
    
    if (tokenStream->enabled & LAYER_PREPROCESSOR) {
        TokenStream* old = tokenStream;
        
        _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(path.text)<<"\n";)
        Preprocess(&info, tokenStream);
        if (tokenStream->enabled == LAYER_PREPROCESSOR)
            tokenStream->print();
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
                ASTScope* body = ParseFile(info, fullPath, item.as);
                if(body){
                    if(item.as.empty()){
                        info.ast->appendToMainBody(body);
                    } else {
                        info.ast->mainBody->add(body, info.ast);
                    }
                }
            }
        }
    }
    
    _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefPath(path.text)<<"\n";)
    ASTScope* body = ParseTokens(tokenStream,info.ast,&info, as);
    return body;
}

Bytecode* CompileSource(CompileOptions options) {
    using namespace engone;
    Bytecode* bytecode=0;
    double seconds = 0;
    
    // NOTE: Parser and generator uses tokens. Do not free tokens before compilation is complete.

    auto startCompileTime = engone::MeasureTime();
    
    CompileInfo compileInfo{};
    compileInfo.ast = AST::Create();
    defer { AST::Destroy(compileInfo.ast); };
    // compileInfo.compilerDir = TrimLastFile(compilerPath);
    compileInfo.importDirectories.push_back(options.compilerDirectory.text + "modules/");
    for(auto& path : options.importDirectories){
        compileInfo.importDirectories.push_back(path);
    }
    
    // ASTScope* body2 = ParseFile(compileInfo, "src/BetBat/StandardLibrary/Basic.btb");
    // compileInfo.ast->appendToMainBody(body2);
    if(options.rawSource.data){
        ASTScope* body = ParseFile(compileInfo, options.rawSource);
        if(body) {
            compileInfo.ast->appendToMainBody(body);
        }
    } else {
        ASTScope* body = ParseFile(compileInfo, options.initialSourceFile.getAbsolute());
        if(body) {
            compileInfo.ast->appendToMainBody(body);
        }
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
        log::out << log::RED<<"Compiler failed with "<<compileInfo.errors<<" error(s) (" << FormatUnit(compileInfo.lines / seconds) << " loc/s)\n";
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
        Interpreter interpreter{};
        // interpreter.silent = true;
        interpreter.execute(bytecode);
        interpreter.cleanup();
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    }
    #ifdef LOG_MEASURES
    PrintMeasures();
    #endif
}