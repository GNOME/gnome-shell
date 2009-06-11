#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include <gmodule.h>

#include <clutter/clutter.h>
#include <clutter/clutter-keysyms.h>

#define TYPE_KEY_GROUP                  (key_group_get_type ())
#define KEY_GROUP(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_KEY_GROUP, KeyGroup))
#define IS_KEY_GROUP(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_KEY_GROUP))
#define KEY_GROUP_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_KEY_GROUP, KeyGroupClass))
#define IS_KEY_GROUP_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_KEY_GROUP))

typedef struct _KeyGroup        KeyGroup;
typedef struct _KeyGroupClass   KeyGroupClass;

struct _KeyGroup
{
  ClutterGroup parent_instance;

  gint selected_index;
};

struct _KeyGroupClass
{
  ClutterGroupClass parent_class;

  void (* activate) (KeyGroup     *group,
                     ClutterActor *child);
};

G_DEFINE_TYPE (KeyGroup, key_group, CLUTTER_TYPE_GROUP);

enum
{
  ACTIVATE,

  LAST_SIGNAL
};

static guint group_signals[LAST_SIGNAL] = { 0, };

static gboolean
key_group_action_move_left (KeyGroup            *self,
                            const gchar         *action_name,
                            guint                key_val,
                            ClutterModifierType  modifiers)
{
  gint n_children;

  g_debug ("%s: activated '%s' (k:%d, m:%d)",
           G_STRLOC,
           action_name,
           key_val,
           modifiers);

  n_children = clutter_group_get_n_children (CLUTTER_GROUP (self));

  self->selected_index -= 1;

  if (self->selected_index < 0)
    self->selected_index = n_children - 1;

  return TRUE;
}

static gboolean
key_group_action_move_right (KeyGroup            *self,
                             const gchar         *action_name,
                             guint                key_val,
                             ClutterModifierType  modifiers)
{
  gint n_children;

  g_debug ("%s: activated '%s' (k:%d, m:%d)",
           G_STRLOC,
           action_name,
           key_val,
           modifiers);

  n_children = clutter_group_get_n_children (CLUTTER_GROUP (self));

  self->selected_index += 1;

  if (self->selected_index >= n_children)
    self->selected_index = 0;

  return TRUE;
}

static gboolean
key_group_action_activate (KeyGroup            *self,
                           const gchar         *action_name,
                           guint                key_val,
                           ClutterModifierType  modifiers)
{
  ClutterActor *child = NULL;

  g_debug ("%s: activated '%s' (k:%d, m:%d)",
           G_STRLOC,
           action_name,
           key_val,
           modifiers);

  if (self->selected_index == -1)
    return FALSE;

  child = clutter_group_get_nth_child (CLUTTER_GROUP (self),
                                       self->selected_index);

  if (child)
    {
      g_signal_emit (self, group_signals[ACTIVATE], 0, child);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
key_group_key_press (ClutterActor    *actor,
                     ClutterKeyEvent *event)
{
  ClutterBindingPool *pool;
  gboolean res;

  pool = clutter_binding_pool_find (G_OBJECT_TYPE_NAME (actor));
  g_assert (pool != NULL);

  res = clutter_binding_pool_activate (pool,
                                       event->keyval,
                                       event->modifier_state,
                                       G_OBJECT (actor));

  /* if we activate a key binding, redraw the actor */
  if (res)
    clutter_actor_queue_redraw (actor);

  return res;
}

static void
key_group_paint (ClutterActor *actor)
{
  KeyGroup *self = KEY_GROUP (actor);
  GList *children, *l;
  gint i;

  children = clutter_container_get_children (CLUTTER_CONTAINER (self));

  for (l = children, i = 0; l != NULL; l = l->next, i++)
    {
      ClutterActor *child = l->data;

      /* paint the selection rectangle */
      if (i == self->selected_index)
        {
          ClutterActorBox box = { 0, };

          clutter_actor_get_allocation_box (child, &box);

          box.x1 -= 2;
          box.y1 -= 2;
          box.x2 += 2;
          box.y2 += 2;

          cogl_set_source_color4ub (255, 255, 0, 224);
          cogl_rectangle (box.x1, box.y1, box.x2, box.y2);
        }

      clutter_actor_paint (child);
    }

  g_list_free (children);
}

static void
key_group_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (key_group_parent_class)->finalize (gobject);
}

static void
key_group_class_init (KeyGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterBindingPool *binding_pool;

  gobject_class->finalize = key_group_finalize;

  actor_class->paint = key_group_paint;
  actor_class->key_press_event = key_group_key_press;

  group_signals[ACTIVATE] =
    g_signal_new (g_intern_static_string ("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (KeyGroupClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  binding_pool = clutter_binding_pool_get_for_class (klass);

  clutter_binding_pool_install_action (binding_pool, "move-right",
                                       CLUTTER_Right, 0,
                                       G_CALLBACK (key_group_action_move_right),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "move-left",
                                       CLUTTER_Left, 0,
                                       G_CALLBACK (key_group_action_move_left),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_Return, 0,
                                       G_CALLBACK (key_group_action_activate),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_KP_Enter, 0,
                                       G_CALLBACK (key_group_action_activate),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_ISO_Enter, 0,
                                       G_CALLBACK (key_group_action_activate),
                                       NULL, NULL);
}

static void
key_group_init (KeyGroup *self)
{
  self->selected_index = -1;
}

static void
on_key_group_activate (KeyGroup     *group,
                       ClutterActor *child)
{
  g_print ("Child '%d' activated!\n", clutter_actor_get_gid (child));
}

G_MODULE_EXPORT int
test_binding_pool_main (int argc, char *argv[])
{
  ClutterActor *stage, *key_group;
  ClutterColor red_color   = { 255,   0,   0, 255 };
  ClutterColor green_color = {   0, 255,   0, 255 };
  ClutterColor blue_color  = {   0,   0, 255, 255 };
  gint group_x, group_y;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  key_group = g_object_new (TYPE_KEY_GROUP, NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), key_group);

  /* add three rectangles to the key group */
  clutter_container_add (CLUTTER_CONTAINER (key_group),
                         g_object_new (CLUTTER_TYPE_RECTANGLE,
                                       "color", &red_color,
                                       "width", 50.0,
                                       "height", 50.0,
                                       "x", 0.0,
                                       "y", 0.0,
                                       NULL),
                         g_object_new (CLUTTER_TYPE_RECTANGLE,
                                       "color", &green_color,
                                       "width", 50.0,
                                       "height", 50.0,
                                       "x", 75.0,
                                       "y", 0.0,
                                       NULL),
                         g_object_new (CLUTTER_TYPE_RECTANGLE,
                                       "color", &blue_color,
                                       "width", 50.0,
                                       "height", 50.0,
                                       "x", 150.0,
                                       "y", 0.0,
                                       NULL),
                         NULL);

  g_signal_connect (key_group,
                    "activate", G_CALLBACK (on_key_group_activate),
                    NULL);

  group_x =
    (clutter_actor_get_width (stage) - clutter_actor_get_width (key_group))
    / 2;
  group_y =
    (clutter_actor_get_height (stage) - clutter_actor_get_height (key_group))
    / 2;

  clutter_actor_set_position (key_group, group_x, group_y);
  clutter_actor_set_reactive (key_group, TRUE);

  clutter_stage_set_key_focus (CLUTTER_STAGE (stage), key_group);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
