/*** cogl_fixed_fragment_shader_variables_start ***/

/* There is no default precision for floats in fragment shaders in
   GLES 2 so we need to define one */
precision highp float;

/*** cogl_fixed_fragment_shader_inputs ***/

/* Inputs from the vertex shader */
varying vec4       frag_color;
varying float      fog_amount;

/*** cogl_fixed_fragment_shader_texturing_options ***/

/* Texturing options */

/*** cogl_fixed_fragment_shader_fogging_options ***/

/* Fogging options */
uniform vec4       fog_color;

/* Alpha test options */
uniform float      alpha_test_ref;

/*** cogl_fixed_fragment_shader_main_declare ***/

void
main (void)
{
  /*** cogl_fixed_fragment_shader_main_start ***/


  /*** cogl_fixed_fragment_shader_fog ***/

  /* Mix the calculated color with the fog color */
  gl_FragColor.rgb = mix (fog_color.rgb, gl_FragColor.rgb, fog_amount);

  /* Alpha testing */

  /*** cogl_fixed_fragment_shader_alpha_never ***/
  discard;
  /*** cogl_fixed_fragment_shader_alpha_less ***/
  if (gl_FragColor.a >= alpha_test_ref)
    discard;
  /*** cogl_fixed_fragment_shader_alpha_equal ***/
  if (gl_FragColor.a != alpha_test_ref)
    discard;
  /*** cogl_fixed_fragment_shader_alpha_lequal ***/
  if (gl_FragColor.a > alpha_test_ref)
    discard;
  /*** cogl_fixed_fragment_shader_alpha_greater ***/
  if (gl_FragColor.a <= alpha_test_ref)
    discard;
  /*** cogl_fixed_fragment_shader_alpha_notequal ***/
  if (gl_FragColor.a == alpha_test_ref)
    discard;
  /*** cogl_fixed_fragment_shader_alpha_gequal ***/
  if (gl_FragColor.a < alpha_test_ref)
    discard;

  /*** cogl_fixed_fragment_shader_end ***/
}
