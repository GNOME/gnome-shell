#include "util.h"

/* misc utility code */


void*
util_malloc0(int size)
{
  void *p;

  p = malloc(size);
  memset(p, 0, size);

  return p;
}

int 
util_next_p2 ( int a )
{
  int rval=1;
  while(rval < a) 
    rval <<= 1;

  return rval;
}

