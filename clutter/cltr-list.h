#ifndef _HAVE_CLTR_LIST_H
#define _HAVE_CLTR_LIST_H

#include "cltr.h"

typedef struct CltrList CltrList;

typedef struct CltrListCell CltrListCell;

#define CLTR_LIST(w) ((CltrList*)(w))

typedef void (*CltrListCellActivate) (CltrList     *list, 
				      CltrListCell *cell,
				      void         *userdata) ;

typedef enum CltrListState
{
  CLTR_LIST_STATE_LOADING       ,
  CLTR_LIST_STATE_LOAD_COMPLETE ,
  CLTR_LIST_STATE_BROWSE        ,
  CLTR_LIST_STATE_SCROLL_UP     ,
  CLTR_LIST_STATE_SCROLL_DOWN    
} 
CltrListState;

CltrListCell*
cltr_list_cell_new(CltrList *list, 
		   Pixbuf   *thump_pixb, 
		   char     *text);

void
cltr_list_append_cell(CltrList *list, CltrListCell *cell);

CltrWidget*
cltr_list_new(int width, 
	      int height,
	      int cell_width,
	      int cell_height);

void
cltr_list_on_activate_cell(CltrList             *list,
			   CltrListCellActivate  callback,
			   gpointer             *userdata);

gboolean
cltr_list_get_active_cell_co_ords(CltrList *list,
				  int      *x1,
				  int      *y1,
				  int      *x2,
				  int      *y2);


void
cltr_list_scroll_down(CltrList *list);

void
cltr_list_scroll_up(CltrList *list);


#endif
