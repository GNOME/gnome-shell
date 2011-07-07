/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_SHADER_BOILERPLATE_H
#define __COGL_SHADER_BOILERPLATE_H

#include "cogl.h"

#define _COGL_COMMON_SHADER_BOILERPLATE_GL \
  "#define COGL_VERSION 100\n" \
  "\n" \
  "#define cogl_modelview_matrix gl_ModelViewMatrix\n" \
  "#define cogl_modelview_projection_matrix gl_ModelViewProjectionMatrix\n" \
  "#define cogl_projection_matrix gl_ProjectionMatrix\n" \
  "#define cogl_texture_matrix gl_TextureMatrix\n" \
  "\n"

#define _COGL_VERTEX_SHADER_BOILERPLATE_GL \
  _COGL_COMMON_SHADER_BOILERPLATE_GL \
  "#define cogl_position_in gl_Vertex\n" \
  "#define cogl_color_in gl_Color\n" \
  "#define cogl_tex_coord_in  gl_MultiTexCoord0\n" \
  "#define cogl_tex_coord0_in gl_MultiTexCoord0\n" \
  "#define cogl_tex_coord1_in gl_MultiTexCoord1\n" \
  "#define cogl_tex_coord2_in gl_MultiTexCoord2\n" \
  "#define cogl_tex_coord3_in gl_MultiTexCoord3\n" \
  "#define cogl_tex_coord4_in gl_MultiTexCoord4\n" \
  "#define cogl_tex_coord5_in gl_MultiTexCoord5\n" \
  "#define cogl_tex_coord6_in gl_MultiTexCoord6\n" \
  "#define cogl_tex_coord7_in gl_MultiTexCoord7\n" \
  "#define cogl_normal_in gl_Normal\n" \
  "\n" \
  "#define cogl_position_out gl_Position\n" \
  "#define cogl_point_size_out gl_PointSize\n" \
  "#define cogl_color_out gl_FrontColor\n" \
  "#define cogl_tex_coord_out gl_TexCoord\n"

#define _COGL_FRAGMENT_SHADER_BOILERPLATE_GL \
  _COGL_COMMON_SHADER_BOILERPLATE_GL \
  "#define cogl_color_in gl_Color\n" \
  "#define cogl_tex_coord_in gl_TexCoord\n" \
  "\n" \
  "#define cogl_color_out gl_FragColor\n" \
  "#define cogl_depth_out gl_FragDepth\n" \
  "\n" \
  "#define cogl_front_facing gl_FrontFacing\n"
#if 0
  /* GLSL 1.2 has a bottom left origin, though later versions
   * allow use of an origin_upper_left keyword which would be
   * more appropriate for Cogl. */
  "#define coglFragCoord   gl_FragCoord\n"
#endif

#define _COGL_COMMON_SHADER_BOILERPLATE_GLES2 \
  "#define COGL_VERSION 100\n" \
  "\n" \
  "uniform mat4 cogl_modelview_matrix;\n" \
  "uniform mat4 cogl_modelview_projection_matrix;\n"  \
  "uniform mat4 cogl_projection_matrix;\n" \
  "uniform float cogl_point_size_in;\n"

/* This declares all of the variables that we might need. This is
   working on the assumption that the compiler will optimise them out
   if they are not actually used. The GLSL spec for GLES at least
   implies that this will happen for varyings but it doesn't
   explicitly so for attributes */
#define _COGL_VERTEX_SHADER_BOILERPLATE_GLES2 \
  _COGL_COMMON_SHADER_BOILERPLATE_GLES2 \
  "#define cogl_color_out _cogl_color\n" \
  "varying vec4 _cogl_color;\n" \
  "#define cogl_tex_coord_out _cogl_tex_coord\n" \
  "#define cogl_position_out gl_Position\n" \
  "#define cogl_point_size_out gl_PointSize\n" \
  "\n" \
  "attribute vec4 cogl_color_in;\n" \
  "attribute vec4 cogl_position_in;\n" \
  "#define cogl_tex_coord_in cogl_tex_coord0_in;\n" \
  "attribute vec3 cogl_normal_in;\n"

#define _COGL_FRAGMENT_SHADER_BOILERPLATE_GLES2 \
  "#if __VERSION__ == 100\n" \
  "precision highp float;\n" \
  "#endif\n" \
  _COGL_COMMON_SHADER_BOILERPLATE_GLES2 \
  "\n" \
  "varying vec4 _cogl_color;\n" \
  "\n" \
  "#define cogl_color_in _cogl_color\n" \
  "#define cogl_tex_coord_in _cogl_tex_coord\n" \
  "\n" \
  "#define cogl_color_out gl_FragColor\n" \
  "#define cogl_depth_out gl_FragDepth\n" \
  "\n" \
  "#define cogl_front_facing gl_FrontFacing\n"

#endif /* __COGL_SHADER_BOILERPLATE_H */

