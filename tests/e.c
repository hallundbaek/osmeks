/*
 * prints "e\n"
 */

#include "tests/lib.h"

int main(void)
{
  int k = syscall_getclock();
  while (k + 2000 > syscall_getclock()){
  }
  puts("e\n");
  return 0;
}
