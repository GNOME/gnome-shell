#include "cltr.h"

#include <X11/keysym.h>

/* temp temp temp */

float Zoom = 1.0;
ClutterPhotoGrid *Grid = NULL;

/* ************* */

static gboolean  
x_event_prepare (GSource  *source,
		 gint     *timeout)
{
  Display *display = ((CltrXEventSource*)source)->display;

  *timeout = -1;

  return XPending (display);
}

static gboolean  
x_event_check (GSource *source) 
{
  CltrXEventSource *display_source = (CltrXEventSource*)source;
  gboolean         retval;

  if (display_source->event_poll_fd.revents & G_IO_IN)
    retval = XPending (display_source->display);
  else
    retval = FALSE;

  return retval;
}

static gboolean  
x_event_dispatch (GSource    *source,
		  GSourceFunc callback,
		  gpointer    user_data)
{
  Display *display = ((CltrXEventSource*)source)->display;
  CltrXEventFunc event_func = (CltrXEventFunc) callback;
  
  XEvent xev;

  if (XPending (display))
    {
      XNextEvent (display, &xev);

      if (event_func)
	(*event_func) (&xev, user_data);
    }

  return TRUE;
}

static const GSourceFuncs x_event_funcs = {
  x_event_prepare,
  x_event_check,
  x_event_dispatch,
  NULL
};

void
cltr_dispatch_keypress(XKeyEvent *xkeyev)
{
  KeySym kc;

  kc = XKeycodeToKeysym(xkeyev->display, xkeyev->keycode, 0);

  switch (kc)
    {
    case XK_Left:
    case XK_KP_Left:
      cltr_photo_grid_navigate(Grid, CLTR_WEST);
      break;
    case XK_Up:
    case XK_KP_Up:
      cltr_photo_grid_navigate(Grid, CLTR_NORTH);
      break;
    case XK_Right:
    case XK_KP_Right:
      cltr_photo_grid_navigate(Grid, CLTR_EAST);
      break;
    case XK_Down:	
    case XK_KP_Down:	
      cltr_photo_grid_navigate(Grid, CLTR_SOUTH);
      break;
    case XK_Return:
      cltr_photo_grid_activate_cell(Grid);
      break;
    default:
      CLTR_DBG("unhandled keysym");
    }
}

static void
cltr_dispatch_x_event (XEvent  *xevent,
		       gpointer data)
{
  /* Should actually forward on to focussed widget */

  switch (xevent->type)
    {
    case MapNotify:
      CLTR_DBG("Map Notify Event");
      break;
    case Expose:
      CLTR_DBG("Expose");
      break;
    case KeyPress:
      CLTR_DBG("KeyPress");
      cltr_dispatch_keypress(&xevent->xkey);
      break;
    }
}

int
cltr_init(int *argc, char ***argv)
{
  int  gl_attributes[] =
    {
      GLX_RGBA, 
      GLX_DOUBLEBUFFER,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      0
    };

  XVisualInfo	       *vinfo;  

  GMainContext         *gmain_context;
  int                   connection_number;
  GSource              *source;
  CltrXEventSource     *display_source;

  /* Not just yet ..
  g_thread_init (NULL);
  XInitThreads ();
  */

  if ((CltrCntx.xdpy = XOpenDisplay(getenv("DISPLAY"))) == NULL)
    {
      return 0;
    }

  CltrCntx.xscreen   = DefaultScreen(CltrCntx.xdpy);
  CltrCntx.xwin_root = RootWindow(CltrCntx.xdpy, CltrCntx.xscreen);

  if ((vinfo = glXChooseVisual(CltrCntx.xdpy, 
			       CltrCntx.xscreen,
			       gl_attributes)) == NULL)
    {
      fprintf(stderr, "Unable to find visual\n");
      return 0;
    }

  CltrCntx.gl_context = glXCreateContext(CltrCntx.xdpy, vinfo, 0, True);

  /* g_main loop stuff */

  gmain_context = g_main_context_default ();

  g_main_context_ref (gmain_context);

  connection_number = ConnectionNumber (CltrCntx.xdpy);
  
  source = g_source_new ((GSourceFuncs *)&x_event_funcs, 
			 sizeof (CltrXEventSource));

  display_source = (CltrXEventSource *)source;

  display_source->event_poll_fd.fd     = connection_number;
  display_source->event_poll_fd.events = G_IO_IN;
  display_source->display              = CltrCntx.xdpy;
  
  g_source_add_poll (source, &display_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, 
			 (GSourceFunc) cltr_dispatch_x_event, 
			 NULL  /* no userdata */, NULL);

  g_source_attach (source, gmain_context);
  g_source_unref (source);

  return 1;
}


