#ifndef _HAVE_CLTR_WIDGET_H
#define _HAVE_CLTR_WIDGET_H

typedef struct CltrWidget CltrWidget;

#include "cltr.h"

#define CLTR_WIDGET(w) ((CltrWidget*)(w))

CltrWidget*
cltr_widget_new(void);

int
cltr_widget_width(CltrWidget *widget);

int
cltr_widget_height(CltrWidget *widget);

void
cltr_widget_show(CltrWidget *widget);

void
cltr_widget_paint(CltrWidget *widget);

gboolean
cltr_widget_handle_xevent(CltrWidget *widget, XEvent *xev);

void
cltr_widget_show_all(CltrWidget *widget);

void
cltr_widget_queue_paint(CltrWidget *widget);

void
cltr_widget_add_child(CltrWidget *widget, CltrWidget *child, int x, int y);

#endif
