#pragma warning(push)
#pragma warning(disable : 4278)
#import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" raw_interfaces_only named_guids
#pragma warning(pop)
#include <string>
#include <vector>
#include <unordered_map>
#include <tlhelp32.h>
#include <atlbase.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

#define RETURN_ON_FAIL(expr, ...) { if (FAILED((expr))) { return false; } }

static bool IsUnderDebugger(ComPtr<EnvDTE::Debugger> debugger, long pid)
{
    ComPtr<EnvDTE::Processes> processes;
    if (FAILED(debugger->get_DebuggedProcesses(&processes)) || !processes)
        return false;
    long process_count = 0;
    if (FAILED(processes->get_Count(&process_count)))
        return false;
    for (long i = 1; i <= process_count; ++i)
    {
        ComPtr<EnvDTE::Process> process;
        if (FAILED(processes->Item(variant_t(i), &process)) || !process)
            continue;

        long process_pid = 0;
        process->get_ProcessID(&process_pid);
        if (process_pid == pid)
            return true;
    }
    return false;
}

bool IsInProcessTree(const std::unordered_map<uint32_t, uint32_t>& parent, uint32_t root, uint32_t pid)
{
    while (pid != root)
    {
        auto it = parent.find(pid);
        if (it == parent.end())
        {
            return false;
        }
        pid = it->second;
    }
    return true;
}

static bool AttachToProcess(uint32_t pid)
{
    RETURN_ON_FAIL(CoInitialize(nullptr));

    ComPtr<IMalloc> memory;
    RETURN_ON_FAIL(CoGetMalloc(1, &memory));

    ComPtr<IRunningObjectTable> running_object_table;
    RETURN_ON_FAIL(GetRunningObjectTable(0, &running_object_table));

    ComPtr<IEnumMoniker> moniker_enumerator;
    running_object_table->EnumRunning(&moniker_enumerator);
    moniker_enumerator->Reset();

    std::vector<ComPtr<IMoniker>> vs_monikers;
    std::vector<ComPtr<EnvDTE::_DTE>> vs_dtes;
    std::vector<ComPtr<EnvDTE::Debugger>> debuggers;
    std::wstring vs_prefix = L"!VisualStudio";
    ComPtr<IMoniker> moniker;
    ULONG fetched = 0;
    while (moniker_enumerator->Next(1, &moniker, &fetched) == 0)
    {
        ComPtr<IBindCtx> ctx;
        RETURN_ON_FAIL(CreateBindCtx(0, &ctx));

        LPOLESTR running_object_name_memory = nullptr;
        moniker->GetDisplayName(ctx.Get(), nullptr, &running_object_name_memory);
        std::wstring running_object_name = running_object_name_memory;
        memory->Free(running_object_name_memory);
        if (running_object_name.compare(0, vs_prefix.size(), vs_prefix) != 0)
            continue;

        ComPtr<IUnknown> running_object;
        running_object_table->GetObject(moniker.Get(), &running_object);
        ComPtr<EnvDTE::_DTE> dte;
        running_object.As(&dte);
        ComPtr<EnvDTE::Debugger> debugger;
        dte->get_Debugger(&debugger);
        if (debugger)
        {
            vs_monikers.emplace_back(moniker);
            vs_dtes.emplace_back(dte);
            debuggers.emplace_back(debugger);
        }
    }

    if (debuggers.empty())
        return false;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    std::unordered_map<uint32_t, uint32_t> parent;
    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(snapshot, &pe))
    {
        do
        {
            parent[pe.th32ProcessID] = pe.th32ParentProcessID;
        } while (Process32Next(snapshot, &pe));
    }
    CloseHandle(snapshot);

    ComPtr<EnvDTE::Debugger> main_debugger;
    for (size_t i = 0; i < debuggers.size(); ++i)
    {
        ComPtr<EnvDTE::Window> window;
        if (FAILED(vs_dtes[i]->get_MainWindow(&window)) || !window)
        {
            continue;
        }
        HWND handle = {};
        window->get_HWnd(reinterpret_cast<long*>(&handle));
        DWORD vs_process_id = 0;
        GetWindowThreadProcessId(handle, &vs_process_id);
        if (IsInProcessTree(parent, vs_process_id, pid))
        {
            main_debugger = debuggers[i];
            break;
        }
    }
    if (!main_debugger)
        main_debugger = debuggers.front();

    if (IsUnderDebugger(main_debugger, pid))
        return true;

    ComPtr<EnvDTE::Processes> processes;
    RETURN_ON_FAIL(main_debugger->get_LocalProcesses(&processes));
    long process_count = 0;
    RETURN_ON_FAIL(processes->get_Count(&process_count));
    for (long i = 1; i <= process_count; ++i)
    {
        ComPtr<EnvDTE::Process> process;
        if (FAILED(processes->Item(variant_t(i), &process)) || !process)
            continue;

        long process_pid = 0;
        process->get_ProcessID(&process_pid);
        if (process_pid == pid)
        {
            return SUCCEEDED(process->Attach());
        }
    }
    return false;
}

inline bool AttachToCurrentProcess()
{
    return AttachToProcess(GetCurrentProcessId());
}
