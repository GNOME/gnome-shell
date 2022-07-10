#include "config.h"

#include "shell-workspace-background.h"

#include "shell-global.h"
#include <meta/meta-workspace-manager.h>

#define BACKGROUND_MARGIN 12

enum
{
  PROP_0,

  PROP_MONITOR_INDEX,
  PROP_STATE_ADJUSTMENT_VALUE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

struct _ShellWorkspaceBackground
{
  /*< private >*/
  StWidget parent_instance;

  int monitor_index;
  double state_adjustment_value;

  MetaRectangle work_area;
  MetaRectangle monitor_geometry;
};

G_DEFINE_TYPE (ShellWorkspaceBackground, shell_workspace_background, ST_TYPE_WIDGET);

static void
on_workareas_changed (ShellWorkspaceBackground *self)
{
  ShellGlobal *global = shell_global_get ();
  MetaDisplay *display = shell_global_get_display (global);
  MetaWorkspaceManager *workspace_manager = shell_global_get_workspace_manager (global);
  MetaWorkspace *workspace =
    meta_workspace_manager_get_workspace_by_index (workspace_manager, 0);

  meta_workspace_get_work_area_for_monitor (workspace,
                                            self->monitor_index,
                                            &self->work_area);

  meta_display_get_monitor_geometry (display,
                                     self->monitor_index,
                                     &self->monitor_geometry);
}

static void
shell_workspace_background_allocate (ClutterActor          *actor,
                                     const ClutterActorBox *box)
{
  ShellWorkspaceBackground *self = SHELL_WORKSPACE_BACKGROUND (actor);
  ShellGlobal *global = shell_global_get ();
  ClutterStage *stage = shell_global_get_stage (global);
  StThemeContext *context = st_theme_context_get_for_stage (stage);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  ClutterActorBox scaled_box, my_box, content_box;
  ClutterActor *child;
  float content_width, content_height;
  float scaled_width, scaled_height;
  float x_scale, y_scale;
  float width, height;
  int left_offset, right_offset;
  int top_offset, bottom_offset;
  int scale_factor;

  scale_factor = st_theme_context_get_scale_factor (context);

  clutter_actor_box_get_size (box, &width, &height);
  scaled_height = height - BACKGROUND_MARGIN * 2 * scale_factor;
  scaled_width = (scaled_height / height) * width;

  scaled_box.x1 = box->x1 + (width - scaled_width) / 2;
  scaled_box.y1 = box->y1 + (height - scaled_height) / 2;
  clutter_actor_box_set_size (&scaled_box, scaled_width, scaled_height);

  clutter_actor_box_interpolate(box, &scaled_box,
                                self->state_adjustment_value, &my_box);

  clutter_actor_set_allocation (actor, &my_box);

  st_theme_node_get_content_box (theme_node, &my_box, &content_box);

  child = clutter_actor_get_first_child (actor);
  clutter_actor_allocate (child, &content_box);

  clutter_actor_box_get_size (&content_box, &content_width, &content_height);
  x_scale = content_width / self->work_area.width;
  y_scale = content_height / self->work_area.height;

  left_offset = self->work_area.x - self->monitor_geometry.x;
  top_offset = self->work_area.y - self->monitor_geometry.y;
  right_offset = self->monitor_geometry.width - self->work_area.width - left_offset;
  bottom_offset = self->monitor_geometry.height - self->work_area.height - top_offset;

  clutter_actor_box_set_origin (&content_box,
                                -left_offset * x_scale,
                                -top_offset * y_scale);
  clutter_actor_box_set_size (&content_box,
                              content_width + (left_offset + right_offset) * x_scale,
                              content_height + (top_offset + bottom_offset) * y_scale);

  child = clutter_actor_get_first_child (child);
  clutter_actor_allocate (child, &content_box);
}

static void
shell_workspace_background_constructed (GObject *object)
{
  G_OBJECT_CLASS (shell_workspace_background_parent_class)->constructed (object);

  on_workareas_changed (SHELL_WORKSPACE_BACKGROUND (object));
}

static void
shell_workspace_background_get_property (GObject      *gobject,
                                         unsigned int  property_id,
                                         GValue       *value,
                                         GParamSpec   *pspec)
{
  ShellWorkspaceBackground *self = SHELL_WORKSPACE_BACKGROUND (gobject);

  switch (property_id)
    {
    case PROP_MONITOR_INDEX:
      g_value_set_int (value, self->monitor_index);
      break;

    case PROP_STATE_ADJUSTMENT_VALUE:
      g_value_set_double (value, self->state_adjustment_value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
    }
}

static void
shell_workspace_background_set_property (GObject      *gobject,
                                         unsigned int  property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ShellWorkspaceBackground *self = SHELL_WORKSPACE_BACKGROUND (gobject);

  switch (property_id)
    {
    case PROP_MONITOR_INDEX:
      {
        int new_value = g_value_get_int (value);
        if (self->monitor_index != new_value)
        {
          self->monitor_index = new_value;
          g_object_notify_by_pspec (gobject, obj_props[PROP_MONITOR_INDEX]);
        }
      }
      break;

    case PROP_STATE_ADJUSTMENT_VALUE:
      {
        double new_value = g_value_get_double (value);
        if (self->state_adjustment_value != new_value)
        {
          self->state_adjustment_value = new_value;
          g_object_notify_by_pspec (gobject, obj_props[PROP_STATE_ADJUSTMENT_VALUE]);
        }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
    }
}

static void
shell_workspace_background_class_init (ShellWorkspaceBackgroundClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  actor_class->allocate = shell_workspace_background_allocate;

  gobject_class->constructed = shell_workspace_background_constructed;
  gobject_class->get_property = shell_workspace_background_get_property;
  gobject_class->set_property = shell_workspace_background_set_property;

  /**
   * ShellWorkspaceBackground:monitor-index:
   */
  obj_props[PROP_MONITOR_INDEX] =
    g_param_spec_int ("monitor-index", "", "",
                      0, G_MAXINT, 0,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * ShellWorkspaceBackground:state-adjustment-value:
   */
  obj_props[PROP_STATE_ADJUSTMENT_VALUE] =
    g_param_spec_double ("state-adjustment-value", "", "",
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
shell_workspace_background_init (ShellWorkspaceBackground *self)
{
  ShellGlobal *global = shell_global_get ();
  MetaDisplay *display = shell_global_get_display (global);

  g_signal_connect_object (display, "workareas-changed",
                           G_CALLBACK (on_workareas_changed),
                           self, G_CONNECT_SWAPPED);
}
