#include "AttachToProcess.h"
#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

int main()
{
    DebugUtils::AttachToCurrentProcess("DebugUtils");
    DebugUtils::OpenFile(__FILE__, __LINE__ + 1, "DebugUtils");
    return 0;
}
