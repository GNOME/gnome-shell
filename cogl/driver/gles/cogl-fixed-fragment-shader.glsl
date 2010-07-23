/*** _cogl_fixed_fragment_shader_variables_start ***/

/* There is no default precision for floats in fragment shaders in
   GLES 2 so we need to define one */
precision highp float;

/*** _cogl_fixed_fragment_shader_inputs ***/

/* Inputs from the vertex shader */
varying vec4 _cogl_color;
varying float _cogl_fog_amount;

/*** _cogl_fixed_fragment_shader_texturing_options ***/

/* Texturing options */

/*** _cogl_fixed_fragment_shader_fogging_options ***/

/* Fogging options */
uniform vec4 _cogl_fog_color;

/* Alpha test options */
uniform float _cogl_alpha_test_ref;

/*** _cogl_fixed_fragment_shader_main_declare ***/

void
main (void)
{
  /*** _cogl_fixed_fragment_shader_main_start ***/


  /*** _cogl_fixed_fragment_shader_fog ***/

  /* Mix the calculated color with the fog color */
  gl_FragColor.rgb = mix (_cogl_fog_color.rgb, gl_FragColor.rgb,
                          _cogl_fog_amount);

  /* Alpha testing */

  /*** _cogl_fixed_fragment_shader_alpha_never ***/
  discard;
  /*** _cogl_fixed_fragment_shader_alpha_less ***/
  if (gl_FragColor.a >= _cogl_alpha_test_ref)
    discard;
  /*** _cogl_fixed_fragment_shader_alpha_equal ***/
  if (gl_FragColor.a != _cogl_alpha_test_ref)
    discard;
  /*** _cogl_fixed_fragment_shader_alpha_lequal ***/
  if (gl_FragColor.a > _cogl_alpha_test_ref)
    discard;
  /*** _cogl_fixed_fragment_shader_alpha_greater ***/
  if (gl_FragColor.a <= _cogl_alpha_test_ref)
    discard;
  /*** _cogl_fixed_fragment_shader_alpha_notequal ***/
  if (gl_FragColor.a == _cogl_alpha_test_ref)
    discard;
  /*** _cogl_fixed_fragment_shader_alpha_gequal ***/
  if (gl_FragColor.a < _cogl_alpha_test_ref)
    discard;

  /*** _cogl_fixed_fragment_shader_end ***/
}

