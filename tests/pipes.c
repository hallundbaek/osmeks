#include "tests/lib.h"

int main(void)
{
  int pipeid,a,b,c,d;
  syscall_create("[pipe]test",20);
  a = syscall_exec("[arkimedes]piperead", 1000);
  b = syscall_exec("[arkimedes]piperead", 2000);
  c = syscall_exec("[arkimedes]piperead", 3000);
  d = syscall_exec("[arkimedes]piperead", 4000);
  pipeid = syscall_open("[pipe]test");
  pipeid = pipeid;
  syscall_write(pipeid, "1111222233334444",  16);
  puts("All data read\n");
  syscall_join(a);
  syscall_join(b);
  syscall_join(c);
  syscall_join(d);
  return 0;
}
