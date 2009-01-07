#include <clutter/clutter.h>
#include <string.h>
#include <stdlib.h>

#include "test-conform-common.h"

#define TEST_FONT "Sans 10"

static const char long_text[] =
  "<b>This</b> <i>is</i> some <span size=\"x-large\">REALLY</span> "
  "long text that contains markup for testing the <tt>use_markup</tt> "
  "property and to test word-wrapping, justification and alignment.";

typedef struct _CallbackData CallbackData;

struct _CallbackData
{
  ClutterActor *stage;
  ClutterActor *label;

  PangoLayout *old_layout;
  gboolean layout_changed;
  PangoRectangle label_extents;

  PangoLayout *test_layout;

  gboolean test_failed;
};

static void
on_paint (ClutterActor *label, CallbackData *data)
{
  PangoLayout *new_layout;

  /* Check whether the layout used for this paint is different from
     the layout used for the last paint */
  new_layout = clutter_text_get_layout (CLUTTER_TEXT (data->label));
  data->layout_changed = data->old_layout != new_layout;

  if (data->old_layout)
    g_object_unref (data->old_layout);
  /* Keep a reference to the old layout so we can be sure it won't
     just reallocate a new layout with the same address */
  data->old_layout = g_object_ref (new_layout);

  pango_layout_get_extents (new_layout, NULL, &data->label_extents);
}

static void
force_redraw (CallbackData *data)
{
  clutter_redraw (CLUTTER_STAGE (clutter_actor_get_stage (data->label)));
}

static gboolean
check_result (CallbackData *data, const char *note,
	      gboolean layout_should_change)
{
  PangoRectangle test_extents;
  gboolean fail = FALSE;

  if (g_test_verbose ())
    g_print ("%s: ", note);

  /* Force a redraw to get the on_paint handler to run */
  force_redraw (data);

  /* Compare the extents from the label with the extents from our test
     layout */
  pango_layout_get_extents (data->test_layout, NULL, &test_extents);
  if (memcmp (&test_extents, &data->label_extents, sizeof (PangoRectangle)))
    {
      if (g_test_verbose ())
        g_print ("extents are different: expected: %d, %d, %d, %d "
                 "-> text: %d, %d, %d, %d\n",
                 test_extents.x / 1024,
                 test_extents.y / 1024,
                 test_extents.width / 1024,
                 test_extents.height / 1024,
                 data->label_extents.x / 1024,
                 data->label_extents.y / 1024,
                 data->label_extents.width / 1024,
                 data->label_extents.height / 1024);

      fail = TRUE;
    }
  else
    {
      if (g_test_verbose ())
        g_print ("extents are the same, ");
    }

  if (data->layout_changed)
    {
      if (g_test_verbose ())
        g_print ("layout changed, ");
    }
  else
    {
      if (g_test_verbose ())
        g_print ("layout did not change, ");
    }

  if (data->layout_changed != layout_should_change)
    fail = TRUE;

  if (fail)
    {
      if (g_test_verbose ())
        g_print ("FAIL\n");

      data->test_failed = TRUE;
    }
  else
    {
      if (g_test_verbose ())
        g_print ("pass\n");
    }

  return fail;
}

