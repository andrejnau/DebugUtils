#pragma warning(push)
#pragma warning(disable : 4278)
#import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" raw_interfaces_only named_guids
#pragma warning(pop)
#include <string>
#include <vector>
#include <wrl.h>
using namespace Microsoft::WRL;

#define RETURN_ON_FAIL(expr, ...) \
        { \
            if (FAILED((expr))) { __debugbreak(); return false; } \
        }

bool IsUnderDebugger(ComPtr<EnvDTE::Debugger> debugger, long pid)
{
    ComPtr<EnvDTE::Processes> processes;
    RETURN_ON_FAIL(debugger->get_DebuggedProcesses(&processes));
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
            return true;
    }
    return false;
}

static bool AttachTo(long pid)
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

    ComPtr<EnvDTE::Debugger> main_debugger;
    for (auto& debugger : debuggers)
    {
        if (IsUnderDebugger(debugger, GetCurrentProcessId()))
        {
            main_debugger = debugger;
            break;
        }
    }
    if (!main_debugger)
        return main_debugger = debuggers.front();

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

inline bool AttachMe()
{
    return AttachTo(GetCurrentProcessId());
}
