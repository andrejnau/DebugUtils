#pragma once
#include <Windows.h>
#include <array>
#include <string>
#include <vector>
#include <optional>

namespace DebugUtils {

static std::string Utf8Encode(const std::wstring& wstr)
{
    if (wstr.empty())
        return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

static std::wstring Utf8Decode(const std::string& str)
{
    if (str.empty())
        return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

static std::string MakeCommandLine(const std::string& app_path, const std::vector<std::string>& app_args)
{
    std::string command_line = app_path;
    for (const auto& arg : app_args)
    {
        command_line += " " + arg;
    }
    return command_line;
}

static std::string GetDirectory(const std::string& path)
{
    auto loc = path.find_last_of("\\/");
    if (loc == std::string::npos)
    {
        std::array<wchar_t, MAX_PATH + 1> buffer = {};
        GetCurrentDirectoryW((DWORD)buffer.size(), buffer.data());
        return Utf8Encode(buffer.data());
    }
    return path.substr(0, loc);
}

static bool AttachToProcessCLI(const std::string& app_path, uint32_t pid, const std::optional<std::string>& title_filter)
{
    std::vector<std::string> args = { "--attach", std::to_string(pid) };
    if (title_filter) {
        args.push_back("--title-filter");
        args.push_back(*title_filter);
    }
    std::wstring command_line = Utf8Decode(MakeCommandLine(app_path, args));
    std::wstring work_directory = Utf8Decode(GetDirectory(app_path));

    STARTUPINFOW startup_info = {};
    PROCESS_INFORMATION process_info = {};

    BOOL is_run = CreateProcessW(
        nullptr,                                       // lpApplicationName,
        const_cast<wchar_t*>(command_line.c_str())  ,  // lpCommandLine,
        nullptr,                                       // lpProcessAttributes,
        nullptr,                                       // lpThreadAttributes,
        FALSE,                                         // bInheritHandles,
        0,                                             // dwCreationFlags,
        nullptr,                                       // lpEnvironment,
        work_directory.c_str(),                        // lpCurrentDirectory,
        &startup_info,                                 // lpStartupInfo,
        &process_info                                  // lpProcessInformation
    );

    if (!is_run)
    {
        return false;
    }
    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return exit_code == 0;
}

inline bool AttachToCurrentProcessCLI(const std::string& app_path, const std::optional<std::string>& title_filter = {})
{
    return AttachToProcessCLI(app_path, GetCurrentProcessId(), title_filter);
}

}  // namespace DebugUtils
