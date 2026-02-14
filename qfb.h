#include <bits/wait.h>
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
void qfb_self_rebuild();
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
        if (arg == NULL)
            break;
        if (cmd->size + sizeof(arg) >= cmd->capacity) {
            cmd->capacity *= 2;
            cmd->args = (char**)realloc(cmd, cmd->capacity);
        }
        cmd->args[i] = arg;
        cmd->size += sizeof(arg);
        cmd->count++;
    }

    va_end(args);
}

pid_t qfb_execute_cmd_async(Qfb_Cmd* cmd) {
    char* cmd_joined = (char*)malloc(cmd->size);

    for (int i = 0; i < cmd->count; ++i) {
        strcat(cmd_joined, cmd->args[i]);
        strcat(cmd_joined, i == cmd->count - 1 ? "" : " "); // Concatenate a space unless we're at the very end.
    }

    // Not needed anymore. We might as well just reset them here.
    cmd->count = 0;
    cmd->size = 0;
    cmd->capacity = 0;

    printf("[QFB] Executing %s\n", cmd_joined);
    pid_t pid = fork();
    if (pid < 0) {
        printf("Could not fork child process: %s\n", strerror(errno));
        return 0;
    }

    if (pid == 0) {
        // Code inside the child process.
        if (execvp(cmd->args[0], (char* const*)cmd->args) < 0) {
            printf("Could not exec child process for %s: %s\n", cmd->args[0], strerror(errno));
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
    unsigned int checksum = 0;
    while (!feof(fp) && !ferror(fp)) {
        checksum ^= fgetc(fp);
    }
    fclose(fp);
    return checksum;
}

// Forgot where I copied this from lmao.
int qfb_ltoa(long value, char* sp) {
    char tmp[16]; // be careful with the length of the buffer
    char* tp = tmp;
    int i;
    unsigned v;
    int radix = 10;

    int sign = (radix == 10 && value < 0);
    if (sign)
        v = -value;
    else
        v = (unsigned)value;

    while (v || tp == tmp) {
        i = v % radix;
        v /= radix;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    if (sign) {
        *sp++ = '-';
        len++;
    }

    while (tp > tmp)
        *sp++ = *--tp;

    return len;
}

void _qfb_self_rebuild(const char* source, const char* dest, int isChecksumBased, const char* checksum) {
    Qfb_Cmd cmd = {0};
    printf("[QFB] Detected a change!\n");

    // TODO: Add support for other compilers.
    // For now we'll just use `cc` and hope it works well with clang and gcc.
    qfb_cmd_append(&cmd, "cc", source, "-o", dest);
    if (isChecksumBased)
        qfb_cmd_append(&cmd, checksum);

    if (qfb_execute_cmd(&cmd) != 0) {
        printf("[QFB] Failed to self build... Continuing...\n");
        return;
    }

    qfb_cmd_append(&cmd, "./qfb");
    if (execvp(cmd.args[0], (char* const*)cmd.args) < 0) {
        printf("Could not exec child process for %s: %s\n", cmd.args[0], strerror(errno));
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
        #define CHECKSUM ""
    #endif // CHECKSUM
    #define CHECKSUM_BUF_SIZE 64
void _qfb_self_rebuild_checksum(const char* dest, const char* source) {
    unsigned int current_checksum = qfb_checksum(source);
    char current_checksum_str[CHECKSUM_BUF_SIZE];
    qfb_ltoa(current_checksum, current_checksum_str);

    if (strcmp(CHECKSUM, "") == 0 || strcmp(current_checksum_str, CHECKSUM) != 0) {
        char new_checksum[CHECKSUM_BUF_SIZE];
        sprintf(new_checksum, "-DCHECKSUM=\"%u\"", current_checksum);
        _qfb_self_rebuild(source, dest, 1, new_checksum);
    }
}

// Here it checks if the source (`qfb.c`) is newer than the dest (usually just `./qfb`)
// and if so, we recompile.
void _qfb_self_rebuild_time(const char* dest, const char* source) {
    struct stat sb_dest;
    struct stat sb_src;
    if (lstat(dest, &sb_dest) == -1 || lstat(source, &sb_src) == -1) {
        printf("[QFB] Couldn't get the required metadata for rebuilding.\n");
    }
    if (sb_dest.st_mtime < sb_src.st_mtime) {
        _qfb_self_rebuild(source, dest, 0, "");
    }
}
#endif // QFB_IMPLEMENTATION
