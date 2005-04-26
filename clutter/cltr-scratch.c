#include "cltr-scratch.h"
#include "cltr-private.h"

struct CltrScratch
{
  CltrWidget   widget;  

  Pixbuf      *pixb;
  CltrTexture *tex;
};

static void
cltr_scratch_show(CltrWidget *widget);

static gboolean 
cltr_scratch_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_scratch_paint(CltrWidget *widget);


CltrWidget*
cltr_scratch_new(int width, int height)
{
  CltrScratch *scratch;
  ClutterFont  *font;
  PixbufPixel   pixel = { 255, 255, 255, 100 };

  scratch = g_malloc0(sizeof(CltrScratch));
  
  scratch->widget.width          = width;
  scratch->widget.height         = height;
  
  scratch->widget.show           = cltr_scratch_show;
  scratch->widget.paint          = cltr_scratch_paint;
  
  scratch->widget.xevent_handler = cltr_scratch_handle_xevent;

  scratch->pixb = pixbuf_new(width, height);

  pixel_set_vals(&pixel, 0, 0, 0, 255);

  pixbuf_fill_rect(scratch->pixb, 0, 0, width, height, &pixel);

  font = font_new ("Sans Bold 72");

  pixel_set_vals(&pixel, 255, 255, 255, 255);

  font_draw(font, scratch->pixb, "Hello", 0, 0, &pixel);  

  scratch->tex = cltr_texture_new(scratch->pixb);

  return CLTR_WIDGET(scratch);
}

static void
cltr_scratch_show(CltrWidget *widget)
{
  cltr_widget_queue_paint(widget);
}

static gboolean 
cltr_scratch_handle_xevent (CltrWidget *widget, XEvent *xev) 
{

  return TRUE;
}

static void
cltr_scratch_paint_old(CltrWidget *widget)
{
  CltrScratch *scratch = CLTR_SCRATCH(widget);

  int times = 100, num = 0;
  int spost = 0;
  float alphainc = 0.9f / times;
  float alpha = 0.1f;

  glPushMatrix();

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING); 

  glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  glEnable(GL_BLEND);


  /*
  glEnable(GL_BLEND);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  */

  for (num = 0;num < times;num++)
    {
      glColor4f(1.0f, 1.0f, 1.0f, alpha);
      cltr_texture_render_to_gl_quad(scratch->tex,
				     widget->x - spost,
				     widget->y - spost,
				     widget->x + widget->width + spost,
				     widget->y + widget->height + spost);
      spost += 2;
      alpha = alpha - alphainc;
    }

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  cltr_texture_render_to_gl_quad(scratch->tex,
				 widget->x,
				 widget->y,
				 widget->x + widget->width ,
				 widget->y + widget->height);


  glDisable(GL_BLEND);

  glPopMatrix();
}


static void
cltr_scratch_paint(CltrWidget *widget)
{
  CltrScratch *scratch = CLTR_SCRATCH(widget);
  
  glPushMatrix();

  CLTR_MARK();

  glEnable(GL_BLEND);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glColor4ub(100, 200, 50, 100);  

  glRecti(widget->x, widget->y, 
	  widget->x + widget->width, 
	  widget->y + widget->height);

  glEnable(GL_TEXTURE_2D);

  cltr_texture_render_to_gl_quad(scratch->tex,
				 widget->x,
				 widget->y,
				 widget->x + widget->width ,
				 widget->y + widget->height);

  glDisable(GL_TEXTURE_2D);

  glDisable(GL_BLEND);

  glPopMatrix();
}
