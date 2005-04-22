#ifndef _HAVE_CLTR_PHOTO_GRID_H
#define _HAVE_CLTR_PHOTO_GRID_H

#include "cltr.h"

typedef struct CltrPhotoGrid CltrPhotoGrid;

typedef struct CltrPhotoGridCell CltrPhotoGridCell;

#define CLTR_PHOTO_GRID(w) (CltrPhotoGrid*)(w)

typedef enum CltrPhotoGridState
{
  CLTR_PHOTO_GRID_STATE_BROWSE        ,
  CLTR_PHOTO_GRID_STATE_ZOOM_IN       ,
  CLTR_PHOTO_GRID_STATE_ZOOMED        ,
  CLTR_PHOTO_GRID_STATE_ZOOM_OUT      ,
  CLTR_PHOTO_GRID_STATE_ZOOMED_MOVE   ,
  CLTR_PHOTO_GRID_STATE_SCROLLED_MOVE ,
} 
CltrPhotoGridState;

typedef enum CltrPhotoGridCellState
{
  CLTR_PHOTO_GRID_CELL_STATE_APPEARING,
  CLTR_PHOTO_GRID_CELL_STATE_STATIC,
} 
CltrPhotoGridCellState;

GMutex*
cltr_photo_grid_mutex(CltrPhotoGrid *grid);

void
cltr_photo_grid_set_populated(CltrPhotoGrid *grid, gboolean populated);

CltrPhotoGridCell*
cltr_photo_grid_cell_new(CltrPhotoGrid *grid,
			 Pixbuf           *pixb);

Pixbuf*
cltr_photo_grid_cell_pixbuf(CltrPhotoGridCell *cell);

CltrPhotoGridCell*
cltr_photo_grid_get_active_cell(CltrPhotoGrid *grid);

void
cltr_photo_grid_set_active_cell(CltrPhotoGrid *grid, CltrPhotoGridCell *cell);

CltrPhotoGridCell*
cltr_photo_grid_get_first_cell(CltrPhotoGrid *grid);


void
cltr_photo_grid_append_cell(CltrPhotoGrid     *grid,
			    CltrPhotoGridCell *cell);

void
cltr_photo_grid_navigate(CltrPhotoGrid *grid,
			 CltrDirection     direction) ;

void 				/* bleh badly named */
cltr_photo_grid_activate_cell(CltrPhotoGrid *grid);

gpointer
cltr_photo_grid_populate(gpointer data) ;

void
cltr_photo_grid_redraw(CltrPhotoGrid *grid);

CltrWidget*
cltr_photo_grid_new(int            width, 
		    int            height,
		    int            n_cols,
		    int            n_rows,
		    const gchar   *img_path);


#endif
