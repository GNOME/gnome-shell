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
#define clutter_actor_set_scale_with_gravityx        clutter_actor_set_scale_with_gravityx_DEPRECATED_BY_clutter_actor_set_scale_with_gravity

#define clutter_entry_set_position                   clutter_entry_set_position_REPLACED_BY_clutter_entry_set_cursor_position
#define clutter_entry_get_position                   clutter_entry_get_position_REPLACED_BY_clutter_entry_get_cursor_position

#define clutter_shader_bind                          clutter_shader_bind_REPLACED_BY_clutter_shader_compile
#define clutter_shader_is_bound                      clutter_shader_is_bound_REPLACED_BY_clutter_shader_is_compiled

#define clutter_texture_new_from_pixbuf              clutter_texture_new_from_pixbuf_DEPRECATED_BY_clutter_texture_new_from_file_OR_clutter_texture_new_AND_clutter_texture_set_from_rgb_data
#define clutter_texture_set_pixbuf                   clutter_texture_set_pixbuf_DEPRECATED_BY_clutter_texture_set_from_rgb_data

#define clutter_actor_query_coords                   clutter_actor_query_coords_REPLACED_BY_clutter_actor_get_preferred_size_OR_clutter_actor_get_allocation_box
#define clutter_actor_request_coords                 clutter_actor_request_coords_REPLACED_BY_clutter_actor_allocate             

#define clutter_actor_get_abs_position               clutter_actor_get_abs_position_REPLACED_BY_clutter_actor_get_transformed_position
#define clutter_actor_get_abs_size                   clutter_actor_get_abs_size_REPLACED_BY_clutter_actor_get_transformed_size
#define clutter_actor_get_abs_opacity                clutter_actor_get_abs_opacity_REPLACED_BY_clutter_actor_get_paint_opacity

#define clutter_stage_get_resolution    clutter_backend_get_resolution
#define clutter_stage_get_resolutionx   clutter_backend_get_resolution

#define clutter_set_use_mipmapped_text               clutter_actor_set_use_mipmapped_text_REPLACED_BY_clutter_set_font_flags
#define clutter_get_use_mipmapped_text               clutter_actor_get_use_mipmapped_text_REPLACED_BY_clutter_get_font_flags

#define clutter_color_parse     clutter_color_parse_REPLACED_BY_clutter_color_from_string
#define clutter_color_from_hlsx clutter_color_from_hlsx_DEPRECATED_BY_clutter_color_from_hls
#define clutter_color_to_hlsx   clutter_color_to_hlsx_DEPRECATED_BY_clutter_color_to_hls
#define clutter_color_shadex    clutter_color_shadex_DEPRECATED_BY_clutter_color_shade

#define clutter_stage_set_perspectivex  clutter_stage_set_perspectivex_DEPRECATED_BY_clutter_stage_set_perspective
#define clutter_stage_set_fogx          clutter_stage_set_fogx_DEPRECATED_BY_clutter_stage_set_fogx

#define clutter_actor_set_rotationx     clutter_actor_set_rotationx_DEPRECATED_BY_clutter_actor_set_rotation
#define clutter_actor_get_rotationx     clutter_actor_get_rotationx_DEPRECATED_BY_clutter_actor_get_rotation
#define clutter_actor_set_scalex        clutter_actor_set_scalex_DEPRECATED_BY_clutter_actor_set_scalex
#define clutter_actor_get_scalex        clutter_actor_get_scalex_DEPRECATED_BY_clutter_actor_get_scalex

#define CLUTTER_ALPHA_RAMP_INC          clutter_ramp_inc_func
#define CLUTTER_ALPHA_RAMP_DEC          clutter_ramp_dec_func
#define CLUTTER_ALPHA_RAMP              clutter_ramp_func
#define CLUTTER_ALPHA_SINE              clutter_sine_func
#define CLUTTER_ALPHA_SINE_INC          clutter_sine_inc_func
#define CLUTTER_ALPHA_SINE_DEC          clutter_sine_dec_func
#define CLUTTER_ALPHA_SINE_HALF         clutter_sine_half_func
#define CLUTTER_ALPHA_SQUARE            clutter_square_func
#define CLUTTER_ALPHA_SMOOTHSTEP_INC    clutter_smoothstep_inc_func
#define CLUTTER_ALPHA_SMOOTHSTEP_DEC    clutter_smoothstep_dec_func
#define CLUTTER_ALPHA_EXP_INC           clutter_exp_inc_func
#define CLUTTER_ALPHA_EXP_DEC           clutter_exp_dec_func