ClutterWindow*
cltr_window_new(int width, int height)
{
  ClutterWindow *win;

  win = util_malloc0(sizeof(ClutterWindow));

  win->width  = width;
  win->height = height;

  win->xwin = XCreateSimpleWindow(CltrCntx.xdpy,
				  CltrCntx.xwin_root,
				  0, 0,
				  width, height,
				  0, 0, WhitePixel(CltrCntx.xdpy, 
						   CltrCntx.xscreen));

  XSelectInput(CltrCntx.xdpy, win->xwin, 
	       StructureNotifyMask|ExposureMask|
	       KeyPressMask|PropertyChangeMask);

  glXMakeCurrent(CltrCntx.xdpy, win->xwin, CltrCntx.gl_context);

  glViewport (0, 0, width, height);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glClearColor (0xff, 0xff, 0xff, 0xff);
  glClearDepth (1.0f);

  glDisable    (GL_DEPTH_TEST);
  glDepthMask  (GL_FALSE);
  glDisable    (GL_CULL_FACE);
  glShadeModel (GL_FLAT);
  glHint       (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glLoadIdentity ();
  glOrtho (0, width, height, 0, -1, 1);

  glMatrixMode (GL_PROJECTION);

  /* likely better somewhere elese */

  glEnable        (GL_TEXTURE_2D);
  glTexParameteri (GL_TEXTURE_2D,  GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D,  GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexEnvi       (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,   GL_REPLACE);

  return win;
}

void
cltr_main_loop()
{
  GMainLoop *loop;

  loop = g_main_loop_new (g_main_context_default (), FALSE);

  CLTR_MARK();

  g_main_loop_run (loop);

}


void
cltr_photo_grid_append_cell(ClutterPhotoGrid *grid,
			    Pixbuf           *pixb,
			    const gchar      *filename)
{
  int neww = 0, newh = 0;

  int maxw = grid->width, maxh = grid->height;

  Pixbuf *pixb_scaled = NULL;

  if (pixb->width > pixb->height) /* landscape */
    {
      if (pixb->width > maxw)
	{
	  neww = maxw;
	  newh = (neww * pixb->height) / pixb->width;
	}
    }
  else                            /* portrait */
    {
      if (pixb->height > maxh)
	{
	  newh = maxh;
	  neww = (newh * pixb->width) / pixb->height;
	}
    }

  if (neww || newh)
    {
      pixb_scaled = pixbuf_scale_down(pixb, neww, newh);
      pixbuf_unref(pixb);
    }
  else pixb_scaled = pixb;

  grid->cells_tail = g_list_append(grid->cells_tail, pixb_scaled);

  CLTR_DBG ("loaded %s at %ix%i", filename, neww, newh);
} 

static void
ctrl_photo_grid_cell_to_coords(ClutterPhotoGrid *grid,
			       GList            *cell,
			       int              *x,
			       int              *y)
{
  int idx;

  idx = g_list_position(grid->cells_tail, cell);

  *y = idx / grid->n_cols;
  *x = idx % grid->n_cols;

  CLTR_DBG("idx: %i x: %i, y: %i", idx, *x , *y);
}

static void
ctrl_photo_grid_get_trans_coords(ClutterPhotoGrid *grid,
				 int              x,
				 int              y,
				 float           *tx,
				 float           *ty)
{
  int max_x = grid->n_cols-1;

  /* XXX figure out what magic 2.0 value comes from */

  /*
  *tx = (float)( (max_x-x) - 1.0 ) * 2.0;
  *ty = (float)( y - 1.0 ) * 2.0;
  */

  /* 
   3x3 0,0 -> 2,-2

   5x5 0,0 ->
  *tx = 4.0;
  *ty = -4.0;

  4x4 0,0 -> 3.0, -3.0
  */

  /*
  float trange = ((float)(grid->n_cols - 1.0) * 2.0);
  float xrange = (float)grid->n_cols - 1.0;
  */

  /*
  *tx = ((max_x-x) * (trange/xrange)) - (trange/2);
  *ty = (y * (trange/xrange)) - (trange/2);
  */

  /* assumes rows = cols */

  *tx = ((max_x-x) * 2.0) - (grid->n_cols - 1.0);
  *ty = (y * 2.0) - (grid->n_cols - 1.0);
}

void
cltr_photo_grid_navigate(ClutterPhotoGrid *grid,
			 CltrDirection     direction) 
{
  GList *cell_orig = grid->cell_active;

  switch (direction)
    {
    case CLTR_SOUTH:
      if (g_list_nth(grid->cell_active, grid->n_cols))
	grid->cell_active = g_list_nth(grid->cell_active, grid->n_cols);
      break;
    case CLTR_NORTH:
      if (g_list_nth_prev(grid->cell_active, grid->n_cols))
	grid->cell_active = g_list_nth_prev(grid->cell_active, grid->n_cols);
      break;
    case CLTR_EAST:
      if (g_list_next(grid->cell_active))
	grid->cell_active = g_list_next(grid->cell_active);
      break;
    case CLTR_WEST:
      if (g_list_previous(grid->cell_active))
	grid->cell_active = g_list_previous(grid->cell_active);
      break;
    }

  if (cell_orig != grid->cell_active) /* we've moved */
    {
      int   x, y;

      if (grid->state == CLTR_PHOTO_GRID_STATE_ZOOMED)
	{
	  grid->state      = CLTR_PHOTO_GRID_STATE_ZOOMED_MOVE;
	  grid->view_min_x = grid->view_max_x; 
	  grid->view_min_y = grid->view_max_y ;
	  grid->anim_step  = 0;
	}
	  
      ctrl_photo_grid_cell_to_coords(grid, grid->cell_active, &x, &y);

      ctrl_photo_grid_get_trans_coords(grid, x, y, 
				       &grid->view_max_x,
				       &grid->view_max_y);
				       
      CLTR_DBG("x: %f, y: %f", grid->view_max_x , grid->view_max_y);
    }
}

void 				/* bleh badly named */
cltr_photo_grid_activate_cell(ClutterPhotoGrid *grid)
{
  if (grid->state == CLTR_PHOTO_GRID_STATE_BROWSE)
    {
      grid->state = CLTR_PHOTO_GRID_STATE_ZOOM_IN;
    }
  else if (grid->state == CLTR_PHOTO_GRID_STATE_ZOOMED)
    {
      grid->state = CLTR_PHOTO_GRID_STATE_ZOOM_OUT;
	/* reset - zoomed moving will have reset */
      grid->view_min_x = 0.0; 
      grid->view_min_y = 0.0;
    }

  /* que a draw ? */
}			      


void
cltr_photo_grid_populate(ClutterPhotoGrid *grid,
			 const char       *imgs_path) 
{
  GDir        *dir;
  GError      *error;
  const gchar *entry = NULL;
  gchar       *fullpath = NULL;
  int          n_pixb = 0, i =0;
  GList       *cell;
  ClutterFont *font = NULL;

  font = font_new("Sans Bold 96");

  if ((dir = g_dir_open (imgs_path, 0, &error)) == NULL)
    {
      /* handle this much better */
      fprintf(stderr, "failed to open '%s'\n", imgs_path);
      return;
    }

  while((entry = g_dir_read_name (dir)) != NULL)
    {
      Pixbuf *pixb = NULL; 
      fullpath = g_strconcat(imgs_path, "/", entry, NULL);

      pixb = pixbuf_new_from_file(fullpath);

      if (pixb)
	{
	  gchar buf[24];

	  g_snprintf(&buf[0], 24, "%i", n_pixb);
	  font_draw(font, pixb, buf, 10, 10);

	  cltr_photo_grid_append_cell(grid, pixb, entry);

	  n_pixb++;
	}

      g_free(fullpath);
    }

  g_dir_close (dir);

  grid->cell_active = g_list_first(grid->cells_tail);

  /* Set up textures */

  grid->texs = util_malloc0(sizeof(GLuint)*n_pixb);
  glGenTextures(n_pixb, grid->texs);

  cell = g_list_first(grid->cells_tail);

  do 
    {
      Pixbuf *tpixb = (Pixbuf *)cell->data;

      glBindTexture(GL_TEXTURE_2D, grid->texs[i]);

      CLTR_DBG("loading texture %i", i);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexEnvi      (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,   GL_REPLACE);

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
		   grid->tex_w,
		   grid->tex_h,
		   0, GL_RGBA, 
		   GL_UNSIGNED_INT_8_8_8_8,
		   grid->tex_data);

      CLTR_GLERR();

      glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
		       (GLsizei)tpixb->width,
		       (GLsizei)tpixb->height,
		       GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
		       tpixb->data);

      CLTR_GLERR();

      i++;
    } 
  while ( (cell = g_list_next(cell)) != NULL );

  glEnable        (GL_TEXTURE_2D);

}

