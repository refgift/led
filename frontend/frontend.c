#include "frontend.h"
#include <stddef.h>

static const Frontend *current;

void
frontend_use (const Frontend *fe)
{
  current = fe;
}

const Frontend *
frontend_current (void)
{
  return current;
}
