#include "BetBat/Util/JSON.h"

#include "Engone/Logger.h"

JSONNode* JSON::parse(const char* text, int length) {
    using namespace engone;
    if(!text || length == 0)
        return nullptr;
        
    int limit = 0x7FFF'FFF0;
    if(length > limit) {
        log::out << log::RED << "JSON parsing is limited to " << limit << " characters because of 32-bit signed integers. I don't know why you would want to parse 2 GB worth of JSON though.\n";
        return nullptr;
    }
    
    #define REPORT(msg) log::out << log::RED << msg
    #undef ERR_HEAD
    #define ERR_HEAD "Error parsing JSON: "
    
    // A list of "environments" instead of a recursive parse function 
    struct Env {
        JSONNode* current = nullptr;
    };
    std::vector<Env> envs;
    
    // Parse root brace
    int head = 0;
    while(true) {
        char chr = text[head];
        head++;
        
        if(chr == '\n' || chr == ' ' || chr == '\t' || chr == '\r') {
            continue;   
        }
        if(chr == '{') {
            break;
        }
        REPORT(ERR_HEAD "Missing initial brace for root object.\n");
        return nullptr;
    }
    
    // Add root node
    auto root_node = createNode(JSONNode::MAP);
    envs.push_back({ root_node });
    
    while(envs.size() > 0) {
        bool pop_env = true;
        
        // Some bools controlling what we parse
        bool parse_quoted_key = false;
        bool parse_key = false;
        bool parse_value = envs.back().current->kind == JSONNode::LIST;
        
        int key_start = 0;
        int key_end = 0;
        std::string last_key{};
        
        while(true) {
            if(head >= length)
                break;
            
            char chr = text[head];
            char next_chr = 0;
            if(head + 1 < length)
                next_chr = text[head + 1];
            head++;
            
            if(parse_key || parse_quoted_key) {
                if((parse_key && chr == ':') || (parse_quoted_key && chr == '"')) {
                    if(parse_key) {
                        last_key = std::string(text + key_start, key_end - key_start);
                    } else  if(parse_quoted_key) {
                        last_key = std::string(text + key_start, head - 1 - key_start);
                        
                        // Quote was parsed but we must also parse colon
                        while(true) {
                            if(head >= length) {
                                REPORT(ERR_HEAD "A colon was missing after a key.\n");
                                return nullptr;
                            }
                            if(text[head++] == ':')
                                break;
                        }
                    }
                    parse_quoted_key = false;
                    parse_key = false;
                    parse_value = true;
                    continue;
                }
                if(parse_key && !(chr == '\n' || chr == ' ' || chr == '\t' || chr == '\r')) {
                    key_end = head;
                }
                // We don't handle quotes in key: { "name-\"Jay\"": 1 }
                Assert(chr != '\\' || next_chr != '"');
                continue;
            }
            
            if(chr == '\n' || chr == ' ' || chr == '\t' || chr == '\r') {
                continue;
            }
            
            // This code pushes/sets a value in a list or map
            #define SET_VALUE\
                if(last_key.size() == 0) {\
                    Assert(envs.back().current->kind == JSONNode::LIST);\
                    envs.back().current->list.push_back(ptr);\
                } else {\
                    Assert(envs.back().current->kind == JSONNode::MAP);\
                    envs.back().current->map[last_key] = ptr;\
                    last_key = "";\
                    parse_value = false;\
                }
            
            if(parse_value) {
                // TODO: Error on missing comma, bracket
                if(chr == ',')
                    continue;
                if(chr == ']') {
                    break;
                }
                
                if(chr == '{') {
                    auto ptr = createNode(JSONNode::MAP);
                    
                    SET_VALUE
                    
                    envs.push_back({ ptr });
                    pop_env = false;
                    break;
                } else if(chr == '[') {
                    auto ptr = createNode(JSONNode::LIST);
                    
                    SET_VALUE
                    
                    envs.push_back({ ptr });
                    pop_env = false;
                    break;
                } else if((chr >= '0' && chr <= '9') || chr == '-') {
                    int int_start = head-1;
                    int int_end = head-1;
                    auto kind = JSONNode::INTEGER;
                    while(true) {
                        if(head >= length) {
                            REPORT(ERR_HEAD "Sudden end of file.\n");
                            return nullptr;
                        }
                        char chr = text[head];
                        head++;
                        
                        if(chr == '.') {
                            kind = JSONNode::DECIMAL;
                            int_end = head+1;
                            continue;
                        }
                        if(chr >= '0' && chr <= '9') {
                            int_end = head+1;
                            continue;
                        }
                        
                        if(chr == '\n' || chr == ' ' || chr == '\t' || chr == '\r' || chr == ',' || chr == ']' || chr == '}') {
                            
                        } else {
                            REPORT(ERR_HEAD "Invalid integer at char index " << head << ".\n");
                            return nullptr;
                        }
                        
                        head--;
                        break;
                    }
                    auto ptr = createNode(kind);
                    if(kind == JSONNode::INTEGER) {
                        std::string tmp = std::string(text + int_start, int_end - int_start);
                        ptr->integer = atoll(tmp.c_str());
                    } else {
                        std::string tmp = std::string(text + int_start, int_end - int_start);
                        ptr->decimal = atof(tmp.c_str());
                    }
                    
                    SET_VALUE
                } else if(chr == '"') {
                    std::string buffer = "";
                    while(true) {
                        if(head >= length) {
                            REPORT(ERR_HEAD "Sudden end of file.\n");
                            return nullptr;
                        }
                        char chr = text[head];
                        char next_chr = 0;
                        if(head+1 < length)
                            next_chr = text[head+1];
                        head++;
                        if(chr == '"')
                            break;
                        
                        if(chr == '\\') {
                            if(next_chr == '\\') {
                                head++;
                                buffer += "\\";
                            } else if(next_chr == '\"') {
                                head++;
                                buffer += "\"";
                            } else if(next_chr == 'b') {
                                head++;
                                buffer += "\b";
                            } else if(next_chr == 'f') {
                                head++;
                                buffer += "\f";
                            } else if(next_chr == 'n') {
                                head++;
                                buffer += "\n";
                            } else if(next_chr == 'r') {
                                head++;
                                buffer += "\r";
                            } else if(next_chr == 't') {
                                head++;
                                buffer += "\t";
                            } else if(next_chr == 'u') {
                                head++;
                                char u0 = text[head];
                                char u1 = text[head+1];
                                char u2 = text[head+2];
                                char u3 = text[head+3];
                                head+=4;
                                
                                bool invalid_code_point = false;
                                
                                u32 point = 0;
                                #define DECODE_HEX(U, BITS)\
                                    if (U >= '0' && U <= '9')\
                                        point |= (U - '0') << BITS;\
                                    else if (U >= 'A' && U <= 'F')\
                                        point |= (10 + U - 'A') << BITS;\
                                    else if (U >= 'a' && U <= 'f')\
                                        point |= (10 + U - 'a') << BITS;\
                                    else {\
                                        invalid_code_point = true;\
                                    }
                                DECODE_HEX(u3,0);
                                DECODE_HEX(u2,4);
                                DECODE_HEX(u1,8);
                                DECODE_HEX(u0,12);
                                
                                if(invalid_code_point) {
                                    REPORT(ERR_HEAD "Escaped unicode had invalid hexidecimals at character index "<< (head-5) <<".");
                                    return nullptr;
                                }
                                
                                if(point < 0x20) {
                                    buffer += (char)point;
                                } else {
                                    buffer += (char)((point>>8) & 0xFF);
                                    buffer += (char)(point & 0xFF);
                                    
                                    REPORT(ERR_HEAD "Escaped unicode is not quite supported (only U+0000 to U+001F are allowed).");
                                    return nullptr;
                                }
                            } else {
                                buffer += chr;
                            }
                        } else {
                            buffer += chr;
                        }
                    }
                    auto ptr = createNode(JSONNode::STRING);
                    ptr->string = buffer;
                    
                    SET_VALUE
                } else {
                    if(chr == 't' || chr == 'f' || chr == 'n') {
                        head--;
                        int start = head;
                        while(head < length) {
                            char chr = text[head];
                            head++;
                            if(chr == '\n' || chr == ' ' || chr == '\t' || chr == '\r' || chr == ',' || chr == ']' || chr == '}') {
                                head--;
                                break;
                            }
                        }
                        std::string str = std::string(text + start, head - start);
                        
                        JSONNode* ptr = nullptr;
                        if(str == "true") {
                            ptr = createNode(JSONNode::BOOLEAN);
                            ptr->boolean = true;
                            
                            SET_VALUE
                            continue;
                        } else if(str == "false") {
                            ptr = createNode(JSONNode::BOOLEAN);
                            ptr->boolean = false;
                            
                            SET_VALUE
                            continue;
                        } else if(str == "null") {
                            ptr = nullptr;
                            
                            SET_VALUE
                            continue;
                        } else {
                            REPORT(ERR_HEAD "Bad token at start of value '" << str<<"'.\n");
                            return nullptr;
                        }
                    }
                    
                    REPORT(ERR_HEAD "Bad character at start of value '" << chr<<"'.\n");
                    return nullptr;
                }
                continue;   
            }
            
            // Parsing key or end of object/map
            
            // TODO: Error on missing comma, brace
             if(chr == ',')
                continue;
            if(chr == '}') {
                break; // end
            }
            
            if(chr == '"') {
                key_start = head;
                parse_quoted_key = true;
                continue;
            } else {
                key_start = head-1;
                key_end = head;
                parse_key = true;
                continue;
            }
        }
        if(pop_env) {
            envs.pop_back();
        }
    }
    #undef ERR_HEAD
    #undef REPORT
    return root_node;
}
void JSON::stringify(JSONNode* node, std::string& out, bool enable_indent, bool enable_quoted_keys, int current_indent) {
    auto Indent = [&](int indent) {
        for(int i=0;i<indent;i++)
            out += "  ";
    };
    if(!node) {
        out += "null";
        return;
    }
    switch(node->kind) {
        case JSONNode::MAP: {
            out += "{\n";
            for(auto& pair : node->map) {
                if(enable_indent)
                    Indent(current_indent+1);
                if(enable_quoted_keys)
                    out += "\"" + pair.first + "\": ";
                else
                    out += pair.first + ": ";
                stringify(pair.second, out, enable_indent, enable_quoted_keys, current_indent+1);
                out += ",\n";
            }
            if(enable_indent)
                Indent(current_indent);
            out += "}";
        } break;
        case JSONNode::LIST: {
            out += "[\n";
            for(auto& elem : node->list) {
                if(enable_indent)
                    Indent(current_indent+1);
                stringify(elem, out, enable_indent, enable_quoted_keys, current_indent+1);
                out += ",\n";
            }
            if(enable_indent)
                Indent(current_indent);
            out += "]";
        } break;
        case JSONNode::INTEGER: {
            out += std::to_string(node->integer);
        } break;
        case JSONNode::DECIMAL: {
            out += std::to_string(node->decimal);
        } break;
        case JSONNode::BOOLEAN: {
            if(node->boolean)
                out += "true";
            else
                out += "false";
        } break;
        case JSONNode::STRING: {
            out += "\"";
            // TODO: Optimize with node->string_may_have_escaped_characters.
            //   If string doesn't have escaped characterswe print the string directly which is fast.
            //   Otherwise we must loop through all characters.
            for(int i=0;i<node->string.size();i++) {
                char chr = node->string[i];
                if(chr == '\\')
                    out += "\\\\";
                else if(chr == '\"')
                    out += "\\\"";
                else if(chr == '\b')
                    out += "\\b";
                else if(chr == '\f')
                    out += "\\f";
                else if(chr == '\n')
                    out += "\\n";
                else if(chr == '\r')
                    out += "\\r";
                else if(chr == '\t')
                    out += "\\t";
                else if(chr < 32) {
                    out += "\\u00";
                    out += std::to_string((chr >> 4) & 1);
                    out += std::to_string((chr & 0xF) < 10 ? chr & 0xF : chr - 10 + 'A');
                } else
                    out += chr;
            }
            
            out += "\"";
        } break;
        default: Assert(false);
    }
}
std::string JSON::toString(JSONNode* node) {
    std::string out{};
    stringify(node, out, true, false);
    return out;
}
bool JSON::equal(JSONNode* a, JSONNode* b) {
    if(a == b) return true;
    if(!a || !b) return false;
    if(a->kind != b->kind) return false;
    
    switch(a->kind) {
        case JSONNode::INTEGER: {
            return a->integer == b->integer;
        } break;
        case JSONNode::DECIMAL: {
            return a->decimal == b->decimal;
            // const double margin = 0.00001;
            // return -margin < a->decimal - b->decimal && a->decimal - b->decimal < a->decimal - b->decimal;
        } break;
        case JSONNode::BOOLEAN: {
            return a->boolean == b->boolean;
        } break;
        case JSONNode::STRING: {
            return a->string == b->string;
        } break;
        case JSONNode::MAP: {
            if(a->map.size() != b->map.size()) return false;
            
            for(auto& a_pair : a->map) {
                auto b_pair = b->map.find(a_pair.first);
                if(b_pair == b->map.end())
                    return false;
                
                bool yes = equal(a_pair.second, b_pair->second);
                if(!yes)
                    return false;
            }
        } break;
        case JSONNode::LIST: {
            if(a->list.size() != b->list.size()) return false;
            
            for(int i=0;i<a->list.size(); i++) {
                bool yes = equal(a->list[i], b->list[i]);
                if(!yes) return false;
            }
        } break;
        default: Assert(false);
    }
    return true;
}
void JSON::print(JSONNode* node) {
    using namespace engone;
    
    std::string out{};
    stringify(node, out, true, false);
    log::out << out << "\n";
}
JSONNode* JSON::createNode(JSONNode::Kind kind) {
    auto ptr = (JSONNode*)allocator.allocate(sizeof(JSONNode), nullptr, 0);
    new (ptr) JSONNode(kind);
    return ptr;
}
std::string JSONNode::toString() {
    JSON tmp{};
    return tmp.toString(this);
}

