#ifndef __CLUTTER_EASING_H__
#define __CLUTTER_EASING_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

/*< private >
 * ClutterEasingFunc:
 * @t: elapsed time
 * @d: total duration
 *
 * Internal type for the easing functions used by Clutter.
 *
 * Return value: the interpolated value, between -1.0 and 2.0
 */
typedef double (* ClutterEasingFunc) (double t, double d);

G_GNUC_INTERNAL
ClutterEasingFunc       clutter_get_easing_func_for_mode        (ClutterAnimationMode mode);

G_GNUC_INTERNAL
const char *            clutter_get_easing_name_for_mode        (ClutterAnimationMode mode);

G_GNUC_INTERNAL
double                  clutter_easing_for_mode                 (ClutterAnimationMode mode,
                                                                 double               t,
                                                                 double               d);

G_GNUC_INTERNAL
double  clutter_linear                  (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_quad            (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_quad           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_quad        (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_cubic           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_cubic          (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_cubic       (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_quart           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_quart          (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_quart       (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_quint           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_quint          (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_quint       (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_sine            (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_sine           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_sine        (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_expo            (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_expo           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_expo        (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_circ            (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_circ           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_circ        (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_elastic         (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_elastic        (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_elastic     (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_back            (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_back           (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_back        (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_bounce          (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_out_bounce         (double t,
                                         double d);
G_GNUC_INTERNAL
double  clutter_ease_in_out_bounce      (double t,
                                         double d);

G_GNUC_INTERNAL
double  clutter_ease_steps_start        (double t,
                                         double d,
                                         int    steps);
G_GNUC_INTERNAL
double  clutter_ease_steps_end          (double t,
                                         double d,
                                         int    steps);
G_GNUC_INTERNAL
double  clutter_ease_cubic_bezier       (double t,
                                         double d,
                                         double x_1,
                                         double y_1,
                                         double x_2,
                                         double y_2);

G_END_DECLS

#endif /* __CLUTTER_EASING_H__ */
