/*** _cogl_fixed_vertex_shader_per_vertex_attribs ***/

/* Per vertex attributes */
attribute vec4 cogl_position_in;
attribute vec4 cogl_color_in;

/*** _cogl_fixed_vertex_shader_transform_matrices ***/

/* Transformation matrices */
uniform mat4 cogl_modelview_matrix;
uniform mat4 cogl_modelview_projection_matrix; /* combined modelview and projection matrix */

/*** _cogl_fixed_vertex_shader_output_variables ***/

/* Outputs to the fragment shader */
varying vec4 _cogl_color;
varying float _cogl_fog_amount;

/*** _cogl_fixed_vertex_shader_fogging_options ***/

/* Fogging options */
uniform float _cogl_fog_density;
uniform float _cogl_fog_start;
uniform float _cogl_fog_end;

/* Point options */
uniform float cogl_point_size_in;

/*** _cogl_fixed_vertex_shader_main_start ***/

void
main (void)
{
  vec4 transformed_tex_coord;

  /* Calculate the transformed position */
  gl_Position = cogl_modelview_projection_matrix * cogl_position_in;

  /* Copy across the point size from the uniform */
  gl_PointSize = cogl_point_size_in;

  /* Calculate the transformed texture coordinate */

  /*** _cogl_fixed_vertex_shader_frag_color_start ***/

  /* Pass the interpolated vertex color on to the fragment shader */
  _cogl_color = cogl_color_in;

  /*** _cogl_fixed_vertex_shader_fog_start ***/

  /* Estimate the distance from the eye using just the z-coordinate to
     use as the fog coord */
  vec4 eye_coord = cogl_modelview_matrix * cogl_position_in;
  float fog_coord = abs (eye_coord.z / eye_coord.w);

  /* Calculate the fog amount per-vertex and interpolate it for the
     fragment shader */

  /*** _cogl_fixed_vertex_shader_fog_exp ***/
  _cogl_fog_amount = exp (-fog_density * fog_coord);
  /*** _cogl_fixed_vertex_shader_fog_exp2 ***/
  _cogl_fog_amount = exp (-_cogl_fog_density * fog_coord
		    * _cogl_fog_density * fog_coord);
  /*** _cogl_fixed_vertex_shader_fog_linear ***/
  _cogl_fog_amount = (_cogl_fog_end - fog_coord) /
                     (_cogl_fog_end - _cogl_fog_start);

  /*** _cogl_fixed_vertex_shader_fog_end ***/
  _cogl_fog_amount = clamp (_cogl_fog_amount, 0.0, 1.0);

  /*** _cogl_fixed_vertex_shader_end ***/
}

