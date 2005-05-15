#ifndef _HAVE_CLTR_ANIMATOR_H
#define _HAVE_CLTR_ANIMATOR_H

#include "cltr.h"

typedef struct CltrAnimator CltrAnimator;

typedef enum CltrAnimatorType
{
  CltrAnimatorFullZoom
}
CltrAnimatorType;

typedef void (*CltrAnimatorFinishFunc) (CltrAnimator *anim, void *userdata) ;

CltrAnimator*
cltr_animator_fullzoom_new(CltrWidget *widget,
			   int         x1,
			   int         y1,
			   int         x2,
			   int         y2);

void
cltr_animator_run(CltrAnimator            *anim,
		  CltrAnimatorFinishFunc   finish_callback,
		  gpointer                *finish_data);

#endif
