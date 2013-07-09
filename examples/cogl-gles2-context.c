#include <cogl/cogl.h>
#include <cogl/cogl-gles2.h>
#include <glib.h>
#include <stdio.h>

#define OFFSCREEN_WIDTH 100
#define OFFSCREEN_HEIGHT 100

typedef struct _Data
{
    CoglContext *ctx;
    CoglFramebuffer *fb;
    CoglPrimitive *triangle;
    CoglPipeline *pipeline;

    CoglTexture *offscreen_texture;
    CoglOffscreen *offscreen;
    CoglGLES2Context *gles2_ctx;
    const CoglGLES2Vtable *gles2_vtable;
} Data;

static gboolean
paint_cb (void *user_data)
{
    Data *data = user_data;
    CoglError *error = NULL;
    const CoglGLES2Vtable *gles2 = data->gles2_vtable;

    /* Draw scene with GLES2 */
    if (!cogl_push_gles2_context (data->ctx,
                                  data->gles2_ctx,
                                  data->fb,
                                  data->fb,
                                  &error))
    {
        g_error ("Failed to push gles2 context: %s\n", error->message);
    }

    /* Clear offscreen framebuffer with a random color */
    gles2->glClearColor (g_random_double (),
                         g_random_double (),
                         g_random_double (),
                         1.0f);
    gles2->glClear (GL_COLOR_BUFFER_BIT);

    cogl_pop_gles2_context (data->ctx);

    /* Draw scene with Cogl */
    cogl_primitive_draw (data->triangle, data->fb, data->pipeline);

    cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

    return FALSE; /* remove the callback */
}

static void
frame_event_cb (CoglOnscreen *onscreen,
                CoglFrameEvent event,
                CoglFrameInfo *info,
                void *user_data)
{
    if (event == COGL_FRAME_EVENT_SYNC)
        paint_cb (user_data);
}

int
main (int argc, char **argv)
{
    Data data;
    CoglOnscreen *onscreen;
    CoglError *error = NULL;
    CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0xff},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    GSource *cogl_source;
    GMainLoop *loop;
    CoglRenderer *renderer;
    CoglDisplay *display;

    renderer = cogl_renderer_new ();
    cogl_renderer_add_constraint (renderer,
                                  COGL_RENDERER_CONSTRAINT_SUPPORTS_COGL_GLES2);
    display = cogl_display_new (renderer, NULL);
    data.ctx = cogl_context_new (display, NULL);

    onscreen = cogl_onscreen_new (data.ctx, 640, 480);
    cogl_onscreen_show (onscreen);
    data.fb = COGL_FRAMEBUFFER (onscreen);

    /* Prepare onscreen primitive */
    data.triangle = cogl_primitive_new_p2c4 (data.ctx,
                                             COGL_VERTICES_MODE_TRIANGLES,
                                             3, triangle_vertices);
    data.pipeline = cogl_pipeline_new (data.ctx);

    data.offscreen_texture = COGL_TEXTURE (
      cogl_texture_2d_new_with_size (data.ctx,
                                     OFFSCREEN_WIDTH,
                                     OFFSCREEN_HEIGHT,
                                     COGL_PIXEL_FORMAT_ANY));
    data.offscreen = cogl_offscreen_new_to_texture (data.offscreen_texture);

    data.gles2_ctx = cogl_gles2_context_new (data.ctx, &error);
    if (!data.gles2_ctx) {
        g_error ("Failed to create GLES2 context: %s\n", error->message);
    }

    data.gles2_vtable = cogl_gles2_context_get_vtable (data.gles2_ctx);

    /* Draw scene with GLES2 */
    if (!cogl_push_gles2_context (data.ctx,
                                  data.gles2_ctx,
                                  data.fb,
                                  data.fb,
                                  &error))
    {
        g_error ("Failed to push gles2 context: %s\n", error->message);
    }

    cogl_pop_gles2_context (data.ctx);

    cogl_source = cogl_glib_source_new (data.ctx, G_PRIORITY_DEFAULT);

    g_source_attach (cogl_source, NULL);

    cogl_onscreen_add_frame_callback (COGL_ONSCREEN (data.fb),
                                      frame_event_cb,
                                      &data,
                                      NULL); /* destroy notify */

    g_idle_add (paint_cb, &data);

    loop = g_main_loop_new (NULL, TRUE);
    g_main_loop_run (loop);

    return 0;
}
