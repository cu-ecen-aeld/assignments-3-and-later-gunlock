#include "systemcalls.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
 */
bool do_system(const char *cmd) {
    /*
     * Completed:  add your code here
     *  Call the system() function with the command set in the cmd
     *   and return a boolean true if the system() call completed with success
     *   or false() if it returned a failure
     */
    if (!cmd || cmd[0] == '\0') {
        return false;
    }

    int result = system(cmd);

    // Based on my interpreation of the instructions, I am only evaluating the
    // the system() result, not the child exit status.  Per the manual page
    // for system(), return values of -1  (child could not be created) and
    // 127 (shell could not be executed in the child process) indicates
    // failure of the system(). However, they do not describe the state
    // of the child exit.

    return (result == -1 || result == 127) ? false : true;
}

/**
 * @param count -The numbers of variables passed to the function. The variables
 * are command to execute. followed by arguments to pass to the command Since
 * exec() does not perform path expansion, the command to execute needs to be an
 * absolute path.
 * @param ... - A list of 1 or more arguments after the @param count argument.
 *   The first is always the full path to the command to execute with execv()
 *   The remaining arguments are a list of arguments to pass to the command in
 * execv()
 * @return true if the command @param ... with arguments @param arguments were
 * executed successfully using the execv() call, false if an error occurred,
 * either in invocation of the fork, waitpid, or execv() command, or if a
 * non-zero return value was returned by the command issued in @param arguments
 * with the specified arguments.
 */

bool do_exec(int count, ...) {
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is
    // complete and may be removed
    // command[count] = command[count];

    /*
     * COMPLETED:
     *   Execute a system command by calling fork, execv(),
     *   and wait instead of system (see LSP page 161).
     *   Use the command[0] as the full path to the command to execute
     *   (first argument to execv), and use the remaining arguments
     *   as second argument to the execv() command.
     *
     */

    enum { CHILD_PROC = 0 };
    bool result = false;

    fflush(stdout);  // prevents duplicate prints
    pid_t pid = fork();
    if (pid < 0) {
        va_end(args);
        return false;
    }

    if (pid == CHILD_PROC) {
        // exec functions only return if there is an error.  On success
        // the process is replaced by the process invoked by the exec()
        // function and the return value is that of command invoked.
        execv(command[0], command);

        // This only executes if execv failed.
        exit(EXIT_FAILURE);
    } else {
        // parent process.
        int child_status;
        if (waitpid(pid, &child_status, 0) != -1 && WIFEXITED(child_status)) {
            int exit_code = WEXITSTATUS(child_status);
            result = exit_code == 0;
        }
    }

    va_end(args);

    return result;
}

/**
 * @param outputfile - The full path to the file to write with command output.
 *   This file will be closed at completion of the function call.
 * All other parameters, see do_exec above
 */
bool do_exec_redirect(const char *outputfile, int count, ...) {
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is
    // complete and may be removed
    // command[count] = command[count];

    /*
     * COMPLETED:
     *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624
     * as a refernce, redirect standard out to a file specified by outputfile. The
     * rest of the behaviour is same as do_exec()
     *
     */

    enum { CHILD_PROC = 0 };
    bool result = false;

    fflush(stdout);  // prevents duplicate prints
    pid_t pid = fork();
    if (pid < 0) {
        va_end(args);
        return false;
    }

    if (pid == CHILD_PROC) {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) {
            exit(EXIT_FAILURE);
        }
        // redirect stdout to file
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);                   // close original fd, stout now point to file
        execv(command[0], command);  // TODO, handle redirect
        exit(EXIT_FAILURE);          // only executes if execv fails
    } else {
        // parent
        int child_status;
        if (waitpid(pid, &child_status, 0) != -1 && WIFEXITED(child_status)) {
            int exit_code = WEXITSTATUS(child_status);
            result = exit_code == 0;
        }
    }

    va_end(args);

    return result;
}
