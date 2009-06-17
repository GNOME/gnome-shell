
#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

/* This test verifies that the simplest usage of the vertex buffer API,
 * where we add contiguous (x,y) GLfloat vertices, and RGBA GLubyte color
 * attributes to a buffer, submit, and draw.
 *
 * It also tries to verify that the enable/disable attribute APIs are working
 * too.
 *
 * If you want visual feedback of what this test paints for debugging purposes,
 * then remove the call to clutter_main_quit() in validate_result.
 */

typedef struct _TestState
{
  CoglHandle buffer;
  CoglHandle texture;
  CoglHandle material;
  ClutterGeometry stage_geom;
  guint frame;
} TestState;

static void
validate_result (TestState *state)
{
  GLubyte pixel[4];
  GLint y_off = 90;

  if (g_test_verbose ())
    g_print ("y_off = %d\n", y_off);

  /* NB: We ignore the alpha, since we don't know if our render target is
   * RGB or RGBA */

#define RED 0
#define GREEN 1
#define BLUE 2

  /* Should see a blue pixel */
  cogl_read_pixels (10, y_off, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixel);
  if (g_test_verbose ())
    g_print ("pixel 0 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] == 0 && pixel[GREEN] == 0 && pixel[BLUE] != 0);

  /* Should see a red pixel */
  cogl_read_pixels (110, y_off, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixel);
  if (g_test_verbose ())
    g_print ("pixel 1 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] != 0 && pixel[GREEN] == 0 && pixel[BLUE] == 0);

  /* Should see a blue pixel */
  cogl_read_pixels (210, y_off, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixel);
  if (g_test_verbose ())
    g_print ("pixel 2 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] == 0 && pixel[GREEN] == 0 && pixel[BLUE] != 0);

  /* Should see a green pixel, at bottom of 4th triangle */
  cogl_read_pixels (310, y_off, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixel);
  if (g_test_verbose ())
    g_print ("pixel 3 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[GREEN] > pixel[RED] && pixel[GREEN] > pixel[BLUE]);

  /* Should see a red pixel, at top of 4th triangle */
  cogl_read_pixels (310, y_off - 70, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixel);
  if (g_test_verbose ())
    g_print ("pixel 4 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] > pixel[GREEN] && pixel[RED] > pixel[BLUE]);


#undef RED
#undef GREEN
#undef BLUE

  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  /* Draw a faded blue triangle */
  cogl_vertex_buffer_enable (state->buffer, "gl_Color::blue");
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_vertex_buffer_draw (state->buffer,
			   GL_TRIANGLE_STRIP, /* mode */
			   0, /* first */
			   3); /* count */

  /* Draw a red triangle */
  /* Here we are testing that the disable attribute works; if it doesn't
   * the triangle will remain faded blue */
  cogl_translate (100, 0, 0);
  cogl_vertex_buffer_disable (state->buffer, "gl_Color::blue");
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_vertex_buffer_draw (state->buffer,
			   GL_TRIANGLE_STRIP, /* mode */
			   0, /* first */
			   3); /* count */

  /* Draw a faded blue triangle */
  /* Here we are testing that the re-enable works; if it doesn't
   * the triangle will remain red */
  cogl_translate (100, 0, 0);
  cogl_vertex_buffer_enable (state->buffer, "gl_Color::blue");
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_vertex_buffer_draw (state->buffer,
			   GL_TRIANGLE_STRIP, /* mode */
			   0, /* first */
			   3); /* count */

  /* Draw a textured triangle */
  cogl_translate (100, 0, 0);
  cogl_vertex_buffer_disable (state->buffer, "gl_Color::blue");
  cogl_set_source (state->material);
  cogl_material_set_color4ub (state->material, 0xff, 0xff, 0xff, 0xff);
  cogl_vertex_buffer_draw (state->buffer,
                           GL_TRIANGLE_STRIP, /* mode */
                           0, /* first */
                           3); /* count */

  /* XXX: Experiments have shown that for some buggy drivers, when using
   * glReadPixels there is some kind of race, so we delay our test for a
   * few frames and a few seconds:
   */
  if (state->frame >= 2)
    validate_result (state);
  else
    g_usleep (G_USEC_PER_SEC);

  state->frame++;
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}



void
test_vertex_buffer_contiguous (TestConformSimpleFixture *fixture,
		               gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterColor stage_clr = {0x0, 0x0, 0x0, 0xff};
  ClutterActor *group;
  guint idle_source;
  guchar tex_data[] = {
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x00, 0xff, 0x00, 0xff,
    0x00, 0xff, 0x00, 0xff
  };

  state.frame = 0;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_clr);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_actor_set_size (group,
			  state.stage_geom.width,
			  state.stage_geom.height);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  state.texture = cogl_texture_new_from_data (2, 2,
                                              COGL_TEXTURE_NO_SLICING,
                                              COGL_PIXEL_FORMAT_RGBA_8888,
                                              COGL_PIXEL_FORMAT_ANY,
                                              0, /* auto calc row stride */
                                              tex_data);

  state.material = cogl_material_new ();
  cogl_material_set_color4ub (state.material, 0x00, 0xff, 0x00, 0xff);
  cogl_material_set_layer (state.material, 0, state.texture);

  {
    GLfloat triangle_verts[3][2] =
      {
	{0.0,	0.0},
	{100.0, 100.0},
	{0.0,	100.0}
      };
    GLbyte triangle_colors[3][4] =
      {
	{0x00, 0x00, 0xff, 0xff}, /* blue */
	{0x00, 0x00, 0xff, 0x00}, /* transparent blue */
	{0x00, 0x00, 0xff, 0x00}  /* transparent blue */
      };
    GLfloat triangle_tex_coords[3][2] =
      {
        {0.0, 0.0},
        {1.0, 1.0},
        {0.0, 1.0}
      };
    state.buffer = cogl_vertex_buffer_new (3 /* n vertices */);
    cogl_vertex_buffer_add (state.buffer,
			    "gl_Vertex",
			    2, /* n components */
			    GL_FLOAT,
			    FALSE, /* normalized */
			    0, /* stride */
			    triangle_verts);
    cogl_vertex_buffer_add (state.buffer,
			    "gl_Color::blue",
			    4, /* n components */
			    GL_UNSIGNED_BYTE,
			    FALSE, /* normalized */
			    0, /* stride */
			    triangle_colors);
    cogl_vertex_buffer_add (state.buffer,
			    "gl_MultiTexCoord0",
			    2, /* n components */
			    GL_FLOAT,
			    FALSE, /* normalized */
			    0, /* stride */
			    triangle_tex_coords);

    cogl_vertex_buffer_submit (state.buffer);
  }

  clutter_actor_show_all (stage);

  clutter_main ();

  cogl_handle_unref (state.buffer);
  cogl_handle_unref (state.material);
  cogl_handle_unref (state.texture);

  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");
}

