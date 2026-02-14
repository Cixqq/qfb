#define QFB_IMPLEMENTATION
#include "qfb.h"

int main(int argc, char** argv) {
    // This function automatically detects if theres
    // any changes to this file and auto recompiles
    // itself accordingly using a checksum-based method.
    // qfb_self_rebuild_checksum(argv);

    // This function also detects changes to this file
    // and auto recompile automatically but it's time-based.
    // qfb_self_rebuild_time(argv);

    Qfb_Cmd cmd = {0};
    qfb_cmd_append(&cmd, "clang", "-Wall", "-ggdb");
    qfb_cmd_append(&cmd, "-o", "qfb");
    qfb_cmd_append(&cmd, "qfb.c");

    if (qfb_execute_cmd(&cmd) != 0) {
        printf("[QFB] Failed to build...\n");
        return 1;
    }

    printf("[QFB] Done!\n");
    return 0;
}
