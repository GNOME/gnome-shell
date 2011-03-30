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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

OPT (HANDLE,
     "Cogl Tracing",
     "ref-counts",
     "CoglObject references",
     "Debug ref counting issues for CoglObjects")
OPT (SLICING,
     "Cogl Tracing",
     "slicing",
     "Trace Texture Slicing",
     "debug the creation of texture slices")
OPT (ATLAS,
     "Cogl Tracing",
     "atlas",
     "Trace Atlas Textures",
     "Debug texture atlas management")
OPT (BLEND_STRINGS,
     "Cogl Tracing",
     "blend-strings",
     "Trace Blend Strings",
     "Debug CoglBlendString parsing")
OPT (JOURNAL,
     "Cogl Tracing",
     "journal",
     "Trace Journal",
     "View all the geometry passing through the journal")
OPT (BATCHING,
     "Cogl Tracing",
     "batching",
     "Trace Batching",
     "Show how geometry is being batched in the journal")
OPT (MATRICES,
     "Cogl Tracing",
     "matrices",
     "Trace matrices",
     "Trace all matrix manipulation")
/* XXX we should replace the "draw" option its very hand wavy... */
OPT (DRAW,
     "Cogl Tracing",
     "draw",
     "Trace Misc Drawing",
     "Trace some misc drawing operations")
OPT (PANGO,
     "Cogl Tracing",
     "pango",
     "Trace Pango Renderer",
     "Trace the Cogl Pango renderer")
OPT (TEXTURE_PIXMAP,
     "Cogl Tracing",
     "texture-pixmap",
     "Trace CoglTexturePixmap backend",
     "Trace the Cogl texture pixmap backend")
OPT (RECTANGLES,
     "Visualize",
     "rectangles",
     "Outline rectangles",
     "Add wire outlines for all rectangular geometry")
OPT (WIREFRAME,
     "Visualize",
     "wireframe",
     "Show wireframes",
     "Add wire outlines for all geometry")
OPT (DISABLE_BATCHING,
     "Root Cause",
     "disable-batching",
     "Disable Journal batching",
     "Disable batching of geometry in the Cogl Journal.")
OPT (DISABLE_VBOS,
     "Root Cause",
     "disable-vbos",
     "Disable GL Vertex Buffers",
     "Disable use of OpenGL vertex buffer objects")
OPT (DISABLE_PBOS,
     "Root Cause",
     "disable-pbos",
     "Disable GL Pixel Buffers",
     "Disable use of OpenGL pixel buffer objects")
OPT (DISABLE_SOFTWARE_TRANSFORM,
     "Root Cause",
     "disable-software-transform",
     "Disable software rect transform",
     "Use the GPU to transform rectangular geometry")
OPT (DUMP_ATLAS_IMAGE,
     "Cogl Specialist",
     "dump-atlas-image",
     "Dump atlas images",
     "Dump texture atlas changes to an image file")
OPT (DISABLE_ATLAS,
     "Root Cause",
     "disable-atlas",
     "Disable texture atlasing",
     "Disable use of texture atlasing")
OPT (DISABLE_SHARED_ATLAS,
     "Root Cause",
     "disable-shared-atlas",
     "Disable sharing the texture atlas between text and images",
     "When this is set the glyph cache will always use a separate texture "
     "for its atlas. Otherwise it will try to share the atlas with images.")
OPT (DISABLE_TEXTURING,
     "Root Cause",
     "disable-texturing",
     "Disable texturing",
     "Disable texturing any primitives")
OPT (DISABLE_ARBFP,
     "Root Cause",
     "disable-arbfp",
     "Disable arbfp",
     "Disable use of ARB fragment programs")
OPT (DISABLE_FIXED,
     "Root Cause",
     "disable-fixed",
     "Disable fixed",
     "Disable use of the fixed function pipeline backend")
OPT (DISABLE_GLSL,
     "Root Cause",
     "disable-glsl",
     "Disable GLSL",
     "Disable use of GLSL")
OPT (DISABLE_BLENDING,
     "Root Cause",
     "disable-blending",
     "Disable blending",
     "Disable use of blending")
OPT (DISABLE_NPOT_TEXTURES,
     "Root Cause",
     "disable-npot-textures",
     "Disable non-power-of-two textures",
     "Makes Cogl think that the GL driver doesn't support NPOT textures "
     "so that it will create sliced textures or textures with waste instead.")
OPT (DISABLE_SOFTWARE_CLIP,
     "Root Cause",
     "disable-software-clip",
     "Disable software clipping",
     "Disables Cogl's attempts to clip some rectangles in software.")
OPT (SHOW_SOURCE,
     "Cogl Tracing",
     "show-source",
     "Show source",
     "Show generated ARBfp/GLSL source code")
OPT (OPENGL,
     "Cogl Tracing",
     "opengl",
     "Trace some OpenGL",
     "Traces some select OpenGL calls")
OPT (OFFSCREEN,
     "Cogl Tracing",
     "offscreen",
     "Trace offscreen support",
     "Debug offscreen support")
OPT (DISABLE_BLENDING,
     "Root Cause",
     "disable-program-caches",
     "Disable program caches",
     "Disable fallback caches for arbfp and glsl programs")
OPT (DISABLE_FAST_READ_PIXEL,
     "Root Cause",
     "disable-fast-read-pixel",
     "Disable read pixel optimization",
     "Disable optimization for reading 1px for simple "
     "scenes of opaque rectangles")
OPT (CLIPPING,
     "Cogl Tracing",
     "clipping",
     "Trace clipping",
     "Logs information about how Cogl is implementing clipping")
