#include <clutter/clutter.h>
#include <gmodule.h>
#include <math.h>

typedef struct _CallbackData CallbackData;
typedef struct _Clip Clip;

typedef enum
  {
    CLIP_NONE,
    CLIP_RECTANGLE,
    CLIP_ROTATED_RECTANGLE,
    CLIP_SHAPES
  } ClipType;

struct _Clip
{
  ClipType type;
  gint x1, y1, x2, y2;
};

struct _CallbackData
{
  ClutterActor *stage;
  CoglHandle hand;

  Clip current_clip;

  GSList *clips;
};

static const char
instructions[] =
  "Left button and drag to draw a rectangle, control+left to draw a rotated "
  "rectangle or shift+left to draw a path. Press 'r' to reset or 'u' "
  "to undo the last clip.";

static void
path_shapes (gint x, gint y, gint width, gint height)
{
  cogl_path_move_to (x, y);
  cogl_path_line_to (x, (y + height * 4 / 5));
  cogl_path_line_to ((x + width * 4 / 15), (y + height * 4 / 5));
  cogl_path_close ();

  cogl_path_rectangle (x + width / 3,
                       y,
                       x + width * 9 / 15,
                       y + height * 4 / 5);

  cogl_path_ellipse ((x + width * 4 / 5),
                     (y + height * 2 / 5),
                     (width * 2 / 15),
                     (height * 2 / 5));
}

static void
draw_shapes (gint x, gint y)
{
  path_shapes (x, y, 300, 100);
  cogl_set_source_color4ub (0x00, 0x00, 0xff, 0xff);
  cogl_path_fill_preserve ();
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_path_stroke ();
}

static void
make_clip_path (Clip *clip)
{
  switch (clip->type)
    {
    case CLIP_NONE:
      break;

    case CLIP_RECTANGLE:
      cogl_path_rectangle (clip->x1,
                           clip->y1,
                           clip->x2,
                           clip->y2);
      break;

    case CLIP_ROTATED_RECTANGLE:
      {
        int size = MIN (ABS (clip->x2 - clip->x1),
                        ABS (clip->y2 - clip->y1));
        int cx = (clip->x1 + clip->x2) / 2;
        int cy = (clip->y1 + clip->y2) / 2;

        cogl_path_move_to (cx - size / 2, cy);
        cogl_path_line_to (cx, cy - size / 2);
        cogl_path_line_to (cx + size / 2, cy);
        cogl_path_line_to (cx, cy + size / 2);
        cogl_path_close ();
      }
      break;

    case CLIP_SHAPES:
      {
        int x, y, width, height;

        if (clip->x1 < clip->x2)
          {
            x = clip->x1;
            width = clip->x2 - x;
          }
        else
          {
            x = clip->x2;
            width = clip->x1 - x;
          }
        if (clip->y1 < clip->y2)
          {
            y = clip->y1;
            height = clip->y2 - y;
          }
        else
          {
            y = clip->y2;
            height = clip->y1 - y;
          }

        path_shapes (x, y, width, height);
      }
      break;
    }
}

static void
on_paint (ClutterActor *actor, CallbackData *data)
{
  int i;
  ClutterGeometry stage_size;
  gint hand_width, hand_height;
  GSList *node;

  clutter_actor_get_allocation_geometry (data->stage, &stage_size);

  hand_width = cogl_texture_get_width (data->hand);
  hand_height = cogl_texture_get_height (data->hand);

  /* Setup the clipping */
  for (node = data->clips; node; node = node->next)
    {
      Clip *clip = (Clip *) node->data;

      if (clip->type == CLIP_RECTANGLE)
        cogl_clip_push_rectangle (clip->x1,
                                  clip->y1,
                                  clip->x2,
                                  clip->y2);
      else if (clip->type == CLIP_ROTATED_RECTANGLE)
        {
          float size = MIN (ABS (clip->x2 - clip->x1),
                            ABS (clip->y2 - clip->y1));
          int cx = (clip->x1 + clip->x2) / 2;
          int cy = (clip->y1 + clip->y2) / 2;

          size = sqrtf ((size / 2) * (size / 2) * 2);

          cogl_push_matrix ();

          /* Rotate 45° about the centre point */
          cogl_translate (cx, cy, 0.0f);
          cogl_rotate (45.0f, 0.0f, 0.0f, 1.0f);
          cogl_clip_push_rectangle (-size / 2, -size / 2, size / 2, size / 2);

          cogl_pop_matrix ();
        }
      else
        {
          make_clip_path (clip);
          cogl_clip_push_from_path ();
        }
    }

  /* Draw a rectangle filling the entire stage */
  cogl_set_source_color4ub (0x80, 0x80, 0xff, 0xff);
  cogl_rectangle (0, 0, stage_size.width, stage_size.height);

  draw_shapes (10, 10);

  /* Draw the hand at different rotations */
  for (i = -2; i <= 2; i++)
    {
      cogl_push_matrix ();

      cogl_translate (stage_size.width / 2 + stage_size.width / 6 * i,
                      stage_size.height / 2, 0);

      cogl_rotate (i * 40, 0, 1, 0);

      cogl_set_source_color4ub (0xff, 0xff, 0xff, 0xff);

      cogl_set_source_texture (data->hand);
      cogl_rectangle_with_texture_coords ((-hand_width / 2),
                                          (-hand_height / 2),
                                          (hand_width / 2),
                                          (hand_height / 2),
                                          0, 0, 1, 1);

      cogl_pop_matrix ();
    }

  draw_shapes (stage_size.width - 310, stage_size.height - 110);

  /* Remove all of the clipping */
  g_slist_foreach (data->clips, (GFunc) cogl_clip_pop, NULL);

  /* Draw the bounding box for each of the clips */
  for (node = data->clips; node; node = node->next)
    {
      Clip *clip = (Clip *) node->data;

      make_clip_path (clip);
      cogl_set_source_color4ub (0x00, 0x00, 0xff, 0xff);
      cogl_path_stroke ();
    }

  /* Draw the bounding box for the pending new clip */
  if (data->current_clip.type != CLIP_NONE)
    {
      make_clip_path (&data->current_clip);
      cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
      cogl_path_stroke ();
    }
}

