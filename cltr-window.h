#ifndef _HAVE_CLTR_WINDOW_H
#define _HAVE_CLTR_WINDOW_H

#include "cltr.h"

typedef struct CltrWindow CltrWindow;

#define CLTR_WINDOW(w) ((CltrWindow*)(w))

CltrWidget*
cltr_window_new(int width, int height);

void
cltr_window_paint(CltrWidget *widget);

void
cltr_window_add_widget(CltrWindow *win, CltrWidget *widget, int x, int y);

/* win only methods */

Window
cltr_window_xwin(CltrWindow *win);

Window
cltr_window_focus_widget(CltrWindow *win, CltrWidget *widget);


#endif
