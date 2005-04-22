#ifndef _HAVE_CLTR_BUTTON_H
#define _HAVE_CLTR_BUTTON_H

#include "cltr.h"

typedef struct CltrButton CltrButton;

#define CLTR_BUTTON(w) ((CltrButton*)(w))

CltrWidget*
cltr_button_new(int width, int height);


#endif
