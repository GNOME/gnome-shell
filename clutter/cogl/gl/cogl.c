/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#include <string.h>
#include <gmodule.h>

#include "cogl.h"

#ifdef HAVE_CLUTTER_GLX
#include <dlfcn.h>
#include <GL/glx.h>

typedef CoglFuncPtr (*GLXGetProcAddressProc) (const guint8 *procName);
#endif

#include "cogl-internal.h"
#include "cogl-context.h"

CoglFuncPtr
cogl_get_proc_address (const gchar* name)
{
  /* Sucks to ifdef here but not other option..? would be nice to
   * split the code up for more reuse (once more backends use this
   */
#if defined(HAVE_CLUTTER_GLX)
  static GLXGetProcAddressProc get_proc_func = NULL;
  static void                 *dlhand = NULL;

  if (get_proc_func == NULL && dlhand == NULL)
    {
      dlhand = dlopen (NULL, RTLD_LAZY);

      if (dlhand)
	{
	  dlerror ();

	  get_proc_func =
            (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddress");

	  if (dlerror () != NULL)
            {
              get_proc_func =
                (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddressARB");
            }

	  if (dlerror () != NULL)
	    {
	      get_proc_func = NULL;
	      g_warning ("failed to bind GLXGetProcAddress "
                         "or GLXGetProcAddressARB");
	    }
	}
    }

  if (get_proc_func)
    return get_proc_func ((unsigned char*) name);

#elif defined(HAVE_CLUTTER_WIN32)

  return (CoglFuncPtr) wglGetProcAddress ((LPCSTR) name);

#else /* HAVE_CLUTTER_WIN32 */

  /* this should find the right function if the program is linked against a
   * library providing it */
  static GModule *module = NULL;
  if (module == NULL)
    module = g_module_open (NULL, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  if (module)
    {
      gpointer symbol;

      if (g_module_symbol (module, name, &symbol))
        return symbol;
    }

#endif /* HAVE_CLUTTER_WIN32 */

  return NULL;
}

gboolean
cogl_check_extension (const gchar *name, const gchar *ext)
{
  gchar *end;
  gint name_len, n;

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (gchar*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end)
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

#ifdef HAVE_CLUTTER_OSX
static gboolean
really_enable_npot (void)
{
  /* OSX backend + ATI Radeon X1600 + NPOT texture + GL_REPEAT seems to crash
   * http://bugzilla.openedhand.com/show_bug.cgi?id=929
   *
   * Temporary workaround until post 0.8 we rejig the features set up a
   * little to allow the backend to overide.
   */
  const char *gl_renderer;
  const char *env_string;

  /* Regardless of hardware, allow user to decide. */
  env_string = g_getenv ("COGL_ENABLE_NPOT");
  if (env_string != NULL)
    return env_string[0] == '1';

  gl_renderer = (char*)glGetString (GL_RENDERER);
  if (strstr (gl_renderer, "ATI Radeon X1600") != NULL)
    return FALSE;

  return TRUE;
}
#endif

void
_cogl_features_init (void)
{
  CoglFeatureFlags  flags = 0;
  const gchar      *gl_extensions;
  GLint             max_clip_planes = 0;
  GLint             num_stencil_bits = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  flags = COGL_FEATURE_TEXTURE_READ_PIXELS;

  gl_extensions = (const gchar*) glGetString (GL_EXTENSIONS);

  if (cogl_check_extension ("GL_ARB_texture_non_power_of_two", gl_extensions))
    {
#ifdef HAVE_CLUTTER_OSX
      if (really_enable_npot ())
#endif
        flags |= COGL_FEATURE_TEXTURE_NPOT;
    }

#ifdef GL_YCBCR_MESA
  if (cogl_check_extension ("GL_MESA_ycbcr_texture", gl_extensions))
    {
      flags |= COGL_FEATURE_TEXTURE_YUV;
    }
#endif

  if (cogl_check_extension ("GL_ARB_shader_objects", gl_extensions) &&
      cogl_check_extension ("GL_ARB_vertex_shader", gl_extensions) &&
      cogl_check_extension ("GL_ARB_fragment_shader", gl_extensions))
    {
      ctx->pf_glCreateProgramObjectARB =
	(COGL_PFNGLCREATEPROGRAMOBJECTARBPROC)
	cogl_get_proc_address ("glCreateProgramObjectARB");

      ctx->pf_glCreateShaderObjectARB =
	(COGL_PFNGLCREATESHADEROBJECTARBPROC)
	cogl_get_proc_address ("glCreateShaderObjectARB");

      ctx->pf_glShaderSourceARB =
	(COGL_PFNGLSHADERSOURCEARBPROC)
	cogl_get_proc_address ("glShaderSourceARB");

      ctx->pf_glCompileShaderARB =
	(COGL_PFNGLCOMPILESHADERARBPROC)
	cogl_get_proc_address ("glCompileShaderARB");

      ctx->pf_glAttachObjectARB =
	(COGL_PFNGLATTACHOBJECTARBPROC)
	cogl_get_proc_address ("glAttachObjectARB");

      ctx->pf_glLinkProgramARB =
	(COGL_PFNGLLINKPROGRAMARBPROC)
	cogl_get_proc_address ("glLinkProgramARB");

      ctx->pf_glUseProgramObjectARB =
	(COGL_PFNGLUSEPROGRAMOBJECTARBPROC)
	cogl_get_proc_address ("glUseProgramObjectARB");

      ctx->pf_glGetUniformLocationARB =
	(COGL_PFNGLGETUNIFORMLOCATIONARBPROC)
	cogl_get_proc_address ("glGetUniformLocationARB");

      ctx->pf_glDeleteObjectARB =
	(COGL_PFNGLDELETEOBJECTARBPROC)
	cogl_get_proc_address ("glDeleteObjectARB");

      ctx->pf_glGetInfoLogARB =
	(COGL_PFNGLGETINFOLOGARBPROC)
	cogl_get_proc_address ("glGetInfoLogARB");

      ctx->pf_glGetObjectParameterivARB =
	(COGL_PFNGLGETOBJECTPARAMETERIVARBPROC)
	cogl_get_proc_address ("glGetObjectParameterivARB");

      ctx->pf_glUniform1fARB =
	(COGL_PFNGLUNIFORM1FARBPROC)
	cogl_get_proc_address ("glUniform1fARB");

      ctx->pf_glVertexAttribPointerARB =
	(COGL_PFNGLVERTEXATTRIBPOINTERARBPROC)
	cogl_get_proc_address ("glVertexAttribPointerARB");

      ctx->pf_glEnableVertexAttribArrayARB =
	(COGL_PFNGLENABLEVERTEXATTRIBARRAYARBPROC)
	cogl_get_proc_address ("glEnableVertexAttribArrayARB");

      ctx->pf_glDisableVertexAttribArrayARB =
	(COGL_PFNGLDISABLEVERTEXATTRIBARRAYARBPROC)
	cogl_get_proc_address ("glDisableVertexAttribArrayARB");

      ctx->pf_glUniform2fARB =
	(COGL_PFNGLUNIFORM2FARBPROC)
	cogl_get_proc_address ("glUniform2fARB");

      ctx->pf_glUniform3fARB =
	(COGL_PFNGLUNIFORM3FARBPROC)
	cogl_get_proc_address ("glUniform3fARB");

      ctx->pf_glUniform4fARB =
	(COGL_PFNGLUNIFORM4FARBPROC)
	cogl_get_proc_address ("glUniform4fARB");

      ctx->pf_glUniform1fvARB =
	(COGL_PFNGLUNIFORM1FVARBPROC)
	cogl_get_proc_address ("glUniform1fvARB");

      ctx->pf_glUniform2fvARB =
	(COGL_PFNGLUNIFORM2FVARBPROC)
	cogl_get_proc_address ("glUniform2fvARB");

      ctx->pf_glUniform3fvARB =
	(COGL_PFNGLUNIFORM3FVARBPROC)
	cogl_get_proc_address ("glUniform3fvARB");

      ctx->pf_glUniform4fvARB =
	(COGL_PFNGLUNIFORM4FVARBPROC)
	cogl_get_proc_address ("glUniform4fvARB");

      ctx->pf_glUniform1iARB =
	(COGL_PFNGLUNIFORM1IARBPROC)
	cogl_get_proc_address ("glUniform1iARB");

      ctx->pf_glUniform2iARB =
	(COGL_PFNGLUNIFORM2IARBPROC)
	cogl_get_proc_address ("glUniform2iARB");

      ctx->pf_glUniform3iARB =
	(COGL_PFNGLUNIFORM3IARBPROC)
	cogl_get_proc_address ("glUniform3iARB");

      ctx->pf_glUniform4iARB =
	(COGL_PFNGLUNIFORM4IARBPROC)
	cogl_get_proc_address ("glUniform4iARB");

      ctx->pf_glUniform1ivARB =
	(COGL_PFNGLUNIFORM1IVARBPROC)
	cogl_get_proc_address ("glUniform1ivARB");

      ctx->pf_glUniform2ivARB =
	(COGL_PFNGLUNIFORM2IVARBPROC)
	cogl_get_proc_address ("glUniform2ivARB");

      ctx->pf_glUniform3ivARB =
	(COGL_PFNGLUNIFORM3IVARBPROC)
	cogl_get_proc_address ("glUniform3ivARB");

      ctx->pf_glUniform4ivARB =
	(COGL_PFNGLUNIFORM4IVARBPROC)
	cogl_get_proc_address ("glUniform4ivARB");

      ctx->pf_glUniformMatrix2fvARB =
	(COGL_PFNGLUNIFORMMATRIX2FVARBPROC)
	cogl_get_proc_address ("glUniformMatrix2fvARB");

      ctx->pf_glUniformMatrix3fvARB =
	(COGL_PFNGLUNIFORMMATRIX3FVARBPROC)
	cogl_get_proc_address ("glUniformMatrix3fvARB");

      ctx->pf_glUniformMatrix4fvARB =
	(COGL_PFNGLUNIFORMMATRIX4FVARBPROC)
	cogl_get_proc_address ("glUniformMatrix4fvARB");

      if (ctx->pf_glCreateProgramObjectARB    &&
	  ctx->pf_glCreateShaderObjectARB     &&
	  ctx->pf_glShaderSourceARB           &&
	  ctx->pf_glCompileShaderARB          &&
	  ctx->pf_glAttachObjectARB           &&
	  ctx->pf_glLinkProgramARB            &&
	  ctx->pf_glUseProgramObjectARB       &&
	  ctx->pf_glGetUniformLocationARB     &&
	  ctx->pf_glDeleteObjectARB           &&
	  ctx->pf_glGetInfoLogARB             &&
	  ctx->pf_glGetObjectParameterivARB   &&
	  ctx->pf_glUniform1fARB              &&
	  ctx->pf_glUniform2fARB              &&
	  ctx->pf_glUniform3fARB              &&
	  ctx->pf_glUniform4fARB              &&
	  ctx->pf_glUniform1fvARB             &&
	  ctx->pf_glUniform2fvARB             &&
	  ctx->pf_glUniform3fvARB             &&
	  ctx->pf_glUniform4fvARB             &&
	  ctx->pf_glUniform1iARB              &&
	  ctx->pf_glUniform2iARB              &&
	  ctx->pf_glUniform3iARB              &&
	  ctx->pf_glUniform4iARB              &&
	  ctx->pf_glUniform1ivARB             &&
	  ctx->pf_glUniform2ivARB             &&
	  ctx->pf_glUniform3ivARB             &&
	  ctx->pf_glUniform4ivARB             &&
	  ctx->pf_glUniformMatrix2fvARB       &&
	  ctx->pf_glUniformMatrix3fvARB       &&
	  ctx->pf_glUniformMatrix4fvARB       &&
	  ctx->pf_glVertexAttribPointerARB    &&
	  ctx->pf_glEnableVertexAttribArrayARB &&
	  ctx->pf_glDisableVertexAttribArrayARB)
	flags |= COGL_FEATURE_SHADERS_GLSL;
    }

  if (cogl_check_extension ("GL_EXT_framebuffer_object", gl_extensions) ||
      cogl_check_extension ("GL_ARB_framebuffer_object", gl_extensions))
    {
      ctx->pf_glGenRenderbuffersEXT =
	(COGL_PFNGLGENRENDERBUFFERSEXTPROC)
	cogl_get_proc_address ("glGenRenderbuffersEXT");

      ctx->pf_glDeleteRenderbuffersEXT =
	(COGL_PFNGLDELETERENDERBUFFERSEXTPROC)
	cogl_get_proc_address ("glDeleteRenderbuffersEXT");

      ctx->pf_glBindRenderbufferEXT =
	(COGL_PFNGLBINDRENDERBUFFEREXTPROC)
	cogl_get_proc_address ("glBindRenderbufferEXT");

      ctx->pf_glRenderbufferStorageEXT =
	(COGL_PFNGLRENDERBUFFERSTORAGEEXTPROC)
	cogl_get_proc_address ("glRenderbufferStorageEXT");

      ctx->pf_glGenFramebuffersEXT =
	(COGL_PFNGLGENFRAMEBUFFERSEXTPROC)
	cogl_get_proc_address ("glGenFramebuffersEXT");

      ctx->pf_glBindFramebufferEXT =
	(COGL_PFNGLBINDFRAMEBUFFEREXTPROC)
	cogl_get_proc_address ("glBindFramebufferEXT");

      ctx->pf_glFramebufferTexture2DEXT =
	(COGL_PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)
	cogl_get_proc_address ("glFramebufferTexture2DEXT");

      ctx->pf_glFramebufferRenderbufferEXT =
	(COGL_PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)
	cogl_get_proc_address ("glFramebufferRenderbufferEXT");

      ctx->pf_glCheckFramebufferStatusEXT =
	(COGL_PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)
	cogl_get_proc_address ("glCheckFramebufferStatusEXT");

      ctx->pf_glDeleteFramebuffersEXT =
	(COGL_PFNGLDELETEFRAMEBUFFERSEXTPROC)
	cogl_get_proc_address ("glDeleteFramebuffersEXT");

      if (ctx->pf_glGenRenderbuffersEXT         &&
	  ctx->pf_glBindRenderbufferEXT         &&
	  ctx->pf_glRenderbufferStorageEXT      &&
	  ctx->pf_glGenFramebuffersEXT          &&
	  ctx->pf_glBindFramebufferEXT          &&
	  ctx->pf_glFramebufferTexture2DEXT     &&
	  ctx->pf_glFramebufferRenderbufferEXT  &&
	  ctx->pf_glCheckFramebufferStatusEXT   &&
	  ctx->pf_glDeleteFramebuffersEXT)
	flags |= COGL_FEATURE_OFFSCREEN;
    }

  if (cogl_check_extension ("GL_EXT_framebuffer_blit", gl_extensions))
    {
      ctx->pf_glBlitFramebufferEXT =
	(COGL_PFNGLBLITFRAMEBUFFEREXTPROC)
	cogl_get_proc_address ("glBlitFramebufferEXT");

      if (ctx->pf_glBlitFramebufferEXT)
	flags |= COGL_FEATURE_OFFSCREEN_BLIT;
    }

  if (cogl_check_extension ("GL_EXT_framebuffer_multisample", gl_extensions))
    {
      ctx->pf_glRenderbufferStorageMultisampleEXT =
	(COGL_PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)
	cogl_get_proc_address ("glRenderbufferStorageMultisampleEXT");

      if (ctx->pf_glRenderbufferStorageMultisampleEXT)
	flags |= COGL_FEATURE_OFFSCREEN_MULTISAMPLE;
    }

  GE( glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;

  if (cogl_check_extension ("GL_ARB_vertex_buffer_object", gl_extensions))
    {
      ctx->pf_glGenBuffersARB =
	    (COGL_PFNGLGENBUFFERSARBPROC)
	    cogl_get_proc_address ("glGenBuffersARB");
      ctx->pf_glBindBufferARB =
	    (COGL_PFNGLBINDBUFFERARBPROC)
	    cogl_get_proc_address ("glBindBufferARB");
      ctx->pf_glBufferDataARB =
	    (COGL_PFNGLBUFFERDATAARBPROC)
	    cogl_get_proc_address ("glBufferDataARB");
      ctx->pf_glBufferSubDataARB =
	    (COGL_PFNGLBUFFERSUBDATAARBPROC)
	    cogl_get_proc_address ("glBufferSubDataARB");
      ctx->pf_glDeleteBuffersARB =
	    (COGL_PFNGLDELETEBUFFERSARBPROC)
	    cogl_get_proc_address ("glDeleteBuffersARB");
      ctx->pf_glMapBufferARB =
	    (COGL_PFNGLMAPBUFFERARBPROC)
	    cogl_get_proc_address ("glMapBufferARB");
      ctx->pf_glUnmapBufferARB =
	    (COGL_PFNGLUNMAPBUFFERARBPROC)
	    cogl_get_proc_address ("glUnmapBufferARB");
      if (ctx->pf_glGenBuffersARB
	  && ctx->pf_glBindBufferARB
	  && ctx->pf_glBufferDataARB
	  && ctx->pf_glBufferSubDataARB
	  && ctx->pf_glDeleteBuffersARB
	  && ctx->pf_glMapBufferARB
	  && ctx->pf_glUnmapBufferARB)
      flags |= COGL_FEATURE_VBOS;
    }

  /* These should always be available because they are defined in GL
     1.2, but we can't call it directly because under Windows
     functions > 1.1 aren't exported */
  ctx->pf_glDrawRangeElements =
        (COGL_PFNGLDRAWRANGEELEMENTSPROC)
        cogl_get_proc_address ("glDrawRangeElements");
  ctx->pf_glActiveTexture =
        (COGL_PFNGLACTIVETEXTUREPROC)
        cogl_get_proc_address ("glActiveTexture");
  ctx->pf_glClientActiveTexture =
        (COGL_PFNGLCLIENTACTIVETEXTUREPROC)
        cogl_get_proc_address ("glClientActiveTexture");

  /* Available in 1.4 */
  ctx->pf_glBlendFuncSeparate =
        (COGL_PFNGLBLENDFUNCSEPARATEPROC)
        cogl_get_proc_address ("glBlendFuncSeparate");

  /* Available in 2.0 */
  ctx->pf_glBlendEquationSeparate =
        (COGL_PFNGLBLENDEQUATIONSEPARATEPROC)
        cogl_get_proc_address ("glBlendEquationSeparate");

  /* Cache features */
  ctx->feature_flags = flags;
  ctx->features_cached = TRUE;
}

