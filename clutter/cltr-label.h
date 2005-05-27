#ifndef _HAVE_CLTR_LABEL_H
#define _HAVE_CLTR_LABEL_H

#include "cltr.h"

typedef struct CltrLabel CltrLabel;

#define CLTR_LABEL(w) ((CltrLabel*)(w))

CltrWidget*
cltr_label_new(const char  *text, 
	       CltrFont    *font,
	       PixbufPixel *col);

void
cltr_label_set_text(CltrLabel *label, char *text);

#endif
