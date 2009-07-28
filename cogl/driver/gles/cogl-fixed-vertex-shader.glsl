/*** cogl_fixed_vertex_shader_per_vertex_attribs ***/

/* Per vertex attributes */
attribute vec4     vertex_attrib;
attribute vec4     color_attrib;

/*** cogl_fixed_vertex_shader_transform_matrices ***/

/* Transformation matrices */
uniform mat4       modelview_matrix;
uniform mat4       mvp_matrix; /* combined modelview and projection matrix */

/*** cogl_fixed_vertex_shader_output_variables ***/

/* Outputs to the fragment shader */
varying vec4       frag_color;
varying float      fog_amount;

/*** cogl_fixed_vertex_shader_fogging_options ***/

/* Fogging options */
uniform float      fog_density;
uniform float      fog_start;
uniform float      fog_end;

/*** cogl_fixed_vertex_shader_main_start ***/

void
main (void)
{
  vec4 transformed_tex_coord;

  /* Calculate the transformed position */
  gl_Position = mvp_matrix * vertex_attrib;

  /* Calculate the transformed texture coordinate */

  /*** cogl_fixed_vertex_shader_frag_color_start ***/

  /* Pass the interpolated vertex color on to the fragment shader */
  frag_color = color_attrib;

  /*** cogl_fixed_vertex_shader_fog_start ***/

  /* Estimate the distance from the eye using just the z-coordinate to
     use as the fog coord */
  vec4 eye_coord = modelview_matrix * vertex_attrib;
  float fog_coord = abs (eye_coord.z / eye_coord.w);

  /* Calculate the fog amount per-vertex and interpolate it for the
     fragment shader */

  /*** cogl_fixed_vertex_shader_fog_exp ***/
  fog_amount = exp (-fog_density * fog_coord);
  /*** cogl_fixed_vertex_shader_fog_exp2 ***/
  fog_amount = exp (-fog_density * fog_coord
		    * fog_density * fog_coord);
  /*** cogl_fixed_vertex_shader_fog_linear ***/
  fog_amount = (fog_end - fog_coord) / (fog_end - fog_start);

  /*** cogl_fixed_vertex_shader_fog_end ***/
  fog_amount = clamp (fog_amount, 0.0, 1.0);

  /*** cogl_fixed_vertex_shader_end ***/
}
