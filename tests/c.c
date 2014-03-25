/*
 * prints "c\n"
 */

#include "tests/lib.h"

int main(void)
{
  int k = syscall_getclock();
  while (k + 3000 > syscall_getclock()){
  }
  puts("c\n");
  return 0;
}
