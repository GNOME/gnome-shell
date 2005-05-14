#include "cltr.h"
#include "cltr-private.h"

typedef void (*CltrAnimatorFinishFunc) (CltrAnimator *anim, void *userdata) ;

struct CltrAnimator
{
  CltrWidget *widget;
  gint        fps;
  int         steps;
};


CltrAnimator*
cltr_animator_new(CltrWidget *widget)
{

}

void
cltr_animator_set_args(CltrAnimator *anim)
{

}

void
cltr_animator_run(CltrAnimator            *anim,
		  CltrAnimatorFinishFunc  *finish_callback
		  gpointer                *finish_data)
{

}

