# Quick and Fast Builder
Build an entire C/C++ project using just C.

## Usage:
```c
#define QFB_IMPLEMENTATION
#include "qfb.h"

int main() {
    Cmd cmd = {0};
    cmd_append(&cmd, "clang", "-Wall", "main.c", "-o", "main");

    if (execute_cmd(&cmd) != 0) {
        printf("[QFB] Failed to build...\n");
        return 1;
    }

    printf("[QFB] Done!\n");
    return 0;
}
```
