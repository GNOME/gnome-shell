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

#define clutter_group_find_child_by_id               clutter_group_find_child_by_id_REPLACED_BY_clutter_container_find_child_by_name

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
#define clutter_actor_set_scale_with_gravity         clutter_actor_set_scale_with_gravity_DEPRECATED_BY_clutter_actor_set_anchor_point_from_gravity
#define clutter_actor_set_scale_with_gravityx        clutter_actor_set_scale_with_gravity_DEPRECATED_BY_clutter_actor_set_anchor_point_from_gravity

#define clutter_entry_set_position                   clutter_entry_set_position_REPLACED_BY_clutter_entry_set_cursor_position
#define clutter_entry_get_position                   clutter_entry_get_position_REPLACED_BY_clutter_entry_get_cursor_position

#define clutter_shader_bind                          clutter_shader_bind_REPLACED_BY_clutter_shader_compile
#define clutter_shader_is_bound                      clutter_shader_is_bound_REPLACED_BY_clutter_shader_is_compiled

#define clutter_texture_new_from_pixbuf              clutter_texture_new_from_pixbuf_DEPRECATED_BY_clutter_texture_new_from_file_OR_clutter_texture_new_AND_clutter_texture_set_from_rgb_data
#define clutter_texture_set_pixbuf                   clutter_texture_set_pixbuf+DEPRECATED_BY_clutter_texture_set_from_rgb_data

#define clutter_actor_query_coords                   clutter_actor_query_coords_REPLACED_BY_clutter_actor_get_preferred_size_OR_clutter_actor_get_allocation_box
#define clutter_actor_request_coords                 clutter_actor_request_coords_REPLACED_BY_clutter_actor_allocate             

#define clutter_actor_get_abs_position               clutter_actor_get_abs_position_REPLACED_BY_clutter_actor_get_transformed_position
#define clutter_actor_get_abs_size                   clutter_actor_get_abs_size_REPLACED_BY_clutter_actor_get_transformed_size
#define clutter_actor_get_abs_opacity                clutter_actor_get_abs_opacity_REPLACED_BY_clutter_actor_get_paint_opacity

#define CLUTTER_ALPHA_RAMP_INC          CLUTTER_ALPHA_RAMP_INC_DEPRECATED_BY_clutter_ramp_inc_func
#define CLUTTER_ALPHA_RAMP_DEC          CLUTTER_ALPHA_RAMP_DEC_DEPRECATED_BY_clutter_ramp_dec_func
#define CLUTTER_ALPHA_RAMP              CLUTTER_ALPHA_RAMP_DEPRECATED_BY_clutter_ramp_func
#define CLUTTER_ALPHA_SINE_INC          CLUTTER_ALPHA_SINE_INC_DEPRECATED_BY_clutter_sine_inc_func
#define CLUTTER_ALPHA_SINE_DEC          CLUTTER_ALPHA_SINE_DEC_DEPRECATED_BY_clutter_sine_dec_func
#define CLUTTER_ALPHA_SINE_HALF         CLUTTER_ALPHA_SINE_HALF_DEPRECATED_BY_clutter_sine_half_func
#define CLUTTER_ALPHA_SINE              CLUTTER_ALPHA_SINE_DEPRECATED_BY_clutter_sine_func
#define CLUTTER_ALPHA_SQUARE            CLUTTER_ALPHA_SQUARE_DEPRECATED_BY_clutter_quare_func
#define CLUTTER_ALPHA_SMOOTHSTEP_INC    CLUTTER_ALPHA_SMOOTHSTEP_INC_DEPRECATED_BY_clutter_smoothstep_inc_func
#define CLUTTER_ALPHA_SMOOTHSTEP_DEC    CLUTTER_ALPHA_SMOOTHSTEP_DEC_DEPRECATED_BY_clutter_smoothstep_dec_func
#define CLUTTER_ALPHA_EXP_INC           CLUTTER_ALPHA_EXP_INC_DEPRECATED_BY_clutter_exp_inc_func
#define CLUTTER_ALPHA_EXP_DEC           CLUTTER_ALPHA_EXP_DEC_DEPRECATED_BY_clutter_exp_dec_func

#endif /* CLUTTER_DEPRECATED_H */
