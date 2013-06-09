#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

/* The state for this example... */
typedef struct _Data
{
  CoglFramebuffer *fb;
  int framebuffer_width;
  int framebuffer_height;

  CoglMatrix view;

  CoglIndices *indices;
  CoglPrimitive *prim;
  CoglTexture *texture;
  CoglPipeline *crate_pipeline;

  CoglPangoFontMap *pango_font_map;
  PangoContext *pango_context;
  PangoFontDescription *pango_font_desc;

  PangoLayout *hello_label;
  int hello_label_width;
  int hello_label_height;

  GTimer *timer;

  CoglBool swap_ready;

} Data;

/* A static identity matrix initialized for convenience. */
static CoglMatrix identity;
/* static colors initialized for convenience. */
static CoglColor white;

/* A cube modelled using 4 vertices for each face.
 *
 * We use an index buffer when drawing the cube later so the GPU will
 * actually read each face as 2 separate triangles.
 */
static CoglVertexP3T2 vertices[] =
{
  /* Front face */
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},

  /* Back face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},

  /* Top face */
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},

  /* Bottom face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},

  /* Right face */
  { /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ 1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */ 1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */ 1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},

  /* Left face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f}
};

static void
paint (Data *data)
{
  CoglFramebuffer *fb = data->fb;
  float rotation;

  cogl_framebuffer_clear4f (fb,
                            COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH,
                            0, 0, 0, 1);

  cogl_framebuffer_push_matrix (fb);

  cogl_framebuffer_translate (fb,
                              data->framebuffer_width / 2,
                              data->framebuffer_height / 2,
                              0);

  cogl_framebuffer_scale (fb, 75, 75, 75);

  /* Update the rotation based on the time the application has been
     running so that we get a linear animation regardless of the frame
     rate */
  rotation = g_timer_elapsed (data->timer, NULL) * 60.0f;

  /* Rotate the cube separately around each axis.
   *
   * Note: Cogl matrix manipulation follows the same rules as for
   * OpenGL. We use column-major matrices and - if you consider the
   * transformations happening to the model - then they are combined
   * in reverse order which is why the rotation is done last, since
   * we want it to be a rotation around the origin, before it is
   * scaled and translated.
   */
  cogl_framebuffer_rotate (fb, rotation, 0, 0, 1);
  cogl_framebuffer_rotate (fb, rotation, 0, 1, 0);
  cogl_framebuffer_rotate (fb, rotation, 1, 0, 0);

  cogl_framebuffer_draw_primitive (fb, data->crate_pipeline, data->prim);

  cogl_framebuffer_pop_matrix (fb);

  /* And finally render our Pango layouts... */

  cogl_pango_render_layout (data->hello_label,
                            (data->framebuffer_width / 2) -
                            (data->hello_label_width / 2),
                            (data->framebuffer_height / 2) -
                            (data->hello_label_height / 2),
                            &white, 0);
}

static void
frame_event_cb (CoglOnscreen *onscreen,
                CoglFrameEvent event,
                CoglFrameInfo *info,
                void *user_data)
{
  Data *data = user_data;

  if (event == COGL_FRAME_EVENT_SYNC)
    data->swap_ready = TRUE;
}

