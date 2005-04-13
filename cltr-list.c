#include "cltr-list.h"
#include "cltr-private.h"

#define ANIM_FPS 200 
#define FPS_TO_TIMEOUT(t) (1000/(t))

typedef struct CltrListCell
{
  CltrRect     rect;
  Pixbuf      *pixb;
  CltrTexture *texture;

} CltrListCell;

struct CltrList
{
  CltrWidget    widget;  
  GList        *cells, *active_cell;
  int           active_cell_y;
  int           cell_height;
  int           cell_width;

  CltrListState state;
  int           scroll_dir;
};

static void
cltr_list_show(CltrWidget *widget);

static gboolean 
cltr_list_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_list_paint(CltrWidget *widget);

static float
distfunc(CltrList *list, int d)
{
  int maxdist = list->widget.height;

  d = (maxdist-ABS(d)) ;
  return ( exp( (float)d/maxdist * 2.0 ) / exp(2.0) );
}

CltrListCell*
cltr_list_cell_new(CltrList *list)
{
  CltrListCell *cell = NULL;
  ClutterFont  *font;
  gchar         buf[24];
  PixbufPixel   pixel = { 255, 20, 20, 255 }, font_pixel = { 255, 255, 255, 200 };

  font = font_new ("Sans Bold 96");

  cell = g_malloc0(sizeof(CltrListCell));

  cell->pixb = pixbuf_new(list->cell_width, list->cell_height);

  pixbuf_fill_rect(cell->pixb, 0, 0, -1, -1, &pixel);

  g_snprintf(&buf[0], 24, "%i %i %i", rand()%10, rand()%10, rand()%10);



  font_draw(font, cell->pixb, buf, 10, 10, &font_pixel);

  cell->texture = cltr_texture_new(cell->pixb);

  return cell;
}

CltrWidget*
cltr_list_new(int width, 
	      int height,
	      int cell_width,
	      int cell_height)
{
  CltrList *list;

  list = g_malloc0(sizeof(CltrList));
  
  list->widget.width          = width;
  list->widget.height         = height;
  
  list->widget.show           = cltr_list_show;
  list->widget.paint          = cltr_list_paint;
  
  list->cell_height = cell_height; /* maximum */
  list->cell_width  = cell_width;  /* maximum */

  list->widget.xevent_handler = cltr_list_handle_xevent;

  return CLTR_WIDGET(list);
}

static void
cltr_list_show(CltrWidget *widget)
{
  CltrList *list = CLTR_LIST(widget);
  
  int n_items = 50, i;

  list->active_cell_y = 100;

  for (i=0; i<n_items; i++)
    {
      list->cells = g_list_append(list->cells, cltr_list_cell_new(list));
    }

  list->active_cell = g_list_first(list->cells);

  cltr_widget_queue_paint(widget);
}

static gboolean 
cltr_list_handle_xevent (CltrWidget *widget, XEvent *xev) 
{
  CltrList *list = CLTR_LIST(widget);

  switch (xev->type)
    {
    case KeyPress:
      {
	KeySym kc;

	kc = XKeycodeToKeysym(xev->xkey.display, xev->xkey.keycode, 0);

	switch (kc)
	  {
	  case XK_Up:
	  case XK_KP_Up:
	    cltr_list_scroll_up(list);
	    break;
	  case XK_Down:	
	  case XK_KP_Down:	
	    cltr_list_scroll_down(list);
	    break;
	  case XK_Return:
	    CLTR_DBG("Return");
	    break;
	  case XK_Left:
	  case XK_KP_Left:
	  case XK_Right:
	  case XK_KP_Right:
	  default:
	    CLTR_DBG("unhandled keysym");
	  }
      }
      break;
    }

  return TRUE;
}

