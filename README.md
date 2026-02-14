# Quick and Fast Builder
Build an entire C/C++ project using just C.

## Usage:
```c
#define QFB_IMPLEMENTATION
#include "qfb.h"

int main() {
    // Checksum-based self-rebuilding function.
    qfb_self_rebuild_checksum();
    // Alternatively a time-based self-rebuilding function can be used.
    // qfb_self_rebuild_time();

    Qfb_Cmd cmd = {0};
    qfb_cmd_append(&cmd, "clang", "-Wall", "main.c", "-o", "main");

    if (qfb_execute_cmd(&cmd) != 0) {
        printf("[QFB] Failed to build...\n");
        return 1;
    }

    printf("[QFB] Done!\n");
    return 0;
}
```
