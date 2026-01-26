#define QFB_IMPLEMENTATION
#include "qfb.h"

int main() {
    Cmd cmd = {0};
    cmd_append(&cmd, "clang", "-Wall");
    cmd_append(&cmd, "-o", "qfb");
    cmd_append(&cmd, "qfb.c");

    if (execute_cmd(&cmd) != 0) {
        printf("[QFB] Failed to build...\n");
        return 1;
    }

    printf("[QFB] Done!\n");
    return 0;
}
