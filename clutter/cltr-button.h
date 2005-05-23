#ifndef _HAVE_CLTR_BUTTON_H
#define _HAVE_CLTR_BUTTON_H

#include "cltr.h"

typedef struct CltrButton CltrButton;

typedef enum CltrButtonState
{
  CltrButtonStateDisabled,
  CltrButtonStateInactive,
  CltrButtonStateFocused,
  CltrButtonStateActive,
 }
CltrButtonState;

typedef void (*CltrButtonActivate) (CltrWidget *widget, void *userdata) ;

#define CLTR_BUTTON(w) ((CltrButton*)(w))

CltrWidget*
cltr_button_new(int width, int height);

void
cltr_button_on_activate(CltrButton         *button,
			CltrButtonActivate  callback,
			void*               userdata);

void
cltr_button_set_label(CltrButton  *button, 
		      const char  *text,
		      CltrFont    *font,
		      PixbufPixel *col);

#endif
