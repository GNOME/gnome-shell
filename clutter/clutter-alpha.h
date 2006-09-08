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

/* clutter-alpha.h */

#include <glib-object.h>
#include <clutter/clutter-timeline.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ALPHA            (clutter_alpha_get_type ())
#define CLUTTER_ALPHA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ALPHA, ClutterAlpha))
#define CLUTTER_ALPHA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ALPHA, ClutterAlphaClass))
#define CLUTTER_IS_ALPHA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ALPHA))
#define CLUTTER_IS_ALPHA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ALPHA))
#define CLUTTER_ALPHA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ALPHA, ClutterAlphaClass))

typedef struct _ClutterAlpha        ClutterAlpha;
typedef struct _ClutterAlphaClass   ClutterAlphaClass; 
typedef struct _ClutterAlphaPrivate ClutterAlphaPrivate;

typedef guint32 (*ClutterAlphaFunc) (ClutterAlpha *alpha,
                                     gpointer      data); 

struct _ClutterAlpha
{
  GInitiallyUnowned parent;

  /*< private >*/
  ClutterAlphaPrivate *priv;
};

struct _ClutterAlphaClass
{
  GInitiallyUnowned parent_class;
  
  void (*_clutter_alpha_1) (void);
  void (*_clutter_alpha_2) (void);
  void (*_clutter_alpha_3) (void);
  void (*_clutter_alpha_4) (void);
  void (*_clutter_alpha_5) (void);
}; 

#define CLUTTER_ALPHA_MIN 0x0000
#define CLUTTER_ALPHA_MAX 0xffff

GType            clutter_alpha_get_type      (void) G_GNUC_CONST;
ClutterAlpha *   clutter_alpha_new           (ClutterTimeline   *timeline,
                                              ClutterAlphaFunc   func,
                                              gpointer           data);
ClutterAlpha *   clutter_alpha_new_full      (ClutterTimeline   *timeline,
                                              ClutterAlphaFunc   func,
                                              gpointer           data,
                                              GDestroyNotify     destroy);
guint32          clutter_alpha_get_value     (ClutterAlpha      *alpha);
void             clutter_alpha_set_func      (ClutterAlpha      *alpha,
                                              ClutterAlphaFunc   func,
                                              gpointer           data,
                                              GDestroyNotify     destroy);
gint             clutter_alpha_get_delay     (ClutterAlpha      *alpha);
void             clutter_alpha_set_delay     (ClutterAlpha      *alpha,
                                              gint               delay);
gboolean         clutter_alpha_get_is_paused (ClutterAlpha      *alpha);
void             clutter_alpha_set_is_paused (ClutterAlpha      *alpha,
                                              gboolean           is_paused);
ClutterTimeline *clutter_alpha_get_timeline  (ClutterAlpha      *alpha);
void             clutter_alpha_set_timeline  (ClutterAlpha      *alpha,
                                              ClutterTimeline   *timeline);


/* predefined alpha functions */
guint32 clutter_alpha_ramp_inc_func (ClutterAlpha *alpha, gpointer data);
guint32 clutter_alpha_ramp_dec_func (ClutterAlpha *alpha, gpointer data);
guint32 clutter_alpha_ramp_func     (ClutterAlpha *alpha, gpointer data);

#define CLUTTER_ALPHA_RAMP_INC clutter_alpha_ramp_inc_func
#define CLUTTER_ALPHA_RAMP_DEC clutter_alpha_ramp_dec_func
#define CLUTTER_ALPHA_RAMP     clutter_alpha_ramp_func

G_END_DECLS

#endif
