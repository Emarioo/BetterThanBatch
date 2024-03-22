#pragma once

#include "Engone/Util/Allocator.h"

/*
    JSON parsing mainly for Language Server Protocol
    
    Parsing has been implemented in a way where it won't always fail if the JSON format is wrong.
    The parser continues parsing even if there are no commas. { a: 2a b : 2} is parsed as { a: 2, b: 2}.
    Parsing WILL ALWAYS produce correct JSON if the format is correct.
*/

struct JSONNode {
    enum Kind {
        INTEGER,
        DECIMAL,
        BOOLEAN,
        STRING,
        LIST,
        MAP,
    };
    Kind kind;
    
    JSONNode(Kind kind) : kind(kind) {
        switch(kind){
            case INTEGER: integer = 0; break;
            case DECIMAL: decimal = 0.0; break;
            case BOOLEAN: boolean = false; break;
            case STRING: new(&string) std::string(); break;
            case LIST: new(&list) std::vector<JSONNode*>(); break;
            case MAP: new(&map) std::unordered_map<std::string, JSONNode*>(); break;
            default: Assert(false);
        }
    }
    ~JSONNode() {
        switch(kind){
            case INTEGER: integer = 0; break;
            case DECIMAL: decimal = 0.0; break;
            case BOOLEAN: boolean = false; break;
            case STRING: string.~basic_string(); break;
            case LIST: list.~vector(); break;
            case MAP: map.~unordered_map(); break;
            default: Assert(false);
        }
    }
    
    // JSONNode* operator [](int index) {
    //     return get(index);
    // }
    // JSONNode* operator [](const std::string& key) {
    //     return get(key);
    // }
    JSONNode* get(int index) {
        if(!this || kind != LIST) return nullptr;
        return list[index];
    }
    JSONNode* get(const std::string& key) {
        if(!this || kind != MAP) return nullptr;
        return map[key];
    }
    i64 get_int() {
        if(!this || kind != INTEGER) return 0;
        return integer;
    }
    double get_dec() {
        if(!this || kind != DECIMAL) return 0;
        return decimal;
    }
    bool get_bool() {
        if(!this || kind != BOOLEAN) return 0;
        return boolean;
    }
    std::string get_str() {
        if(!this || kind != STRING) return "null";
        return string;
    }
    std::string toString();
    
private:
    union {
        i64 integer;
        double decimal;
        bool boolean;
        std::string string;
        std::vector<JSONNode*> list;
        std::unordered_map<std::string, JSONNode*> map;
    };
    
    friend class JSON;
};

// JSON object
struct JSON {
    
    JSONNode* parse(const char* text, int length);
    void stringify(JSONNode* node, std::string& out, bool enable_indent = false, bool enable_quoted_keys = true, int current_indent = 0);
    // recommended for small nodes
    std::string toString(JSONNode* node);
    
    // std::vector<JSONNode*> 
    
    void print(JSONNode* node);
    
    bool equal(JSONNode* a, JSONNode* b);
    
private:
    
    engone::HeapAllocator allocator{};
    
    JSONNode* createNode(JSONNode::Kind kind);
    
};

void TestJSON();