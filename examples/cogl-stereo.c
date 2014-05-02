#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>

typedef struct _Data
{
    CoglContext *ctx;
    CoglFramebuffer *fb;
    CoglPrimitive *left_triangle;
    CoglPrimitive *right_triangle;
    CoglPipeline *pipeline;

    unsigned int redraw_idle;
    CoglBool is_dirty;
    CoglBool draw_ready;
} Data;

static gboolean
paint_cb (void *user_data)
{
    Data *data = user_data;

    data->redraw_idle = 0;
    data->is_dirty = FALSE;
    data->draw_ready = FALSE;

    cogl_framebuffer_set_stereo_mode (data->fb, COGL_STEREO_BOTH);
    cogl_framebuffer_clear4f (data->fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

    cogl_framebuffer_set_stereo_mode (data->fb, COGL_STEREO_LEFT);
    cogl_primitive_draw (data->left_triangle,
                         data->fb,
                         data->pipeline);

    if (cogl_framebuffer_get_is_stereo (data->fb))
      {
	cogl_framebuffer_set_stereo_mode (data->fb, COGL_STEREO_RIGHT);
	cogl_primitive_draw (data->right_triangle,
			     data->fb,
			     data->pipeline);
      }

    cogl_onscreen_swap_buffers (data->fb);

    return G_SOURCE_REMOVE;
}

static void
maybe_redraw (Data *data)
{
    if (data->is_dirty && data->draw_ready && data->redraw_idle == 0) {
        /* We'll draw on idle instead of drawing immediately so that
         * if Cogl reports multiple dirty rectangles we won't
         * redundantly draw multiple frames */
        data->redraw_idle = g_idle_add (paint_cb, data);
    }
}

static void
frame_event_cb (CoglOnscreen *onscreen,
                CoglFrameEvent event,
                CoglFrameInfo *info,
                void *user_data)
{
    Data *data = user_data;

    if (event == COGL_FRAME_EVENT_SYNC) {
        data->draw_ready = TRUE;
        maybe_redraw (data);
    }
}

static void
dirty_cb (CoglOnscreen *onscreen,
          const CoglOnscreenDirtyInfo *info,
          void *user_data)
{
    Data *data = user_data;

    data->is_dirty = TRUE;
    maybe_redraw (data);
}

int
main (int argc, char **argv)
{
    Data data;
    CoglRenderer *renderer;
    CoglOnscreenTemplate *onscreen_template;
    CoglDisplay *display;
    CoglOnscreen *onscreen;
    CoglError *error = NULL;
    CoglVertexP2C4 left_triangle_vertices[] = {
        {0.05, 0.7, 0xff, 0x00, 0x00, 0xff},
        {-0.65, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.75, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    CoglVertexP2C4 right_triangle_vertices[] = {
        {-0.05, 0.7, 0xff, 0x00, 0x00, 0xff},
        {-0.75, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.65, -0.7, 0x00, 0x00, 0xff, 0xff}
    };
    GSource *cogl_source;
    GMainLoop *loop;

    data.redraw_idle = 0;
    data.is_dirty = FALSE;
    data.draw_ready = TRUE;

    renderer = cogl_renderer_new ();
    onscreen_template = cogl_onscreen_template_new (NULL);
    cogl_onscreen_template_set_stereo_enabled (onscreen_template, TRUE);
    display = cogl_display_new (renderer, onscreen_template);

    data.ctx = cogl_context_new (display, &error);
    if (!data.ctx) {
        fprintf (stderr, "Failed to create stereo context: %s\n", error->message);
        return 1;
    }

    onscreen = cogl_onscreen_new (data.ctx, 640, 480);
    cogl_onscreen_show (onscreen);
    data.fb = onscreen;

    cogl_onscreen_set_resizable (onscreen, TRUE);

    data.left_triangle = cogl_primitive_new_p2c4 (data.ctx,
						  COGL_VERTICES_MODE_TRIANGLES,
						  3, left_triangle_vertices);
    data.right_triangle = cogl_primitive_new_p2c4 (data.ctx,
						   COGL_VERTICES_MODE_TRIANGLES,
						   3, right_triangle_vertices);
    data.pipeline = cogl_pipeline_new (data.ctx);

    cogl_source = cogl_glib_source_new (data.ctx, G_PRIORITY_DEFAULT);

    g_source_attach (cogl_source, NULL);

    cogl_onscreen_add_frame_callback (data.fb,
                                      frame_event_cb,
                                      &data,
                                      NULL); /* destroy notify */
    cogl_onscreen_add_dirty_callback (data.fb,
                                      dirty_cb,
                                      &data,
                                      NULL); /* destroy notify */

    loop = g_main_loop_new (NULL, TRUE);
    g_main_loop_run (loop);

    return 0;
}
