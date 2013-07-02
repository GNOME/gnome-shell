#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
    CoglOnscreenTemplate *onscreen_template;
    CoglDisplay *display;
    CoglContext *ctx;
    CoglOnscreen *onscreen;
    CoglFramebuffer *fb;
    CoglError *error = NULL;
    CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0xff},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    CoglPrimitive *triangle;
    CoglTexture *tex;
    CoglOffscreen *offscreen;
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
    fb = onscreen;

    cogl_framebuffer_set_samples_per_pixel (fb, 4);

    if (!cogl_framebuffer_allocate (fb, &error))
      {
        fprintf (stderr, "Failed to allocate 4x msaa offscreen framebuffer, "
                 "disabling msaa for onscreen rendering: %s\n", error->message);
        cogl_error_free (error);
        cogl_framebuffer_set_samples_per_pixel (fb, 0);

        error = NULL;
        if (!cogl_framebuffer_allocate (fb, &error))
          {
            fprintf (stderr, "Failed to allocate framebuffer: %s\n", error->message);
            return 1;
          }
      }

    cogl_onscreen_show (onscreen);

    tex = cogl_texture_2d_new_with_size (ctx, 320, 480);
    offscreen = cogl_offscreen_new_with_texture (tex);
    offscreen_fb = offscreen;
    cogl_framebuffer_set_samples_per_pixel (offscreen_fb, 4);
    if (!cogl_framebuffer_allocate (offscreen_fb, &error))
      {
        cogl_error_free (error);
        error = NULL;
        fprintf (stderr, "Failed to allocate 4x msaa offscreen framebuffer, "
                 "disabling msaa for offscreen rendering");
        cogl_framebuffer_set_samples_per_pixel (offscreen_fb, 0);
      }

    triangle = cogl_primitive_new_p2c4 (ctx, COGL_VERTICES_MODE_TRIANGLES,
                                        3, triangle_vertices);
    pipeline = cogl_pipeline_new (ctx);

    for (;;) {
        CoglPollFD *poll_fds;
        int n_poll_fds;
        int64_t timeout;
        CoglPipeline *texture_pipeline;

        cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

        cogl_framebuffer_push_matrix (fb);
        cogl_framebuffer_scale (fb, 0.5, 1, 1);
        cogl_framebuffer_translate (fb, -1, 0, 0);
        cogl_primitive_draw (triangle, fb, pipeline);
        cogl_framebuffer_pop_matrix (fb);

        cogl_primitive_draw (triangle, fb, pipeline);
        cogl_framebuffer_resolve_samples (offscreen_fb);

        texture_pipeline = cogl_pipeline_new (ctx);
        cogl_pipeline_set_layer_texture (texture_pipeline, 0, tex);
        cogl_framebuffer_draw_rectangle (fb, texture_pipeline, 0, 1, 1, -1);
        cogl_object_unref (texture_pipeline);

        cogl_onscreen_swap_buffers (onscreen);

        cogl_poll_renderer_get_info (cogl_context_get_renderer (ctx),
                                     &poll_fds, &n_poll_fds, &timeout);
        g_poll ((GPollFD *) poll_fds, n_poll_fds, 0);
        cogl_poll_renderer_dispatch (cogl_context_get_renderer (ctx),
                                     poll_fds, n_poll_fds);
    }

    return 0;
}
