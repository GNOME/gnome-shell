/* Per vertex attributes */
attribute vec4 vertex_attrib;
attribute vec4 tex_coord_attrib;
attribute vec4 color_attrib;

/* Transformation matrices */
uniform mat4 mvp_matrix; /* combined modelview and projection matrix */
uniform mat4 texture_matrix;

/* Outputs to the fragment shader */
varying vec4 frag_color;
varying vec2 tex_coord;

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
}
