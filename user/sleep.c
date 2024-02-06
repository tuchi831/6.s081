#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char const *argv[])
{
  if (argv[1][0]=='-'){
    fprintf(2, "请输入正数\n");
  }
  if (argc != 2) { //参数错误
    fprintf(2, "usage: sleep <time>\n");
    exit(1);
  }
  sleep(atoi(argv[1]));
  exit(0);
}
