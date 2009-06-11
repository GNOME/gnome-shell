
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

#define QUAD_WIDTH 20

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define MASK_RED(COLOR)   ((COLOR & 0xff000000) >> 24);
#define MASK_GREEN(COLOR) ((COLOR & 0xff0000) >> 16);
#define MASK_BLUE(COLOR)  ((COLOR & 0xff00) >> 8);
#define MASK_ALPHA(COLOR) (COLOR & 0xff);

#define BLEND_CONSTANT_UNUSED 0xDEADBEEF
#define TEX_CONSTANT_UNUSED   0xDEADBEEF

typedef struct _TestState
{
  guint frame;
  ClutterGeometry stage_geom;
} TestState;


static void
check_pixel (GLubyte *pixel, guint32 color)
{
  guint8 r = MASK_RED (color);
  guint8 g = MASK_GREEN (color);
  guint8 b = MASK_BLUE (color);
  guint8 a = MASK_ALPHA (color);

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
test_blend (TestState *state,
            int x,
            int y,
            guint32 src_color,
            guint32 dst_color,
            const char *blend_string,
            guint32 blend_constant,
            guint32 expected_result)
{
  /* src color */
  guint8 Sr = MASK_RED (src_color);
  guint8 Sg = MASK_GREEN (src_color);
  guint8 Sb = MASK_BLUE (src_color);
  guint8 Sa = MASK_ALPHA (src_color);
  /* dest color */
  guint8 Dr = MASK_RED (dst_color);
  guint8 Dg = MASK_GREEN (dst_color);
  guint8 Db = MASK_BLUE (dst_color);
  guint8 Da = MASK_ALPHA (dst_color);
  /* blend constant - when applicable */
  guint8 Br = MASK_RED (blend_constant);
  guint8 Bg = MASK_GREEN (blend_constant);
  guint8 Bb = MASK_BLUE (blend_constant);
  guint8 Ba = MASK_ALPHA (blend_constant);
  CoglColor blend_const_color;

  CoglHandle material;
  gboolean status;
  GError *error = NULL;
  GLubyte pixel[4];
  GLint y_off;
  GLint x_off;

  /* First write out the destination color without any blending... */
  material = cogl_material_new ();
  cogl_material_set_color4ub (material, Dr, Dg, Db, Da);
  cogl_material_set_blend (material, "RGBA = ADD (SRC_COLOR, 0)", NULL);
  cogl_set_source (material);
  cogl_rectangle (x * QUAD_WIDTH,
                  y * QUAD_WIDTH,
                  x * QUAD_WIDTH + QUAD_WIDTH,
                  y * QUAD_WIDTH + QUAD_WIDTH);
  cogl_material_unref (material);

  /*
   * Now blend a rectangle over our well defined destination:
   */

  material = cogl_material_new ();
  cogl_material_set_color4ub (material, Sr, Sg, Sb, Sa);

  status = cogl_material_set_blend (material, blend_string, &error);
  if (!status)
    {
      /* It's not strictly a test failure; you need a more capable GPU or
       * driver to test this blend string. */
      g_debug ("Failed to test blend string %s: %s",
               blend_string, error->message);
    }

  cogl_color_set_from_4ub (&blend_const_color, Br, Bg, Bb, Ba);
  cogl_material_set_blend_constant (material, &blend_const_color);

  cogl_set_source (material);
  cogl_rectangle (x * QUAD_WIDTH,
                  y * QUAD_WIDTH,
                  x * QUAD_WIDTH + QUAD_WIDTH,
                  y * QUAD_WIDTH + QUAD_WIDTH);
  cogl_material_unref (material);

  /* See what we got... */

  /* NB: glReadPixels is done in GL screen space so y = 0 is at the bottom */
  y_off = state->stage_geom.height - y * QUAD_WIDTH - (QUAD_WIDTH / 2);
  x_off = x * QUAD_WIDTH + (QUAD_WIDTH / 2);

  /* XXX:
   * We haven't always had good luck with GL drivers implementing glReadPixels
   * reliably and skipping the first two frames improves our chances... */
  if (state->frame <= 2)
    return;

  glReadPixels (x_off, y_off, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
  if (g_test_verbose ())
    {
      g_print ("test_blend (%d, %d):\n%s\n", x, y, blend_string);
      g_print ("  src color = %02x, %02x, %02x, %02x\n", Sr, Sg, Sb, Sa);
      g_print ("  dst color = %02x, %02x, %02x, %02x\n", Dr, Dg, Db, Da);
      if (blend_constant != BLEND_CONSTANT_UNUSED)
        g_print ("  blend constant = %02x, %02x, %02x, %02x\n",
                 Br, Bg, Bb, Ba);
      else
        g_print ("  blend constant = UNUSED\n");
      g_print ("  result = %x, %x, %x, %x\n",
               pixel[RED], pixel[GREEN], pixel[BLUE], pixel[ALPHA]);
    }

  check_pixel (pixel, expected_result);
}

static CoglHandle
make_texture (guint32 color)
{
  guchar *tex_data, *p;
  guint8 r = MASK_RED (color);
  guint8 g = MASK_GREEN (color);
  guint8 b = MASK_BLUE (color);
  guint8 a = MASK_ALPHA (color);
  CoglHandle tex;

  tex_data = g_malloc (QUAD_WIDTH * QUAD_WIDTH * 4);

  for (p = tex_data + QUAD_WIDTH * QUAD_WIDTH * 4; p > tex_data;)
    {
      *(--p) = a;
      *(--p) = b;
      *(--p) = g;
      *(--p) = r;
    }

  /* Note: we don't use COGL_PIXEL_FORMAT_ANY for the internal format here
   * since we don't want to allow Cogl to premultiply our data. */
  tex = cogl_texture_new_from_data (QUAD_WIDTH,
                                    QUAD_WIDTH,
                                    COGL_TEXTURE_NONE,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    QUAD_WIDTH * 4,
                                    tex_data);

  g_free (tex_data);

  return tex;
}

static void
test_tex_combine (TestState *state,
                  int x,
                  int y,
                  guint32 tex0_color,
                  guint32 tex1_color,
                  guint32 combine_constant,
                  const char *combine_string,
                  guint32 expected_result)
{
  CoglHandle tex0, tex1;

  /* combine constant - when applicable */
  guint8 Cr = MASK_RED (combine_constant);
  guint8 Cg = MASK_GREEN (combine_constant);
  guint8 Cb = MASK_BLUE (combine_constant);
  guint8 Ca = MASK_ALPHA (combine_constant);
  CoglColor combine_const_color;

  CoglHandle material;
  gboolean status;
  GError *error = NULL;
  GLubyte pixel[4];
  GLint y_off;
  GLint x_off;


  tex0 = make_texture (tex0_color);
  tex1 = make_texture (tex1_color);

  material = cogl_material_new ();

  cogl_material_set_color4ub (material, 0x80, 0x80, 0x80, 0x80);
  cogl_material_set_blend (material, "RGBA = ADD (SRC_COLOR, 0)", NULL);

  cogl_material_set_layer (material, 0, tex0);
  cogl_material_set_layer_combine (material, 0,
                                   "RGBA = REPLACE (TEXTURE)", NULL);

  cogl_material_set_layer (material, 1, tex1);
  status = cogl_material_set_layer_combine (material, 1,
                                            combine_string, &error);
  if (!status)
    {
      /* It's not strictly a test failure; you need a more capable GPU or
       * driver to test this texture combine string. */
      g_debug ("Failed to test texture combine string %s: %s",
               combine_string, error->message);
    }

  cogl_color_set_from_4ub (&combine_const_color, Cr, Cg, Cb, Ca);
  cogl_material_set_layer_combine_constant (material, 1, &combine_const_color);

  cogl_set_source (material);
  cogl_rectangle (x * QUAD_WIDTH,
                  y * QUAD_WIDTH,
                  x * QUAD_WIDTH + QUAD_WIDTH,
                  y * QUAD_WIDTH + QUAD_WIDTH);
  cogl_material_unref (material);

  cogl_handle_unref (tex0);
  cogl_handle_unref (tex1);

  /* See what we got... */

  /* NB: glReadPixels is done in GL screen space so y = 0 is at the bottom */
  y_off = state->stage_geom.height - y * QUAD_WIDTH - (QUAD_WIDTH / 2);
  x_off = x * QUAD_WIDTH + (QUAD_WIDTH / 2);

  /* XXX:
   * We haven't always had good luck with GL drivers implementing glReadPixels
   * reliably and skipping the first two frames improves our chances... */
  if (state->frame <= 2)
    return;

  glReadPixels (x_off, y_off, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
  if (g_test_verbose ())
    {
      g_print ("test_tex_combine (%d, %d):\n%s\n", x, y, combine_string);
      g_print ("  texture 0 color = 0x%08lX\n", (unsigned long)tex0_color);
      g_print ("  texture 1 color = 0x%08lX\n", (unsigned long)tex1_color);
      if (combine_constant != TEX_CONSTANT_UNUSED)
        g_print ("  combine constant = %02x, %02x, %02x, %02x\n",
                 Cr, Cg, Cb, Ca);
      else
        g_print ("  combine constant = UNUSED\n");
      g_print ("  result = %02x, %02x, %02x, %02x\n",
               pixel[RED], pixel[GREEN], pixel[BLUE], pixel[ALPHA]);
    }

  check_pixel (pixel, expected_result);
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  int frame_num;

  test_blend (state, 0, 0, /* position */
              0xff0000ff, /* src */
              0xffffffff, /* dst */
              "RGBA = ADD (SRC_COLOR, 0)",
              BLEND_CONSTANT_UNUSED,
              0xff0000ff); /* expected */

  test_blend (state, 1, 0, /* position */
              0x11223344, /* src */
              0x11223344, /* dst */
              "RGBA = ADD (SRC_COLOR, DST_COLOR)",
              BLEND_CONSTANT_UNUSED,
              0x22446688); /* expected */

  test_blend (state, 2, 0, /* position */
              0x80808080, /* src */
              0xffffffff, /* dst */
              "RGBA = ADD (SRC_COLOR * (CONSTANT), 0)",
              0x80808080, /* constant (RGBA all = 0.5 when normalized) */
              0x40404040); /* expected */

  test_blend (state, 3, 0, /* position */
              0x80000080, /* src (alpha = 0.5 when normalized) */
              0x40000000, /* dst */
              "RGBA = ADD (SRC_COLOR * (SRC_COLOR[A]),"
              "            DST_COLOR * (1-SRC_COLOR[A]))",
              BLEND_CONSTANT_UNUSED,
              0x60000040); /* expected */

  /* XXX:
   * For all texture combine tests tex0 will use a combine mode of
   * "RGBA = REPLACE (TEXTURE)"
   */

  test_tex_combine (state, 4, 0, /* position */
                    0x11111111, /* texture 0 color */
                    0x22222222, /* texture 1 color */
                    TEX_CONSTANT_UNUSED,
                    "RGBA = ADD (PREVIOUS, TEXTURE)", /* tex combine */
                    0x33333333); /* expected */

  test_tex_combine (state, 5, 0, /* position */
                    0x40404040, /* texture 0 color */
                    0x80808080, /* texture 1 color (RGBA all = 0.5) */
                    TEX_CONSTANT_UNUSED,
                    "RGBA = MODULATE (PREVIOUS, TEXTURE)", /* tex combine */
                    0x20202020); /* expected */

  test_tex_combine (state, 6, 0, /* position */
                    0xffffff80, /* texture 0 color (alpha = 0.5) */
                    0xDEADBE40, /* texture 1 color */
                    TEX_CONSTANT_UNUSED,
                    "RGB = REPLACE (PREVIOUS)"
                    "A = MODULATE (PREVIOUS, TEXTURE)", /* tex combine */
                    0xffffff20); /* expected */

  /* XXX: we are assuming test_tex_combine creates a material with
   * a color of 0x80808080 (i.e. the "PRIMARY" color) */
  test_tex_combine (state, 7, 0, /* position */
                    0xffffff80, /* texture 0 color (alpha = 0.5) */
                    0xDEADBE20, /* texture 1 color */
                    TEX_CONSTANT_UNUSED,
                    "RGB = REPLACE (PREVIOUS)"
                    "A = MODULATE (PRIMARY, TEXTURE)", /* tex combine */
                    0xffffff10); /* expected */

  /* XXX: Experiments have shown that for some buggy drivers, when using
   * glReadPixels there is some kind of race, so we delay our test for a
   * few frames and a few seconds:
   */
  frame_num = state->frame++;
  if (frame_num < 2)
    g_usleep (G_USEC_PER_SEC);

  /* Comment this out if you want visual feedback for what this test paints */
#if 1
  if (frame_num == 3)
    clutter_main_quit ();
#endif
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_blend_strings (TestConformSimpleFixture *fixture,
                    gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  guint idle_source;

  state.frame = 0;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (stage);

  clutter_main ();

  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");
}

