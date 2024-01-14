
#include "BetBat/Fuzzer.h"
#include "Engone/Util/RandomUtility.h"
#include "Engone/Util/Array.h"
#include "BetBat/Util/Utility.h"

float node_chance[]{
    0.2,    //  EXPR,
    0.1,    //  DECL,
    0.1004,   //  BODY,
    0.105,   //  IF,
    0.103,   //  WHILE,
};
int node_chance_len = sizeof(node_chance)/sizeof(*node_chance);

const char* forbidden_names[]{
    "fn",
    "struct",
    "union",
    "while","for",
    "if",
    "sizeof",
    "nameof",
    "typeof",
    "switch",
    "case",
};

std::string GenRandomString() {
    while(true){
        std::string out = "";
        int len = 2 + engone::Random32() % 10;
        for(int i=0;i<len;i++)
            out += 'a' + engone::Random32() % 26;
        bool restart = false;
        for(int i=0;i<sizeof(forbidden_names)/sizeof(*forbidden_names);i++)
            if(out == forbidden_names[i]) {
                restart = true;
                break;
            }
        if (!restart) {
            return out;
        }
    }
    return "";
}

FuzzerNode* FuzzerContext::createRandomNode(int depth) {
    FuzzerNode* node = createNode(FuzzerNode::EXPR); // temporary type
    
    float total = 0;
    for(int i=0;i<node_chance_len;i++)
        total += node_chance[i];
    
    float chance = engone::GetRandom();
    chance *= total;
    float head = 0;
    // node->type = (FuzzerNode::Type)(FuzzerNode::TYPE_MAX-1);
    for(int i=0;i<node_chance_len;i++) {
        FuzzerNode::Type type = (FuzzerNode::Type)(i);
        if(depth < 6 || type == FuzzerNode::DECL || type == FuzzerNode::EXPR ) {
            if(head + node_chance[i] > chance) {
                node->type = type;
                break;
            }
        }
        head += node_chance[i];
    }
    
    return node;
}
FuzzerNode* FuzzerContext::createNode(FuzzerNode::Type type) {
    static int nextId = 1;
    if(!options->allocator)
        options->allocator = engone::GlobalHeapAllocator();
    FuzzerNode* node = (FuzzerNode*)options->allocator->allocate(sizeof(FuzzerNode));
    new(node)FuzzerNode(type);
    node->id = nextId++;
    allocatedNodes.add(node);
    return node;
}

const char* table_node_names[] {
    "expr",
    "decl",
    "body",
    "if",
    "while",
};