static gboolean
on_button_press (ClutterActor *stage, ClutterButtonEvent *event,
                 CallbackData *data)
{
  data->current_clip.x1 = data->current_clip.x2 = event->x;
  data->current_clip.y1 = data->current_clip.y2 = event->y;

  switch (event->button)
    {
    case CLUTTER_BUTTON_PRIMARY:
      if (clutter_event_has_shift_modifier ((ClutterEvent *) event))
        data->current_clip.type = CLIP_SHAPES;
      else if (clutter_event_has_control_modifier ((ClutterEvent *) event))
        data->current_clip.type = CLIP_ROTATED_RECTANGLE;
      else
        data->current_clip.type = CLIP_RECTANGLE;
      break;

    case CLUTTER_BUTTON_SECONDARY:
      data->current_clip.type = CLIP_ROTATED_RECTANGLE;
      break;

    case CLUTTER_BUTTON_MIDDLE:
      data->current_clip.type = CLIP_SHAPES;
      break;

    default:
      data->current_clip.type = CLIP_NONE;
      break;
    }

  clutter_actor_queue_redraw (stage);

  return FALSE;
}

static gboolean
on_button_release (ClutterActor *stage, ClutterButtonEvent *event,
                   CallbackData *data)
{
  if (data->current_clip.type != CLIP_NONE)
    {
      data->clips = g_slist_prepend (data->clips,
                                     g_slice_copy (sizeof (Clip),
                                                   &data->current_clip));

      data->current_clip.type = CLIP_NONE;
    }

  clutter_actor_queue_redraw (stage);

  return FALSE;
}

static gboolean
on_motion (ClutterActor *stage, ClutterMotionEvent *event,
           CallbackData *data)
{
  if (data->current_clip.type != CLIP_NONE)
    {
      data->current_clip.x2 = event->x;
      data->current_clip.y2 = event->y;

      clutter_actor_queue_redraw (stage);
    }

  return FALSE;
}

static void
free_clips (CallbackData *data)
{
  GSList *node;

  for (node = data->clips; node; node = node->next)
    g_slice_free (Clip, node->data);

  g_slist_free (data->clips);

  data->clips = NULL;
}

static gboolean
on_key_press (ClutterActor *stage,
              ClutterEvent *event,
              CallbackData *data)
{
  switch (clutter_event_get_key_symbol (event))
    {
    case CLUTTER_KEY_r:
      free_clips (data);
      clutter_actor_queue_redraw (stage);
      break;

    case CLUTTER_KEY_u:
      if (data->clips)
        {
          g_slice_free (Clip, data->clips->data);
          data->clips = g_slist_delete_link (data->clips, data->clips);
          clutter_actor_queue_redraw (stage);
       }
      break;
    }

  return FALSE;
}

G_MODULE_EXPORT int
test_clip_main (int argc, char **argv)
{
  CallbackData data;
  ClutterActor *stub_actor, *label;
  gchar *file;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  data.current_clip.type = CLIP_NONE;
  data.clips = NULL;

  data.stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (data.stage), "Clipping");
  g_signal_connect (data.stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  stub_actor = clutter_rectangle_new ();
  clutter_container_add (CLUTTER_CONTAINER (data.stage), stub_actor, NULL);

  file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  data.hand = cogl_texture_new_from_file (file,
                                          COGL_TEXTURE_NONE,
                                          COGL_PIXEL_FORMAT_ANY,
                                          NULL);
  g_free (file);

  label = clutter_text_new_with_text ("Sans 12px", instructions);
  clutter_text_set_line_wrap (CLUTTER_TEXT (label), TRUE);
  clutter_actor_set_width (label, clutter_actor_get_width (data.stage) - 310);
  clutter_actor_set_y (label,
                       clutter_actor_get_height (data.stage)
                       - clutter_actor_get_height (label));
  clutter_container_add (CLUTTER_CONTAINER (data.stage), label, NULL);

  g_signal_connect (stub_actor, "paint", G_CALLBACK (on_paint), &data);

  g_signal_connect (data.stage, "button-press-event",
                    G_CALLBACK (on_button_press), &data);
  g_signal_connect (data.stage, "button-release-event",
                    G_CALLBACK (on_button_release), &data);
  g_signal_connect (data.stage, "motion-event",
                    G_CALLBACK (on_motion), &data);
  g_signal_connect (data.stage, "key-press-event",
                    G_CALLBACK (on_key_press), &data);

  clutter_actor_show (data.stage);

  clutter_main ();

  cogl_handle_unref (data.hand);

  free_clips (&data);

  return 0;
}

G_MODULE_EXPORT const char *
test_clip_describe (void)
{
  return "Actor clipping with various techniques";
}
