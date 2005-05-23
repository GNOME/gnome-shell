#include "cltr-overlay.h"
#include "cltr-private.h"

struct CltrOverlay
{
  CltrWidget   widget;  
};

static void
cltr_overlay_show(CltrWidget *widget);

static gboolean 
cltr_overlay_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_overlay_paint(CltrWidget *widget);


CltrWidget*
cltr_overlay_new(int width, int height)
{
  CltrOverlay *overlay;

  overlay = g_malloc0(sizeof(CltrOverlay));
  
  overlay->widget.width          = width;
  overlay->widget.height         = height;
  
  overlay->widget.show           = cltr_overlay_show;
  overlay->widget.paint          = cltr_overlay_paint;
  
  overlay->widget.xevent_handler = cltr_overlay_handle_xevent;

  return CLTR_WIDGET(overlay);
}

static void
cltr_overlay_show(CltrWidget *widget)
{

}

static gboolean 
cltr_overlay_handle_xevent (CltrWidget *widget, XEvent *xev) 
{

  return FALSE;
}

static void
cltr_overlay_paint(CltrWidget *widget)
{
  glEnable(GL_BLEND);

  glColor4f(0.5, 0.5, 0.5, 0.5);

  cltr_glu_rounded_rect_filled(widget->x,
			       widget->y,
			       widget->x + widget->width,
			       widget->y + widget->height,
			       widget->width/30,
			       NULL);

  glDisable(GL_BLEND);

}
