/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_CLUTTER_ELEMENT_H
#define _HAVE_CLUTTER_ELEMENT_H

/* clutter-element.h */

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_GEOMETRY (clutter_geometry_get_type ())
#define CLUTTER_TYPE_ELEMENT_BOX (clutter_element_box_get_type ())

#define CLUTTER_TYPE_ELEMENT clutter_element_get_type()

#define CLUTTER_ELEMENT(obj) \
 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ELEMENT, ClutterElement))
#define CLUTTER_ELEMENT_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ELEMENT, ClutterElementClass))
#define CLUTTER_IS_ELEMENT(obj) \
 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ELEMENT))
#define CLUTTER_IS_ELEMENT_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ELEMENT))
#define CLUTTER_ELEMENT_GET_CLASS(obj) \
 (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ELEMENT, ClutterElementClass))

#define CLUTTER_ELEMENT_SET_FLAGS(e,f) ((e)->flags |= (f))  
#define CLUTTER_ELEMENT_UNSET_FLAGS(e,f) ((e)->flags &= ~(f))  

#define CLUTTER_ELEMENT_IS_MAPPED(e) ((e)->flags & CLUTTER_ELEMENT_MAPPED)  
#define CLUTTER_ELEMENT_IS_REALIZED(e) ((e)->flags & CLUTTER_ELEMENT_REALIZED)
#define CLUTTER_ELEMENT_IS_VISIBLE(e) \
 (CLUTTER_ELEMENT_IS_MAPPED(e) && CLUTTER_ELEMENT_IS_REALIZED(e))  

typedef struct _ClutterElement         ClutterElement;
typedef struct _ClutterElementClass    ClutterElementClass;
typedef struct _ClutterElementBox      ClutterElementBox;
typedef struct _ClutterElementPrivate  ClutterElementPrivate;
typedef struct _ClutterGeometry        ClutterGeometry;

typedef void (*ClutterCallback) (ClutterElement *element, gpointer data);

struct _ClutterGeometry
{ 
  gint x;
  gint y;
  guint width;
  guint height;
};

GType clutter_geometry_get_type (void) G_GNUC_CONST;

typedef enum 
{
  CLUTTER_ELEMENT_MIRROR_X = 1 << 1,
  CLUTTER_ELEMENT_MIRROR_Y = 1 << 2
} ClutterElementTransform;

typedef enum
{
  CLUTTER_ELEMENT_MAPPED   = 1 << 1,
  CLUTTER_ELEMENT_REALIZED = 1 << 2
} ClutterElementFlags;

struct _ClutterElementBox { gint x1, y1, x2, y2; };

GType clutter_element_box_get_type (void) G_GNUC_CONST;

struct _ClutterElement
{
  /*< public >*/
  GObject                 parent;
  guint32                 flags;

  /*< private >*/
  ClutterElementPrivate  *priv;
};

struct _ClutterElementClass
{
  GObjectClass parent_class;

  void (* show)                (ClutterElement        *element);
  void (* hide)                (ClutterElement        *element);
  void (* realize)             (ClutterElement        *element);
  void (* unrealize)           (ClutterElement        *element);
  void (* paint)               (ClutterElement        *element);
  void (* request_coords)      (ClutterElement        *element,
				ClutterElementBox     *box);
  void (* allocate_coords)     (ClutterElement        *element,
				ClutterElementBox     *box);
  void (* set_depth)           (ClutterElement        *element,
		                gint                   depth);
  gint (* get_depth)           (ClutterElement        *element);

  /* to go ? */
  void (* show_all)            (ClutterElement        *element);
  void (* hide_all)            (ClutterElement        *element);
  void (* queue_redraw)        (ClutterElement        *element);
};

GType clutter_element_get_type (void);

void
clutter_element_show (ClutterElement *self);

void
clutter_element_hide (ClutterElement *self);

void
clutter_element_realize (ClutterElement *self);

void
clutter_element_unrealize (ClutterElement *self);

void
clutter_element_paint (ClutterElement *self);

void
clutter_element_queue_redraw (ClutterElement  *self);

void
clutter_element_request_coords (ClutterElement    *self,
				ClutterElementBox *box);

void
clutter_element_allocate_coords (ClutterElement    *self,
				 ClutterElementBox *box);

void
clutter_element_set_geometry (ClutterElement  *self,
			      ClutterGeometry *geom);

void
clutter_element_get_geometry (ClutterElement  *self,
			      ClutterGeometry *geom);

void
clutter_element_get_coords (ClutterElement *self,
			    gint           *x1,
			    gint           *y1,
			    gint           *x2,
			    gint           *y2);

void
clutter_element_set_position (ClutterElement *self,
			      gint            x,
			      gint            y);

void
clutter_element_set_size (ClutterElement *self,
			  gint            width,
			  gint            height);

void
clutter_element_get_abs_position (ClutterElement *self,
				  gint           *x,
				  gint           *y);

guint
clutter_element_get_width (ClutterElement *self);

guint
clutter_element_get_height (ClutterElement *self);

gint
clutter_element_get_x (ClutterElement *self);

gint
clutter_element_get_y (ClutterElement *self);

void
clutter_element_rotate_z (ClutterElement          *self,
			  gfloat                   angle,
			  gint                     x,
			  gint                     y);

void
clutter_element_rotate_x (ClutterElement          *self,
			  gfloat                   angle,
			  gint                     y,
			  gint                     z);

void
clutter_element_rotate_y (ClutterElement          *self,
			  gfloat                   angle,
			  gint                     x,
			  gint                     z);

void
clutter_element_set_opacity (ClutterElement *self,
			     guint8          opacity);

guint8
clutter_element_get_opacity (ClutterElement *self);

void
clutter_element_set_name (ClutterElement *self,
			  const gchar    *id);

const gchar*
clutter_element_get_name (ClutterElement *self);

guint32
clutter_element_get_id (ClutterElement *self);

void
clutter_element_set_clip (ClutterElement *self,
			  gint            xoff, 
			  gint            yoff, 
			  gint            width, 
			  gint            height);

void
clutter_element_remove_clip (ClutterElement *self);

void
clutter_element_set_parent (ClutterElement *self, ClutterElement *parent);

ClutterElement*
clutter_element_get_parent (ClutterElement *self);

void
clutter_element_raise (ClutterElement *self, ClutterElement *below);

void
clutter_element_lower (ClutterElement *self, ClutterElement *above);

void
clutter_element_raise_top (ClutterElement *self);

void
clutter_element_lower_bottom (ClutterElement *self);

G_END_DECLS

#endif
