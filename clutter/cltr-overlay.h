#ifndef _HAVE_CLTR_OVERLAY_H
#define _HAVE_CLTR_OVERLAY_H

#include "cltr.h"

typedef struct CltrOverlay CltrOverlay;

#define CLTR_OVERLAY(w) ((CltrOverlay*)(w))

CltrWidget*
cltr_overlay_new(int width, int height);


#endif
