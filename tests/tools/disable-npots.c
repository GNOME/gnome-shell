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
#include <stdlib.h>

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

static gboolean
get_gl_version (const gchar *version_string,
                int *major_out, int *minor_out)
{
  const char *major_end, *minor_end;
  int major = 0, minor = 0;

  if (version_string == NULL)
    return FALSE;

  /* Extract the major number */
  for (major_end = version_string; *major_end >= '0'
	 && *major_end <= '9'; major_end++)
    major = (major * 10) + *major_end - '0';
  /* If there were no digits or the major number isn't followed by a
     dot then it is invalid */
  if (major_end == version_string || *major_end != '.')
    return FALSE;

  /* Extract the minor number */
  for (minor_end = major_end + 1; *minor_end >= '0'
	 && *minor_end <= '9'; minor_end++)
    minor = (minor * 10) + *minor_end - '0';
  /* If there were no digits or there is an unexpected character then
     it is invalid */
  if (minor_end == major_end + 1
      || (*minor_end && *minor_end != ' ' && *minor_end != '.'))
    return FALSE;

  *major_out = major;
  *minor_out = minor;

  return TRUE;
}

const GLubyte *
glGetString (GLenum name)
{
  const gchar *ret = NULL;
  static GetStringFunc func = NULL;
  static gchar *extensions = NULL;
  static gchar *version = NULL;

  if (func == NULL
      && (func = (GetStringFunc) dlsym (LIB_HANDLE, "glGetString")) == NULL)
    fprintf (stderr, "dlsym: %s\n", dlerror ());
  else if (func == glGetString)
    fprintf (stderr, "dlsym returned the wrapper of glGetString\n");
  else
    {
      ret = (const gchar *) (* func) (name);

      if (name == GL_EXTENSIONS)
	{
	  if (extensions == NULL)
	    {
	      if ((extensions = strdup ((char *) ret)) == NULL)
		fprintf (stderr, "strdup: %s\n", strerror (errno));
	      else
		{
		  gchar *dst = extensions, *src = extensions;

		  while (1)
		    {
		      const char * const *str = bad_strings;
		      gchar *end;

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
      else if (name == GL_VERSION)
        {
          int gl_major, gl_minor;

          /* If the GL version is >= 2.0 then Cogl will assume it
             supports NPOT textures anyway so we need to tweak it */
          if (get_gl_version ((const gchar *) ret,
                              &gl_major, &gl_minor) && gl_major >= 2)
            {
              if (version == NULL)
                {
                  const gchar *tail = strchr (ret, ' ');
                  if (tail)
                    {
                      version = malloc (3 + strlen (tail) + 1);
                      if (version)
                        {
                          strcpy (version + 3, tail);
                          memcpy (version, "1.9", 3);
                        }
                    }
                  else
                    version = strdup ("1.9");
                }

              ret = version;
            }
        }
    }

  return (const GLubyte *) ret;
}
