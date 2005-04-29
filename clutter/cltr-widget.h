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

int
cltr_widget_abs_x(CltrWidget *widget);

int
cltr_widget_abs_y(CltrWidget *widget);

int
cltr_widget_abs_x2(CltrWidget *widget);

int
cltr_widget_abs_y2(CltrWidget *widget);


/* These are hacky see notes in .c */
void
cltr_widget_focus(CltrWidget *widget);

void
cltr_widget_unfocus(CltrWidget *widget);

/* ******************************* */

void
cltr_widget_set_focus_next(CltrWidget    *widget,
			   CltrWidget    *widget_to_focus,
			   CltrDirection  direction);

CltrWidget*
cltr_widget_get_focus_next(CltrWidget    *widget,
			   CltrDirection  direction);

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