void
cltr_photo_grid_redraw(ClutterPhotoGrid *grid)
{
  int x = 0, y = 0, rows = grid->n_rows, cols = 0, i =0;
  GList *cell;
  float zoom, trans_x, trans_y;

  glLoadIdentity (); 		/* XXX pushmatrix */

  glClearColor( 0.6, 0.6, 0.62, 1.0);
  glClear    (GL_COLOR_BUFFER_BIT);

  /*

  glTranslatef( 0.0, 0.0, 0.1);
  */

  /* Assume zoomed out */
  zoom    = grid->zoom_min;
  trans_x = grid->view_min_x;
  trans_y = grid->view_min_y;

  if (grid->state != CLTR_PHOTO_GRID_STATE_BROWSE)
    {
      /* Assume zoomed in */
      zoom    = grid->zoom_max; 
      trans_x = grid->view_max_x;
      trans_y = grid->view_max_y;

      if (grid->state == CLTR_PHOTO_GRID_STATE_ZOOM_IN)
	{
	  grid->anim_step++;

	  /* Are we zoomed all the way in > */
	  if (grid->anim_step >= grid->anim_n_steps)
	    {
	      grid->state     = CLTR_PHOTO_GRID_STATE_ZOOMED;
	      grid->anim_step = 0;
	      /* zoom            = grid->zoom_max; set above */
	    }
	  else 
	    {
	      float f = (float)grid->anim_step/grid->anim_n_steps;

	      zoom = grid->zoom_min + ((grid->zoom_max - grid->zoom_min) * f);
	      trans_x = (grid->view_max_x - grid->view_min_x) * f;
	      trans_y = (grid->view_max_y - grid->view_min_y) * f;
	    }
	
	} 
      else if (grid->state == CLTR_PHOTO_GRID_STATE_ZOOM_OUT)
	{
	  grid->anim_step++;
	  
	  if (grid->anim_step >= grid->anim_n_steps)
	    {
	      zoom            = grid->zoom_min;
	      grid->anim_step = 0;
	      grid->state     = CLTR_PHOTO_GRID_STATE_BROWSE;
	      
	    }
	  else 
	    {
	      float f = (float)(grid->anim_n_steps - grid->anim_step ) 
		        / grid->anim_n_steps;

	      zoom = grid->zoom_min + (grid->zoom_max - grid->zoom_min) * f;
	      trans_x = (grid->view_max_x - grid->view_min_x) * f;
	      trans_y = (grid->view_max_y - grid->view_min_y) * f;

#if 0
	      zoom = grid->zoom_min
		+ (( (grid->zoom_max - grid->zoom_min) / grid->anim_n_steps ) 
                     * (grid->anim_n_steps - grid->anim_step) );
#endif
	    }
	}
      else if (grid->state == CLTR_PHOTO_GRID_STATE_ZOOMED_MOVE)
	{
	  grid->anim_step++;

	  if (grid->anim_step >= grid->anim_n_steps)
	    {
	      grid->state     = CLTR_PHOTO_GRID_STATE_ZOOMED;
	      grid->anim_step = 0;
	    }
	  else
	    {
	      float f = (float)grid->anim_step/grid->anim_n_steps;

	      trans_x = grid->view_min_x + ((grid->view_max_x - grid->view_min_x) * f);
	      trans_y = grid->view_min_y + ((grid->view_max_y - grid->view_min_y) * f);

	    }
	}

    glTranslatef( trans_x, trans_y, 0.0);

    glScalef( zoom, zoom, zoom);

    }
  

  cell = g_list_first(grid->cells_tail);

  while (rows--)
    {
      cols = grid->n_cols;
      x = 0; 
      while (cols--)
	{
	  Pixbuf *pixb = NULL;
	  float   tx, ty;
	  int     x1, x2, y1, y2, thumb_w, thumb_h;
	  int     ns_border, ew_border;

	  pixb = (Pixbuf *)cell->data;

	  thumb_w = (pixb->width  / grid->n_cols);
	  thumb_h = (pixb->height / grid->n_rows);

	  ew_border = thumb_w/8;
	  ns_border = thumb_h/8; 

	  thumb_w -= (2 * ew_border);
	  thumb_h -= (2 * ns_border);

	  x1 = x + ew_border;
	  y1 = y + ns_border;

	  x2 = x1 + thumb_w; 
	  y2 = y1 + thumb_h;

	  tx = (float) pixb->width  / grid->tex_w;
	  ty = (float) pixb->height / grid->tex_h;

	  glBindTexture(GL_TEXTURE_2D, grid->texs[i]);

	  glBegin (GL_QUADS);
	  glTexCoord2f (tx, ty);   glVertex2i   (x2, y2);
	  glTexCoord2f (0,  ty);   glVertex2i   (x1, y2);
	  glTexCoord2f (0,  0);    glVertex2i   (x1, y1);
	  glTexCoord2f (tx, 0);    glVertex2i   (x2, y1);
	  glEnd ();	

	  if (cell == grid->cell_active 
	      && grid->state == CLTR_PHOTO_GRID_STATE_BROWSE)
	    {
	      glBegin (GL_QUADS);
	      glTexCoord2f (tx, ty);   glVertex2i   (x2+5, y2+5);
	      glTexCoord2f (0,  ty);   glVertex2i   (x1-5, y2+5);
	      glTexCoord2f (0,  0);    glVertex2i   (x1-5, y1-5);
	      glTexCoord2f (tx, 0);    glVertex2i   (x2+5, y1-5);
	      glEnd ();	
	    }
	  else
	    {
	      glBegin (GL_QUADS);
	      glTexCoord2f (tx, ty);   glVertex2i   (x2, y2);
	      glTexCoord2f (0,  ty);   glVertex2i   (x1, y2);
	      glTexCoord2f (0,  0);    glVertex2i   (x1, y1);
	      glTexCoord2f (tx, 0);    glVertex2i   (x2, y1);
	      glEnd ();	
	    }

	  cell = g_list_next(cell);

	  if (!cell)
	    goto finish;

	  x += grid->cell_width;
	  i++;
	}
      y += grid->cell_height;
    }

 finish:
  glXSwapBuffers(CltrCntx.xdpy, grid->parent->xwin);  

}

