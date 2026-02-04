#include <bits/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char** args;
    int size;
    int capacity;
    int count;
} Cmd;

#ifndef _QFB_GUARD
    #define _QFB_GUARD
void _cmd_append(Cmd* cmd, ...);
int execute_cmd(Cmd* cmd);       // Thread-safety NOT GUARANTEED.
int execute_cmd_async(Cmd* cmd); // Thread-safety NOT GUARANTEED.
#endif                           // _QFB_GUARD

#define cmd_append(cmd, ...) _cmd_append(cmd, __VA_ARGS__, NULL)

#ifndef QFB_IMPLEMENTATION
void _cmd_append(Cmd* cmd, ...) {
    va_list args;
    va_start(args, cmd);

    if (cmd->args == NULL || cmd->capacity == 0) {
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

pid_t execute_cmd_async(Cmd* cmd) {
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

int execute_cmd(Cmd* cmd) {
    int status = 1;
    pid_t pid = execute_cmd_async(cmd);
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}
#endif // QFB_IMPLEMENTATION
