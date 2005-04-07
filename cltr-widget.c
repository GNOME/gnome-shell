#include "cltr-widget.h"
#include "cltr-private.h"

CltrWidget*
cltr_widget_new(void)
{
  CltrWidget *w = NULL;

  w = g_malloc0(sizeof(CltrWidget));

  return w;
}

void
cltr_widget_show(CltrWidget *widget)
{
  widget->visible = TRUE;

  if (widget->show)
    {
      widget->show(widget);
    }
}

void
cltr_widget_show_all(CltrWidget *widget)
{
  GList *widget_item =  widget->children;;

  if (widget_item)
    {
      do 
	{
	  CltrWidget *child = CLTR_WIDGET(widget_item->data);

	  cltr_widget_show(child);
	}
      while ((widget_item = g_list_next(widget_item)) != NULL);
    }

  cltr_widget_show(widget);
}

void
cltr_widget_add_child(CltrWidget *widget, CltrWidget *child, int x, int y)
{
  widget->children = g_list_append(widget->children, child);

  child->parent = widget;
  child->x      = x;
  child->y      = y;
}


void
cltr_widget_hide(CltrWidget *widget)
{
  widget->visible = FALSE;
}

void
cltr_widget_paint(CltrWidget *widget)
{
  if (widget->visible)
    {
      GList *child_item =  widget->children;;

      if (widget->paint)
	widget->paint(widget);

      /* Recurse down */
      if (child_item)
	{
	  do 
	    {
	      CltrWidget *child = CLTR_WIDGET(child_item->data);
	      
	      if (child->visible)
		cltr_widget_paint(child);
	      
	    }
	  while ((child_item = g_list_next(child_item)) != NULL);
	}
    }
}

void
cltr_widget_queue_paint(CltrWidget *widget)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();

  g_queue_push_head (ctx->internal_event_q, (gpointer)widget);
}

gboolean
cltr_widget_handle_xevent(CltrWidget *widget, XEvent *xev)
{
  if (widget && widget->xevent_handler)
    return widget->xevent_handler(widget, xev);

  return FALSE;
}
