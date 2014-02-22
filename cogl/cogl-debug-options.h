/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

OPT (OBJECT,
     N_("Cogl Tracing"),
     "ref-counts",
     N_("CoglObject references"),
     N_("Debug ref counting issues for CoglObjects"))
OPT (SLICING,
     N_("Cogl Tracing"),
     "slicing",
     N_("Trace Texture Slicing"),
     N_("debug the creation of texture slices"))
OPT (ATLAS,
     N_("Cogl Tracing"),
     "atlas",
     N_("Trace Atlas Textures"),
     N_("Debug texture atlas management"))
OPT (BLEND_STRINGS,
     N_("Cogl Tracing"),
     "blend-strings",
     N_("Trace Blend Strings"),
     N_("Debug CoglBlendString parsing"))
OPT (JOURNAL,
     N_("Cogl Tracing"),
     "journal",
     N_("Trace Journal"),
     N_("View all the geometry passing through the journal"))
OPT (BATCHING,
     N_("Cogl Tracing"),
     "batching",
     N_("Trace Batching"),
     N_("Show how geometry is being batched in the journal"))
OPT (MATRICES,
     N_("Cogl Tracing"),
     "matrices",
     N_("Trace matrices"),
     N_("Trace all matrix manipulation"))
/* XXX we should replace the "draw" option its very hand wavy... */
OPT (DRAW,
     N_("Cogl Tracing"),
     "draw",
     N_("Trace Misc Drawing"),
     N_("Trace some misc drawing operations"))
OPT (PANGO,
     N_("Cogl Tracing"),
     "pango",
     N_("Trace Pango Renderer"),
     N_("Trace the Cogl Pango renderer"))
OPT (TEXTURE_PIXMAP,
     N_("Cogl Tracing"),
     "texture-pixmap",
     N_("Trace CoglTexturePixmap backend"),
     N_("Trace the Cogl texture pixmap backend"))
OPT (RECTANGLES,
     N_("Visualize"),
     "rectangles",
     N_("Outline rectangles"),
     N_("Add wire outlines for all rectangular geometry"))
OPT (WIREFRAME,
     N_("Visualize"),
     "wireframe",
     N_("Show wireframes"),
     N_("Add wire outlines for all geometry"))
OPT (DISABLE_BATCHING,
     N_("Root Cause"),
     "disable-batching",
     N_("Disable Journal batching"),
     N_("Disable batching of geometry in the Cogl Journal."))
OPT (DISABLE_VBOS,
     N_("Root Cause"),
     "disable-vbos",
     N_("Disable GL Vertex Buffers"),
     N_("Disable use of OpenGL vertex buffer objects"))
OPT (DISABLE_PBOS,
     N_("Root Cause"),
     "disable-pbos",
     N_("Disable GL Pixel Buffers"),
     N_("Disable use of OpenGL pixel buffer objects"))
OPT (DISABLE_SOFTWARE_TRANSFORM,
     N_("Root Cause"),
     "disable-software-transform",
     N_("Disable software rect transform"),
     N_("Use the GPU to transform rectangular geometry"))
OPT (DUMP_ATLAS_IMAGE,
     N_("Cogl Specialist"),
     "dump-atlas-image",
     N_("Dump atlas images"),
     N_("Dump texture atlas changes to an image file"))
OPT (DISABLE_ATLAS,
     N_("Root Cause"),
     "disable-atlas",
     N_("Disable texture atlasing"),
     N_("Disable use of texture atlasing"))
OPT (DISABLE_SHARED_ATLAS,
     N_("Root Cause"),
     "disable-shared-atlas",
     N_("Disable sharing the texture atlas between text and images"),
     N_("When this is set the glyph cache will always use a separate texture "
        "for its atlas. Otherwise it will try to share the atlas with images."))
OPT (DISABLE_TEXTURING,
     N_("Root Cause"),
     "disable-texturing",
     N_("Disable texturing"),
     N_("Disable texturing any primitives"))
OPT (DISABLE_ARBFP,
     N_("Root Cause"),
     "disable-arbfp",
     N_("Disable arbfp"),
     N_("Disable use of ARB fragment programs"))
OPT (DISABLE_FIXED,
     N_("Root Cause"),
     "disable-fixed",
     N_("Disable fixed"),
     N_("Disable use of the fixed function pipeline backend"))
OPT (DISABLE_GLSL,
     N_("Root Cause"),
     "disable-glsl",
     N_("Disable GLSL"),
     N_("Disable use of GLSL"))
OPT (DISABLE_BLENDING,
     N_("Root Cause"),
     "disable-blending",
     N_("Disable blending"),
     N_("Disable use of blending"))
OPT (DISABLE_NPOT_TEXTURES,
     N_("Root Cause"),
     "disable-npot-textures",
     N_("Disable non-power-of-two textures"),
     N_("Makes Cogl think that the GL driver doesn't support NPOT textures "
        "so that it will create sliced textures or textures with waste instead."))
OPT (DISABLE_SOFTWARE_CLIP,
     N_("Root Cause"),
     "disable-software-clip",
     N_("Disable software clipping"),
     N_("Disables Cogl's attempts to clip some rectangles in software."))
OPT (SHOW_SOURCE,
     N_("Cogl Tracing"),
     "show-source",
     N_("Show source"),
     N_("Show generated ARBfp/GLSL source code"))
OPT (OPENGL,
     N_("Cogl Tracing"),
     "opengl",
     N_("Trace some OpenGL"),
     N_("Traces some select OpenGL calls"))
OPT (OFFSCREEN,
     N_("Cogl Tracing"),
     "offscreen",
     N_("Trace offscreen support"),
     N_("Debug offscreen support"))
OPT (DISABLE_BLENDING,
     N_("Root Cause"),
     "disable-program-caches",
     N_("Disable program caches"),
     N_("Disable fallback caches for arbfp and glsl programs"))
OPT (DISABLE_FAST_READ_PIXEL,
     N_("Root Cause"),
     "disable-fast-read-pixel",
     N_("Disable read pixel optimization"),
     N_("Disable optimization for reading 1px for simple "
        "scenes of opaque rectangles"))
OPT (CLIPPING,
     N_("Cogl Tracing"),
     "clipping",
     N_("Trace clipping"),
     N_("Logs information about how Cogl is implementing clipping"))
OPT (PERFORMANCE,
     N_("Cogl Tracing"),
     "performance",
     N_("Trace performance concerns"),
     N_("Tries to highlight sub-optimal Cogl usage."))
