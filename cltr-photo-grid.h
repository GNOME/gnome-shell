#ifndef _HAVE_CLTR_PHOTO_GRID_H
#define _HAVE_CLTR_PHOTO_GRID_H

#include "cltr.h"

typedef struct ClutterPhotoGrid ClutterPhotoGrid;

typedef struct ClutterPhotoGridCell ClutterPhotoGridCell;

typedef enum ClutterPhotoGridState
{
  CLTR_PHOTO_GRID_STATE_LOADING       ,
  CLTR_PHOTO_GRID_STATE_LOAD_COMPLETE ,
  CLTR_PHOTO_GRID_STATE_BROWSE        ,
  CLTR_PHOTO_GRID_STATE_ZOOM_IN       ,
  CLTR_PHOTO_GRID_STATE_ZOOMED        ,
  CLTR_PHOTO_GRID_STATE_ZOOM_OUT      ,
  CLTR_PHOTO_GRID_STATE_ZOOMED_MOVE   ,
  CLTR_PHOTO_GRID_STATE_SCROLLED_MOVE ,
} 
ClutterPhotoGridState;

typedef enum ClutterPhotoGridCellState
{
  CLTR_PHOTO_GRID_CELL_STATE_APPEARING,
  CLTR_PHOTO_GRID_CELL_STATE_STATIC,
} 
ClutterPhotoGridCellState;


struct ClutterPhotoGridCell
{
  Pixbuf      *pixb;
  float        angle;
  CltrTexture *texture;
  gint         anim_step;

  ClutterPhotoGridCellState state;
};

struct ClutterPhotoGrid
{
  /* XXX should be base widget stuff  */
  int            x,y;
  int            width;
  int            height;
  ClutterWindow *parent;

  /* ****** */

  gchar         *img_path;

  int            n_rows;
  int            n_cols;
  int            row_offset; 	/* for scrolling */

  int            cell_width;
  int            cell_height;

  GList         *cells_tail;
  GList         *cell_active;

  /* animation / zoom etc stuff  */

  int            anim_n_steps, anim_step;

  float          zoom_min, zoom_max, zoom_step;

  float          view_min_x, view_max_x, view_min_y, view_max_y; 

  float          scroll_dist;

  ClutterPhotoGridState  state;

  int                    scroll_state, scroll_step; /* urg */

};

ClutterPhotoGridCell*
cltr_photo_grid_cell_new(ClutterPhotoGrid *grid,
			 Pixbuf           *pixb,
			 const gchar      *filename);

void
cltr_photo_grid_append_cell(ClutterPhotoGrid     *grid,
			    ClutterPhotoGridCell *cell);

void
cltr_photo_grid_navigate(ClutterPhotoGrid *grid,
			 CltrDirection     direction) ;

void 				/* bleh badly named */
cltr_photo_grid_activate_cell(ClutterPhotoGrid *grid);

gpointer
cltr_photo_grid_populate(gpointer data) ;

void
cltr_photo_grid_redraw(ClutterPhotoGrid *grid);

ClutterPhotoGrid*
cltr_photo_grid_new(ClutterWindow *win, 
		    int            n_cols,
		    int            n_rows,
		    const gchar   *imgs_path);

#endif