void TestJSON() {
    using namespace engone;
    JSON json{};
    
    int total_tests = 0;
    int failed_tests = 0;
    
    auto test=[&](const char* str) {
        total_tests++;
        auto n = json.parse(str, strlen(str));
        if(!n) {
            log::out << log::RED << "Failed "<< total_tests<<"\n";
            log::out << "Original: "<< str << "\n";
            failed_tests++;
            return;
        }
        
        std::string res{};
        json.stringify(n, res, false, false);
        auto n2 = json.parse(res.c_str(), res.length());
        
        if(json.equal(n, n2)) {
            json.print(n);
        } else {
            log::out << log::RED << "Failed "<< total_tests<<"\n";
            log::out << "Original: "<< str << "\n";
            log::out << "Result:\n";
            json.print(n);
            failed_tests++;
        }
    };
    
    // test("{a:1,b:2}");
    // test("{ \"name\": \"yes\", \"age\": 23.3 }");
    // test("{ a: [ 1, 2 ] }");
    // test("{ a: { b: 2, c: \"Hello!\"}, arr: [1, 3, 4, 6, \"WOOW\", {x: 1.2, y: 5}] }");
    // test("{ a :5}");
    // test("{ b: true, f: false, a: null }");
    // test("{ a: \"\\\\\", b: \"\\\"\", c: \"\\uXXXX\" }");
    // test("{ a: \"\\\\\", b: \"\\\"\", c: \"\\u0015\" }");
    
    const char* str = "{\"configuration\": {"
        "\"type\": \"object\","
        "\"title\": \"Example configuration\","
        "\"properties\": {"
            "\"languageServerExample.maxNumberOfProblems\": {"
                "\"scope\": \"resource\","
                "\"type\": \"number\","
                "\"default\": 100,"
                "\"description\": \"Controls the maximum number of problems produced by the server.\""
            "}}}}";
    auto node = json.parse(str,strlen(str));
    
    log::out << node->get("configuration")->get("type")->toString() << "\n";
    
    
    
    // json.print(nod);
    
    // {
    //     // const char* str = "{ a: [1,2] }";
    //     const char* str = "{ a: { b: 2, c: \"Hello!\"}, arr: [1, 3, 4, 6, \"WOOW\", {x: 1.2, y: 5}] }";
    //     auto node = json.parse(str, strlen(str));
    //     json.print(node);
    // }
    
    if(failed_tests == 0) {
        log::out << log::GREEN << "Success ("<< (total_tests-failed_tests) << "/"<<total_tests << ")\n";
    } else {
        log::out << log::RED << "Failure ("<< (total_tests-failed_tests) << "/"<<total_tests << ")\n";
    }
    
    #undef TEST
}