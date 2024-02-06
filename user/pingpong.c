#include "kernel/types.h"
#include "user/user.h"

#define RD 0 //pipe的read�?
#define WR 1 //pipe的write�?

int main(int argc, char const *argv[]) {
    char buf = 'P'; //用于传送的字节

    int fd_c2p[2]; //子进�?->父进�?
    int fd_p2c[2]; //父进�?->子进�?
    pipe(fd_c2p);
    pipe(fd_p2c);

    int pid = fork();
    int exit_status = 0;

    if (pid < 0) {
        fprintf(2, "fork() error!\n");
        close(fd_c2p[RD]);
        close(fd_c2p[WR]);
        close(fd_p2c[RD]);
        close(fd_p2c[WR]);
        exit(1);
    } else if (pid == 0) { //子进�?
       // printf("%d\n",fd_p2c[WR]);
       // printf("%d\n",fd_c2p[RD]);

        close(fd_p2c[WR]);
        close(fd_c2p[RD]);
        
       // printf("%d\n",fd_p2c[WR]);
        printf("%d\n",fd_c2p[RD]);
        
        
        if (read(fd_p2c[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child read() error!\n");
            exit_status = 1; //标记出错
        } else {
            fprintf(1, "%d: received ping\n", getpid());
        }

        if (write(fd_c2p[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child write() error!\n");
            exit_status = 1;
        }
        // printf("%d\n",fd_p2c[WR]);
        // printf("%d\n",fd_c2p[RD]);

        close(fd_p2c[RD]);
        close(fd_c2p[WR]);

        exit(exit_status);
    } else { //父进�?
        // printf("%d\n",fd_p2c[WR]);
        // printf("%d\n",fd_c2p[RD]);

        close(fd_p2c[RD]);
        close(fd_c2p[WR]);

        if (write(fd_p2c[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent write() error!\n");
            exit_status = 1;
        }

        if (read(fd_c2p[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent read() error!\n");
            exit_status = 1; //标记出错
        } else {
            fprintf(1, "%d: received pong\n", getpid());
        }

        close(fd_p2c[WR]);
        close(fd_c2p[RD]);

        exit(exit_status);
    }
}
