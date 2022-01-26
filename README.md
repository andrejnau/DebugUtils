## Helper that allows attach Visual Studio to process via C++ code

```
#include "AttachToProcess.h"
DebugUtils::AttachToProcess(pid);
```

In some cases, you should use AttachToProcessCLI instead of AttachToProcess:
* To prevent hang when used from DllMain.
* If you do not use Visual Studio compiler.

```
#include "AttachToProcessCLI.h"
DebugUtils::AttachToProcessCLI("path/to/AttachToProcessCLI.exe", pid);
```

Known limitations:
* Doesn't work if Visual Studio and process have different permissions, e.g. one of them was run as administrator.
