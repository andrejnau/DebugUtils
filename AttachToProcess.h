#pragma once
#pragma warning(push)
#pragma warning(disable : 4278)
#import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" raw_interfaces_only named_guids
#pragma warning(pop)
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <tlhelp32.h>
#include <atlbase.h>
#include <wrl.h>
#include "AttachToProcessCLI.h"
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

static bool IsInProcessTree(const std::unordered_map<uint32_t, uint32_t>& parent, uint32_t root, uint32_t pid)
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

static std::vector<ComPtr<EnvDTE::_DTE>> GetVsDte(const std::optional<std::string>& title_filter)
{
    if (FAILED(CoInitialize(nullptr)))
        return {};

    ComPtr<IMalloc> memory;
    if (FAILED(CoGetMalloc(1, &memory)))
        return {};

    ComPtr<IRunningObjectTable> running_object_table;
    if (FAILED(GetRunningObjectTable(0, &running_object_table)))
        return {};

    ComPtr<IEnumMoniker> moniker_enumerator;
    if (FAILED(running_object_table->EnumRunning(&moniker_enumerator)))
        return {};
    if (FAILED(moniker_enumerator->Reset()))
        return {};

    std::vector<ComPtr<EnvDTE::_DTE>> vs_dte;
    std::wstring vs_prefix = L"!VisualStudio";
    ComPtr<IMoniker> moniker;
    ULONG fetched = 0;
    while (moniker_enumerator->Next(1, &moniker, &fetched) == S_OK) {
        ComPtr<IBindCtx> ctx;
        if (FAILED(CreateBindCtx(0, &ctx)))
            return {};

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

        if (title_filter) {
            ComPtr<EnvDTE::Window> window;
            if (dte->get_MainWindow(&window)) {
                continue;
            }
            HWND handle = {};
            window->get_HWnd(reinterpret_cast<long*>(&handle));
            wchar_t buffer[MAX_PATH] = {};
            GetWindowTextW(handle, buffer, std::size(buffer));
            std::string title = DebugUtils::Utf8Encode(buffer);
            if (title.find(*title_filter) == std::string::npos) {
                continue;
            }
        }
        vs_dte.emplace_back(dte);
    }
    return vs_dte;
}

static bool AttachToProcess(uint32_t pid, const std::optional<std::string>& title_filter = {})
{
    std::vector<ComPtr<EnvDTE::_DTE>> vs_dte = GetVsDte(title_filter);
    std::vector<ComPtr<EnvDTE::Debugger>> debuggers;
    for (auto& dte : vs_dte) {
        ComPtr<EnvDTE::Debugger> debugger;
        if (FAILED(dte->get_Debugger(&debugger)))
            continue;
        debuggers.push_back(debugger);
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
        if (FAILED(vs_dte[i]->get_MainWindow(&window)))
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

static bool OpenFile(const std::string& path, int line = -1, const std::optional<std::string>& title_filter = {})
{
    std::vector<ComPtr<EnvDTE::_DTE>> vs_dte = GetVsDte(title_filter);
    if (vs_dte.empty()) {
        return false;
    }
    auto dte = vs_dte.front();
    
    CComBSTR view_kind(EnvDTE::vsViewKindCode);
    CComBSTR file_path(path.c_str());
    ComPtr<EnvDTE::Window> window;
    dte->OpenFile(view_kind, file_path, &window);

    ComPtr<EnvDTE::Document> doc;
    window->get_Document(&doc);
    doc->Activate();

    if (line != -1) {
        ComPtr<IDispatch> selection;
        doc->get_Selection(&selection);
        ComPtr<EnvDTE::TextSelection> text_selection;
        selection.As(&text_selection);
        if (text_selection) {
            text_selection->GotoLine(line, false);
        } else {
            return false;
        }
    }

    ComPtr<EnvDTE::Window> main_window;
    if (FAILED(dte->get_MainWindow(&main_window))) {
        return false;
    }
    HWND handle = {};
    main_window->get_HWnd(reinterpret_cast<long*>(&handle));
    SetForegroundWindow(handle);
    return true;
}

inline bool AttachToCurrentProcess(const std::optional<std::string>& title_filter = {})
{
    return AttachToProcess(GetCurrentProcessId(), title_filter);
}

}  // namespace DebugUtils
