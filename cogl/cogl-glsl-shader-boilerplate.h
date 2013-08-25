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

#define _COGL_COMMON_SHADER_BOILERPLATE \
  "#define COGL_VERSION 100\n" \
  "\n" \
  "uniform mat4 cogl_modelview_matrix;\n" \
  "uniform mat4 cogl_modelview_projection_matrix;\n"  \
  "uniform mat4 cogl_projection_matrix;\n"

/* This declares all of the variables that we might need. This is
 * working on the assumption that the compiler will optimise them out
 * if they are not actually used. The GLSL spec at least implies that
 * this will happen for varyings but it doesn't explicitly so for
 * attributes */
#define _COGL_VERTEX_SHADER_BOILERPLATE \
  _COGL_COMMON_SHADER_BOILERPLATE \
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

#define _COGL_FRAGMENT_SHADER_BOILERPLATE \
  "#ifdef GL_ES\n" \
  "precision highp float;\n" \
  "#endif\n" \
  _COGL_COMMON_SHADER_BOILERPLATE \
  "\n" \
  "varying vec4 _cogl_color;\n" \
  "\n" \
  "#define cogl_color_in _cogl_color\n" \
  "#define cogl_tex_coord_in _cogl_tex_coord\n" \
  "\n" \
  "#define cogl_color_out gl_FragColor\n" \
  "#define cogl_depth_out gl_FragDepth\n" \
  "\n" \
  "#define cogl_front_facing gl_FrontFacing\n" \
  "\n" \
  "#define cogl_point_coord gl_PointCoord\n"
#if 0
  /* GLSL 1.2 has a bottom left origin, though later versions
   * allow use of an origin_upper_left keyword which would be
   * more appropriate for Cogl. */
  "#define coglFragCoord   gl_FragCoord\n"
#endif

#endif /* __COGL_SHADER_BOILERPLATE_H */

