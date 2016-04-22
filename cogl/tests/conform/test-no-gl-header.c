#undef COGL_COMPILATION
#include <cogl/cogl.h>

/* If you just include cogl/cogl.h, you shouldn't end up including any
   GL headers */
#ifdef GL_TRUE
#error "Including cogl.h shouldn't be including any GL headers"
#endif

void test_no_gl_header (void);

void
test_no_gl_header (void)
{
}