int
main (int argc, char **argv)
{
  CoglContext *ctx;
  CoglOnscreen *onscreen;
  CoglFramebuffer *fb;
  CoglError *error = NULL;
  Data data;
  PangoRectangle hello_label_size;
  float fovy, aspect, z_near, z_2d, z_far;
  CoglDepthState depth_state;

  ctx = cogl_context_new (NULL, &error);
  if (!ctx) {
      fprintf (stderr, "Failed to create context: %s\n", error->message);
      return 1;
  }

  onscreen = cogl_onscreen_new (ctx, 640, 480);
  fb = COGL_FRAMEBUFFER (onscreen);
  data.fb = fb;
  data.framebuffer_width = cogl_framebuffer_get_width (fb);
  data.framebuffer_height = cogl_framebuffer_get_height (fb);

  data.timer = g_timer_new ();

  cogl_onscreen_show (onscreen);

  cogl_framebuffer_set_viewport (fb, 0, 0,
                                 data.framebuffer_width,
                                 data.framebuffer_height);

  fovy = 60; /* y-axis field of view */
  aspect = (float)data.framebuffer_width/(float)data.framebuffer_height;
  z_near = 0.1; /* distance to near clipping plane */
  z_2d = 1000; /* position to 2d plane */
  z_far = 2000; /* distance to far clipping plane */

  cogl_framebuffer_perspective (fb, fovy, aspect, z_near, z_far);

  /* Since the pango renderer emits geometry in pixel/device coordinates
   * and the anti aliasing is implemented with the assumption that the
   * geometry *really* does end up pixel aligned, we setup a modelview
   * matrix so that for geometry in the plane z = 0 we exactly map x
   * coordinates in the range [0,stage_width] and y coordinates in the
   * range [0,stage_height] to the framebuffer extents with (0,0) being
   * the top left.
   *
   * This is roughly what Clutter does for a ClutterStage, but this
   * demonstrates how it is done manually using Cogl.
   */
  cogl_matrix_init_identity (&data.view);
  cogl_matrix_view_2d_in_perspective (&data.view, fovy, aspect, z_near, z_2d,
                                      data.framebuffer_width,
                                      data.framebuffer_height);
  cogl_framebuffer_set_modelview_matrix (fb, &data.view);

  /* Initialize some convenient constants */
  cogl_matrix_init_identity (&identity);
  cogl_color_init_from_4ub (&white, 0xff, 0xff, 0xff, 0xff);

  /* rectangle indices allow the GPU to interpret a list of quads (the
   * faces of our cube) as a list of triangles.
   *
   * Since this is a very common thing to do
   * cogl_get_rectangle_indices() is a convenience function for
   * accessing internal index buffers that can be shared.
   */
  data.indices = cogl_get_rectangle_indices (ctx, 6 /* n_rectangles */);
  data.prim = cogl_primitive_new_p3t2 (ctx, COGL_VERTICES_MODE_TRIANGLES,
                                       G_N_ELEMENTS (vertices),
                                       vertices);
  /* Each face will have 6 indices so we have 6 * 6 indices in total... */
  cogl_primitive_set_indices (data.prim,
                              data.indices,
                              6 * 6);

  /* Load a jpeg crate texture from a file */
  printf ("crate.jpg (CC by-nc-nd http://bit.ly/9kP45T) ShadowRunner27 http://bit.ly/m1YXLh\n");
  data.texture = COGL_TEXTURE (
    cogl_texture_2d_new_from_file (ctx,
                                   COGL_EXAMPLES_DATA "crate.jpg",
                                   COGL_PIXEL_FORMAT_ANY,
                                   &error));
  if (!data.texture)
    g_error ("Failed to load texture: %s", error->message);

  /* a CoglPipeline conceptually describes all the state for vertex
   * processing, fragment processing and blending geometry. When
   * drawing the geometry for the crate this pipeline says to sample a
   * single texture during fragment processing... */
  data.crate_pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_set_layer_texture (data.crate_pipeline, 0, data.texture);

  /* Since the box is made of multiple triangles that will overlap
   * when drawn and we don't control the order they are drawn in, we
   * enable depth testing to make sure that triangles that shouldn't
   * be visible get culled by the GPU. */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);

  cogl_pipeline_set_depth_state (data.crate_pipeline, &depth_state, NULL);

  /* Setup a Pango font map and context */

  data.pango_font_map = COGL_PANGO_FONT_MAP (cogl_pango_font_map_new());

  cogl_pango_font_map_set_use_mipmapping (data.pango_font_map, TRUE);

  data.pango_context = cogl_pango_font_map_create_context (data.pango_font_map);

  data.pango_font_desc = pango_font_description_new ();
  pango_font_description_set_family (data.pango_font_desc, "Sans");
  pango_font_description_set_size (data.pango_font_desc, 30 * PANGO_SCALE);

  /* Setup the "Hello Cogl" text */

  data.hello_label = pango_layout_new (data.pango_context);
  pango_layout_set_font_description (data.hello_label, data.pango_font_desc);
  pango_layout_set_text (data.hello_label, "Hello Cogl", -1);

  pango_layout_get_extents (data.hello_label, NULL, &hello_label_size);
  data.hello_label_width = PANGO_PIXELS (hello_label_size.width);
  data.hello_label_height = PANGO_PIXELS (hello_label_size.height);

  cogl_push_framebuffer (fb);

  data.swap_ready = TRUE;

  cogl_onscreen_add_frame_callback (COGL_ONSCREEN (fb),
                                    frame_event_cb,
                                    &data,
                                    NULL); /* destroy notify */


  while (1)
    {
      CoglPollFD *poll_fds;
      int n_poll_fds;
      int64_t timeout;

      if (data.swap_ready)
        {
          paint (&data);
          cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));
        }

      cogl_poll_renderer_get_info (cogl_context_get_renderer (ctx),
                                   &poll_fds, &n_poll_fds, &timeout);

      g_poll ((GPollFD *) poll_fds, n_poll_fds,
              timeout == -1 ? -1 : timeout / 1000);

      cogl_poll_renderer_dispatch (cogl_context_get_renderer (ctx),
                                   poll_fds, n_poll_fds);
    }

  return 0;
}

