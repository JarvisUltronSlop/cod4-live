#include <universal/q_shared.h>
#include <Windows.h>
#include <script/scr_main.h>
#include <script/scr_stringlist.h>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

scrVarPub_t scrVarPub;
scrMemTreePub_t scrMemTreePub;
scrStringDebugGlob_t* scrStringDebugGlob;
static scrStringDebugGlob_t referenceDebug;
alignas(16) static char memory[128];

void MyAssertHandler(const char* file, int line, int, const char* format, ...)
{
    std::fprintf(stderr, "Engine assertion at %s:%d: ", file, line);
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    std::abort();
}
void Sys_Error(const char* message, ...) { throw std::runtime_error(message); }
void Scr_TerminalError(const char* message) { throw std::runtime_error(message); }
#define PROF_SCOPED(name) (void)0
#include "generated_script_references.inc"

static void Require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
template<class Action> static bool Rejects(Action action)
{
    try { action(); } catch (const std::runtime_error&) { return true; }
    return false;
}
static RefString* ResetString(uint32_t count)
{
    scrMemTreePub.mt_buffer = memory;
    auto* value = GetRefString(1);
    value->data = (17u << 24) | (1u << 16) | count;
    std::memset(&referenceDebug, 0, sizeof(referenceDebug));
    referenceDebug.refCount[1] = count;
    referenceDebug.totalRefCount = count;
    scrStringDebugGlob = &referenceDebug;
    return value;
}

int main()
{
    auto* value = ResetString(65534);
    SL_AddRefToString(1);
    Require(value->refCount == 65535 && value->user == 1 && value->byteLen == 17,
        "last valid string reference preserves metadata");
    const auto fullString = value->data;
    Require(Rejects([] { SL_AddRefToString(1); }), "saturated string reference rejected");
    Require(value->data == fullString && referenceDebug.refCount[1] == 65535 &&
        referenceDebug.totalRefCount == 65535, "string overflow leaves count and metadata intact");
    Require(Rejects([&] { SL_AddUserInternal(value, 2); }), "new user cannot overflow count");
    Require(value->data == fullString && referenceDebug.totalRefCount == 65535,
        "rejected new user does not alter ownership bits");
    SL_AddUserInternal(value, 1);
    Require(value->data == fullString && referenceDebug.totalRefCount == 65535,
        "existing user needs no extra reference at capacity");

    value = ResetString(65534);
    SL_AddUserInternal(value, 2);
    Require(value->refCount == 65535 && value->user == 3 && value->byteLen == 17 &&
        referenceDebug.totalRefCount == 65535, "new user and last reference update together");
    value = ResetString(1);
    SL_AddUserInternal(value, 0);
    SL_AddUserInternal(value, 0);
    Require(value->refCount == 3 && value->user == 1, "anonymous users retain each reference");

    // Competing increments may use the remaining100 slots exactly once each;
    // overflow must never carry into the packed ownership/length fields.
    value = ResetString(65435);
    scrStringDebugGlob = nullptr;
    std::atomic<unsigned> accepted{0}, rejected{0};
    std::vector<std::thread> threads;
    for (unsigned t = 0; t < 8; ++t)
        threads.emplace_back([&] {
            for (unsigned i = 0; i < 200; ++i)
                if (Rejects([] { SL_AddRefToString(1); })) ++rejected;
                else ++accepted;
        });
    for (auto& thread : threads) thread.join();
    Require(accepted == 100 && rejected == 1500 && value->refCount == 65535 &&
        value->user == 1 && value->byteLen == 17, "concurrent increments stop exactly at capacity");
    value = ResetString(1);
    scrStringDebugGlob = nullptr;
    threads.clear();
    for (unsigned t = 0; t < 8; ++t)
        threads.emplace_back([&] { SL_AddUserInternal(value, 2); });
    for (auto& thread : threads) thread.join();
    Require(value->refCount == 2 && value->user == 3 && value->byteLen == 17,
        "concurrent ownership registration adds one reference");

    // Exercise the actual vector header alias and debug accounting too.
    auto* vector = reinterpret_cast<RefVector*>(memory + 3 * MT_NODE_SIZE);
    vector->head = 65534;
    scrVarPub.totalVectorRefCount = 65534;
    std::memset(&referenceDebug, 0, sizeof(referenceDebug));
    referenceDebug.refCount[3] = 65534;
    scrStringDebugGlob = &referenceDebug;
    AddRefToVector(vector->vec);
    Require(vector->refCount == 65535 && scrVarPub.totalVectorRefCount == 65535 &&
        referenceDebug.refCount[3] == 65535, "last valid vector reference");
    Require(Rejects([&] { AddRefToVector(vector->vec); }), "saturated vector rejected");
    Require(vector->head == 65535 && scrVarPub.totalVectorRefCount == 65535 &&
        referenceDebug.refCount[3] == 65535, "vector overflow leaves accounting intact");
    vector->byteLen = 1;
    const auto constantVector = vector->head;
    AddRefToVector(vector->vec);
    Require(vector->head == constantVector && scrVarPub.totalVectorRefCount == 65535,
        "immutable vectors still bypass reference accounting");
    std::puts("Script packed reference production boundaries passed");
}
