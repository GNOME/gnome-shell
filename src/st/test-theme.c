/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <clutter/clutter.h>
#include "st-theme.h"
#include "st-theme-context.h"
#include <math.h>
#include <string.h>

static StThemeNode *root;
static StThemeNode *group1;
static StThemeNode *text1;
static StThemeNode *text2;
static StThemeNode *group2;
static StThemeNode *text3;
static StThemeNode *text4;
static StThemeNode *group3;
static StThemeNode *cairo_texture;
static gboolean fail;

static const char *test;

static void
assert_font (StThemeNode *node,
	     const char  *node_description,
	     const char  *expected)
{
  char *value = pango_font_description_to_string (st_theme_node_get_font (node));

  if (strcmp (expected, value) != 0)
    {
      g_print ("%s: %s.font: expected: %s, got: %s\n",
	       test, node_description, expected, value);
      fail = TRUE;
    }

  g_free (value);
}

static char *
text_decoration_to_string (StTextDecoration decoration)
{
  GString *result = g_string_new (NULL);

  if (decoration & ST_TEXT_DECORATION_UNDERLINE)
    g_string_append(result, " underline");
  if (decoration & ST_TEXT_DECORATION_OVERLINE)
    g_string_append(result, " overline");
  if (decoration & ST_TEXT_DECORATION_LINE_THROUGH)
    g_string_append(result, " line_through");
  if (decoration & ST_TEXT_DECORATION_BLINK)
    g_string_append(result, " blink");

  if (result->len > 0)
    g_string_erase (result, 0, 1);
  else
    g_string_append(result, "none");

  return g_string_free (result, FALSE);
}

static void
assert_text_decoration (StThemeNode     *node,
			const char      *node_description,
			StTextDecoration expected)
{
  StTextDecoration value = st_theme_node_get_text_decoration (node);
  if (expected != value)
    {
      char *es = text_decoration_to_string (expected);
      char *vs = text_decoration_to_string (value);

      g_print ("%s: %s.text-decoration: expected: %s, got: %s\n",
	       test, node_description, es, vs);
      fail = TRUE;

      g_free (es);
      g_free (vs);
    }
}

static void
assert_foreground_color (StThemeNode *node,
			 const char  *node_description,
			 guint32      expected)
{
  ClutterColor color;
  st_theme_node_get_foreground_color (node, &color);
  guint32 value = clutter_color_to_pixel (&color);

  if (expected != value)
    {
      g_print ("%s: %s.color: expected: #%08x, got: #%08x\n",
	       test, node_description, expected, value);
      fail = TRUE;
    }
}

static void
assert_background_color (StThemeNode *node,
			 const char  *node_description,
			 guint32      expected)
{
  ClutterColor color;
  st_theme_node_get_background_color (node, &color);
  guint32 value = clutter_color_to_pixel (&color);

  if (expected != value)
    {
      g_print ("%s: %s.background-color: expected: #%08x, got: #%08x\n",
	       test, node_description, expected, value);
      fail = TRUE;
    }
}

static void
assert_background_image (StThemeNode *node,
			 const char  *node_description,
			 const char  *expected)
{
  const char *value = st_theme_node_get_background_image (node);
  if (expected == NULL)
    expected = "(null)";
  if (value == NULL)
    value = "(null)";

  if (strcmp (expected, value) != 0)
    {
      g_print ("%s: %s.background-image: expected: %s, got: %s\n",
	       test, node_description, expected, value);
      fail = TRUE;
    }
}

#define LENGTH_EPSILON 0.001

static void
assert_length (const char     *node_description,
	       const char     *property_description,
	       double          expected,
	       double          value)
{
  if (fabs (expected - value) > LENGTH_EPSILON)
    {
      g_print ("%s %s.%s: expected: %3f, got: %3f\n",
	       test, node_description, property_description, expected, value);
      fail = TRUE;
    }
}

static void
test_defaults (void)
{
  test = "defaults";
  /* font comes from context */
  assert_font (root, "stage", "sans-serif 12");
  /* black is the default foreground color */
  assert_foreground_color (root, "stage", 0x00000ff);
}

static void
test_lengths (void)
{
  test = "lengths";
  /* 12pt == 16px at 96dpi */
  assert_length ("group1", "padding-top", 16.,
		 st_theme_node_get_padding (group1, ST_SIDE_TOP));
  /* 12px == 12px */
  assert_length ("group1", "padding-right", 12.,
		 st_theme_node_get_padding (group1, ST_SIDE_RIGHT));
  /* 2em == 32px (with a 12pt font) */
  assert_length ("group1", "padding-bottom", 32.,
		 st_theme_node_get_padding (group1, ST_SIDE_BOTTOM));
  /* 1in == 72pt == 96px, at 96dpi */
  assert_length ("group1", "padding-left", 96.,
		 st_theme_node_get_padding (group1, ST_SIDE_LEFT));
}