void PrintTree(FuzzerContext* context, FuzzerNode* node = nullptr, int depth = 0) {
    using namespace engone;
    auto indent = [&](int n) {
        for(int k=0;k<n;k++) log::out << " ";
    };
    if(node) {
        indent(depth);
        log::out << table_node_names[node->type] << "\n";
                
        if(node->type == FuzzerNode::IF) {
            PrintTree(context, node->nodes[0], depth + 1);
            PrintTree(context, node->nodes[1], depth + 1);
            if(node->nodes.size()>2) {
                indent(depth);
                log::out << "else\n";
                PrintTree(context, node->nodes[2], depth + 1);
            }
        } else {
            for(int j=0;j<node->nodes.size();j++) {
                PrintTree(context, node->nodes[j], depth + 1);
            }
        }
    } else {
        for(int i=0;i<context->imports.size();i++) {
            auto& import = context->imports[i];
            log::out << log::GOLD << "FILE "<<import->filename<<"\n";
            
            PrintTree(context, import->root);
        }
    }
}
bool GenerateTree(FuzzerContext* context) {
    
    for(int i=0;i<context->imports.size();i++) {
        auto& import = context->imports[i];
        import->root = context->createNode(FuzzerNode::BODY);
        context->current_import_index = i;
        
        struct Thing {
            FuzzerNode* node;
            int depth = 0;
        };

        DynamicArray<Thing> aFuzzinStack;
        aFuzzinStack.add({import->root});
        int iterations=0;
        while(aFuzzinStack.size() > 0 && iterations < context->options->node_iterations_per_file) {
            iterations++;
            Thing thing = aFuzzinStack.last();
            FuzzerNode* current = thing.node;
            int depth = thing.depth;
            Assert(current->type == FuzzerNode::BODY);
            if(engone::GetRandom() > 0.8 && (aFuzzinStack.size()!=1))
                aFuzzinStack.pop();

            FuzzerNode* node = context->createRandomNode(depth);
            current->nodes.add(node);

            switch(node->type) {
            case FuzzerNode::BODY: {
                aFuzzinStack.add({node,depth + 1});
                break;
            }
            case FuzzerNode::IF: {
                node->nodes.add(context->createNode(FuzzerNode::EXPR)); // condition
                node->nodes.add(context->createNode(FuzzerNode::BODY)); // if body
                aFuzzinStack.add({node->nodes[1],depth + 1});
                
                if(engone::GetRandom() > 0.6) {
                    node->nodes.add(context->createNode(FuzzerNode::BODY)); // else body
                    aFuzzinStack.add({node->nodes[2],depth + 1});
                }
                break;
            }
            case FuzzerNode::WHILE: {
                node->nodes.add(context->createNode(FuzzerNode::EXPR)); // condition
                node->nodes.add(context->createNode(FuzzerNode::BODY)); // body
                aFuzzinStack.add({node->nodes[1], depth + 1});
                break;
            }
            // case FuzzerNode::FUNC: {
                
            // }
            // case FuzzerNode::STRUCT: {
                
            // }
            // case FuzzerNode::SWITCH: {
                
            // }
            case FuzzerNode::EXPR: {
                break;
            }
            case FuzzerNode::DECL: {
                break;
            }
            }
        }
    }
    
    // PrintTree(context);
    
    return true;
}
bool GenerateNode(FuzzerContext* context, FuzzerNode* node, FuzzerNode* parent, int depth) {
    using namespace engone;
    auto indent = [&](int n) {
        for(int i=0;i<n;i++) context->stream.write_no_null("   ");
    };
    
    switch(node->type) {
    case FuzzerNode::BODY: {
        context->scopes.add({});
        context->scopes.last().used_ids = context->identifiers.size();
        // curly braces is handled by the caller
        for(int i=0;i<node->nodes.size();i++) {
            FuzzerNode* next = node->nodes[i];
            if(next->type == FuzzerNode::EXPR)
                 indent(depth);
            if(next->type == FuzzerNode::BODY) {
                indent(depth);
                context->stream.write_no_null("{\n");
                GenerateNode(context, next, node, depth+1);
                indent(depth);
                context->stream.write_no_null("}\n");
            }else {
                GenerateNode(context, next, node, depth);
            }
            if(next->type == FuzzerNode::EXPR || next->type == FuzzerNode::DECL) {
                context->stream.write1(';'); // TODO: Apply corruption
                context->stream.write1('\n');
            }
        }
        context->identifiers.resize(context->scopes.last().used_ids);
        context->scopes.pop();
        break;
    }
    case FuzzerNode::WHILE: {
        indent(depth);
        context->stream.write_no_null("while ");
        Assert(node->nodes[0]); //condition
        GenerateNode(context, node->nodes[0], node, 0);
        
        context->stream.write_no_null(" {\n");
        Assert(node->nodes[1]); // body
        GenerateNode(context, node->nodes[1], node, depth + 1);
        indent(depth);
        context->stream.write1('}');
        context->stream.write1('\n');
        break;
    }
    case FuzzerNode::IF: {
        indent(depth);
        context->stream.write_no_null("if ");
        Assert(node->nodes[0]); //condition
        GenerateNode(context, node->nodes[0], node, 0);
        
        context->stream.write_no_null(" {\n");
        Assert(node->nodes[1]); // body
        GenerateNode(context, node->nodes[1], node, depth + 1);
        indent(depth);
        context->stream.write1('}');
        
        if(node->nodes.size()>2){ // else body
            context->stream.write_no_null(" else {\n");
        
            GenerateNode(context, node->nodes[2], node, depth + 1);
            indent(depth);
            context->stream.write_no_null("}\n");
        } else {
            context->stream.write1('\n'); // don't do newline with else if but need some logic to deal with that.
        }
        break;
    }
    case FuzzerNode::EXPR: {
        // context->stream.write_no_null("EXPR");
        if(context->identifiers.size() > 0) {
            int index = engone::Random32() % context->identifiers.size();
            context->stream.write_no_null(context->identifiers[index].c_str());
        } else {
            context->stream.write_no_null("123");
        }
        
        break;   
    }
    case FuzzerNode::DECL: {
        indent(depth);
        // context->stream.write_no_null("DECL");
        while(true){
            std::string name = GenRandomString();
            bool restart = false;
            // TODO: Optimize with map? How to we deal with scopes in the map or so?
            for(int i=0;i<context->identifiers.size();i++){
                if(context->identifiers[i] == name) {
                    restart = true;
                    break;
                }
            }
            if(restart)
                continue;

            context->identifiers.add(name);
            context->stream.write(name.c_str(),name.length());
            context->stream.write_no_null(": i32");
            break;
        }
        break;   
    }
    }
    return true;
}
bool GenerateSourceCode(FuzzerContext* context) {
    context->stream.reserve(0x10000);
    for(int i=0;i<context->imports.size();i++) {
        auto& import = context->imports[i];
        context->current_import_index = i;
        
        context->stream.resetHead();
        
        if(i!=context->imports.size()-1) {
            context->stream.write_no_null("#import \"");
            context->stream.write_no_null(context->imports[i+1]->filename.c_str());
            context->stream.write_no_null("\"\n");
        }
        GenerateNode(context, import->root, nullptr, 0);
        
        auto file = engone::FileOpen("tests/fuzzer_gen/"+import->filename, engone::FILE_CLEAR_AND_WRITE);
        ByteStream::Iterator iter{};
        while(context->stream.iterate(iter)) {
            engone::FileWrite(file,iter.ptr, iter.size);
        }
        engone::FileClose(file);
    }
    
    return true;
}

