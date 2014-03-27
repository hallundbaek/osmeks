#include "tests/lib.h"

int main(void)
{
  int pipeid,a,b,c,d;
  syscall_create("[pipe]test",20);
  a = syscall_exec("[arkimedes]piperead", -1);
  b = syscall_exec("[arkimedes]piperead", -1);
  c = syscall_exec("[arkimedes]piperead", -1);
  d = syscall_exec("[arkimedes]piperead", -1);
  pipeid = syscall_open("[pipe]test");
  pipeid = pipeid;
  syscall_write(pipeid, "0",  1);
  syscall_write(pipeid, "12345",  5);
  syscall_write(pipeid, "678",  3);
  syscall_write(pipeid, "9ABCDE",  6);
  syscall_delete("[pipe]test");

  syscall_create("[pipe]new",20);
  pipeid = syscall_open("[pipe]new");
  d = syscall_exec("[arkimedes]pipereaddelete", -1);
  syscall_write(pipeid, "LONGERTHAN4", 11);
  syscall_join(a);
  syscall_join(b);
  syscall_join(c);
  syscall_join(d);
  return 0;
}
