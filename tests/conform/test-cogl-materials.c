#include "config.h"

/* XXX: we currently include config.h above as a hack so we can
 * determine if we are running with GLES2 or not but since Clutter
 * uses the experimental Cogl api that will also define
 * COGL_ENABLE_EXPERIMENTAL_2_0_API. The cogl_material_ api isn't
 * exposed if COGL_ENABLE_EXPERIMENTAL_2_0_API is defined though so we
 * undef it before cogl.h is included */
#undef COGL_ENABLE_EXPERIMENTAL_2_0_API

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

static TestConformGLFunctions gl_functions;

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24)
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16)
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8)
#define MASK_ALPHA(COLOR) (COLOR & 0xff)

#ifndef GL_VERSION
#define GL_VERSION 0x1F02
#endif

#ifndef GL_MAX_TEXTURE_IMAGE_UNITS
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#endif
#ifndef GL_MAX_VERTEX_ATTRIBS
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif

typedef struct _TestState
{
  ClutterGeometry stage_geom;
} TestState;


static void
check_pixel (TestState *state, int x, int y, guint32 color)
{
  int y_off;
  int x_off;
  guint8 pixel[4];
  guint8 r = MASK_RED (color);
  guint8 g = MASK_GREEN (color);
  guint8 b = MASK_BLUE (color);
  guint8 a = MASK_ALPHA (color);

  /* See what we got... */

  y_off = y * QUAD_WIDTH + (QUAD_WIDTH / 2);
  x_off = x * QUAD_WIDTH + (QUAD_WIDTH / 2);

  cogl_read_pixels (x_off, y_off, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    pixel);
  if (g_test_verbose ())
    g_print ("  result = %02x, %02x, %02x, %02x\n",
             pixel[RED], pixel[GREEN], pixel[BLUE], pixel[ALPHA]);

  if (g_test_verbose ())
    g_print ("  expected = %x, %x, %x, %x\n",
             r, g, b, a);
  /* FIXME - allow for hardware in-precision */
  g_assert (pixel[RED] == r);
  g_assert (pixel[GREEN] == g);
  g_assert (pixel[BLUE] == b);

  /* FIXME
   * We ignore the alpha, since we don't know if our render target is
   * RGB or RGBA */
  /* g_assert (pixel[ALPHA] == a); */
}

static void
test_material_with_primitives (TestState *state,
                               int x, int y,
                               guint32 color)
{
  CoglTextureVertex verts[4];
  CoglHandle vbo;

  verts[0].x = 0;
  verts[0].y = 0;
  verts[0].z = 0;
  verts[1].x = 0;
  verts[1].y = QUAD_WIDTH;
  verts[1].z = 0;
  verts[2].x = QUAD_WIDTH;
  verts[2].y = QUAD_WIDTH;
  verts[2].z = 0;
  verts[3].x = QUAD_WIDTH;
  verts[3].y = 0;
  verts[3].z = 0;

  cogl_push_matrix ();

  cogl_translate (x * QUAD_WIDTH, y * QUAD_WIDTH, 0);

  cogl_rectangle (0, 0, QUAD_WIDTH, QUAD_WIDTH);

  cogl_translate (0, QUAD_WIDTH, 0);
  cogl_polygon (verts, 4, FALSE);

  cogl_translate (0, QUAD_WIDTH, 0);
  vbo = cogl_vertex_buffer_new (4);
  cogl_vertex_buffer_add (vbo,
                          "gl_Vertex",
                          2, /* n components */
                          COGL_ATTRIBUTE_TYPE_FLOAT,
                          FALSE, /* normalized */
                          sizeof (CoglTextureVertex), /* stride */
                          verts);
  cogl_vertex_buffer_draw (vbo,
                           COGL_VERTICES_MODE_TRIANGLE_FAN,
                           0, /* first */
                           4); /* count */
  cogl_handle_unref (vbo);

  cogl_pop_matrix ();

  check_pixel (state, x, y,   color);
  check_pixel (state, x, y+1, color);
  check_pixel (state, x, y+2, color);
}

static void
test_invalid_texture_layers (TestState *state, int x, int y)
{
  CoglHandle        material = cogl_material_new ();

  /* explicitly create a layer with an invalid handle. This may be desireable
   * if the user also sets a texture combine string that e.g. refers to a
   * constant color. */
  cogl_material_set_layer (material, 0, COGL_INVALID_HANDLE);

  cogl_set_source (material);

  cogl_handle_unref (material);

  /* We expect a white fallback material to be used */
  test_material_with_primitives (state, x, y, 0xffffffff);
}

#ifdef COGL_HAS_GLES2
static gboolean
using_gles2_driver (void)
{
  /* FIXME: This should probably be replaced with some way to query
     the driver from Cogl */
  return g_str_has_prefix ((const char *) gl_functions.glGetString (GL_VERSION),
                           "OpenGL ES 2");
}
#endif