bool GenerateFuzzedFiles(FuzzerOptions& options, const std::string& entry_file) {
    using namespace engone;
    
    FuzzerContext context{};
    context.options = &options;
    
    for(int i=0;i<options.file_count;i++) {
        context.imports.add(new FuzzerImport());
        if(i == 0)
            context.imports.last()->filename = "main.btb";
        else
            context.imports.last()->filename = "f"+std::to_string(i)+".btb";
    }
    defer {
        for(int i=0;i<options.file_count;i++) {
            delete context.imports.get(i);
        }
        for(int i=0;i<context.allocatedNodes.size();i++) {
            context.allocatedNodes[i]->~FuzzerNode();
            context.options->allocator->allocate(0, context.allocatedNodes[i], sizeof(FuzzerNode));
        }
    };
    // options.random_seed = 3479576075;
    if(options.random_seed!=0)
        engone::SetRandomSeed(options.random_seed);
    else
        engone::SetRandomSeed(engone::StartMeasure());
    log::out << log::GOLD<<"Fuzzer settings\n";
    log::out << " file count: " << options.file_count<<"\n";
    log::out << " iterations per file: " << options.node_iterations_per_file<<"\n";
    log::out << " random seed: " << engone::GetRandomSeed()<<"\n";
    log::out << log::GRAY<<"Generating...\r";
    log::out.flush();

    GenerateTree(&context);
    GenerateSourceCode(&context);
    
    return true;
}