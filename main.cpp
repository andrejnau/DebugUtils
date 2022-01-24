#include <Windows.h>
#include "AttachToProcess.h"
#include "AttachToProcessCLI.h"

int main()
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc != 2)
    {
        return -1;
    }
    uint32_t pid = std::stoul(argv[1]);
    return AttachToProcess(pid) ? 0 : 1;
}