ClutterPhotoGrid*
cltr_photo_grid_new(ClutterWindow *win, 
		    int            n_cols,
		    int            n_rows,
		    const char    *imgs_path)
{
  ClutterPhotoGrid *grid = NULL;
  int               x,y;

  grid = util_malloc0(sizeof(ClutterPhotoGrid));

  grid->width  = win->width;
  grid->height = win->height;
  grid->n_cols = n_cols;
  grid->n_rows = n_rows;
  grid->parent = win;

  grid->cell_width  = grid->width  / n_cols;
  grid->cell_height = grid->height / n_rows;

  grid->state = CLTR_PHOTO_GRID_STATE_BROWSE;

  grid->anim_n_steps = 50; /* value needs to be calced dep on rows */
  grid->anim_step    = 0;

  /* 
     grid->zoom_step = 0.05;
     grid->zoom      = 1.0;
  */
  grid->zoom_min  = 1.0;		      
  grid->view_min_x = 0.0;
  grid->view_min_y = 0.0;


  /* Assmes cols == rows */
  grid->zoom_max  = /* 1.0 + */  (float) (n_rows * 1.0) ;


  /* Below needs to go else where - some kind of texture manager/helper */

  grid->tex_w    = 1024;   
  grid->tex_h    = 512;

  grid->tex_data = malloc (grid->tex_w * grid->tex_h * 4);

  /* Load  */
  cltr_photo_grid_populate(grid, imgs_path);

  grid->cell_active = g_list_first(grid->cells_tail);

  ctrl_photo_grid_cell_to_coords(grid, grid->cell_active, &x, &y);

  ctrl_photo_grid_get_trans_coords(grid, 
				   x, y, 
				   &grid->view_max_x,
				   &grid->view_max_y);
  cltr_photo_grid_redraw(grid);

  return grid;
}

gboolean
idle_cb(gpointer data)
{
  ClutterPhotoGrid *grid = (ClutterPhotoGrid *)data;

  /*
  if (grid->state != CLTR_PHOTO_GRID_STATE_BROWSE
      && grid->state != CLTR_PHOTO_GRID_STATE_ZOOMED)
  */
  cltr_photo_grid_redraw(grid);

  return TRUE;
}

int
main(int argc, char **argv)
{
  ClutterPhotoGrid *grid = NULL;
  ClutterWindow    *win = NULL;

  cltr_init(&argc, &argv);

  win = cltr_window_new(640, 480);

  grid = cltr_photo_grid_new(win, 4, 4, argv[1]);

  Grid = grid; 			/* laaaaaazy globals */

  cltr_photo_grid_redraw(grid);

  g_idle_add(idle_cb, grid);

  XFlush(CltrCntx.xdpy);

  XMapWindow(CltrCntx.xdpy, grid->parent->xwin);

  XFlush(CltrCntx.xdpy);

  cltr_main_loop();

  /*
  {
    for (;;) 
      {
	cltr_photo_grid_redraw(grid);
	XFlush(CltrCntx.xdpy);
      }
  }
  */

  return 0;
}
