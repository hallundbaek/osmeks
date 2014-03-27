#include "tests/lib.h"

int main(void)
{
  int pipeid;
  char string[4];
  pipeid = syscall_open("[pipe]new");
  syscall_read(pipeid, string, 4);
  syscall_write(1, string, 4);
  printf("\n");
  syscall_delete("[pipe]new");
  return 0;
}
