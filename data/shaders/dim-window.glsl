#version 100
uniform sampler2D sampler0;
uniform float fraction;
uniform float height;
const float c = -0.2;
const float border_max_height = 60.0;

mat4 contrast = mat4 (1.0 + c, 0.0, 0.0, 0.0,
                      0.0, 1.0 + c, 0.0, 0.0,
                      0.0, 0.0, 1.0 + c, 0.0,
                      0.0, 0.0, 0.0, 1.0);
vec4 off = vec4(0.633, 0.633, 0.633, 0);
void main()
{
  vec4 color = texture2D(sampler0, gl_TexCoord[0].st);
  float y = height * gl_TexCoord[0][1];

  // To reduce contrast, blend with a mid gray
  gl_FragColor = color * contrast - off * c;

  // We only fully dim at a distance of BORDER_MAX_HEIGHT from the edge and
  // when the fraction is 1.0. For other locations and fractions we linearly
  // interpolate back to the original undimmed color.
  gl_FragColor = color + (gl_FragColor - color) * min(y / border_max_height, 1.0);
  gl_FragColor = color + (gl_FragColor - color) * fraction;
}
