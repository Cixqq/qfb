#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char** args;
    int size;
    int capacity;
    int count;
} Qfb_Cmd;

#ifndef _QFB_GUARD
    #define _QFB_GUARD
void _qfb_cmd_append(Qfb_Cmd* cmd, ...);
int qfb_execute_cmd(Qfb_Cmd* cmd);       // Thread-safety NOT GUARANTEED.
int qfb_execute_cmd_async(Qfb_Cmd* cmd); // Thread-safety NOT GUARANTEED.
void qfb_self_rebuild(void);
    #define qfb_cmd_append(cmd, ...) _qfb_cmd_append(cmd, __VA_ARGS__, NULL)
    #define qfb_self_rebuild_checksum(argv)                                                                            \
        do {                                                                                                           \
            /* Who knows if we ever needed argc in the future... */                                                    \
            _qfb_self_rebuild_checksum(argv[0], __FILE__);                                                             \
        } while (0)
    #define qfb_self_rebuild_time(argv)                                                                                \
        do {                                                                                                           \
            /* Who knows if we ever needed argc in the future... */                                                    \
            _qfb_self_rebuild_time(argv[0], __FILE__);                                                                 \
        } while (0)
#endif // _QFB_GUARD

#ifdef QFB_IMPLEMENTATION
void _qfb_cmd_append(Qfb_Cmd* cmd, ...) {
    va_list args;
    va_start(args, cmd);

    if (cmd->args == NULL || cmd->capacity <= 0 || cmd->size <= 0 || cmd->count <= 0) {
        cmd->capacity = sizeof(char*) * 8;
        cmd->args = (char**)malloc(cmd->capacity);
        cmd->size = 0;
        cmd->count = 0;
    }

    for (int i = cmd->count;; ++i) {
        char* arg = va_arg(args, char*);
        // TODO: Trim whitespaces.
        if (arg == NULL || *arg == ' ')
            break;
        if (cmd->size + strlen(arg) >= cmd->capacity) {
            cmd->capacity *= 2;
            cmd->args = (char**)realloc(cmd->args, cmd->capacity);
        }
        cmd->args[i] = arg;
        cmd->size += sizeof(arg);
        cmd->count++;
    }

    va_end(args);
}

pid_t qfb_execute_cmd_async(Qfb_Cmd* cmd) {
    char* cmd_joined = (char*)calloc(1, cmd->size);

    for (int i = 0; i < cmd->count; ++i) {
        strcat(cmd_joined, cmd->args[i]);
        strcat(cmd_joined, i == cmd->count - 1 ? "" : " "); // Concatenate a space unless we're at the very end.
    }

    // Not needed anymore. We might as well just reset them here.
    cmd->count = 0;
    cmd->size = 0;
    cmd->capacity = 0;

    printf("[QFB] Executing %s\n", cmd_joined);
    free(cmd_joined);
    pid_t pid = fork();
    if (pid < 0) {
        printf("[QFB] Could not fork child process: %s\n", strerror(errno));
        return 0;
    }

    if (pid == 0) {
        // Code inside the child process.
        if (execvp(cmd->args[0], (char* const*)cmd->args) < 0) {
            printf("[QFB] Could not exec child process for %s: %s\n", cmd->args[0], strerror(errno));
            exit(1);
        }
    } else {
        // Code inside the parent process.
        free(cmd->args);
    }

    return pid;
}

int qfb_execute_cmd(Qfb_Cmd* cmd) {
    int status = 1;
    pid_t pid = qfb_execute_cmd_async(cmd);
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

unsigned int qfb_checksum(const char* source) {
    FILE* fp = fopen(source, "rb");
    if (!fp) {
        printf("[QFB] Couldn't open %s.\n", source);
        return 0;
    }
    unsigned int checksum = 0;
    while (!feof(fp) && !ferror(fp)) {
        checksum ^= fgetc(fp);
    }
    fclose(fp);
    return checksum;
}

void _qfb_self_rebuild(const char* source, const char* dest, char* flags) {
    Qfb_Cmd cmd = {0};
    printf("[QFB] Detected a change!\n");

    // TODO: Add support for other compilers.
    // For now we'll just use `cc` and hope it works well with clang and gcc.
    qfb_cmd_append(&cmd, "cc", source, "-o", dest);
    if (flags)
        qfb_cmd_append(&cmd, flags);

    if (qfb_execute_cmd(&cmd) != 0) {
        printf("[QFB] Failed to self build... Continuing...\n");
        return;
    }

    qfb_cmd_append(&cmd, "./qfb");
    if (execvp(cmd.args[0], (char* const*)cmd.args) < 0) {
        printf("[QFB] Could not exec child process for %s: %s\n", cmd.args[0], strerror(errno));
        exit(1);
    }
}
// If the recipe file gets too big we might need to
// switch from hardcoding sizes on the stack to a proper
// dynamically sized string implementation which shouldn't
// be that hard I suppose.

// The core idea is that we check if an embedded checksum (`#define CHECKSUM`)
// equals to the current checksum and if they don't we just recompile.
    #ifndef CHECKSUM
        #define CHECKSUM 0
    #endif // CHECKSUM
    #define CHECKSUM_BUF_SIZE 64
void _qfb_self_rebuild_checksum(const char* dest, const char* source) {
    unsigned int current_checksum = qfb_checksum(source);

    if (CHECKSUM == 0 || current_checksum != CHECKSUM) {
        if (current_checksum == 0) {
            printf("[QFB] Couldn't generate a hash for the checksum... Continuing.\n");
            return;
        }
        char new_checksum[CHECKSUM_BUF_SIZE];
        sprintf(new_checksum, "-DCHECKSUM=%u", current_checksum);
        _qfb_self_rebuild(source, dest, new_checksum);
    }
}

// Here it checks if the source (`qfb.c`) is newer than the dest (usually just `./qfb`)
// and if so, we recompile.
void _qfb_self_rebuild_time(const char* dest, const char* source) {
    struct stat sb_dest;
    struct stat sb_src;
    if (lstat(dest, &sb_dest) == -1 || lstat(source, &sb_src) == -1) {
        printf("[QFB] Couldn't get the required metadata for rebuilding. Continuing...\n");
        return;
    }
    if (sb_dest.st_mtime < sb_src.st_mtime) {
        _qfb_self_rebuild(source, dest, NULL);
    }
}
#endif // QFB_IMPLEMENTATION
