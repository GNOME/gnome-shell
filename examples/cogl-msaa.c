#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

CoglColor black;

int
main (int argc, char **argv)
{
    CoglOnscreenTemplate *onscreen_template;
    CoglDisplay *display;
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
    CoglHandle tex;
    CoglHandle offscreen;
    CoglFramebuffer *offscreen_fb;
    CoglPipeline *pipeline;

    onscreen_template = cogl_onscreen_template_new (NULL);
    cogl_onscreen_template_set_samples_per_pixel (onscreen_template, 4);
    display = cogl_display_new (NULL, onscreen_template);

    if (!cogl_display_setup (display, &error))
      {
        fprintf (stderr, "Platform doesn't support onscreen 4x msaa rendering: %s\n",
                 error->message);
        return 1;
      }

    ctx = cogl_context_new (display, &error);
    if (!ctx)
      {
        fprintf (stderr, "Failed to create context: %s\n", error->message);
        return 1;
      }

    onscreen = cogl_onscreen_new (ctx, 640, 480);
    /* Eventually there will be an implicit allocate on first use so this
     * will become optional... */
    fb = COGL_FRAMEBUFFER (onscreen);

    cogl_framebuffer_set_samples_per_pixel (fb, 4);

    if (!cogl_framebuffer_allocate (fb, &error))
      {
        g_error_free (error);
        fprintf (stderr, "Failed to allocate 4x msaa offscreen framebuffer, "
                 "disabling msaa for onscreen rendering\n");
        cogl_framebuffer_set_samples_per_pixel (fb, 0);

        error = NULL;
        if (!cogl_framebuffer_allocate (fb, &error))
          {
            fprintf (stderr, "Failed to allocate framebuffer: %s\n", error->message);
            return 1;
          }
      }

    cogl_onscreen_show (onscreen);

    tex = cogl_texture_new_with_size (320, 480,
                                      COGL_TEXTURE_NO_SLICING,
                                      COGL_PIXEL_FORMAT_ANY);
    offscreen = cogl_offscreen_new_to_texture (tex);
    offscreen_fb = COGL_FRAMEBUFFER (offscreen);
    cogl_framebuffer_set_samples_per_pixel (offscreen_fb, 4);
    if (!cogl_framebuffer_allocate (offscreen_fb, &error))
      {
        g_error_free (error);
        error = NULL;
        fprintf (stderr, "Failed to allocate 4x msaa offscreen framebuffer, "
                 "disabling msaa for offscreen rendering");
        cogl_framebuffer_set_samples_per_pixel (offscreen_fb, 0);
      }

    cogl_push_framebuffer (fb);

    triangle = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                        3, triangle_vertices);
    pipeline = cogl_pipeline_new ();

    for (;;) {
        CoglPollFD *poll_fds;
        int n_poll_fds;
        gint64 timeout;

        cogl_clear (&black, COGL_BUFFER_BIT_COLOR);

        cogl_push_matrix ();
        cogl_scale (0.5, 1, 1);
        cogl_translate (-1, 0, 0);
        cogl_set_source (pipeline);
        cogl_primitive_draw (triangle);
        cogl_pop_matrix ();

        cogl_push_framebuffer (offscreen_fb);
        cogl_primitive_draw (triangle);
        cogl_framebuffer_resolve_samples (offscreen_fb);
        cogl_pop_framebuffer ();

        cogl_set_source_texture (tex);
        cogl_rectangle (0, 1, 1, -1);

        cogl_framebuffer_swap_buffers (fb);

        cogl_poll_get_info (ctx, &poll_fds, &n_poll_fds, &timeout);
        g_poll ((GPollFD *) poll_fds, n_poll_fds, 0);
        cogl_poll_dispatch (ctx, poll_fds, n_poll_fds);
    }

    return 0;
}
