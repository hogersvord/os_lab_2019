
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Display child process information
static void ShowProcess(pid_t pid) {
    char command[128];

    snprintf(command, sizeof(command),
             "ps -o pid,ppid,stat,comm -p %ld", (long)pid);

    if (system(command) == -1) {
        perror("system");
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    pid_t child_pid = fork();

    if (child_pid < 0) {
        perror("fork");
        return 1;
    }

    if (child_pid == 0) {
        printf("Child: PID = %ld. Exiting now.\n",
               (long)getpid());
        _exit(0);
    }

    printf("Parent: PID = %ld, child PID = %ld.\n",
           (long)getpid(), (long)child_pid);

    // Parent does not call waitpid yet
    sleep(2);

    printf("\nBefore waitpid(): child should be a zombie (STAT = Z).\n");
    ShowProcess(child_pid);

    // Remove the zombie process
    int status;

    if (waitpid(child_pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    printf("\nAfter waitpid(): child has been reaped.\n");

    if (WIFEXITED(status)) {
        printf("Child exit code: %d\n", WEXITSTATUS(status));
    }

    ShowProcess(child_pid);

    return 0;
}
