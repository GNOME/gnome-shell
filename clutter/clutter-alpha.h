/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
 *             Tomas Frydrych <tf@openedhand.com>
 *
 * Copyright (C) 2006, 2007 OpenedHand
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

#ifndef __CLUTTER_ALPHA_H__
#define __CLUTTER_ALPHA_H__

#include <clutter/clutter-timeline.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ALPHA              (clutter_alpha_get_type ())
#define CLUTTER_ALPHA(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ALPHA, ClutterAlpha))
#define CLUTTER_ALPHA_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ALPHA, ClutterAlphaClass))
#define CLUTTER_IS_ALPHA(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ALPHA))
#define CLUTTER_IS_ALPHA_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ALPHA))
#define CLUTTER_ALPHA_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ALPHA, ClutterAlphaClass))

typedef struct _ClutterAlpha            ClutterAlpha;
typedef struct _ClutterAlphaClass       ClutterAlphaClass;
typedef struct _ClutterAlphaPrivate     ClutterAlphaPrivate;

/**
 * ClutterAlphaFunc:
 * @alpha: a #ClutterAlpha
 * @user_data: user data passed to the function
 *
 * A function of time, which returns a value between 0 and
 * %CLUTTER_ALPHA_MAX_ALPHA.
 *
 * Return value: an unsigned integer value, between 0 and
 * %CLUTTER_ALPHA_MAX_ALPHA.
 *
 * Since: 0.2
 */
typedef guint32 (*ClutterAlphaFunc) (ClutterAlpha *alpha,
                                     gpointer      user_data); 

/**
 * ClutterAlpha:
 *
 * #ClutterAlpha combines a #ClutterTimeline and a function.
 * The contents of the #ClutterAlpha structure are private and should
 * only be accessed using the provided API.
 *
 * Since: 0.2
 */
struct _ClutterAlpha
{
  /*< private >*/
  GInitiallyUnowned parent;
  ClutterAlphaPrivate *priv;
};

/**
 * ClutterAlphaClass:
 *
 * Base class for #ClutterAlpha
 *
 * Since: 0.2
 */
struct _ClutterAlphaClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;
  
  void (*_clutter_alpha_1) (void);
  void (*_clutter_alpha_2) (void);
  void (*_clutter_alpha_3) (void);
  void (*_clutter_alpha_4) (void);
  void (*_clutter_alpha_5) (void);
}; 

/**
 * CLUTTER_ALPHA_MAX_ALPHA:
 *
 * Maximum value returned by #ClutterAlphaFunc
 *
 * Since: 0.2
 */
#define CLUTTER_ALPHA_MAX_ALPHA (65535.0f)

GType clutter_alpha_get_type (void) G_GNUC_CONST;

ClutterAlpha *   clutter_alpha_new              (void);
ClutterAlpha *   clutter_alpha_new_full         (ClutterTimeline  *timeline,
                                                 gulong            mode);
ClutterAlpha *   clutter_alpha_new_with_func    (ClutterTimeline  *timeline,
                                                 ClutterAlphaFunc  func,
                                                 gpointer          data,
                                                 GDestroyNotify    destroy);

guint32          clutter_alpha_get_alpha        (ClutterAlpha     *alpha);
void             clutter_alpha_set_func         (ClutterAlpha     *alpha,
                                                 ClutterAlphaFunc  func,
                                                 gpointer          data,
                                                 GDestroyNotify    destroy);
void             clutter_alpha_set_closure      (ClutterAlpha     *alpha,
                                                 GClosure         *closure);
void             clutter_alpha_set_timeline     (ClutterAlpha     *alpha,
                                                 ClutterTimeline  *timeline);
ClutterTimeline *clutter_alpha_get_timeline     (ClutterAlpha     *alpha);
void             clutter_alpha_set_mode         (ClutterAlpha     *alpha,
                                                 gulong            mode);
gulong           clutter_alpha_get_mode         (ClutterAlpha     *alpha);

gulong           clutter_alpha_register_func    (ClutterAlphaFunc  func,
                                                 gpointer          data);
gulong           clutter_alpha_register_closure (GClosure         *closure);

/* convenience functions */
guint32             clutter_ramp_inc_func       (ClutterAlpha     *alpha,
						 gpointer          dummy);
guint32             clutter_ramp_dec_func       (ClutterAlpha     *alpha,
						 gpointer          dummy);
guint32             clutter_ramp_func           (ClutterAlpha     *alpha,
						 gpointer          dummy);

guint32             clutter_sine_func           (ClutterAlpha     *alpha,
						 gpointer          dummy);
guint32             clutter_sine_inc_func       (ClutterAlpha     *alpha,
						 gpointer          dummy);
guint32             clutter_sine_dec_func       (ClutterAlpha     *alpha,
						 gpointer          dummy);
guint32             clutter_sine_half_func      (ClutterAlpha     *alpha,
						 gpointer          dummy);
guint32             clutter_sine_in_func        (ClutterAlpha     *alpha,
                                                 gpointer          dummy);
guint32             clutter_sine_out_func       (ClutterAlpha     *alpha,
                                                 gpointer          dummy);
guint32             clutter_sine_in_out_func    (ClutterAlpha     *alpha,
                                                 gpointer          dummy);

guint32             clutter_square_func         (ClutterAlpha     *alpha,
						 gpointer          dummy);

guint32             clutter_smoothstep_inc_func (ClutterAlpha     *alpha,
			                         gpointer          dummy);
guint32             clutter_smoothstep_dec_func (ClutterAlpha     *alpha,
			                         gpointer          dummy);

guint32             clutter_exp_inc_func        (ClutterAlpha     *alpha,
						 gpointer          dummy);
guint32             clutter_exp_dec_func        (ClutterAlpha     *alpha,
						 gpointer          dummy);

guint32             clutter_ease_in_func        (ClutterAlpha     *alpha,
                                                 gpointer          dummy);
guint32             clutter_ease_out_func       (ClutterAlpha     *alpha,
                                                 gpointer          dummy);
guint32             clutter_ease_in_out_func    (ClutterAlpha     *alpha,
                                                 gpointer          dummy);

guint32             clutter_exp_in_func         (ClutterAlpha     *alpha,
                                                 gpointer          dummy);
guint32             clutter_exp_out_func        (ClutterAlpha     *alpha,
                                                 gpointer          dummy);
guint32             clutter_exp_in_out_func     (ClutterAlpha     *alpha,
                                                 gpointer          dummy);

G_END_DECLS

#endif /* __CLUTTER_ALPHA_H__ */
