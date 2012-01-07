#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

CoglColor black;

int
main (int argc, char **argv)
{
    CoglContext *ctx;
    CoglOnscreen *onscreen;
    CoglFramebuffer *fb;
    GError *error = NULL;
    CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    CoglPrimitive *triangle;

    ctx = cogl_context_new (NULL, &error);
    if (!ctx) {
        fprintf (stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    onscreen = cogl_onscreen_new (ctx, 640, 480);
    cogl_onscreen_show (onscreen);

    fb = COGL_FRAMEBUFFER (onscreen);
    cogl_push_framebuffer (fb);

    triangle = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                        3, triangle_vertices);
    for (;;) {
        CoglPollFD *poll_fds;
        int n_poll_fds;
        gint64 timeout;

        cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
        cogl_primitive_draw (triangle);
        cogl_framebuffer_swap_buffers (fb);

        cogl_poll_get_info (ctx, &poll_fds, &n_poll_fds, &timeout);
        g_poll ((GPollFD *) poll_fds, n_poll_fds, 0);
        cogl_poll_dispatch (ctx, poll_fds, n_poll_fds);
    }

    return 0;
}
