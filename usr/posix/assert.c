
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void _assert(void *expr, void *filename, unsigned lineno)
{
  printf("Assertion failed: %s, file %s, line %d\n", expr, filename, lineno);
  exit(3);
}
