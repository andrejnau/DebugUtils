#include <vector>
#include <string>
#include <functional>
#include "AttachToProcess.h"
#include "AttachToProcessCLI.h"

std::vector<std::string> GetArgs()
{
    std::vector<std::string> args;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        args.push_back(DebugUtils::Utf8Encode(argv[i]));
    }
    return args;
}

int main()
{
    std::vector<std::string> args = GetArgs();
    uint32_t pid = 0;
    std::string path;
    int line = -1;
    std::optional<std::string> title_filter;
    std::function<void()> run;
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == "--attach") {
            pid = std::stoul(args[++i]);
            run = [&] {
                DebugUtils::AttachToProcess(pid, title_filter);
            };
        } else if (args[i] == "--open-file") {
            path = args[++i];
            run = [&] {
                DebugUtils::OpenFile(path, line, title_filter);
            };
        } else if (args[i] == "--line") {
            line = std::stoi(args[++i]);
        } else if (args[i] == "--title-filter") {
            title_filter = args[++i];
        }
    }
    if (run) {
        run();
    }
    return 0;
}
