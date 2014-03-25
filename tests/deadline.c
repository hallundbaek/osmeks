#include "tests/lib.h"

int main(void)
{
  int a,b,c,d,e;
  a = syscall_exec("[arkimedes]a", -1);
  b = syscall_exec("[arkimedes]b", 9000);
  c = syscall_exec("[arkimedes]c", 3000);
  d = syscall_exec("[arkimedes]d", 4000);
  e = syscall_exec("[arkimedes]e", 2000);
  syscall_join(a);
  syscall_join(b);
  syscall_join(c);
  syscall_join(d);
  syscall_join(e);
  return 0;
}