#define clutter_ramp_inc_func           clutter_ramp_inc_func_DEPRECATED_BY_CLUTTER_LINEAR
#define clutter_ramp_dec_func           clutter_ramp_dec_func_DEPRECATED_BY_CLUTTER_LINEAR
#define clutter_ramp_func               clutter_ramp_func_DEPRECATED_BY_CLUTTER_LINEAR
#define clutter_sine_inc_func           clutter_sine_inc_func_DEPRECATED_BY_CLUTTER_EASE_OUT_SINE
#define clutter_sine_dec_func           clutter_sine_dec_func_DEPRECATED_BY_CLUTTER_EASE_IN_SINE
#define clutter_sine_half_func          clutter_sine_half_func_DEPRECATED_BY_CLUTTER_EASE_IN_OUT_SINE
#define clutter_sine_func               clutter_sine_func_DEPRECATED_BY_CLUTTER_EASE_IN_OUT_SINE
#define clutter_square_func             clutter_square_func_REMOVED
#define clutter_smoothstep_inc_func     clutter_smoothstep_inc_func_DEPRECATED_BY_CLUTTER_EASE_IN_CUBIC
#define clutter_smoothstep_dec_func     clutter_smoothstep_dec_func_DEPRECATED_BY_CLUTTER_EASE_OUT_CUBIC
#define clutter_exp_inc_func            clutter_exp_inc_func_DEPRECATED_BY_CLUTTER_EASE_IN_EXPO
#define clutter_exp_dec_func            clutter_exp_dec_func_DEPRECATED_BY_CLUTTER_EASE_OUT_EXPO

#define clutter_behaviour_path_get_knots        clutter_behaviour_path_get_knots_REPLACED_BY_clutter_path_get_nodes
#define clutter_behaviour_path_append_knots     clutter_behaviour_path_append_knots_REPLACED_BY_clutter_path_add_string
#define clutter_behaviour_path_append_knot      clutter_behaviour_path_append_knot_REPLACED_BY_clutter_path_add_string
#define clutter_behaviour_path_insert_knot      clutter_behaviour_path_insert_knot_REPLACED_BY_clutter_path_insert_node
#define clutter_behaviour_path_remove_knot      clutter_behaviour_path_remove_knot_REPLACED_BY_clutter_path_remove_node
#define clutter_behaviour_path_clear            clutter_behaviour_path_clear_REPLACED_BY_clutter_path_clear

#define ClutterFixed    ClutterFixed_REPLACED_BY_CoglFixed
#define ClutterAngle    ClutterAngle_REPLACED_BY_CoglAngle

#define CFX_ONE         CFX_ONE_REPLACED_BY_COGL_FIXED_1
#define CFX_HALF        CFX_HALF_REPLACED_BY_COGL_FIXED_0_5
#define CFX_PI          CFX_PI_REPLACED_BY_COGL_FIXED_PI
#define CFX_2PI         CFX_2PI_REPLACED_BY_COGL_FIXED_2_PI

#define CLUTTER_INT_TO_FIXED    CLUTTER_INT_TO_FIXED_REPLACED_BY_COGL_FIXED_FROM_INT
#define CLUTTER_FIXED_TO_INT    CLUTTER_FIXED_TO_INT_REPLACED_BY_COGL_FIXED_TO_INT
#define CLUTTER_FLOAT_TO_FIXED  CLUTTER_FLOAT_TO_FIXED_REPLACED_BY_COGL_FIXED_FROM_FLOAT
#define CLUTTER_FIXED_TO_FLOAT  CLUTTER_FIXED_TO_FLOAT_REPLACED_BY_COGL_FIXED_TO_FLOAT

#define CLUTTER_FIXED_MUL       CLUTTER_FIXED_MUL_REPLACED_BY_COGL_FIXED_FAST_MUL
#define CLUTTER_FIXED_DIV       CLUTTER_FIXED_DIV_REPLACED_BY_COGL_FIXED_FAST_DIV

#define clutter_qmulx   clutter_qmulx_REPLACED_BY_COGL_FIXED_MUL
#define clutter_qdivx   clutter_qdivx_REPLACED_BY_COGL_FIXED_DIV
#define clutter_sinx    clutter_sinx_REPLACED_BY_cogl_fixed_sin
#define clutter_cosx    clutter_cosx_REPLACED_BY_cogl_fixed_cos

#define clutter_media_set_position      clutter_media_set_position_DEPRECATED_BY_clutter_media_set_progress
#define clutter_media_get_position      clutter_media_get_position_DEPRECATED_BY_clutter_media_get_progress
#define clutter_media_set_volume        clutter_media_set_volume_DEPRECATED_BY_clutter_media_set_audio_volume
#define clutter_media_get_volume        clutter_media_get_volume_DEPRECATED_BY_clutter_media_get_audio_volume
#define clutter_media_get_buffer_percent        clutter_media_get_buffer_percent_DEPRECATED_BY_clutter_media_get_buffer_fill

#define CLUTTER_TYPE_LABEL      CLUTTER_TYPE_LABEL_DEPRECATED_BY_CLUTTER_TYPE_TEXT
#define CLUTTER_TYPE_ENTRY      CLUTTER_TYPE_ENTRY_DEPRECATED_BY_CLUTTER_TYPE_TEXT
#define clutter_label_new       clutter_label_new_DEPRECATED_BY_clutter_text_new
#define clutter_entry_new       clutter_entry_new_DEPRECATED_BY_clutter_text_new

#define CLUTTER_TYPE_TEXTURE_HANDLE     CLUTTER_TYPE_TEXTURE_HANDLE_REPLACED_BY_COGL_TYPE_HANDLE

#endif /* CLUTTER_DEPRECATED_H */
