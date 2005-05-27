#include "cltr-widget.h"
#include "cltr-private.h"

CltrWidget*
cltr_widget_new(void)
{
  CltrWidget *w = NULL;

  w = g_malloc0(sizeof(CltrWidget));

  return w;
}

int
cltr_widget_abs_x(CltrWidget *widget)
{
  int x = widget->x;

  /* XXX we really need to identify top level window
   *     this assummes its positioned at 0,0 - but really 
   *     it could be anywhere and need to account for this. 
  */

  while ((widget = widget->parent) != NULL)
    x += widget->x;

  return x;
}

int
cltr_widget_abs_y(CltrWidget *widget)
{
  int y = widget->y;

  while ((widget = widget->parent) != NULL)
    y += widget->y;

  return y;
}

int
cltr_widget_abs_x2(CltrWidget *widget)
{
  return cltr_widget_abs_x(widget) + cltr_widget_width(widget);
}

int
cltr_widget_abs_y2(CltrWidget *widget)
{
  return cltr_widget_abs_y(widget) + cltr_widget_height(widget);
}


int
cltr_widget_width(CltrWidget *widget)
{
  return widget->width;
}

int
cltr_widget_height(CltrWidget *widget)
{
  return widget->height;
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
cltr_widget_unref(CltrWidget *widget)
{
  widget->refcnt--;

  if (widget->refcnt < 0 && widget->destroy)
    {
      widget->destroy(widget);
    }
}


/* XXX Focus hacks; 
 * 
 * Should not call directly but via cltr_window_focus_widget()
 *
 * need to sort this out. 
*/
void
cltr_widget_focus(CltrWidget *widget)
{
  if (widget->focus_in)
    {
      widget->focus_in(widget);
    }
}

void
cltr_widget_unfocus(CltrWidget *widget)
{
  if (widget->focus_out)
    {
      widget->focus_out(widget);
    }
}

void
cltr_widget_set_focus_next(CltrWidget    *widget,
			   CltrWidget    *widget_to_focus,
			   CltrDirection  direction)
{
  switch (direction)
    {
    case CLTR_NORTH:
      widget->focus_next_north = widget_to_focus;
      break;
    case CLTR_SOUTH:
      widget->focus_next_south = widget_to_focus;
      break;
    case CLTR_EAST:
      widget->focus_next_east = widget_to_focus;
      break;
    case CLTR_WEST:
      widget->focus_next_west = widget_to_focus;
      break;
    }
}

CltrWidget*
cltr_widget_get_focus_next(CltrWidget    *widget,
			   CltrDirection  direction)
{
  switch (direction)
    {
    case CLTR_NORTH:
      return widget->focus_next_north;
    case CLTR_SOUTH:
      return widget->focus_next_south;
    case CLTR_EAST:
      return widget->focus_next_east;
    case CLTR_WEST:
      return widget->focus_next_west;
    }

  return NULL;
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

	  cltr_widget_show_all(child);
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
cltr_widget_remove_child(CltrWidget *widget, CltrWidget *child)
{
  widget->children = g_list_remove(widget->children, child);

  child->parent = NULL;
  child->x      = 0;
  child->y      = 0;
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

  g_async_queue_push (ctx->internal_event_q, (gpointer)widget);
}

gboolean
cltr_widget_handle_xevent(CltrWidget *widget, XEvent *xev)
{
  if (!widget->visible)
    return FALSE;

  if (widget && widget->xevent_handler)
    return widget->xevent_handler(widget, xev);

  return FALSE;
}
