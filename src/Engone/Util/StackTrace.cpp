#include "Engone/Util/StackTrace.h"

// #include "Engone/Util/Array.h"
#include "Engone/Logger.h"
#include "Engone/PlatformLayer.h"

// TODO: NOTHING IS THREAD SAFE

// Stores stack trace and other data
struct AssertHandler {
    struct Trace {
        const char* name = nullptr;
        const char* file = nullptr;
        int line = 0;
        std::vector<std::function<void()>> funcs;
    };
    // we use std::vector instead of DynamicArray because it has asserts in it, infinite loop would happen
    std::vector<Trace> traces;

    void print_trace();
};

// assert handler is a pointer because we can't control when it is destructed if it is a global object.
// if allocator is destroyed first and then assert handler, then when handler frees an allocation from the global allocator it will crash because the allocator has corrupt memory (since it was destroyed first)
// a pointer let's us skip that annoyance
static AssertHandler* g_assertHandler = nullptr;

void AssertHandler::print_trace() {
    using namespace engone;
    if (traces.size() == 0) 
        log::out << log::GRAY << "no traces\n";
    else
        log::out << log::RED << "Stack Trace:\n";

    for(auto& trace : traces) {
        std::string name = trace.file;
        for(int i=0;i<name.size();i++)
            if (name[i] == '\\')
                *((char*)name.data() + i) = '/';
        
        // printing the absolute path spams the terminal
        // so we trim the beginning
        int at = name.find("/include/");
        if (at != -1) at += 9;
        if (at == -1) {
            at = name.find("/src/");
            if (at != -1) at += 5;
        }
        if (at != -1)
            name = name.substr(at);

        log::out << log::RED << " ";
        if (trace.line)
            log::out <<trace.name << " - " << name << ":" << trace.line<<"\n";
        else
            log::out << trace.name << " - " << name << ":" << trace.line<<"\n";
    }
}

// we don't need to destroy it, we never initialize it again
void EnsureInitializedStackTrace() {
    if(!g_assertHandler) {
        g_assertHandler = new AssertHandler();
    }
}

void PushStackTrace(const char* name, const char* file, int line) {
    EnsureInitializedStackTrace();

    g_assertHandler->traces.push_back({name, file, line});
}
void PopStackTrace() {
    EnsureInitializedStackTrace();

    g_assertHandler->traces.pop_back();
}
void SetCallbackOnAssert(std::function<void()> func) {
    EnsureInitializedStackTrace();

    g_assertHandler->traces.back().funcs.push_back(func);
}
void PopLastCallback() {
    EnsureInitializedStackTrace();

    g_assertHandler->traces.back().funcs.pop_back();
}
void FireAssertHandler() {
    using namespace engone;
    EnsureInitializedStackTrace();

    bool has_any_callback = false;
    for (auto& trace : g_assertHandler->traces) {
        if(trace.funcs.size() != 0) {
            has_any_callback = true;
            break;
        }
    }

    if (has_any_callback) {
        log::out << log::RED << "CALLBACKS FROM ASSERT:\n";

        for (auto& trace : g_assertHandler->traces) {
            for(auto& f : trace.funcs)
                f();
        }
    }

    g_assertHandler->print_trace();
}