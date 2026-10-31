// Prints where an Osteon crash happened from a Windows minidump: the exception, the faulting function, and the
// functions found on the crashing thread's stack (candidate return addresses), using the build's PDB.
//   dumpinfo.exe <dump.dmp> <Osteon.exe with its .pdb beside it>
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#pragma comment(lib, "dbghelp.lib")

static void* mapFile(const char* path, size_t& size) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return nullptr;
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz); size = (size_t)sz.QuadPart;
    HANDLE m = CreateFileMappingA(f, nullptr, PAGE_READONLY, 0, 0, nullptr);
    return MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
}

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: dumpinfo <dump.dmp> <Osteon.exe>\n"); return 1; }
    size_t size = 0;
    void* base = mapFile(argv[1], size);
    if (!base) { printf("cannot open dump\n"); return 1; }
    MINIDUMP_DIRECTORY* dir = nullptr; void* stream = nullptr; ULONG len = 0;

    // Module list: where Osteon.exe was loaded.
    uint64_t modBase = 0; uint32_t modSize = 0;
    if (MiniDumpReadDumpStream(base, ModuleListStream, &dir, &stream, &len)) {
        auto* ml = (MINIDUMP_MODULE_LIST*)stream;
        for (ULONG i = 0; i < ml->NumberOfModules; i++) {
            auto& m = ml->Modules[i];
            auto* name = (MINIDUMP_STRING*)((char*)base + m.ModuleNameRva);
            std::wstring w(name->Buffer, name->Length / 2);
            if (w.find(L"Osteon.exe") != std::wstring::npos) { modBase = m.BaseOfImage; modSize = m.SizeOfImage; }
        }
    }
    printf("module base 0x%llx size 0x%x\n", (unsigned long long)modBase, modSize);

    uint32_t threadId = 0; uint64_t excAddr = 0; uint32_t code = 0; uint64_t rsp = 0;
    std::vector<uint64_t> infoParams;
    if (MiniDumpReadDumpStream(base, ExceptionStream, &dir, &stream, &len)) {
        auto* e = (MINIDUMP_EXCEPTION_STREAM*)stream;
        threadId = e->ThreadId; excAddr = e->ExceptionRecord.ExceptionAddress; code = e->ExceptionRecord.ExceptionCode;
        for (DWORD i = 0; i < e->ExceptionRecord.NumberParameters && i < 4; i++) infoParams.push_back(e->ExceptionRecord.ExceptionInformation[i]);
        auto* ctx = (CONTEXT*)((char*)base + e->ThreadContext.Rva);
        rsp = ctx->Rsp;
    }
    printf("exception 0x%08x at 0x%llx (RVA 0x%llx)\n", code, (unsigned long long)excAddr, (unsigned long long)(excAddr - modBase));
    if (!infoParams.empty()) printf("  access %s of address 0x%llx\n", infoParams[0] == 0 ? "read" : infoParams[0] == 1 ? "write" : "execute", (unsigned long long)(infoParams.size() > 1 ? infoParams[1] : 0));

    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, nullptr, FALSE);
    const uint64_t fake = 0x140000000ull;
    std::string exe = argv[2];
    DWORD64 loaded = SymLoadModuleEx(proc, nullptr, exe.c_str(), nullptr, fake, modSize, nullptr, 0);
    if (!loaded) printf("cannot load symbols (error %lu)\n", GetLastError());
    auto describe = [&](uint64_t rva) {
        char buf[sizeof(SYMBOL_INFO) + 512] = {};
        auto* s = (SYMBOL_INFO*)buf; s->SizeOfStruct = sizeof(SYMBOL_INFO); s->MaxNameLen = 500;
        DWORD64 disp = 0;
        std::string out = "(unknown)";
        if (SymFromAddr(proc, fake + rva, &disp, s)) {
            char tmp[700]; snprintf(tmp, sizeof tmp, "%s+0x%llx", s->Name, (unsigned long long)disp); out = tmp;
            IMAGEHLP_LINE64 ln = {sizeof(ln)}; DWORD d2 = 0;
            if (SymGetLineFromAddr64(proc, fake + rva, &d2, &ln)) { snprintf(tmp, sizeof tmp, "  [%s:%lu]", ln.FileName, ln.LineNumber); out += tmp; }
        }
        return out;
    };
    printf("fault: %s\n", describe(excAddr - modBase).c_str());

    // The crashing thread's stack memory: look for values that point into the module (return addresses).
    if (MiniDumpReadDumpStream(base, ThreadListStream, &dir, &stream, &len)) {
        auto* tl = (MINIDUMP_THREAD_LIST*)stream;
        for (ULONG i = 0; i < tl->NumberOfThreads; i++) {
            auto& t = tl->Threads[i];
            if (t.ThreadId != threadId) continue;
            uint64_t start = t.Stack.StartOfMemoryRange;
            const uint8_t* mem = (const uint8_t*)base + t.Stack.Memory.Rva;
            uint32_t bytes = t.Stack.Memory.DataSize;
            printf("stack (candidates near RSP 0x%llx):\n", (unsigned long long)rsp);
            int shown = 0;
            for (uint32_t off = 0; off + 8 <= bytes && shown < 40; off += 8) {
                uint64_t v = *(const uint64_t*)(mem + off);
                if (v >= modBase && v < modBase + modSize) {
                    printf("  %s\n", describe(v - modBase).c_str());
                    shown++;
                }
            }
        }
    }
    return 0;
}
