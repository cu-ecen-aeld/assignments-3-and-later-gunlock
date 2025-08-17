#include <stdio.h>

#include "systemcalls.h"

int main() {
    bool result = do_exec_redirect("testfile.txt", 3, "/bin/sh", "-c", "echo home is $HOME");
    printf("%s", result ? "true" : "false");

    return 0;
}
