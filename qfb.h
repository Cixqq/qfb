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

void _cmd_append(Cmd* cmd, ...);
int execute_cmd(Cmd* cmd); // Thread-safety NOT GUARANTEED.

#define cmd_append(cmd, ...) _cmd_append(cmd, __VA_ARGS__, NULL)

#ifdef QFB_IMPLEMENTATION
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

int execute_cmd(Cmd* cmd) {
    char* cmd_joined = (char*)malloc(cmd->size);

    for (int i = 0; i < cmd->count; ++i) {
        strcat(cmd_joined, cmd->args[i]);
        strcat(cmd_joined, " ");
    }

    // Not needed anymore. We might as well just reset them here.
    cmd->count = 0;
    cmd->size = 0;
    cmd->capacity = 0;

    printf("[QFB] Executing %s\n", cmd_joined);
    free(cmd_joined);
    int cpid = fork();
    if (cpid < 0) {
        printf("Could not fork child process: %s", strerror(errno));
        return 1;
    }

    int status = 1;
    if (cpid == 0) {
        // Code inside the child process.
        if (execvp(cmd->args[0], (char* const*)cmd->args) < 0) {
            printf("Could not exec child process for %s: %s", cmd->args[0], strerror(errno));
            exit(1);
        }
    } else {
        // Code inside the parent process.
        free(cmd->args); // Not entirely sure if this leaks memory lol.
        waitpid(cpid, &status, 0);
    }

    return status;
}
#endif // QFB_IMPLEMENTATION