static gboolean
do_tests (CallbackData *data)
{
  PangoFontDescription *fd;
  static const ClutterColor red = { 0xff, 0x00, 0x00, 0xff };
  PangoAttrList *attr_list, *attr_list_copy;
  PangoAttribute *attr;

  /* TEST 1: change the text */
  clutter_text_set_text (CLUTTER_TEXT (data->label), "Counter 0");
  pango_layout_set_text (data->test_layout, "Counter 0", -1);
  g_assert (check_result (data, "Change text", TRUE) == FALSE);

  /* TEST 2: change a single character */
  clutter_text_set_text (CLUTTER_TEXT (data->label), "Counter 1");
  pango_layout_set_text (data->test_layout, "Counter 1", -1);
  g_assert (check_result (data, "Change a single character", TRUE) == FALSE);

  /* TEST 3: move the label */
  clutter_actor_set_position (data->label, 10, 0);
  g_assert (check_result (data, "Move the label", FALSE) == FALSE);

  /* TEST 4: change the font */
  clutter_text_set_font_name (CLUTTER_TEXT (data->label), "Serif 15");
  fd = pango_font_description_from_string ("Serif 15");
  pango_layout_set_font_description (data->test_layout, fd);
  pango_font_description_free (fd);
  g_assert (check_result (data, "Change the font", TRUE) == FALSE);

  /* TEST 5: change the color */
  clutter_text_set_color (CLUTTER_TEXT (data->label), &red);
  g_assert (check_result (data, "Change the color", FALSE) == FALSE);

  /* TEST 6: change the attributes */
  attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attr->start_index = 0;
  attr->end_index = 2;
  attr_list = pango_attr_list_new ();
  pango_attr_list_insert (attr_list, attr);
  attr_list_copy = pango_attr_list_copy (attr_list);
  clutter_text_set_attributes (CLUTTER_TEXT (data->label), attr_list);
  pango_layout_set_attributes (data->test_layout, attr_list_copy);
  pango_attr_list_unref (attr_list_copy);
  pango_attr_list_unref (attr_list);
  g_assert (check_result (data, "Change the attributes", TRUE) == FALSE);

  /* TEST 7: change the text again */
  clutter_text_set_attributes (CLUTTER_TEXT (data->label), NULL);
  clutter_text_set_text (CLUTTER_TEXT (data->label), long_text);
  pango_layout_set_attributes (data->test_layout, NULL);
  pango_layout_set_text (data->test_layout, long_text, -1);
  g_assert (check_result (data, "Change the text again", TRUE) == FALSE);

  /* TEST 8: enable markup */
  clutter_text_set_use_markup (CLUTTER_TEXT (data->label), TRUE);
  pango_layout_set_markup (data->test_layout, long_text, -1);
  g_assert (check_result (data, "Enable markup", TRUE) == FALSE);

  /* This part can't be a test because Clutter won't restrict the
     width if wrapping and ellipsizing is disabled so the extents will
     be different, but we still want to do it for the later tests */
  clutter_actor_set_width (data->label, 200);
  pango_layout_set_width (data->test_layout, 200 * PANGO_SCALE);
  /* Force a redraw so that changing the width won't affect the
     results */
  force_redraw (data);

  /* TEST 9: enable ellipsize */
  clutter_text_set_ellipsize (CLUTTER_TEXT (data->label),
			       PANGO_ELLIPSIZE_END);
  pango_layout_set_ellipsize (data->test_layout, PANGO_ELLIPSIZE_END);
  g_assert (check_result (data, "Enable ellipsize", TRUE) == FALSE);
  clutter_text_set_ellipsize (CLUTTER_TEXT (data->label),
			       PANGO_ELLIPSIZE_NONE);
  pango_layout_set_ellipsize (data->test_layout, PANGO_ELLIPSIZE_NONE);
  force_redraw (data);

  /* TEST 10: enable line wrap */
  clutter_text_set_line_wrap (CLUTTER_TEXT (data->label), TRUE);
  pango_layout_set_wrap (data->test_layout, PANGO_WRAP_WORD);
  g_assert (check_result (data, "Enable line wrap", TRUE) == FALSE);

  /* TEST 11: change wrap mode */
  clutter_text_set_line_wrap_mode (CLUTTER_TEXT (data->label),
				    PANGO_WRAP_CHAR);
  pango_layout_set_wrap (data->test_layout, PANGO_WRAP_CHAR);
  g_assert (check_result (data, "Change wrap mode", TRUE) == FALSE);

  /* TEST 12: enable justify */
  clutter_text_set_justify (CLUTTER_TEXT (data->label), TRUE);
  pango_layout_set_justify (data->test_layout, TRUE);
  /* Pango appears to have a bug which means that you can't change the
     justification after setting the text but this fixes it.
     See http://bugzilla.gnome.org/show_bug.cgi?id=551865 */
  pango_layout_context_changed (data->test_layout);
  g_assert (check_result (data, "Enable justify", TRUE) == FALSE);

  /* TEST 13: change alignment */
  clutter_text_set_alignment (CLUTTER_TEXT (data->label), PANGO_ALIGN_RIGHT);
  pango_layout_set_alignment (data->test_layout, PANGO_ALIGN_RIGHT);
  g_assert (check_result (data, "Change alignment", TRUE) == FALSE);

  clutter_main_quit ();

  return FALSE;
}

static PangoLayout *
make_layout_like_label (ClutterText *label)
{
  PangoLayout *label_layout, *new_layout;
  PangoContext *context;
  PangoFontDescription *fd;

  /* Make another layout using the same context as the layout from the
     label */
  label_layout = clutter_text_get_layout (label);
  context = pango_layout_get_context (label_layout);
  new_layout = pango_layout_new (context);
  fd = pango_font_description_from_string (TEST_FONT);
  pango_layout_set_font_description (new_layout, fd);
  pango_font_description_free (fd);

  return new_layout;
}

void
test_text_cache (TestConformSimpleFixture *fixture,
                 gconstpointer _data)
{
  CallbackData data;

  memset (&data, 0, sizeof (data));

  data.stage = clutter_stage_get_default ();

  data.label = clutter_text_new_with_text (TEST_FONT, "");

  data.test_layout = make_layout_like_label (CLUTTER_TEXT (data.label));

  g_signal_connect (data.label, "paint", G_CALLBACK (on_paint), &data);

  clutter_container_add (CLUTTER_CONTAINER (data.stage), data.label, NULL);

  clutter_actor_show (data.stage);

  clutter_threads_add_idle ((GSourceFunc) do_tests, &data);

  clutter_main ();

  if (g_test_verbose ())
    g_print ("\nOverall result: ");

  if (g_test_verbose ())
    {
      if (data.test_failed)
        g_print ("FAIL\n");
      else
        g_print ("pass\n");
    }
  else
    g_assert (data.test_failed != TRUE);
}

