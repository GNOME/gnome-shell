#ifndef _HAVE_CLTR_H
#define _HAVE_CLTR_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <X11/Xlib.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <glib.h>

#include "pixbuf.h"
#include "fonts.h"

#define CLTR_WANT_DEBUG 1

#if (CLTR_WANT_DEBUG)

#define CLTR_DBG(x, a...) \
 g_printerr ( __FILE__ ":%d,%s() " x "\n", __LINE__, __func__, ##a)

#define CLTR_GLERR()                                           \
 {                                                             \
  GLenum err = glGetError (); 	/* Roundtrip */                \
  if (err != GL_NO_ERROR)                                      \
    {                                                          \
      const GLubyte *message = gluErrorString (err);           \
      g_printerr (__FILE__ ": GL Error: %s [at %s:%d]\n",      \
		  __func__, __LINE__);                         \
    }                                                          \
 }

#else

#define CLTR_DBG(x, a...) do {} while (0)
#define CLTR_GLERR()      do {} while (0)

#endif

#define CLTR_MARK() CLTR_DBG("mark")

#endif
