#include "cltr-photo-grid.h"

ClutterPhotoGridCell*
cltr_photo_grid_cell_new(ClutterPhotoGrid *grid,
			 Pixbuf           *pixb,
			 const gchar      *filename)
{
  ClutterPhotoGridCell *cell = NULL;
  int                   maxw = grid->width, maxh = grid->height;
  int                   neww = 0, newh = 0;

  cell = g_malloc0(sizeof(ClutterPhotoGridCell));

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
      cell->pixb = pixbuf_scale_down(pixb, neww, newh);
      pixbuf_unref(pixb);
    }
  else cell->pixb = pixb;

  CLTR_DBG ("loaded %s at %ix%i", filename, neww, newh);

  return cell;
}

void
cltr_photo_grid_append_cell(ClutterPhotoGrid     *grid,
			    ClutterPhotoGridCell *cell)
{
  grid->cells_tail = g_list_append(grid->cells_tail, cell);


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
ctrl_photo_grid_get_zoomed_coords(ClutterPhotoGrid *grid,
				  int              x,
				  int              y,
				  float            zoom,
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

  /*
  *tx = ((max_x-x) * (grid->zoom_max/2.0)) - (grid->n_cols - 1.0);
  *ty = (y * (grid->zoom_max/2.0)) - (grid->n_cols - 1.0);
  */
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
      float zoom = grid->zoom_min;

      if (grid->state == CLTR_PHOTO_GRID_STATE_ZOOMED)
	{
	  grid->state      = CLTR_PHOTO_GRID_STATE_ZOOMED_MOVE;
	  grid->view_min_x = grid->view_max_x; 
	  grid->view_min_y = grid->view_max_y ;
	  grid->anim_step  = 0;
	  zoom             = grid->zoom_max;
	}
	  
      ctrl_photo_grid_cell_to_coords(grid, grid->cell_active, &x, &y);

      ctrl_photo_grid_get_zoomed_coords(grid, x, y, zoom, 
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
			 const gchar      *imgs_path) 
{
  GDir        *dir;
  GError      *error;
  const gchar *entry = NULL;
  gchar       *fullpath = NULL;
  int          n_pixb = 0, i =0;
  GList       *cell_item;
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
	  ClutterPhotoGridCell *cell;
	  gchar                 buf[24];

	  cell = cltr_photo_grid_cell_new(grid, pixb, entry);

	  g_snprintf(&buf[0], 24, "%i", n_pixb);
	  font_draw(font, cell->pixb, buf, 10, 10);

	  cltr_photo_grid_append_cell(grid, cell);

	  n_pixb++;
	}

      g_free(fullpath);
    }

  g_dir_close (dir);

  grid->cell_active = g_list_first(grid->cells_tail);

  /* Set up textures */

  grid->texs = util_malloc0(sizeof(GLuint)*n_pixb);
  glGenTextures(n_pixb, grid->texs);

  cell_item = g_list_first(grid->cells_tail);

  do 
    {
      ClutterPhotoGridCell *cell = (ClutterPhotoGridCell *)cell_item->data;

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
		       (GLsizei)cell->pixb->width,
		       (GLsizei)cell->pixb->height,
		       GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
		       cell->pixb->data);

      CLTR_GLERR();

      i++;
    } 
  while ( (cell_item = g_list_next(cell_item)) != NULL );
}

void
cltr_photo_grid_redraw(ClutterPhotoGrid *grid)
{
  int x = 0, y = 0, rows = grid->n_rows, cols = 0, i =0;
  GList *cell_item;
  float zoom, trans_x, trans_y;

  glPushMatrix();

  glClear(GL_COLOR_BUFFER_BIT);

  glClearColor( 0.6, 0.6, 0.62, 1.0);
  glEnable(GL_TEXTURE_2D);

  glDisable(GL_LIGHTING); 
  glDisable(GL_DEPTH_TEST);

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


    }

  glTranslatef( trans_x, trans_y, 0.0);
  glScalef( zoom, zoom, 0.0);

  cell_item = g_list_first(grid->cells_tail);

  while (rows--)
    {
      cols = grid->n_cols;
      x = 0; 
      while (cols--)
	{
	  ClutterPhotoGridCell *cell = (ClutterPhotoGridCell *)cell_item->data;
	  Pixbuf *pixb = NULL;
	  float   tx, ty;
	  int     x1, x2, y1, y2, thumb_w, thumb_h;
	  int     ns_border, ew_border;

	  pixb = cell->pixb;

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

	  glPushMatrix();

#if 0
	  glRotatef ( 45.0, x1+(thumb_w/2) - 320, 0.0 /*y2+(thumb_h/2) - 240*/, 0.0);
#endif
	  /* Border - why need tex disabled ? */
	  glDisable(GL_TEXTURE_2D);
	  if (cell_item == grid->cell_active 
	      && grid->state == CLTR_PHOTO_GRID_STATE_BROWSE)
	    glColor4f(1.0, 1.0, 1.0, 1.0);
	  else
	    glColor4f(0.8, 0.8, 0.8, 1.0);
	  glRecti(x1-2, y1-2, x2+2, y2+2);
	  glEnable(GL_TEXTURE_2D);

	  glBindTexture(GL_TEXTURE_2D, grid->texs[i]);

	  /*
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
	  */
	    {
	      glBegin (GL_QUADS);
	      glTexCoord2f (tx, ty);   glVertex2i   (x2, y2);
	      glTexCoord2f (0,  ty);   glVertex2i   (x1, y2);
	      glTexCoord2f (0,  0);    glVertex2i   (x1, y1);
	      glTexCoord2f (tx, 0);    glVertex2i   (x2, y1);
	      glEnd ();	
	    }

	   glPopMatrix();

	  cell_item = g_list_next(cell_item);

	  if (!cell_item)
	    goto finish;

	  x += grid->cell_width;
	  i++;
	}
      y += grid->cell_height;
    }

 finish:

  glPopMatrix();

  /*
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBlendFunc(GL_ONE, GL_SRC_ALPHA);
  glColor4f(1.0, 1.0, 1.0, xxx);
  glRecti(0, 0, 640, 480);

  xxx += 0.01;
  */

  glXSwapBuffers(CltrCntx.xdpy, grid->parent->xwin);  

}

ClutterPhotoGrid*
cltr_photo_grid_new(ClutterWindow *win, 
		    int            n_cols,
		    int            n_rows,
		    const gchar   *imgs_path)
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
  grid->zoom_min  = 0.8;		      
  grid->view_min_x = 0.0;
  grid->view_min_y = 0.0;


  /* Assmes cols == rows */
  grid->zoom_max  = /* 1.0 + */  (float) (n_rows * 1.0);

  /* Below needs to go else where - some kind of texture manager/helper */

  grid->tex_w    = 1024;   
  grid->tex_h    = 512;

  grid->tex_data = malloc (grid->tex_w * grid->tex_h * 4);

  /* Load  */
  cltr_photo_grid_populate(grid, imgs_path);

  grid->cell_active = g_list_first(grid->cells_tail);

  ctrl_photo_grid_cell_to_coords(grid, grid->cell_active, &x, &y);

  ctrl_photo_grid_get_zoomed_coords(grid, grid->zoom_min,
				    x, y, 
				    &grid->view_max_x,
				    &grid->view_max_y);
  cltr_photo_grid_redraw(grid);

  return grid;
}
