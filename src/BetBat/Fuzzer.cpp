
#include "BetBat/Fuzzer.h"
#include "Engone/Util/RandomUtility.h"
#include "Engone/Util/Array.h"
#include "BetBat/Util/Utility.h"

float node_chance[]{
    0.2,
    0.03,
    0.06,
    0.06,
    0.2,
};
int node_chance_len = sizeof(node_chance)/sizeof(*node_chance);

FuzzerNode* GenRandomNode() {
    float chance = engone::GetRandom();
    FuzzerNode* node = new FuzzerNode();
    
    float total = 0;
    for(int i=0;i<node_chance_len;i++)
        total += node_chance[i];
    
    chance *= total;
    float head = 0;
    for(int i=0;i<node_chance_len;i++) {
        if(head + node_chance[i] < chance)
            node->type = (FuzzerNode::Type)(i);
        head += node_chance[i];
    }
    
    return node;
}

const char* table_node_names[] {
    "expr",
    "body",
    "if",
    "while",
    "assign",
};

void PrintTree(FuzzerContext* context) {
    using namespace engone;
    auto indent = [&](int n) {
        for(int k=0;k<n;k++) log::out << " ";
    };
    for(int i=0;i<context->imports.size();i++) {
        auto& import = context->imports[i];
        log::out << log::GOLD << "FILE "<<import->filename<<"\n";
        
        struct Thing {
            FuzzerNode* node;
            int depth = 0;
        };
        DynamicArray<Thing> aFuzzinStack;
        aFuzzinStack.add({import->root});
        while(aFuzzinStack.size() > 0) {
            FuzzerNode* node = aFuzzinStack.last().node;
            int depth = aFuzzinStack.last().depth;
            aFuzzinStack.pop();
            
            indent(depth);
            log::out << table_node_names[node->type] << "\n";
            
            if(node->type == FuzzerNode::IF) {
                aFuzzinStack.add({node->nodes[0], depth + 1});
                aFuzzinStack.add({node->nodes[1], depth + 1});
                
                if(node->nodes.size()>2) {
                    indent(depth);
                    log::out << "else\n";
                    aFuzzinStack.add({node->nodes[2], depth + 1});
                }
            } else {
                for(int j=node->nodes.size()-1;j>=0;j--) {
                    aFuzzinStack.add({node->nodes[j], depth + 1});
                }
            }
        }
    }
}
bool GenerateTree(FuzzerContext* context) {
    
    for(int i=0;i<context->imports.size();i++) {
        auto& import = context->imports[i];
        import->root = new FuzzerNode(FuzzerNode::BODY);
        context->current_import_index = i;
        
        DynamicArray<FuzzerNode*> aFuzzinStack;
        aFuzzinStack.add(import->root);
        int iterations=0;
        while(aFuzzinStack.size() > 0 && iterations < context->options->node_iterations_per_file) {
            iterations++;
            FuzzerNode* current = aFuzzinStack.last();
            switch(current->type) {
            case FuzzerNode::BODY: {
                if(engone::GetRandom() > 0.65 && (aFuzzinStack.size()!=1))
                    aFuzzinStack.pop();
                
                FuzzerNode* node = GenRandomNode();
                current->nodes.add(node);
                 
                aFuzzinStack.add(node);
                break;
            }
            case FuzzerNode::IF: {
                aFuzzinStack.pop();
                current->nodes.add(new FuzzerNode(FuzzerNode::EXPR)); // condition
                current->nodes.add(new FuzzerNode(FuzzerNode::BODY)); // if body
                aFuzzinStack.add(current->nodes[1]);
                
                if(engone::GetRandom() > 0.6) {
                    current->nodes.add(new FuzzerNode(FuzzerNode::BODY)); // else body
                    aFuzzinStack.add(current->nodes[2]);
                }
                break;
            }
            case FuzzerNode::WHILE: {
                aFuzzinStack.pop();
                current->nodes.add(new FuzzerNode(FuzzerNode::EXPR)); // condition
                current->nodes.add(new FuzzerNode(FuzzerNode::BODY)); // body
                aFuzzinStack.add(current->nodes[1]);
                break;
            }
            // case FuzzerNode::FUNC: {
                
            // }
            // case FuzzerNode::STRUCT: {
                
            // }
            // case FuzzerNode::SWITCH: {
                
            // }
            case FuzzerNode::EXPR: {
                aFuzzinStack.pop(); // expression is handled in the second phase
                break;
            }
            case FuzzerNode::ASSIGN: {
                aFuzzinStack.pop(); // expression is handled in the second phase
                current->nodes.add(new FuzzerNode(FuzzerNode::EXPR));
                break;
            }
            }
        }
    }
    
    PrintTree(context);
    
    return true;
}
bool GenerateNode(FuzzerContext* context, FuzzerNode* node, int depth) {
    using namespace engone;
    auto indent = [&](int n) {
        for(int i=0;i<n;i++) context->stream.write_no_null("   ");
    };
    
    switch(node->type) {
    case FuzzerNode::BODY: {
        // curly braces is handled by the caller
        for(int i=0;i<node->nodes.size();i++) {
            FuzzerNode* next = node->nodes[i];
            if(next->type == FuzzerNode::EXPR)
                 indent(depth);
            GenerateNode(context, next, depth);
            if(next->type == FuzzerNode::EXPR || next->type == FuzzerNode::ASSIGN)
                context->stream.write1(';'); // TODO: Apply corruption
            if(next->type == FuzzerNode::EXPR)
                context->stream.write1('\n');
                
        }
        break;
    }
    case FuzzerNode::WHILE: {
        indent(depth);
        context->stream.write_no_null("while ");
        Assert(node->nodes[0]); //condition
        GenerateNode(context, node->nodes[0], 0);
        
        context->stream.write_no_null(" {\n");
        Assert(node->nodes[1]); // body
        GenerateNode(context, node->nodes[1], depth + 1);
        indent(depth);
        context->stream.write1('}');
        context->stream.write1('\n');
        break;  
    }
    case FuzzerNode::IF: {
        indent(depth);
        context->stream.write_no_null("if ");
        Assert(node->nodes[0]); //condition
        GenerateNode(context, node->nodes[0], 0);
        
        context->stream.write_no_null(" {\n");
        Assert(node->nodes[1]); // body
        GenerateNode(context, node->nodes[1], depth + 1);
        indent(depth);
        context->stream.write1('}');
        
        if(node->nodes.size()>2){ // else body
            context->stream.write_no_null(" else {\n");
        
            GenerateNode(context, node->nodes[2], depth + 1);
            indent(depth);
            context->stream.write_no_null("}\n");
        } else {
            context->stream.write1('\n'); // don't do newline with else if but need some logic to deal with that.
        }
        break;
    }
    case FuzzerNode::EXPR: {
        context->stream.write_no_null("EXPR");
        break;   
    }
    case FuzzerNode::ASSIGN: {
        indent(depth);
        context->stream.write_no_null("ASSIGN\n");
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
        
        GenerateNode(context, import->root, 0);
        
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
    // FuzzedText text{};
    // bool yes = GenerateFuzzedFiles(options, &text);
    // Assert(yes);
    
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
        context.imports.cleanup();
    };
    if(options.random_seed!=0)
        engone::SetRandomSeed(options.random_seed);
    else
        engone::SetRandomSeed(engone::StartMeasure());
    engone::log::out << "using seed "<< engone::GetRandomSeed() << "\n";
    GenerateTree(&context);
    GenerateSourceCode(&context);
    
    return true;
}
/*
    UNUSED CODE
*/
#ifdef gone
const char fuzzer_op_table[]{
    '+', '*','/','<','>','-','%','&','|','^'
};

bool FuzzExpression_v1(FuzzerContext& context) {
    using namespace engone;
    enum State {
        STATE_VALUE,
        STATE_OPERATION,
    };
    State state = STATE_VALUE;
    
    u32 bits;
    #define EXTRACT(BITS) (bits = random & ((1<<BITS)-1), (random >>= BITS), bits)
    #define EXTRACT_V(V,BITS) (bits = (V) & ((1<<BITS)-1), ((V) >>= BITS), bits)
    
    while(!context.output->filled) {
        u32 random = Random32();
        
        if(state == STATE_VALUE) {
            u32 value_type = EXTRACT(1);
            
            if (value_type == 0) {
                u32 len = 1 + EXTRACT(3);
                if (EXTRACT(1)) {
                    context.output->write('-');
                }
                for(int i=0;i<len;i++) {
                    char chr = '0' + Random32() % 10;
                    context.output->write(chr);
                }
                context.output->write(' ');
            } else if(value_type == 1) {
                u32 len = 1 + EXTRACT(3);
                for(int i=0;i<len;i++) {
                    u32 r = Random32();
                    char chr;
                    if(i==0) {
                        chr = (EXTRACT_V(r,1) & 1) ? 'a' + (EXTRACT_V(r,5)%26) : 'A' + (EXTRACT_V(r,5)%26);
                    } else {
                        chr = (EXTRACT_V(r,1) & 1) ? 'a' + (EXTRACT_V(r,5)%26) : (EXTRACT_V(r,1) & 1 ? 'A' + (EXTRACT_V(r,5) %26) : '0' + (EXTRACT_V(r,3) % 10));
                    }
                    context.output->write(chr);
                }
                context.output->write(' ');
            } else {
                // TODO: fncall
                // TODO: strings
            }
            
            if (EXTRACT(4) == 0)
                break;
            state = STATE_OPERATION;
        } else if(state == STATE_OPERATION) {
            char op = fuzzer_op_table[random%(sizeof(fuzzer_op_table)/sizeof(*fuzzer_op_table))];
            context.output->write(op);
            context.output->write(' ');
            state = STATE_VALUE;
        } else {
            Assert(false);   
        }
    }
    #undef EXTRACT
    #undef EXTRACT_V
    return true;
}
bool GenerateFuzz_v1(FuzzerOptions& options, FuzzedText* output) {
    using namespace engone;
    Assert(output);
    
    if(!options.allocator)
        options.allocator = GlobalHeapAllocator();
    
    FuzzerContext context{};
    context.options = &options;
    context.output = output;
    
    output->buffer_max = options.requested_size;
    output->buffer = (char*)options.allocator->allocate(output->buffer_max);
    
    context.env_stack.add(ENV_BODY);
    
    // GENERATE CORRECT CODE
    while (!output->filled && context.env_stack.size()!=0) {
        u32 random = Random32();
        
        switch(context.env_stack.last()){
        case ENV_BODY: {
            if ((random & 7) == 0) {
                context.env_stack.pop();
            } else {
                u32 random = Random32();
                enum GenType {
                    GEN_IF,
                    GEN_WHILE,
                    GEN_EXPR,
                    GEN_ASSIGNMENT,
                    // GEN_FOR,
                    GEN_MAX,
                };
                GenType type = (GenType)(random % GEN_MAX);
                if(type == GEN_IF) {
                    output->write("if",2);
                    output->write(' ');
                    FuzzExpression_v1(context);
                    output->write('{');
                    context.env_stack.add(ENV_BODY);
                    // output->write('}');
                } else if (type == GEN_WHILE) {
                    output->write("while",5);
                    output->write(' ');
                    FuzzExpression_v1(context);
                    output->write('{');
                    context.env_stack.add(ENV_BODY);
                } else if(type == GEN_ASSIGNMENT) {
                    // output->write("while",5);
                    // output->write(' ');
                    // FuzzExpression_v1(context);
                    // output->write('{');
                    // context.env_stack.add(ENV_BODY);
                    
                    // identifiers
                    
                } else if(type == GEN_EXPR) {
                    context.env_stack.add(ENV_EXPR);
                }
            }
            break;
        }
        case ENV_EXPR: {
            FuzzExpression_v1(context);
            output->write('\n');
            context.env_stack.pop();
            // u32 random = Random32();
            // int type = random & 0x8000'0000;
            // if(type == 0) {
            //     int len = random & 15;
            //     for(int i=0;i<len;i++) {
            //         char chr = 'A' + Random32() % 26;
            //         if (((random >> (5+i)) & 0x1) == 0) {
            //             chr |= 32;
            //         }
            //         WRITE(chr)
            //     }
            //     WRITE(' ')
            //     // if (random & 0x4000'0000) {
            //     // }
            //     if ((random & 0x0730'0000) == 0) {
            //         WRITE('\n')
            //     }
            // } else {
            //     char random = 32 + Random32()%(127-32);
            //     WRITE(random)
            //     if(random == ';') {
            //         WRITE('\n')
            //     }
            // }
            break;
        }
        }
    }
    
    // APPLY SOME CORRUPTION
    int corruption_count = options.corruption_frequency * output->written;
    for(int i=0;i<corruption_count;i++) {
        int index = Random32()%output->written;
        
        u8 chr = output->buffer[index] - 32;
        chr = (u8)(options.corruption_strength * roundf(2*GetRandom()-1) + chr) % (127-32);
        output->buffer[index] = chr + 32;
    }
    
    return true;
}


#endif