/*
 * prints "d\n"
 */

#include "tests/lib.h"

int main(void)
{
  int k = syscall_getclock();
  while (k + 4000 > syscall_getclock()){
  }
  puts("d\n");
  return 0;
}
