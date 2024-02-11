#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main()
{
    if (fork() == 0) {
       // write(1, "hello ", 6);
        fprintf(2,"hello ");
        exit(0);
    } else {
        wait(0);
        fprintf(2,"world\n");
       // write(1, "world\n", 6);
    }
    exit(0);
}