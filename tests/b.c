/*
 * prints "b\n"
 */

#include "tests/lib.h"

int main(void)
{
  int k = syscall_getclock();
  while (k + 9000 > syscall_getclock()){
  }
  puts("b\n");
  return 0;
}