static void
test_using_all_layers (TestState *state, int x, int y)
{
  CoglHandle material = cogl_material_new ();
  guint8 white_pixel[] = { 0xff, 0xff, 0xff, 0xff };
  guint8 red_pixel[] = { 0xff, 0x00, 0x00, 0xff };
  CoglHandle white_texture;
  CoglHandle red_texture;
  int n_layers;
  int i;

  /* Create a material that uses the maximum number of layers. All but
     the last layer will use a solid white texture. The last layer
     will use a red texture. The layers will all be modulated together
     so the final fragment should be red. */

  white_texture = cogl_texture_new_from_data (1, 1, COGL_TEXTURE_NONE,
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                              COGL_PIXEL_FORMAT_ANY,
                                              4, white_pixel);
  red_texture = cogl_texture_new_from_data (1, 1, COGL_TEXTURE_NONE,
                                            COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                            COGL_PIXEL_FORMAT_ANY,
                                            4, red_pixel);

  /* FIXME: Cogl doesn't provide a way to query the maximum number of
     texture layers so for now we'll just ask GL directly. */
#ifdef COGL_HAS_GLES2
  if (using_gles2_driver ())
    {
      int n_image_units, n_attribs;
      /* GLES 2 doesn't have GL_MAX_TEXTURE_UNITS and it uses
         GL_MAX_TEXTURE_IMAGE_UNITS instead */
      gl_functions.glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &n_image_units);
      /* Cogl needs a vertex attrib for each layer to upload the texture
         coordinates */
      gl_functions.glGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &n_attribs);
      /* We can't use two of the attribs because they are used by the
         position and color */
      n_attribs -= 2;
      n_layers = MIN (n_attribs, n_image_units);
    }
  else
#endif
    {
#if defined(COGL_HAS_GLES1) || defined(COGL_HAS_GL)
      gl_functions.glGetIntegerv (GL_MAX_TEXTURE_UNITS, &n_layers);
#endif
    }

  /* FIXME: is this still true? */
  /* Cogl currently can't cope with more than 32 layers so we'll also
     limit the maximum to that. */
  if (n_layers > 32)
    n_layers = 32;

  for (i = 0; i < n_layers; i++)
    {
      cogl_material_set_layer_filters (material, i,
                                       COGL_MATERIAL_FILTER_NEAREST,
                                       COGL_MATERIAL_FILTER_NEAREST);
      cogl_material_set_layer (material, i,
                               i == n_layers - 1 ? red_texture : white_texture);
    }

  cogl_set_source (material);

  cogl_handle_unref (material);
  cogl_handle_unref (white_texture);
  cogl_handle_unref (red_texture);

  /* We expect the final fragment to be red */
  test_material_with_primitives (state, x, y, 0xff0000ff);
}

static void
test_invalid_texture_layers_with_constant_colors (TestState *state,
                                                  int x, int y)
{
  CoglHandle material = cogl_material_new ();
  CoglColor constant_color;

  /* explicitly create a layer with an invalid handle */
  cogl_material_set_layer (material, 0, COGL_INVALID_HANDLE);

  /* ignore the fallback texture on the layer and use a constant color
     instead */
  cogl_color_init_from_4ub (&constant_color, 0, 0, 255, 255);
  cogl_material_set_layer_combine (material, 0,
                                   "RGBA=REPLACE(CONSTANT)",
                                   NULL);
  cogl_material_set_layer_combine_constant (material, 0, &constant_color);

  cogl_set_source (material);

  cogl_handle_unref (material);

  /* We expect the final fragments to be green */
  test_material_with_primitives (state, x, y, 0x0000ffff);
}

static void
basic_ref_counting_destroy_cb (void *user_data)
{
  gboolean *destroyed_flag = user_data;

  g_assert (*destroyed_flag == FALSE);

  *destroyed_flag = TRUE;
}

static void
test_basic_ref_counting (void)
{
  CoglMaterial *material_parent;
  gboolean parent_destroyed = FALSE;
  CoglMaterial *material_child;
  gboolean child_destroyed = FALSE;
  static CoglUserDataKey user_data_key;

  /* This creates a material with a copy and then just unrefs them
     both without setting them as a source. They should immediately be
     freed. We can test whether they were freed or not by registering
     a destroy callback with some user data */

  material_parent = cogl_material_new ();
  /* Set some user data so we can detect when the material is
     destroyed */
  cogl_object_set_user_data (COGL_OBJECT (material_parent),
                             &user_data_key,
                             &parent_destroyed,
                             basic_ref_counting_destroy_cb);

  material_child = cogl_material_copy (material_parent);
  cogl_object_set_user_data (COGL_OBJECT (material_child),
                             &user_data_key,
                             &child_destroyed,
                             basic_ref_counting_destroy_cb);

  cogl_object_unref (material_child);
  cogl_object_unref (material_parent);

  g_assert (parent_destroyed);
  g_assert (child_destroyed);
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  test_invalid_texture_layers (state,
                               0, 0 /* position */
                               );
  test_invalid_texture_layers_with_constant_colors (state,
                                                    1, 0 /* position */
                                                    );
  test_using_all_layers (state,
                         2, 0 /* position */
                         );

  test_basic_ref_counting ();

  /* Comment this out if you want visual feedback for what this test paints */
#if 1
  clutter_main_quit ();
#endif
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return G_SOURCE_CONTINUE;
}

void
test_cogl_materials (TestConformSimpleFixture *fixture,
                     gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  guint idle_source;

  test_conform_get_gl_functions (&gl_functions);

  stage = clutter_stage_new ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = clutter_threads_add_idle (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (stage);

  clutter_main ();

  g_source_remove (idle_source);

  clutter_actor_destroy (stage);

  if (g_test_verbose ())
    g_print ("OK\n");
}
