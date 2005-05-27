#ifndef _HAVE_CLTR_ANIMATOR_H
#define _HAVE_CLTR_ANIMATOR_H

#include "cltr.h"

typedef struct CltrAnimator CltrAnimator;

typedef enum CltrAnimatorType
{
  CltrAnimatorZoom,
  CltrAnimatorFullZoom,
  CltrAnimatorMove
}
CltrAnimatorType;

typedef void (*CltrAnimatorFinishFunc) (CltrAnimator *anim, void *userdata) ;

CltrAnimator*
cltr_animator_zoom_new(CltrWidget *widget,
		       int         src_x1,
		       int         src_y1,
		       int         src_x2,
		       int         src_y2,
		       int         dst_x1,
		       int         dst_y1,
		       int         dst_x2,
		       int         dst_y2);

CltrAnimator*
cltr_animator_fullzoom_new(CltrWidget *widget,
			   int         x1,
			   int         y1,
			   int         x2,
			   int         y2);

/* HACK */
void
cltr_animator_reset(CltrAnimator *anim);

void
cltr_animator_run(CltrAnimator            *anim,
		  CltrAnimatorFinishFunc   finish_callback,
		  gpointer                *finish_data);

#endif
