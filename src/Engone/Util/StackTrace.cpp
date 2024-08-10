#include "Engone/Util/StackTrace.h"

// #include "Engone/Util/Array.h"
#include "Engone/Logger.h"
#include "Engone/PlatformLayer.h"

// Stores stack trace and other data
// Each thread should have a handler
struct AssertHandler {
    struct Trace {
        const char* name = nullptr;
        const char* file = nullptr;
        int line = 0;
        std::vector<std::function<void()>> funcs;
        std::function<void()> single_func;
    };
    // we use std::vector instead of DynamicArray because it has asserts in it, infinite loop would happen
    std::vector<Trace> traces;

    void print_trace();
};

struct MasterAssertHandler {
    engone::TLSIndex tls_index;
};

// assert handler is a pointer because we can't control when it is destructed if it is a global object.
// if allocator is destroyed first and then assert handler, then when handler frees an allocation from the global allocator it will crash because the allocator has corrupt memory (since it was destroyed first)
// a pointer let's us skip that annoyance
static MasterAssertHandler* g_masterAssertHandler = nullptr;

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

void InitAssertHandler() {
    #ifdef ENABLE_ASSERT_HANDLER
    if(!g_masterAssertHandler) {
        // The fi
        g_masterAssertHandler = new MasterAssertHandler();
        g_masterAssertHandler->tls_index = engone::Thread::CreateTLSIndex();
    }
    void* value = engone::Thread::GetTLSValue(g_masterAssertHandler->tls_index);
    // TODO: Handle error (value != 0)
    auto handler = new AssertHandler();
    bool yes = engone::Thread::SetTLSValue(g_masterAssertHandler->tls_index, handler);
    // TODO: Handle error (!yes)
    #endif
}

// we don't need to destroy it, we never initialize it again.
// This is a private function.
// void EnsureInitializedStackTrace() {
// #ifdef ENABLE_ASSERT_HANDLER
//     if(!g_assertHandler)
//         g_assertHandler = new AssertHandler();
// #endif
// }

#define DEF_HANDLER \
    auto handler = ((AssertHandler*)engone::Thread::GetTLSValue(g_masterAssertHandler->tls_index));\
    if(!handler) return;

void PushStackTrace(const char* name, const char* file, int line) {
#ifdef ENABLE_ASSERT_HANDLER
    DEF_HANDLER
    handler->traces.push_back({name, file, line});
#endif
}
void PopStackTrace() {
#ifdef ENABLE_ASSERT_HANDLER
    DEF_HANDLER
    handler->traces.pop_back();
#endif
}
void PushCallbackOnAssert(std::function<void()> func) {
#ifdef ENABLE_ASSERT_HANDLER
    DEF_HANDLER
    handler->traces.back().funcs.push_back(func);
#endif
}
void SetSingleCallbackOnAssert(std::function<void()> func) {
#ifdef ENABLE_ASSERT_HANDLER
    DEF_HANDLER
    handler->traces.back().single_func = func;
#endif
}
void PopLastCallback() {
#ifdef ENABLE_ASSERT_HANDLER
    DEF_HANDLER
    handler->traces.back().funcs.pop_back();
#endif
}
void FireAssertHandler() {
#ifdef ENABLE_ASSERT_HANDLER
    using namespace engone;
    DEF_HANDLER

    bool has_any_callback = false;
    for (auto& trace : handler->traces) {
        if(trace.funcs.size() != 0) {
            has_any_callback = true;
            break;
        }
    }
    if(handler->traces.size() > 0 && handler->traces.back().single_func) {
        has_any_callback = true;
    }

    if (has_any_callback) {
        log::out << log::RED << "CALLBACKS FROM ASSERT:\n";

        for (auto& trace : handler->traces) {
            for(auto& f : trace.funcs)
                f();
        }
        // only fire the most recent single_func
        for(int i=handler->traces.size()-1;i>=0;i--) {
            auto& trace = handler->traces[i];
            if(trace.single_func) {
                trace.single_func();
                break;
            }
        }
    }

    handler->print_trace();
#endif
}