static void
test_classes (void)
{
  test = "classes";
  /* .special-text class overrides size and style;
   * the ClutterTexture.special-text selector doesn't match */
  assert_font (text1, "text1", "sans-serif Italic 32px");
}

static void
test_type_inheritance (void)
{
  test = "type_inheritance";
  /* From ClutterTexture element selector */
  assert_length ("cairoTexture", "padding-top", 10.,
		 st_theme_node_get_padding (cairo_texture, ST_SIDE_TOP));
  /* From ClutterCairoTexture element selector */
  assert_length ("cairoTexture", "padding-right", 20.,
		 st_theme_node_get_padding (cairo_texture, ST_SIDE_RIGHT));
}

static void
test_adjacent_selector (void)
{
  test = "adjacent_selector";
  /* #group1 > #text1 matches text1 */
  assert_foreground_color (text1, "text1", 0x00ff00ff);
  /* stage > #text2 doesn't match text2 */
  assert_foreground_color (text2, "text2", 0x000000ff);
}

static void
test_background (void)
{
  test = "background";
  /* group1 has a background: shortcut property setting color and image */
  assert_background_color (group1, "group1", 0xff0000ff);
  assert_background_image (group1, "group1", "st/some-background.png");
  /* text1 inherits the background image but not the color */
  assert_background_color (text1,  "text1",  0x00000000);
  assert_background_image (text1,  "text1",  "st/some-background.png");
  /* text1 inherits inherits both, but then background: none overrides both */
  assert_background_color (text2,  "text2",  0x00000000);
  assert_background_image (text2,  "text2",  NULL);
  /* background-image property */
  assert_background_image (group2, "group2", "st/other-background.png");
}

static void
test_font (void)
{
  test = "font";
  /* font specified with font: */
  assert_font (group2, "group2", "serif Italic 12px");
  /* text3 inherits and overrides individually properties */
  assert_font (text3,  "text3",  "serif Bold Oblique Small-Caps 24px");
}

static void
test_pseudo_class (void)
{
  test = "pseudo_class";
  /* text4 has :visited and :hover pseudo-classes, so should pick up both of these */
  assert_foreground_color (text4,   "text4",  0x888888ff);
  assert_text_decoration  (text4,   "text4",  ST_TEXT_DECORATION_UNDERLINE);
  /* :hover pseudo-class matches, but class doesn't match */
  assert_text_decoration  (group3,  "group3", 0);
}

static void
test_inline_style (void)
{
  test = "inline_style";
  /* These properties come from the inline-style specified when creating the node */
  assert_foreground_color (text3,   "text3",  0x00000ffff);
  assert_length ("text3", "padding-bottom", 12.,
                 st_theme_node_get_padding (text3, ST_SIDE_BOTTOM));
}

int
main (int argc, char **argv)
{
  StTheme *theme;
  StThemeContext *context;

  clutter_init (&argc, &argv);

  theme = st_theme_new ("st/test-theme.css",
                        NULL, NULL);

  context = st_theme_context_new ();
  st_theme_context_set_theme (context, theme);
  st_theme_context_set_resolution (context, 96.);
  st_theme_context_set_font (context,
				pango_font_description_from_string ("sans-serif 12"));

  root = st_theme_context_get_root_node (context);
  group1 = st_theme_node_new (context, root, NULL,
                              CLUTTER_TYPE_GROUP, "group1", NULL, NULL, NULL);
  text1 = st_theme_node_new  (context, group1, NULL,
                              CLUTTER_TYPE_TEXT, "text1", "special-text", NULL, NULL);
  text2 = st_theme_node_new  (context, group1, NULL,
                              CLUTTER_TYPE_TEXT, "text2", NULL, NULL, NULL);
  group2 = st_theme_node_new (context, root, NULL,
                              CLUTTER_TYPE_GROUP, "group2", NULL, NULL, NULL);
  text3 = st_theme_node_new  (context, group2, NULL,
                              CLUTTER_TYPE_TEXT, "text3", NULL, NULL,
                              "color: #0000ff; padding-bottom: 12px;");
  text4 = st_theme_node_new  (context, group2, NULL,
                              CLUTTER_TYPE_TEXT, "text4", NULL, "visited hover", NULL);
  group3 = st_theme_node_new (context, group2, NULL,
                              CLUTTER_TYPE_GROUP, "group3", NULL, "hover", NULL);
  cairo_texture = st_theme_node_new (context, root, NULL,
                                     CLUTTER_TYPE_CAIRO_TEXTURE, "cairoTexture", NULL, NULL, NULL);

  test_defaults ();
  test_lengths ();
  test_classes ();
  test_type_inheritance ();
  test_adjacent_selector ();
  test_background ();
  test_font ();
  test_pseudo_class ();
  test_inline_style ();

  return fail ? 1 : 0;
}
