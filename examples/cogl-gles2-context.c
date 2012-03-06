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
    GError *error = NULL;
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
    cogl_framebuffer_draw_primitive (data->fb, data->pipeline, data->triangle);

    cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

    /* If the driver can deliver swap complete events then we can remove
     * the idle paint callback until we next get a swap complete event
     * otherwise we keep the idle paint callback installed and simply
     * paint as fast as the driver will allow... */
    if (cogl_has_feature (data->ctx, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
      return FALSE; /* remove the callback */
    else
      return TRUE;
}

static void
swap_complete_cb (CoglFramebuffer *framebuffer, void *user_data)
{
    g_idle_add (paint_cb, user_data);
}

int
main (int argc, char **argv)
{
    Data data;
    CoglOnscreen *onscreen;
    GError *error = NULL;
    CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    GSource *cogl_source;
    GMainLoop *loop;

    data.ctx = cogl_context_new (NULL, NULL);

    onscreen = cogl_onscreen_new (data.ctx, 640, 480);
    cogl_onscreen_show (onscreen);
    data.fb = COGL_FRAMEBUFFER (onscreen);

    /* Prepare onscreen primitive */
    data.triangle = cogl_primitive_new_p2c4 (data.ctx,
                                             COGL_VERTICES_MODE_TRIANGLES,
                                             3, triangle_vertices);
    data.pipeline = cogl_pipeline_new (data.ctx);

    data.offscreen_texture =
      cogl_texture_new_with_size (OFFSCREEN_WIDTH,
                                  OFFSCREEN_HEIGHT,
                                  COGL_TEXTURE_NO_SLICING,
                                  COGL_PIXEL_FORMAT_ANY);
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

    if (cogl_has_feature (data.ctx, COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
      cogl_onscreen_add_swap_buffers_callback (COGL_ONSCREEN (data.fb),
                                               swap_complete_cb, &data);

    g_idle_add (paint_cb, &data);

    loop = g_main_loop_new (NULL, TRUE);
    g_main_loop_run (loop);

    return 0;
}
