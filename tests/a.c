/*
 * prints "a\n"
 */

#include "tests/lib.h"

int main(void)
{
  int k = syscall_getclock();
  while (k + 10000 > syscall_getclock()){
  }
  puts("a\n");
  return 0;
}
