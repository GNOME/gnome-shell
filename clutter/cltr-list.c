#include "cltr-list.h"
#include "cltr-private.h"

#define ANIM_FPS 50 
#define FPS_TO_TIMEOUT(t) (1000/(t))

struct CltrListCell
{
  CltrRect     rect;

  Pixbuf      *thumb_pixb;
  CltrTexture *thumb_texture;

  Pixbuf      *text_pixb;
  CltrTexture *text_texture;

};

struct CltrList
{
  CltrWidget    widget;  
  GList        *cells, *active_cell;
  int           active_cell_y;
  int           cell_height;
  int           cell_width;

  int           n_cells;

  CltrListCellActivate cell_activate_cb;
  gpointer             cell_activate_data;

  CltrListState state;
  int           scroll_dir;

};

#define PAD 10

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
  return ( exp( (float)d/maxdist * 0.8 ) / exp(0.8) ) ;
}

CltrListCell*
cltr_list_cell_new(CltrList *list, 
		   Pixbuf   *thumb_pixb, 
		   char     *text)
{
  CltrListCell *cell = NULL;
  ClutterFont  *font;
  PixbufPixel   pixel = { 0, 0, 0, 0 }, font_pixel = { 255, 255, 255, 255};

  font = font_new ("Sans Bold 24");

  cell = g_malloc0(sizeof(CltrListCell));

  cell->thumb_pixb = thumb_pixb;
  pixbuf_ref(cell->thumb_pixb);
  cell->thumb_texture = cltr_texture_new(cell->thumb_pixb);

  cell->text_pixb = pixbuf_new(list->cell_width - (list->cell_width/4), 
			       (list->cell_height/2) - (2*PAD));

  pixbuf_fill_rect(cell->text_pixb, 0, 0, -1, -1, &pixel);
  font_draw(font, cell->text_pixb, text, 0, 0, &font_pixel);
  cell->text_texture = cltr_texture_new(cell->text_pixb);

  return cell;
}

