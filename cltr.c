#include "cltr.h"

float Zoom = 0.1;

typedef struct ClutterMainContext ClutterMainContext;

struct ClutterMainContext
{
  Display    *xdpy;
  Window      xwin_root;
  int         xscreen;
  GC          xgc;
  GLXContext  gl_context;
};

typedef struct ClutterWindow ClutterWindow;

struct ClutterWindow
{
  Window xwin;
  int    width;
  int    height;
};

ClutterMainContext CltrCntx;

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
  XEvent xev;

    for (;;) 
    {
      XNextEvent(CltrCntx.xdpy, &xev);
    }
}

/* xxxxxxxxxxxxx */

typedef struct ClutterPhotoGrid ClutterPhotoGrid;

typedef struct ClutterPhotoGridCell ClutterPhotoGridCell;

typedef enum ClutterPhotoGridState
{
  CLTR_PHOTO_GRID_STATE_BROWSE,
  CLTR_PHOTO_GRID_STATE_ZOOM_IN,
  CLTR_PHOTO_GRID_STATE_ZOOMED,
  CLTR_PHOTO_GRID_STATE_ZOOM_OUT,
} 
ClutterPhotoGridState;

struct ClutterPhotoGrid
{
  /* XXX should be base widget stuff  */
  int            x,y;
  int            width;
  int            height;
  ClutterWindow *parent;

  /* ****** */

  int            n_rows;
  int            n_cols;

  int            cell_width;
  int            cell_height;

  GList         *cells_tail;

  int            tex_w;
  int            tex_h;
  int           *tex_data;
  GLuint        *texs;

  ClutterPhotoGridState  state;
};

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
	  cltr_photo_grid_append_cell(grid, pixb, entry);
	  n_pixb++;
	}

      g_free(fullpath);
    }

  g_dir_close (dir);

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

      glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
		       (GLsizei)tpixb->width,
		       (GLsizei)tpixb->height,
		       GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
		       tpixb->data);

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

  glClearColor( 0.0, 0.0, 0.0, 1.0);
  glClear    (GL_COLOR_BUFFER_BIT);

  /*

  glTranslatef( 0.0, 0.0, 0.1);
  */

  glLoadIdentity ();
  glScalef( Zoom, Zoom, Zoom);

  Zoom += 0.01;

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

	  ew_border = thumb_w/4;
	  ns_border = thumb_h/4; 

	  thumb_w -= (2 * ew_border);
	  thumb_h -= (2 * ns_border);

	  x1 = x + ew_border;
	  y1 = y + ns_border;

	  x2 = x1 + thumb_w; 
	  y2 = y1 + thumb_h;

	  glBindTexture(GL_TEXTURE_2D, grid->texs[i]);

	  /*
	  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
			   (GLsizei)pixb->width,
			   (GLsizei)pixb->height,
			   GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
			   pixb->data);
	  */
	  tx = (float) pixb->width  / grid->tex_w;
	  ty = (float) pixb->height / grid->tex_h;

	  glBegin (GL_QUADS);
	  glTexCoord2f (tx, ty);   glVertex2i   (x2, y2);
	  glTexCoord2f (0,  ty);   glVertex2i   (x1, y2);
	  glTexCoord2f (0,  0);    glVertex2i   (x1, y1);
	  glTexCoord2f (tx, 0);    glVertex2i   (x2, y1);
	  glEnd ();	

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

  grid = util_malloc0(sizeof(ClutterPhotoGrid));

  grid->width  = win->width;
  grid->height = win->height;
  grid->n_cols = n_cols;
  grid->n_rows = n_rows;
  grid->parent = win;

  grid->cell_width  = grid->width  / n_cols;
  grid->cell_height = grid->height / n_rows;

  /* Below needs to go else where */

  grid->tex_w    = 1024;   
  grid->tex_h    = 512;

  grid->tex_data = malloc (grid->tex_w * grid->tex_h * 4);

  /*
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
		grid->tex_w, grid->tex_h,
		0, GL_RGBA, GL_UNSIGNED_BYTE, grid->tex_data);
  */

  /* Load  */
  cltr_photo_grid_populate(grid, imgs_path);

  cltr_photo_grid_redraw(grid);

  return grid;
}

static Bool
get_xevent_timed(Display        *dpy, 
		 XEvent         *event_return, 
		 struct timeval *tv)
{
  if (tv->tv_usec == 0 && tv->tv_sec == 0)
    {
      XNextEvent(dpy, event_return);
      return True;
    }

  XFlush(dpy);

  if (XPending(dpy) == 0) 
    {
      int fd = ConnectionNumber(dpy);
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(fd, &readset);

      if (select(fd+1, &readset, NULL, NULL, tv) == 0) 
	return False;
      else {
	XNextEvent(dpy, event_return);
	return True;
      }

    } else {
      XNextEvent(dpy, event_return);
      return True;
    }
}


int
main(int argc, char **argv)
{
  ClutterPhotoGrid *grid = NULL;
  ClutterWindow    *win = NULL;

  cltr_init(&argc, &argv);

  win = cltr_window_new(640, 480);

  grid = cltr_photo_grid_new(win, 3, 3, argv[1]);

  cltr_photo_grid_redraw(grid);

  XFlush(CltrCntx.xdpy);

  XMapWindow(CltrCntx.xdpy, grid->parent->xwin);

  {
    for (;;) 
      {
	cltr_photo_grid_redraw(grid);
	XFlush(CltrCntx.xdpy);
      }
  }

  return 0;
}
