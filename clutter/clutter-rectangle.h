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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_RECTANGLE_H__
#define __CLUTTER_RECTANGLE_H__

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-color.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_RECTANGLE                  (clutter_rectangle_get_type())
#define CLUTTER_RECTANGLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_RECTANGLE, ClutterRectangle))
#define CLUTTER_RECTANGLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_RECTANGLE, ClutterRectangleClass))
#define CLUTTER_IS_RECTANGLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_RECTANGLE))
#define CLUTTER_IS_RECTANGLE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_RECTANGLE))
#define CLUTTER_RECTANGLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_RECTANGLE, ClutterRectangleClass))

typedef struct _ClutterRectangle        ClutterRectangle;
typedef struct _ClutterRectangleClass   ClutterRectangleClass;
typedef struct _ClutterRectanglePrivate ClutterRectanglePrivate;

/**
 * ClutterRectangle:
 *
 * The #ClutterRectangle structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.1
 */
struct _ClutterRectangle
{
  /*< private >*/
  ClutterActor           parent;

  ClutterRectanglePrivate *priv;
}; 

/**
 * ClutterRectangleClass:
 *
 * The #ClutterRectangleClass structure contains only private data
 *
 * Since: 0.1
 */
struct _ClutterRectangleClass 
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_rectangle1) (void);
  void (*_clutter_rectangle2) (void);
  void (*_clutter_rectangle3) (void);
  void (*_clutter_rectangle4) (void);
};

GType clutter_rectangle_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_rectangle_new              (void);
ClutterActor *clutter_rectangle_new_with_color   (const ClutterColor *color);

void          clutter_rectangle_get_color        (ClutterRectangle   *rectangle,
                                                  ClutterColor       *color);
void          clutter_rectangle_set_color        (ClutterRectangle   *rectangle,
						  const ClutterColor *color);
guint         clutter_rectangle_get_border_width (ClutterRectangle   *rectangle);
void          clutter_rectangle_set_border_width (ClutterRectangle   *rectangle,
                                                  guint               width);
void          clutter_rectangle_get_border_color (ClutterRectangle   *rectangle,
                                                  ClutterColor       *color);
void          clutter_rectangle_set_border_color (ClutterRectangle   *rectangle,
                                                  const ClutterColor *color);

G_END_DECLS

#endif /* __CLUTTER_RECTANGLE_H__ */
