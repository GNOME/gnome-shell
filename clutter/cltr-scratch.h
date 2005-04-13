#ifndef _HAVE_CLTR_SCRATCH_H
#define _HAVE_CLTR_SCRATCH_H

#include "cltr.h"

typedef struct CltrScratch CltrScratch;

#define CLTR_SCRATCH(w) ((CltrScratch*)(w))

CltrWidget*
cltr_scratch_new(int width, int height);


#endif
