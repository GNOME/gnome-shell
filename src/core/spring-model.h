/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "window.h"

typedef struct XYPair Point;
typedef struct XYPair Vector;
typedef struct Spring Spring;
typedef struct Object Object;
typedef struct Model Model;

Model *model_new (MetaRectangle *rectangle,
		  gboolean       expand);
void model_destroy (Model *model);
void
model_get_position (Model *model,
		    int	   i,
		    int    j,
		    double *x,
		    double *y);
void
model_step (Model *model);
void
model_destroy (Model *model);
gboolean
model_is_calm (Model *model);
void
model_set_anchor (Model *model,
		  int	 x,
		  int    y);
void
model_begin_move (Model *model, int x, int y);
void
model_update_move (Model *model, int x, int y);
