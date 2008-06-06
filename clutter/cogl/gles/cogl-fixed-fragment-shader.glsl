/* There is no default precision for floats in fragment shaders in
   GLES 2 so we need to define one */
precision mediump float;

/* Inputs from the vertex shader */
varying vec4       frag_color;
varying vec2       tex_coord;
varying float      fog_amount;

/* Texturing options */
uniform bool       texture_2d_enabled;
uniform sampler2D  texture_unit;
uniform bool       alpha_only;

/* Fogging options */
uniform bool       fog_enabled;
uniform vec4       fog_color;

/* Alpha test options */
uniform bool       alpha_test_enabled;
uniform int        alpha_test_func;
uniform float      alpha_test_ref;

/* Alpha test functions */
const int GL_NEVER    = 0x0200;
const int GL_LESS     = 0x0201;
const int GL_EQUAL    = 0x0202;
const int GL_LEQUAL   = 0x0203;
const int GL_GREATER  = 0x0204;
const int GL_NOTEQUAL = 0x0205;
const int GL_GEQUAL   = 0x0206;

void
main (void)
{
  if (texture_2d_enabled)
    {
      if (alpha_only)
	{
	  /* If the texture only has an alpha channel (eg, with the
	     textures from the pango renderer) then the RGB components
	     will be black. We want to use the RGB from the current
	     color in that case */
	  gl_FragColor = frag_color;
	  gl_FragColor.a *= texture2D (texture_unit, tex_coord).a;
	}
      else
	gl_FragColor = frag_color * texture2D (texture_unit, tex_coord);
    }
  else
    gl_FragColor = frag_color;

  if (fog_enabled)
    /* Mix the calculated color with the fog color */
    gl_FragColor.rgb = mix (fog_color.rgb, gl_FragColor.rgb, fog_amount);

  /* Alpha testing */
  if (alpha_test_enabled)
    {
      if (alpha_test_func == GL_NEVER)
	discard;
      else if (alpha_test_func == GL_LESS)
	{
	  if (gl_FragColor.a >= alpha_test_ref)
	    discard;
	}
      else if (alpha_test_func == GL_EQUAL)
	{
	  if (gl_FragColor.a != alpha_test_ref)
	    discard;
	}
      else if (alpha_test_func == GL_LEQUAL)
	{
	  if (gl_FragColor.a > alpha_test_ref)
	    discard;
	}
      else if (alpha_test_func == GL_GREATER)
	{
	  if (gl_FragColor.a <= alpha_test_ref)
	    discard;
	}
      else if (alpha_test_func == GL_NOTEQUAL)
	{
	  if (gl_FragColor.a == alpha_test_ref)
	    discard;
	}
      else if (alpha_test_func == GL_GEQUAL)
	{
	  if (gl_FragColor.a < alpha_test_ref)
	    discard;
	}
    }
}
