#include "cltr-animator.h"
#include "cltr-private.h"


struct CltrAnimator 
{
  CltrWidget *widget;
  gint        fps;
  int         n_steps, step;

  CltrAnimatorFinishFunc anim_finish_cb;
  gpointer              *anim_finish_data;

  WidgetPaintMethod      wrapped_paint_func;

  int         zoom_init_x1, zoom_init_y1, zoom_init_x2, zoom_init_y2;
  int         zoom_dest_x1, zoom_dest_y1, zoom_dest_x2, zoom_dest_y2;
};

static void
cltr_animator_wrapped_paint(CltrWidget *widget);

CltrAnimator*
cltr_animator_fullzoom_new(CltrWidget *widget,
			   int         x1,
			   int         y1,
			   int         x2,
			   int         y2)
{
  CltrAnimator *anim = g_malloc0(sizeof(CltrAnimator));

  anim->zoom_init_x1 = x1;
  anim->zoom_init_x2 = x2;
  anim->zoom_init_y1 = y1;
  anim->zoom_init_y2 = y2;

  anim->zoom_dest_x1 = cltr_widget_abs_x(widget);
  anim->zoom_dest_x2 = cltr_widget_abs_x2(widget);
  anim->zoom_dest_y1 = cltr_widget_abs_y(widget);
  anim->zoom_dest_y2 = cltr_widget_abs_y2(widget);

  anim->wrapped_paint_func = widget->paint;

  anim->widget = widget;
  widget->anim = anim;

  anim->n_steps = 10;
  anim->step    = 0;
  anim->fps     = 50;

  return anim;
}


CltrAnimator*
cltr_animator_new(CltrWidget *widget)
{
  return NULL;
}

void
cltr_animator_set_args(CltrAnimator *anim)
{

}

static void
cltr_animator_wrapped_paint(CltrWidget *widget)
{
  CltrAnimator *anim = widget->anim;
  float         tx = 0.0, ty = 0.0;
  float            x1, x2, y1, y2;

  /* zoom here */

  float f = (float)anim->step/anim->n_steps;

  int init_width = anim->zoom_init_x2 - anim->zoom_init_x1;
  int dest_width = anim->zoom_dest_x2 - anim->zoom_dest_x1;

  int init_height = anim->zoom_init_y2 - anim->zoom_init_y1;
  int dest_height = anim->zoom_dest_y2 - anim->zoom_dest_y1;

  float max_zoom_x = (float)dest_width/init_width;
  float max_zoom_y = (float)dest_height/init_height;

  float trans_y =  ((float)anim->zoom_init_y1);

  glPushMatrix();

  CLTR_DBG("f is %f ( %i/%i ) max_zoom x: %f y: %f, zooming to %f, %f", 
	   f, anim->step, anim->n_steps, max_zoom_x, max_zoom_y,
	   (f * max_zoom_x), (f * max_zoom_y)); 

#if 0
  glTranslatef (0.0 /* - (float)(anim->zoom_dest_x1) * ( (max_zoom_x * f) )*/,
		- trans_y * f * max_zoom_y,
		0.0);

  if ((f * max_zoom_x) > 1.0 && (f * max_zoom_y) > 1.0)
    {
      glScalef ((f * max_zoom_x), (f * max_zoom_y), 0.0);
    }
#endif

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  /*   glOrtho (0, widget->width, widget->height, 0, -1, 1); */



  /* 800 -> 80, 800-80 = 720/n_steps = x , cur = 80 + x * (n_steps - steps) */

  x2 = anim->zoom_init_x2 + ( ( ((float)anim->zoom_dest_x2 - anim->zoom_init_x2)/ (float)anim->n_steps) * (anim->n_steps - anim->step) ); 

  x1 = anim->zoom_init_x1 + ( ( ((float)anim->zoom_dest_x1 - anim->zoom_init_x1)/ (float)anim->n_steps) * (anim->n_steps - anim->step) ); 

  y1 = anim->zoom_init_y1 + ( ( ((float)anim->zoom_dest_y1 - anim->zoom_init_y1)/ (float)anim->n_steps) * (anim->n_steps - anim->step) ); 

  y2 = anim->zoom_init_y2 + ( ( ((float)anim->zoom_dest_y2 - anim->zoom_init_y2)/ (float)anim->n_steps) * (anim->n_steps - anim->step) ); 

  /*
  glOrtho( anim->zoom_init_x1, x2-1,
	   anim->zoom_init_y2-1, anim->zoom_init_y1,
	   -1, 1);
  */

  glOrtho( x1, x2-1, y2-1, y1,
	   -1, 1);

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  anim->wrapped_paint_func(widget);

  glPopMatrix();

  /* reset here */
}


static gboolean
cltr_animator_timeout_cb(gpointer data)
{
  CltrAnimator *anim = (CltrAnimator *)data;

  anim->step++;

  if (anim->step > anim->n_steps)
    return FALSE;

  cltr_widget_queue_paint(anim->widget);

  return TRUE;
}

void
cltr_animator_run(CltrAnimator            *anim,
		  CltrAnimatorFinishFunc   finish_callback,
		  gpointer                *finish_data)
{
  anim->anim_finish_cb   = finish_callback;
  anim->anim_finish_data = finish_data;

  anim->widget->paint = cltr_animator_wrapped_paint;

  anim->step = 0;

  g_timeout_add(FPS_TO_TIMEOUT(anim->fps), 
		cltr_animator_timeout_cb, anim);
}