void
cltr_list_cell_set_pixbuf(CltrListCell *cell,
			  Pixbuf       *thumb_pixb)
{
  cltr_texture_unref(cell->thumb_texture); 

  cell->thumb_pixb = thumb_pixb;

  cell->thumb_texture = cltr_texture_new(cell->thumb_pixb);

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

void
cltr_list_append_cell(CltrList *list, CltrListCell *cell)
{
  list->cells = g_list_append(list->cells, cell);

  if (!list->active_cell)
    list->active_cell = g_list_first(list->cells);

  list->n_cells++;
}

static void
video_box_co_ords(CltrList    *list,
		  CltrListCell *cell, 
		  int          *x1, int *y1, int *x2, int *y2)
{
  CltrWidget *widget = CLTR_WIDGET(list);
  int         vw, vh;

  vh  = cltr_rect_y2(cell->rect) - cltr_rect_y1(cell->rect);
  vh -= (PAD*2);
  vw  = ( widget->width * vh ) / widget->height;
  
  *x1 = cltr_rect_x1(cell->rect) + PAD; *x2 = *x1 + vw;
  *y1 = cltr_rect_y1(cell->rect) + PAD; *y2 = *y1 + vh;
}

/* 
 * This is messy hack as cells arn't real widgets :(
 *
 */
gboolean
cltr_list_get_active_cell_video_box_co_ords(CltrList *list,
					    int      *x1,
					    int      *y1,
					    int      *x2,
					    int      *y2)
{
  if (list->active_cell)
    {
      CltrListCell *cell = list->active_cell->data; 

      video_box_co_ords(list, cell, x1, y1, x2, y2);

      return TRUE;
    }
  return FALSE;
}

static void
cltr_list_show(CltrWidget *widget)
{
  CltrList     *list = CLTR_LIST(widget);
  CltrListCell *cell = NULL;
  
  if (list->active_cell_y == 0)
    {
      list->active_cell_y = (widget->height / 2) - (list->cell_height/2);

      list->active_cell = g_list_first(list->cells);

      cell = list->active_cell->data;

      cell->rect.y = list->active_cell_y;
    }

  list->state = CLTR_LIST_STATE_BROWSE;

  cltr_list_update_layout(list);

  cltr_widget_queue_paint(widget);
}

void
cltr_list_on_activate_cell(CltrList             *list,
			   CltrListCellActivate  callback,
			   gpointer             *userdata)
{
  list->cell_activate_cb      = callback;
  list->cell_activate_data    = userdata;
}

CltrListCell*
cltr_list_get_active_cell(CltrList *list)
{
  if (list->active_cell)
    return list->active_cell->data;

  return NULL;
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
	    if (list->cell_activate_cb && list->active_cell)
	      list->cell_activate_cb(list, 
				     list->active_cell->data,
				     list->cell_activate_data);
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
  int i = 0;

  for (;;)
    {
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
	      cell_top->rect.y += 1;
	    }
	  else
	    {
	      list->active_cell = cell_item;
	      list->state = CLTR_LIST_STATE_BROWSE;
	      return;
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
	      cell_top->rect.y -= 1;
	    }
	  else
	    {
	      list->active_cell = cell_item;
	      list->state = CLTR_LIST_STATE_BROWSE;
	      return;
	    }
	}

      if (++i > 10)
	return;

      cltr_list_update_layout(list);
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
cltr_list_update_layout(CltrList *list)
{
  GList        *cell_item = NULL;
  CltrListCell *cell = NULL;
  int           last;

  cell_item = g_list_first(list->cells);
  cell = (CltrListCell *)cell_item->data;

  last = cell->rect.y;

  while (cell_item)
    {
      float scale = 0.0;

      cell = (CltrListCell *)cell_item->data;

      cell->rect.y = last;
       
      if (cell->rect.y + cell->rect.height >= 0)
	{
	  scale = distfunc(list, cell->rect.y - list->active_cell_y);

	  cell->rect.width  = list->cell_width * scale;
	  cell->rect.height = list->cell_height * scale;
	  
	  cell->rect.x = (list->widget.width - cell->rect.width) / 2;  
	}
      
      last = cell->rect.y + cell->rect.height;

      cell_item = g_list_next(cell_item);
    }

}

static void
cltr_list_paint(CltrWidget *widget)
{
  GList        *cell_item = NULL;
  CltrList     *list = CLTR_LIST(widget);
  CltrListCell *cell = NULL;
  int           last;

  PixbufPixel col       = { 0xff, 0, 0, 0xff };
  PixbufPixel bgcol     = { 0xe7, 0xe7, 0xe7, 0xff };
  PixbufPixel boxcol    = { 0xd7, 0xd7, 0xd7, 0xff };
  PixbufPixel hlfontcol = { 0xff, 0x33, 0x66, 0xff };

  CLTR_MARK();

  cell_item = g_list_first(list->cells);
  cell = (CltrListCell *)cell_item->data;
  last = cell->rect.y;

  glPushMatrix();

  cltr_glu_set_color(&bgcol);

  glRecti(0, 0, widget->width, widget->height);

  glEnable(GL_TEXTURE_2D);

  glEnable(GL_BLEND);

  cltr_list_update_layout(list);

  while (cell_item)
    {
      float scale = 0.0;

      col.r = 0xff; col.g = 0;  col.b = 0; col.a = 0xff;

      cell = (CltrListCell *)cell_item->data;

      last = cell->rect.y + cell->rect.height;
      
      scale = distfunc(list, cell->rect.y - list->active_cell_y);

      if (last > 0 && cell->rect.y < list->widget.width) /* crappy clip */
	{
	  glDisable(GL_TEXTURE_2D);

	  if (cell_item == list->active_cell && list->state == CLTR_LIST_STATE_BROWSE)
	    col.b = 0xff;
	  else
	    col.b = 0x00;

	  cltr_glu_set_color(&boxcol);

	  cltr_glu_rounded_rect_filled(cltr_rect_x1(cell->rect),
				       cltr_rect_y1(cell->rect) + (5.0 * scale),
				       cltr_rect_x2(cell->rect),
				       cltr_rect_y2(cell->rect) - (5.0 * scale),
				       10,
				       &boxcol);

	  col.r = 0xff; col.g = 0xff;  col.b = 0xff; col.a = 0xff;

	  /*
	  cltr_glu_rounded_rect(cltr_rect_x1(cell->rect) + 10,
				cltr_rect_y1(cell->rect) + 12,
				cltr_rect_x2(cell->rect) - 10,
				cltr_rect_y2(cell->rect) - 12,
				10,
				&col);
	  */

	  glEnable(GL_TEXTURE_2D);

	  glEnable(GL_BLEND);

	  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	  {
	    /* Video Box */

	    int vx1, vx2, vy1, vy2;

	    video_box_co_ords(list, cell, &vx1, &vy1, &vx2, &vy2);

	    cltr_texture_render_to_gl_quad(cell->thumb_texture,
					   vx1, vy1, vx2, vy2);

	    /* Text */

	    if (cell_item == list->active_cell 
		&& list->state == CLTR_LIST_STATE_BROWSE)
	      cltr_glu_set_color(&hlfontcol);
	    else
	      glColor4f(0.4, 0.4, 0.4, 1.0); 
	    
	    cltr_texture_render_to_gl_quad(cell->text_texture,
					   vx2 + PAD,
					   vy1,
					   cltr_rect_x2(cell->rect) - PAD,
					   vy1 + (list->cell_height/2) - PAD);

	  }

	  glDisable(GL_BLEND);

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
