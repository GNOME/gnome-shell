/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
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

#ifndef __CLUTTER_ALPHA_H__
#define __CLUTTER_ALPHA_H__

#include <glib-object.h>
#include <clutter/clutter-timeline.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ALPHA clutter_alpha_get_type()

#define CLUTTER_ALPHA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_ALPHA, ClutterAlpha))

#define CLUTTER_ALPHA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_ALPHA, ClutterAlphaClass))

#define CLUTTER_IS_ALPHA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_ALPHA))

#define CLUTTER_IS_ALPHA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_ALPHA))

#define CLUTTER_ALPHA_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_ALPHA, ClutterAlphaClass))

typedef struct _ClutterAlpha        ClutterAlpha;
typedef struct _ClutterAlphaClass   ClutterAlphaClass; 
typedef struct _ClutterAlphaPrivate ClutterAlphaPrivate;

typedef guint32 (*ClutterAlphaFunc) (ClutterAlpha *alpha,
                                     gpointer      user_data); 

struct _ClutterAlpha
{
  /*< private >*/
  GInitiallyUnowned parent;
  ClutterAlphaPrivate *priv;
};

struct _ClutterAlphaClass
{
  GInitiallyUnownedClass parent_class;
  
  void (*_clutter_alpha_1) (void);
  void (*_clutter_alpha_2) (void);
  void (*_clutter_alpha_3) (void);
  void (*_clutter_alpha_4) (void);
  void (*_clutter_alpha_5) (void);
}; 


#define CLUTTER_ALPHA_MAX_ALPHA 0xffff

GType clutter_alpha_get_type (void) G_GNUC_CONST;

ClutterAlpha *   clutter_alpha_new          (void);
ClutterAlpha *   clutter_alpha_new_full     (ClutterTimeline  *timeline,
                                             ClutterAlphaFunc  func,
                                             gpointer          data,
                                             GDestroyNotify    destroy);
guint32          clutter_alpha_get_alpha    (ClutterAlpha     *alpha);
void             clutter_alpha_set_func     (ClutterAlpha     *alpha,
                                             ClutterAlphaFunc  func,
                                             gpointer          data,
                                             GDestroyNotify    destroy);
void             clutter_alpha_set_timeline (ClutterAlpha     *alpha,
                                             ClutterTimeline  *timeline);
ClutterTimeline *clutter_alpha_get_timeline (ClutterAlpha     *alpha);

/* convenience functions */
#define CLUTTER_ALPHA_RAMP_INC  clutter_ramp_inc_func
#define CLUTTER_ALPHA_RAMP_DEC  clutter_ramp_dec_func
#define CLUTTER_ALPHA_RAMP      clutter_ramp_func
#define CLUTTER_ALPHA_SINE      clutter_sine_func
/* FIXME add SINE_INC/DEC */
#define CLUTTER_ALPHA_SQUARE    clutter_square_func

guint32          clutter_ramp_inc_func      (ClutterAlpha     *alpha,
                                             gpointer          dummy);
guint32          clutter_ramp_dec_func      (ClutterAlpha     *alpha,
                                             gpointer          dummy);
guint32          clutter_ramp_func          (ClutterAlpha     *alpha,
                                             gpointer          dummy);
guint32          clutter_sine_func          (ClutterAlpha     *alpha,
                                             gpointer          dummy);
guint32          clutter_square_func        (ClutterAlpha     *alpha,
                                             gpointer          dummy);

G_END_DECLS

#endif /* __CLUTTER_ALPHA_H__ */
