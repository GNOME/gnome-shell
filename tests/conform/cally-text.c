#include <clutter/clutter.h>
#include <string.h>
#include <stdlib.h>

#include "test-conform-common.h"

#define TEST_FONT "Sans 10"

typedef struct _CallbackData CallbackData;

struct _CallbackData
{
  ClutterActor *stage;
  ClutterActor *label;
  gint          offset;
  gboolean      test_failed;

  gint          extents_x;
  gint          extents_y;
  gint          extents_width;
  gint          extents_height;
  GSList       *run_attributes;
  GSList       *default_attributes;
  CallbackData *next;
};


static gint
attribute_lookup_func (gconstpointer data,
                       gconstpointer user_data)
{
    AtkAttribute *lookup_attr = (AtkAttribute*) user_data;
    AtkAttribute *at = (AtkAttribute *) data;
    if (!data)
        return -1;
    if (!g_strcmp0 (at->name, lookup_attr->name))
        return g_strcmp0 (at->value, lookup_attr->value);
    return -1;
}

/* check l1 is a sub-set of l2 */
static gboolean
compare_lists (GSList* l1, GSList* l2)
{
  gboolean fail = FALSE;

  if (l2 && !l1)
    return TRUE;

  while (l1)
    {
        AtkAttribute *at = (AtkAttribute *) l1->data;
        GSList* result = g_slist_find_custom ((GSList*) l2,
                                              (gconstpointer) at,
                                              attribute_lookup_func);
        if (!result)
          {
            fail = TRUE;
            break;
          }
        l1 = g_slist_next (l1);
    }

  return fail;
}

static void
dump_attribute_set (AtkAttributeSet *at_set)
{
  GSList *attrs = (GSList*) at_set;

  while (attrs) {
      AtkAttribute *at = (AtkAttribute *) attrs->data;
      g_print ("text attribute %s = %s\n", at->name, at->value);
      attrs = g_slist_next (attrs);
  }

}

static gboolean
check_result (CallbackData *data)
{
  gboolean fail = FALSE;
  gchar *text = NULL;
  const gchar *expected_text = NULL;
  AtkObject *object = NULL;
  AtkText *cally_text = NULL;
  gunichar unichar;
  gunichar expected_char;
  gint x, y, width, height;
  gint pos;
  AtkAttributeSet *at_set = NULL;
  GSList *attrs;
  gint start = -1;
  gint end = -1;

  object = atk_gobject_accessible_for_object (G_OBJECT (data->label));
  cally_text = ATK_TEXT (object);

  if (!cally_text) {
      g_print("no text\n");
    return TRUE;
  }

  text = atk_text_get_text (cally_text, 0, -1);
  expected_text = clutter_text_get_text (CLUTTER_TEXT (data->label));

  if (g_strcmp0 (expected_text, text) != 0)
    {
      if (g_test_verbose ())
        g_print ("text value differs %s vs %s\n", expected_text, text);
      fail = TRUE;
    }

  unichar = atk_text_get_character_at_offset (cally_text, data->offset);
  expected_char = g_utf8_get_char (g_utf8_offset_to_pointer (text,  data->offset));
  if (expected_char != unichar)
    {
      if (g_test_verbose ())
        g_print ("text af offset differs\n");
      fail = TRUE;
    }

  atk_text_get_character_extents (cally_text, data->offset, &x, &y, &width, &height,
                                  ATK_XY_WINDOW);
  if (x != data->extents_x)
    {
      if (g_test_verbose ())
        g_print ("extents x position at index 0 differs (current value=%d)\n", x);
      fail = TRUE;
    }
  if (y != data->extents_y)
    {
      if (g_test_verbose ())
        g_print ("extents y position at index 0 differs (current value=%d)\n", y);
      fail = TRUE;
    }
  if (width != data->extents_width)
    {
      if (g_test_verbose ())
        g_print ("extents width at index 0 differs (current value=%d)\n", width);
      fail = TRUE;
    }
  if (height != data->extents_height)
    {
      if (g_test_verbose ())
        g_print ("extents height at index 0 differs (current value=%d)\n", height);
      fail = TRUE;
    }

  pos = atk_text_get_offset_at_point (cally_text, x, y, ATK_XY_WINDOW);
  if (pos != data->offset)
    {
      if (g_test_verbose ())
        g_print ("offset at position (%d, %d) differs (current value=%d)\n", x,
                 y, pos);
      fail = TRUE;
    }

  at_set = atk_text_get_run_attributes (cally_text, 0,
                                        &start, &end);
  if (start != 0)
    {
      if (g_test_verbose ())
          g_print ("run attributes start offset is not 0: %d\n", start);
      fail = TRUE;
    }
  if (end != g_utf8_strlen (text, -1))
    {
      if (g_test_verbose ())
          g_print ("run attributes end offset is not text length: %d\n", end);
      fail = TRUE;
    }

  attrs = (GSList*) at_set;
  fail = compare_lists (attrs, data->run_attributes);
  if (fail && g_test_verbose ())
    {
      g_print ("run attributes mismatch\n");
      dump_attribute_set (attrs);
    }

  at_set = atk_text_get_default_attributes (cally_text);
  attrs = (GSList*) at_set;
  fail = compare_lists (attrs, data->default_attributes);
  if (fail && g_test_verbose ())
    {
      g_print ("default attributes mismatch\n");
      dump_attribute_set (attrs);
    }

  g_free (text);
  text = NULL;

  if (fail)
    {
      if (g_test_verbose ())
        g_print ("FAIL\n");
      data->test_failed = TRUE;
    }
  else if (g_test_verbose ())
    g_print ("pass\n");

  return fail;
}

