#include <Windows.h>
#include <array>
#include <string>
#include <vector>
#include <codecvt>

std::string MakeCommandLine(const std::string& app_path, const std::vector<std::string>& app_args)
{
    std::string command_line = app_path;
    for (const auto& arg : app_args)
    {
        command_line += " " + arg;
    }
    return command_line;
}

std::string GetDirectory(const std::string& path)
{
    auto loc = path.find_last_of("\\/");
    if (loc == std::string::npos)
    {
        std::array<wchar_t, MAX_PATH + 1> buffer = {};
        GetCurrentDirectoryW((DWORD)buffer.size(), buffer.data());
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(buffer.data());
    }
    return path.substr(0, loc);
}

bool AttachToProcessCLI(const std::string& app_path, uint32_t pid)
{
    std::string command_line = MakeCommandLine(app_path, { std::to_string(pid) });
    std::string work_directory = GetDirectory(app_path);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    std::wstring command_line_w = converter.from_bytes(command_line);
    std::wstring work_directory_w = converter.from_bytes(work_directory);

    STARTUPINFOW startup_info = {};
    PROCESS_INFORMATION process_info = {};

    BOOL is_run = CreateProcessW(
        nullptr,                                       // lpApplicationName,
        const_cast<wchar_t*>(command_line_w.c_str()),  // lpCommandLine,
        nullptr,                                       // lpProcessAttributes,
        nullptr,                                       // lpThreadAttributes,
        FALSE,                                         // bInheritHandles,
        0,                                             // dwCreationFlags,
        nullptr,                                       // lpEnvironment,
        work_directory_w.c_str(),                      // lpCurrentDirectory,
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

inline bool AttachToCurrentProcessCLI(const std::string& app_path)
{
    return AttachToProcessCLI(app_path, GetCurrentProcessId());
}
