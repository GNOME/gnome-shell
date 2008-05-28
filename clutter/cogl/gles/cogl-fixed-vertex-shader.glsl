/* Per vertex attributes */
attribute vec4     vertex_attrib;
attribute vec4     tex_coord_attrib;
attribute vec4     color_attrib;

/* Transformation matrices */
uniform mat4       modelview_matrix;
uniform mat4       mvp_matrix; /* combined modelview and projection matrix */
uniform mat4       texture_matrix;

/* Outputs to the fragment shader */
varying vec4       frag_color;
varying vec2       tex_coord;
varying float      fog_amount;

/* Fogging options */
uniform bool       fog_enabled;
uniform int        fog_mode;
uniform float      fog_density;
uniform float      fog_start;
uniform float      fog_end;

/* Fogging modes */
const int          GL_LINEAR = 0x2601;
const int          GL_EXP    = 0x0800;
const int          GL_EXP2   = 0x0801;

void
main (void)
{
  /* Calculate the transformed position */
  gl_Position = mvp_matrix * vertex_attrib;

  /* Calculate the transformed texture coordinate */
  vec4 transformed_tex_coord = texture_matrix * tex_coord_attrib;
  tex_coord = transformed_tex_coord.st / transformed_tex_coord.q;

  /* Pass the interpolated vertex color on to the fragment shader */
  frag_color = color_attrib;

  if (fog_enabled)
    {
      /* Estimate the distance from the eye using just the
	 z-coordinate to use as the fog coord */
      vec4 eye_coord = modelview_matrix * vertex_attrib;
      float fog_coord = abs (eye_coord.z / eye_coord.w);

      /* Calculate the fog amount per-vertex and interpolate it for
	 the fragment shader */
      if (fog_mode == GL_EXP)
	fog_amount = exp (-fog_density * fog_coord);
      else if (fog_mode == GL_EXP2)
	fog_amount = exp (-fog_density * fog_coord
			  * fog_density * fog_coord);
      else
	fog_amount = (fog_end - fog_coord) / (fog_end - fog_start);

      fog_amount = clamp (fog_amount, 0.0, 1.0);
    }
}
