#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

typedef struct _Data
{
    CoglContext *ctx;
    CoglFramebuffer *fb;
    CoglPrimitive *triangle;
    CoglPipeline *pipeline;
} Data;

static CoglBool
paint_cb (void *user_data)
{
    Data *data = user_data;

    cogl_framebuffer_clear4f (data->fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);
    cogl_framebuffer_draw_primitive (data->fb, data->pipeline, data->triangle);
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
        g_idle_add (paint_cb, user_data);
}

int
main (int argc, char **argv)
{
    Data data;
    CoglOnscreen *onscreen;
    CoglError *error = NULL;
    CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    GSource *cogl_source;
    GMainLoop *loop;

    data.ctx = cogl_context_new (NULL, &error);
    if (!data.ctx) {
        fprintf (stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    onscreen = cogl_onscreen_new (data.ctx, 640, 480);
    cogl_onscreen_show (onscreen);
    data.fb = COGL_FRAMEBUFFER (onscreen);

    cogl_onscreen_set_resizable (onscreen, TRUE);

    data.triangle = cogl_primitive_new_p2c4 (data.ctx,
                                             COGL_VERTICES_MODE_TRIANGLES,
                                             3, triangle_vertices);
    data.pipeline = cogl_pipeline_new (data.ctx);

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
