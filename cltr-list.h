#ifndef _HAVE_CLTR_LIST_H
#define _HAVE_CLTR_LIST_H

#include "cltr.h"

typedef struct CltrList CltrList;

#define CLTR_LIST(w) ((CltrList*)(w))

typedef enum CltrListState
{
  CLTR_LIST_STATE_LOADING       ,
  CLTR_LIST_STATE_LOAD_COMPLETE ,
  CLTR_LIST_STATE_BROWSE        ,
  CLTR_LIST_STATE_SCROLL_UP     ,
  CLTR_LIST_STATE_SCROLL_DOWN    
} 
CltrListState;


CltrWidget*
cltr_list_new(int width, 
	      int height,
	      int cell_width,
	      int cell_height);

void
cltr_list_scroll_down(CltrList *list);

void
cltr_list_scroll_up(CltrList *list);


#endif
