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

namespace DebugUtils {

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
        if (FAILED(process->get_ProcessID(&process_pid)))
            continue;
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
            return false;
        pid = it->second;
    }
    return true;
}

static bool AttachToProcess(uint32_t pid)
{
    if (FAILED(CoInitialize(nullptr)))
        return false;

    ComPtr<IMalloc> memory;
    if (FAILED(CoGetMalloc(1, &memory)))
        return false;

    ComPtr<IRunningObjectTable> running_object_table;
    if (FAILED(GetRunningObjectTable(0, &running_object_table)))
        return false;

    ComPtr<IEnumMoniker> moniker_enumerator;
    if (FAILED(running_object_table->EnumRunning(&moniker_enumerator)))
        return false;
    if (FAILED(moniker_enumerator->Reset()))
        return false;

    std::vector<ComPtr<IMoniker>> vs_monikers;
    std::vector<ComPtr<EnvDTE::_DTE>> vs_dtes;
    std::vector<ComPtr<EnvDTE::Debugger>> debuggers;
    std::wstring vs_prefix = L"!VisualStudio";
    ComPtr<IMoniker> moniker;
    ULONG fetched = 0;
    while (moniker_enumerator->Next(1, &moniker, &fetched) == S_OK)
    {
        ComPtr<IBindCtx> ctx;
        if (FAILED(CreateBindCtx(0, &ctx)))
            return false;

        LPOLESTR running_object_name_memory = nullptr;
        if (FAILED(moniker->GetDisplayName(ctx.Get(), nullptr, &running_object_name_memory)))
            continue;
        std::wstring running_object_name = running_object_name_memory;
        memory->Free(running_object_name_memory);
        if (running_object_name.compare(0, vs_prefix.size(), vs_prefix) != 0)
            continue;

        ComPtr<IUnknown> running_object;
        if (FAILED(running_object_table->GetObject(moniker.Get(), &running_object)))
            continue;
        ComPtr<EnvDTE::_DTE> dte;
        if (FAILED(running_object.As(&dte)))
            continue;
        ComPtr<EnvDTE::Debugger> debugger;
        if (FAILED(dte->get_Debugger(&debugger)))
            continue;

        vs_monikers.emplace_back(moniker);
        vs_dtes.emplace_back(dte);
        debuggers.emplace_back(debugger);
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
        if (IsUnderDebugger(debuggers[i], pid))
            return false;

        ComPtr<EnvDTE::Window> window;
        if (FAILED(vs_dtes[i]->get_MainWindow(&window)))
            continue;
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

    ComPtr<EnvDTE::Processes> processes;
    if (FAILED(main_debugger->get_LocalProcesses(&processes)))
        return false;
    long process_count = 0;
    if (FAILED(processes->get_Count(&process_count)))
        return false;
    for (long i = 1; i <= process_count; ++i)
    {
        ComPtr<EnvDTE::Process> process;
        if (FAILED(processes->Item(variant_t(i), &process)))
            continue;

        long process_pid = 0;
        if (FAILED(process->get_ProcessID(&process_pid)))
            continue;
        if (process_pid == pid)
            return SUCCEEDED(process->Attach());
    }
    return false;
}

inline bool AttachToCurrentProcess()
{
    return AttachToProcess(GetCurrentProcessId());
}

}  // namespace DebugUtils
