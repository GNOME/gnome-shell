/*
 * This file can be build as a shared library and then used as an
 * LD_PRELOAD to fake a system where NPOTs is not supported. It simply
 * overrides glGetString and removes the extension strings.
 */

/* This is just included to get the right GL header */
#include <cogl/cogl.h>

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* If RTLD_NEXT isn't available then try just using NULL */
#ifdef  RTLD_NEXT
#define LIB_HANDLE    RTLD_NEXT
#else
#define LIB_HANDLE    NULL
#endif

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
  static GetStringFunc func = NULL;
  static GLubyte *extensions = NULL;

  if (func == NULL
      && (func = (GetStringFunc) dlsym (LIB_HANDLE, "glGetString")) == NULL)
    fprintf (stderr, "dlsym: %s\n", dlerror ());
  else if (func == glGetString)
    fprintf (stderr, "dlsym returned the wrapper of glGetString\n");
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
