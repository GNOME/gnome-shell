#ifndef _CLUTTER_DEPRECATED_H
#define _CLUTTER_DEPRECATED_H

/* This header contains defines that makes the compiler provide useful
 * direction for resolving compile problems when code is using old APIs
 * When using a function name that no longer applies the compiler will
 * tell the developer the name of the new function call.
 *
 * Functions that are simply renamed should give errors containing
 * _REPLACED_BY_ whilst functions that are deprecated by new functions with
 * new * functionality should giver errors containing _DEPRECATED_BY_.
 */

#define clutter_behaviour_ellipse_set_angle_begin    cairo_behaviour_ellipse_set_angle_begin_REPLACED_BY_clutter_behaviour_set_angle_start
#define clutter_behaviour_ellipse_set_angle_beginx   cairo_behaviour_ellipse_set_angle_beginx_REPLACED_BY_clutter_behaviour_set_angle_startx
#define clutter_behaviour_ellipse_get_angle_begin    cairo_behaviour_ellipse_get_angle_begin_REPLACED_BY_clutter_behaviour_get_angle_start
#define clutter_behaviour_ellipse_get_angle_beginx   cairo_behaviour_ellipse_get_angle_beginx_REPLACED_BY_clutter_behaviour_get_angle_startx

#endif /* CLUTTER_DEPRECATED_H */