static gboolean
do_tests (CallbackData *data)
{
  while (data)
    {
        gboolean result = check_result (data);
        g_assert (result == FALSE);
        data = data->next;
    }

  clutter_main_quit ();

  return FALSE;
}

static GSList*
build_attribute_set (const gchar* first_attribute, ...)
{
  AtkAttributeSet *return_set = g_slist_alloc ();
  va_list args;
  const gchar *name;
  const gchar *value;
  gint i = 0;

  value = first_attribute;
  va_start (args, first_attribute);

  while (value)
    {
        if ((i> 0) && (i % 2 != 0))
          {
            AtkAttribute *at = g_malloc (sizeof (AtkAttribute));
            at->name = g_strdup (name);
            at->value = g_strdup (value);
            return_set = g_slist_prepend (return_set, at);
          }
        i++;
        name = g_strdup (value);
        value = va_arg (args, gchar*);
    }
  va_end (args);
  return return_set;
}

void
cally_text (void)
{
  CallbackData data;
  CallbackData data1;
  GSList* default_attributes =  build_attribute_set ("left-margin", "0",
                                                 "right-margin", "0",
                                                 "indent", "0",
                                                 "invisible", "false",
                                                 "editable", "false",
                                                 "pixels-above-lines", "0",
                                                 "pixels-below-lines", "0",
                                                 "pixels-inside-wrap", "0",
                                                 "bg-full-height", "0",
                                                 "bg-stipple", "false",
                                                 "fg-stipple", "false",
                                                 "fg-color", "0,0,0",
                                                 "wrap-mode", "word",
                                                 "justification", "left",
                                                 "size", "10",
                                                 "weight", "400",
                                                 "family-name", "Sans",
                                                 "stretch", "normal",
                                                 "variant", "normal",
                                                 "style", "normal",
                                                 "language", "en-us",
                                                 "direction", "ltr",
                                                 NULL);

  memset (&data, 0, sizeof (data));

  data.stage = clutter_stage_new ();

  data.default_attributes = default_attributes;
  data.run_attributes = build_attribute_set ("fg-color", "0,0,0", NULL);

  data.label = clutter_text_new_with_text (TEST_FONT, "Lorem ipsum dolor sit amet");

  clutter_container_add (CLUTTER_CONTAINER (data.stage), data.label, NULL);
  data.offset = 6;
  data.extents_x = 64;
  data.extents_y = 99;
  data.extents_width = 3;
  data.extents_height = 17;
  clutter_actor_set_position (data.label, 20, 100);

  memset (&data1, 0, sizeof (data1));
  data1.stage = data.stage;
  data1.default_attributes = default_attributes;
  data1.run_attributes = build_attribute_set ("bg-color", "0,65535,0",
                                              "fg-color", "65535,65535,0",
                                              "strikethrough", "true", NULL);

  data1.label = clutter_text_new_with_text (TEST_FONT, "");
  clutter_text_set_markup (CLUTTER_TEXT(data1.label), "<span fgcolor=\"#FFFF00\" bgcolor=\"#00FF00\"><s>Lorem ipsum dolor sit amet</s></span>");

  clutter_container_add (CLUTTER_CONTAINER (data1.stage), data1.label, NULL);
  data1.offset = 10;
  data1.extents_x = 90;
  data1.extents_y = 199;
  data1.extents_width = 13;
  data1.extents_height = 17;
  clutter_actor_set_position (data1.label, 20, 200);
  data.next = &data1;

  clutter_actor_show (data.stage);
  clutter_threads_add_idle ((GSourceFunc) do_tests, &data);
  clutter_main ();

  clutter_actor_destroy (data.stage);

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
    {
      g_assert (data.test_failed != TRUE);
      g_assert (data1.test_failed != TRUE);
    }
}

