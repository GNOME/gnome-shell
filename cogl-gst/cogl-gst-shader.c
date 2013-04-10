/*
 * Cogl-GStreamer.
 *
 * GStreamer integration library for Cogl.
 *
 * cogl-gst-video-sink-private.h - Miscellaneous video sink functions
 *
 * Authored by Jonathan Matthew  <jonathan@kaolin.wh9.net>,
 *             Chris Lord        <chris@openedhand.com>
 *             Damien Lespiau    <damien.lespiau@intel.com>
 *             Matthew Allum     <mallum@openedhand.com>
 *             Plamena Manolova  <plamena.n.manolova@intel.com>
 *
 * Copyright (C) 2007, 2008 OpenedHand
 * Copyright (C) 2009, 2010, 2013 Intel Corporation
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-gst-shader-private.h"

const char
_cogl_gst_shader_rgba_to_rgba_decl[] =
  "vec4\n"
  "cogl_gst_sample_video (vec2 UV)\n"
  "{\n"
  "  return texture2D (cogl_sampler0, UV);\n"
  "}\n";

const char
_cogl_gst_shader_yv12_to_rgba_decl[] =
  "vec4\n"
  "cogl_gst_sample_video (vec2 UV)\n"
  "{\n"
  "  float y = 1.1640625 * (texture2D (cogl_sampler0, UV).a - 0.0625);\n"
  "  float u = texture2D (cogl_sampler1, UV).a - 0.5;\n"
  "  float v = texture2D (cogl_sampler2, UV).a - 0.5;\n"
  "  vec4 color;\n"
  "  color.r = y + 1.59765625 * v;\n"
  "  color.g = y - 0.390625 * u - 0.8125 * v;\n"
  "  color.b = y + 2.015625 * u;\n"
  "  color.a = 1.0;\n"
  "  return color;\n"
  "}\n";

const char
_cogl_gst_shader_ayuv_to_rgba_decl[] =
  "vec4\n"
  "cogl_gst_sample_video (vec2 UV)\n"
  "{\n"
  "  vec4 color = texture2D (cogl_sampler0, UV);\n"
  "  float y = 1.1640625 * (color.g - 0.0625);\n"
  "  float u = color.b - 0.5;\n"
  "  float v = color.a - 0.5;\n"
  "  color.a = color.r;\n"
  "  color.r = y + 1.59765625 * v;\n"
  "  color.g = y - 0.390625 * u - 0.8125 * v;\n"
  "  color.b = y + 2.015625 * u;\n"
  "  return color;\n"
  "}\n";

const char
_cogl_gst_shader_default_sample[] =
  "  cogl_layer *= cogl_gst_sample_video (cogl_tex_coord0_in.st);\n";
