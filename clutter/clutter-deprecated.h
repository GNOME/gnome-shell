#ifndef _CLUTTER_DEPRECATED_H
#define _CLUTTER_DEPRECATED_H

/* This header contains defines that makes the compiler provide useful
 * direction for resolving compile problems when code is using old APIs
 * When using a function name that no longer applies the compiler will
 * tell the developer the name of the new function call.
 *
 * Functions that are simply renamed should give errors containing
 * _REPLACED_BY_ whilst functions that are deprecated by new functions with
 * new functionality should giver errors containing _DEPRECATED_BY_.
 */

#define clutter_behaviour_ellipse_set_angle_begin    clutter_behaviour_ellipse_set_angle_begin_REPLACED_BY_clutter_behaviour_set_angle_start
#define clutter_behaviour_ellipse_set_angle_beginx   clutter_behaviour_ellipse_set_angle_beginx_REPLACED_BY_clutter_behaviour_set_angle_startx
#define clutter_behaviour_ellipse_get_angle_begin    clutter_behaviour_ellipse_get_angle_begin_REPLACED_BY_clutter_behaviour_get_angle_start
#define clutter_behaviour_ellipse_get_angle_beginx   clutter_behaviour_ellipse_get_angle_beginx_REPLACED_BY_clutter_behaviour_get_angle_startx
#define clutter_behaviour_bspline_append             clutter_behaviour_bspline_append_REPLACED_BY_clutter_behaviour_bspline_append_knots

#define clutter_actor_get_id                         clutter_actor_get_id_REPLACED_BY_clutter_actor_get_gid

#define clutter_actor_rotate_x                       clutter_actor_rotate_x_DEPRECATED_BY_clutter_actor_set_rotation
#define clutter_actor_rotate_y                       clutter_actor_rotate_y_DEPRECATED_BY_clutter_actor_set_rotation
#define clutter_actor_rotate_z                       clutter_actor_rotate_z_DEPRECATED_BY_clutter_actor_set_rotation
#define clutter_actor_rotate_xx                      clutter_actor_rotate_xx_DEPRECATED_BY_clutter_actor_set_rotationx
#define clutter_actor_rotate_yx                      clutter_actor_rotate_yx_DEPRECATED_BY_clutter_actor_set_rotationx
#define clutter_actor_rotate_zx                      clutter_actor_rotate_zx_DEPRECATED_BY_clutter_actor_set_rotationx
#define clutter_actor_get_rxang                      clutter_actor_get_rxang_DEPRECATED_BY_clutter_actor_get_rotation
#define clutter_actor_get_ryang                      clutter_actor_get_ryang_DEPRECATED_BY_clutter_actor_get_rotation
#define clutter_actor_get_rzang                      clutter_actor_get_rzang_DEPRECATED_BY_clutter_actor_get_rotation
#define clutter_actor_get_rxangx                     clutter_actor_get_rxangx_DEPRECATED_BY_clutter_actor_get_rotationx
#define clutter_actor_get_ryangx                     clutter_actor_get_ryangx_DEPRECATED_BY_clutter_actor_get_rotationx
#define clutter_actor_get_rzangx                     clutter_actor_get_rzangx_DEPRECATED_BY_clutter_actor_get_rotationx

#endif /* CLUTTER_DEPRECATED_H */
