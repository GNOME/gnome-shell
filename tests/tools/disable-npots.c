/*
 * This file can be build as a shared library and then used as an
 * LD_PRELOAD to fake a system where NPOTs is not supported. It simply
 * overrides glGetString and removes the extension strings.
 */

#include <GL/gl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

typedef const GLubyte * (* GetStringFunc) (GLenum name);

static const char * const bad_strings[]
= { "GL_ARB_texture_non_power_of_two",
    "GL_ARB_texture_rectangle",
    "GL_EXT_texture_rectangle",
    NULL };

const GLubyte *
glGetString (GLenum name)
{
  const GLubyte *ret = NULL;
  static void *gl_lib = NULL;
  static GetStringFunc func = NULL;
  static GLubyte *extensions = NULL;

  if (gl_lib == NULL
      && (gl_lib = dlopen ("libGL.so", RTLD_LAZY)) == NULL)
    fprintf (stderr, "dlopen: %s\n", dlerror ());
  else if (func == NULL
	   && (func = (GetStringFunc) dlsym (gl_lib, "glGetString")) == NULL)
    fprintf (stderr, "dlsym: %s\n", dlerror ());
  else
    {
      ret = (* func) (name);

      if (name == GL_EXTENSIONS)
	{
	  if (extensions == NULL)
	    {
	      if ((extensions = (GLubyte *) strdup ((char *) ret)) == NULL)
		fprintf (stderr, "strdup: %s\n", strerror (errno));
	      else
		{
		  GLubyte *dst = extensions, *src = extensions;

		  while (1)
		    {
		      const char * const *str = bad_strings;
		      GLubyte *end;

		      while (isspace (*src))
			*(dst++) = *(src++);

		      if (*src == 0)
			break;

		      for (end = src + 1; *end && !isspace (*end); end++);

		      while (*str && strncmp ((char *) src, *str, end - src))
			str++;

		      if (*str == NULL)
			{
			  memcpy (dst, src, end - src);
			  dst += end - src;
			}

		      src = end;
		    }

		  *dst = '\0';
		}
	    }

	  ret = extensions;
	}
    }

  return ret;
}