static void
cltr_list_animate(CltrList *list)
{
  GList        *cell_item = NULL;
  CltrListCell *next_active = NULL, *cell_top = NULL;

  cell_top  = (CltrListCell *)g_list_nth_data(list->cells, 0);

  if (list->state == CLTR_LIST_STATE_SCROLL_UP)
    {
      cell_item = g_list_previous(list->active_cell);

      if (!cell_item)
	{
	  list->state = CLTR_LIST_STATE_BROWSE;
	  return;
	}

      next_active = (CltrListCell *)cell_item->data;

      if (next_active->rect.y < list->active_cell_y)
	{
	  cell_top->rect.y += 2;
	}
      else
	{
	  list->active_cell = cell_item;
	  list->state = CLTR_LIST_STATE_BROWSE;
	}
    }
  else if (list->state == CLTR_LIST_STATE_SCROLL_DOWN)
    {
      cell_item = g_list_next(list->active_cell);
      
      if (!cell_item)
	{
	  list->state = CLTR_LIST_STATE_BROWSE;
	  return;
	}

      next_active = (CltrListCell *)cell_item->data;

      if (next_active->rect.y > list->active_cell_y)
	{
	  cell_top->rect.y -= 2;
	}
      else
	{
	  list->active_cell = cell_item;
	  list->state = CLTR_LIST_STATE_BROWSE;
	}
    }
}

gboolean
cltr_list_timeout_cb(gpointer data)
{
  CltrList *list = (CltrList *)data;

  cltr_list_animate(list);

  cltr_widget_queue_paint(CLTR_WIDGET(list));

  switch(list->state)
    {
    case CLTR_LIST_STATE_SCROLL_UP:
    case CLTR_LIST_STATE_SCROLL_DOWN:   
      return TRUE;
    case CLTR_LIST_STATE_LOADING:
    case CLTR_LIST_STATE_LOAD_COMPLETE:
    case CLTR_LIST_STATE_BROWSE:
    default:
      return FALSE;
    }
}

static void
cltr_list_paint(CltrWidget *widget)
{
  GList        *cell_item = NULL;
  CltrList     *list = CLTR_LIST(widget);
  CltrListCell *cell = NULL;

  int       last;

  cell_item = g_list_first(list->cells);
  cell = (CltrListCell *)cell_item->data;
  last = cell->rect.y;

  glPushMatrix();

  glEnable(GL_TEXTURE_2D);

  glEnable(GL_BLEND);

  while (cell_item)
    {
      cell = (CltrListCell *)cell_item->data;

      cell->rect.y = last;
       
      if (cell->rect.y + cell->rect.height >= 0)
	{
	  cell->rect.width  = list->cell_width * distfunc(list, cell->rect.y - list->active_cell_y);
	  cell->rect.height = list->cell_height * distfunc(list, cell->rect.y - list->active_cell_y);
	  
	  /* cell->rect.x = (list->widget.width - cell->rect.width) / 6;  */
	  cell->rect.x = 0;
	}
      
      last = cell->rect.y + cell->rect.height;
      
      if (last > 0 && cell->rect.y < list->widget.width) /* crappy clip */
	{
	  cltr_texture_render_to_gl_quad(cell->texture,
					 cltr_rect_x1(cell->rect),
					 cltr_rect_y1(cell->rect),
					 cltr_rect_x2(cell->rect),
					 cltr_rect_y2(cell->rect));
	}

      cell_item = g_list_next(cell_item);
    }

  glDisable(GL_BLEND);

  glDisable(GL_TEXTURE_2D);

  glPopMatrix();
}


void
cltr_list_scroll_down(CltrList *list)
{
  list->state = CLTR_LIST_STATE_SCROLL_DOWN;

  g_timeout_add(FPS_TO_TIMEOUT(ANIM_FPS), 
		cltr_list_timeout_cb, list);
}

void
cltr_list_scroll_up(CltrList *list)
{
  list->state = CLTR_LIST_STATE_SCROLL_UP;

  g_timeout_add(FPS_TO_TIMEOUT(ANIM_FPS), 
		cltr_list_timeout_cb, list);
}
