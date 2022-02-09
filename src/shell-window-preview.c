#include "config.h"

#include "shell-window-preview.h"

enum
{
  PROP_0,

  PROP_WINDOW_CONTAINER,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

struct _ShellWindowPreview
{
  /*< private >*/
  StWidget parent_instance;

  ClutterActor *window_container;
};

G_DEFINE_TYPE (ShellWindowPreview, shell_window_preview, ST_TYPE_WIDGET);

static void
shell_window_preview_get_property (GObject      *gobject,
                                   unsigned int  property_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  ShellWindowPreview *self = SHELL_WINDOW_PREVIEW (gobject);

  switch (property_id)
    {
    case PROP_WINDOW_CONTAINER:
      g_value_set_object (value, self->window_container);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
    }
}

static void
shell_window_preview_set_property (GObject      *gobject,
                                   unsigned int  property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ShellWindowPreview *self = SHELL_WINDOW_PREVIEW (gobject);

  switch (property_id)
    {
    case PROP_WINDOW_CONTAINER:
      if (g_set_object (&self->window_container, g_value_get_object (value)))
        g_object_notify_by_pspec (gobject, obj_props[PROP_WINDOW_CONTAINER]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
    }
}

static void
shell_window_preview_get_preferred_width (ClutterActor *actor,
                                          float         for_height,
                                          float        *min_width_p,
                                          float        *natural_width_p)
{
  ShellWindowPreview *self = SHELL_WINDOW_PREVIEW (actor);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  float min_width, nat_width;

  st_theme_node_adjust_for_height (theme_node, &for_height);

  clutter_actor_get_preferred_width (self->window_container, for_height,
                                     &min_width, &nat_width);

  st_theme_node_adjust_preferred_width (theme_node, &min_width, &nat_width);

  if (min_width_p)
    *min_width_p = min_width;

  if (natural_width_p)
    *natural_width_p = nat_width;
}

static void
shell_window_preview_get_preferred_height (ClutterActor *actor,
                                           float         for_width,
                                           float        *min_height_p,
                                           float        *natural_height_p)
{
  ShellWindowPreview *self = SHELL_WINDOW_PREVIEW (actor);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  float min_height, nat_height;

  st_theme_node_adjust_for_width (theme_node, &for_width);

  clutter_actor_get_preferred_height (self->window_container, for_width,
                                      &min_height, &nat_height);

  st_theme_node_adjust_preferred_height (theme_node, &min_height, &nat_height);

  if (min_height_p)
    *min_height_p = min_height;

  if (natural_height_p)
    *natural_height_p = nat_height;
}

static void
shell_window_preview_allocate (ClutterActor          *actor,
                               const ClutterActorBox *box)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  ClutterActorBox content_box;
  float x, y, max_width, max_height;
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_set_allocation (actor, box);

  st_theme_node_get_content_box (theme_node, box, &content_box);

  clutter_actor_box_get_origin (&content_box, &x, &y);
  clutter_actor_box_get_size (&content_box, &max_width, &max_height);

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    clutter_actor_allocate_available_size (child, x, y, max_width, max_height);
}

static void
shell_window_preview_dispose (GObject *gobject)
{
  ShellWindowPreview *self = SHELL_WINDOW_PREVIEW (gobject);

  g_clear_object (&self->window_container);

  G_OBJECT_CLASS (shell_window_preview_parent_class)->dispose (gobject);
}

static void
shell_window_preview_init (ShellWindowPreview *self)
{
}

static void
shell_window_preview_class_init (ShellWindowPreviewClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  actor_class->get_preferred_width = shell_window_preview_get_preferred_width;
  actor_class->get_preferred_height = shell_window_preview_get_preferred_height;
  actor_class->allocate = shell_window_preview_allocate;

  gobject_class->dispose = shell_window_preview_dispose;
  gobject_class->get_property = shell_window_preview_get_property;
  gobject_class->set_property = shell_window_preview_set_property;

  /**
   * ShellWindowPreview:window-container:
   */
  obj_props[PROP_WINDOW_CONTAINER] =
    g_param_spec_object ("window-container",
                         "window-container",
                         "window-container",
                         CLUTTER_TYPE_ACTOR,
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}
