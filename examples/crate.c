#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

/* The state for this example... */
typedef struct _Data
{
  int framebuffer_width;
  int framebuffer_height;

  CoglMatrix view;

  CoglPrimitive *prim;
  CoglHandle texture;
  CoglPipeline *crate_pipeline;

  /* The cube continually rotates around each axis. */
  float rotate_x;
  float rotate_y;
  float rotate_z;

  CoglPangoFontMap *pango_font_map;
  PangoContext *pango_context;
  PangoFontDescription *pango_font_desc;

  PangoLayout *hello_label;
  int hello_label_width;
  int hello_label_height;

} Data;

/* A static identity matrix initialized for convenience. */
static CoglMatrix identity;
/* static colors initialized for convenience. */
static CoglColor black;
static CoglColor white;

/* A cube modelled as a list of triangles. Potentially this could be
 * done more efficiently as a triangle strip or using a separate index
 * array, but this way is pretty simple, if a little verbose. */
CoglVertexP3T2 vertices[] =
{
  /* Front face */
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},

  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},

  /* Back face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},

  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},

  /* Top face */
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},

  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},

  /* Bottom face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},

  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},

  /* Right face */
  { /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ 1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */ 1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},

  { /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ 1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},
  { /* pos = */ 1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},

  /* Left face */
  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},
  { /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f},

  { /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},
  { /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f},
  { /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f}
};

static void
paint (Data *data)
{
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR|COGL_BUFFER_BIT_DEPTH);

  cogl_push_matrix ();

  cogl_translate (data->framebuffer_width / 2, data->framebuffer_height / 2, 0);

  cogl_scale (75, 75, 75);

  /* Rotate the cube separately around each axis.
   *
   * Note: Cogl matrix manipulation follows the same rules as for
   * OpenGL. We use column-major matrices and - if you consider the
   * transformations happening to the model - then they are combined
   * in reverse order which is why the rotation is done last, since
   * we want it to be a rotation around the origin, before it is
   * scaled and translated.
   */
  cogl_rotate (data->rotate_x++, 0, 0, 1);
  cogl_rotate (data->rotate_y++, 0, 1, 0);
  cogl_rotate (data->rotate_z++, 1, 0, 0);

  /* Whenever you draw something with Cogl using geometry defined by
   * one of cogl_rectangle, cogl_polygon, cogl_path or
   * cogl_vertex_buffer then you have a current pipeline that defines
   * how that geometry should be processed.
   *
   * Here we are making our crate pipeline current which will sample
   * the crate texture when fragment processing. */
  cogl_set_source (data->crate_pipeline);

  /* Give Cogl some geometry to draw. */
  cogl_primitive_draw (data->prim);

  cogl_set_depth_test_enabled (FALSE);

  cogl_pop_matrix ();

  /* And finally render our Pango layouts... */

  cogl_pango_render_layout (data->hello_label,
                            (data->framebuffer_width / 2) -
                            (data->hello_label_width / 2),
                            (data->framebuffer_height / 2) -
                            (data->hello_label_height / 2),
                            &white, 0);
}

int
main (int argc, char **argv)
{
  CoglContext *ctx;
  CoglOnscreen *onscreen;
  CoglFramebuffer *fb;
  GError *error = NULL;
  Data data;
  PangoRectangle hello_label_size;
  float fovy, aspect, z_near, z_2d, z_far;
  CoglDepthState depth_state;

  g_type_init ();

  ctx = cogl_context_new (NULL, &error);
  if (!ctx) {
      fprintf (stderr, "Failed to create context: %s\n", error->message);
      return 1;
  }

  data.framebuffer_width = 640;
  data.framebuffer_height = 480;
  onscreen = cogl_onscreen_new (ctx, data.framebuffer_width, data.framebuffer_height);
  /* Eventually there will be an implicit allocate on first use so this
   * will become optional... */
  fb = COGL_FRAMEBUFFER (onscreen);
  if (!cogl_framebuffer_allocate (fb, &error)) {
      fprintf (stderr, "Failed to allocate framebuffer: %s\n", error->message);
      return 1;
  }

  cogl_onscreen_show (onscreen);

  cogl_push_framebuffer (fb);
  cogl_set_viewport (0, 0, data.framebuffer_width, data.framebuffer_height);

  fovy = 60; /* y-axis field of view */
  aspect = (float)data.framebuffer_width/(float)data.framebuffer_height;
  z_near = 0.1; /* distance to near clipping plane */
  z_2d = 1000; /* position to 2d plane */
  z_far = 2000; /* distance to far clipping plane */

  cogl_perspective (fovy, aspect, z_near, z_far);

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
  cogl_set_modelview_matrix (&data.view);
  cogl_pop_framebuffer ();

  /* Initialize some convenient constants */
  cogl_matrix_init_identity (&identity);
  cogl_color_set_from_4ub (&black, 0x00, 0x00, 0x00, 0xff);
  cogl_color_set_from_4ub (&white, 0xff, 0xff, 0xff, 0xff);

  data.prim = cogl_primitive_new_p3t2 (COGL_VERTICES_MODE_TRIANGLES,
                                       G_N_ELEMENTS (vertices),
                                       vertices);

  /* Load a jpeg crate texture from a file */
  printf ("crate.jpg (CC by-nc-nd http://bit.ly/9kP45T) ShadowRunner27 http://bit.ly/m1YXLh\n");
  data.texture = cogl_texture_new_from_file (COGL_EXAMPLES_DATA "crate.jpg",
                                             COGL_TEXTURE_NO_SLICING,
                                             COGL_PIXEL_FORMAT_ANY,
                                             NULL);
  if (!data.texture)
    g_error ("Failed to load texture");

  /* a CoglPipeline conceptually describes all the state for vertex
   * processing, fragment processing and blending geometry. When
   * drawing the geometry for the crate this pipeline says to sample a
   * single texture during fragment processing... */
  data.crate_pipeline = cogl_pipeline_new ();
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

  while (1)
    {
      paint (&data);
      cogl_framebuffer_swap_buffers (fb);
    }

  return 0;
}

