/*
 * nbtk-widget.c: Base class for Nbtk actors
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 *             Thomas Wood <thomas@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "nbtk-widget.h"

#include "nbtk-marshal.h"
#include "nbtk-private.h"
#include "nbtk-texture-cache.h"
#include "nbtk-texture-frame.h"
#include "nbtk-tooltip.h"

#include <toolkit/shell-theme-context.h>
#include <big/rectangle.h>

/*
 * Forward declaration for sake of NbtkWidgetChild
 */
struct _NbtkWidgetPrivate
{
  ShellTheme *theme;
  ShellThemeNode *theme_node;
  gchar *pseudo_class;
  gchar *style_class;
  gchar *inline_style;

  ClutterActor *border_image;
  ClutterActor *background_image;
  ClutterColor bg_color;

  gboolean is_stylable : 1;
  gboolean has_tooltip : 1;
  gboolean is_style_dirty : 1;

  NbtkTooltip *tooltip;
};

/**
 * SECTION:nbtk-widget
 * @short_description: Base class for stylable actors
 *
 * #NbtkWidget is a simple abstract class on top of #ClutterActor. It
 * provides basic themeing properties.
 *
 * Actors in the Nbtk library should subclass #NbtkWidget if they plan
 * to obey to a certain #NbtkStyle.
 */

enum
{
  PROP_0,

  PROP_THEME,
  PROP_PSEUDO_CLASS,
  PROP_STYLE_CLASS,
  PROP_STYLE,

  PROP_STYLABLE,

  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_TEXT
};

enum
{
  STYLE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (NbtkWidget, nbtk_widget, CLUTTER_TYPE_ACTOR);

#define NBTK_WIDGET_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NBTK_TYPE_WIDGET, NbtkWidgetPrivate))

static void nbtk_widget_recompute_style (NbtkWidget     *widget,
					 ShellThemeNode *old_theme_node);

static void
nbtk_widget_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);

  switch (prop_id)
    {
    case PROP_THEME:
      nbtk_widget_set_theme (actor, g_value_get_object (value));
      break;

    case PROP_PSEUDO_CLASS:
      nbtk_widget_set_style_pseudo_class (actor, g_value_get_string (value));
      break;

    case PROP_STYLE_CLASS:
      nbtk_widget_set_style_class_name (actor, g_value_get_string (value));
      break;

    case PROP_STYLE:
      nbtk_widget_set_style (actor, g_value_get_string (value));
      break;

    case PROP_STYLABLE:
      if (actor->priv->is_stylable != g_value_get_boolean (value))
        {
          actor->priv->is_stylable = g_value_get_boolean (value);
          clutter_actor_queue_relayout ((ClutterActor *)gobject);
        }
      break;

    case PROP_HAS_TOOLTIP:
      nbtk_widget_set_has_tooltip (actor, g_value_get_boolean (value));
      break;

    case PROP_TOOLTIP_TEXT:
      nbtk_widget_set_tooltip_text (actor, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_widget_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);
  NbtkWidgetPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_THEME:
      g_value_set_object (value, priv->theme);
      break;

    case PROP_PSEUDO_CLASS:
      g_value_set_string (value, priv->pseudo_class);
      break;

    case PROP_STYLE_CLASS:
      g_value_set_string (value, priv->style_class);
      break;

    case PROP_STYLE:
      g_value_set_string (value, priv->inline_style);
      break;

    case PROP_STYLABLE:
      g_value_set_boolean (value, priv->is_stylable);
      break;

    case PROP_HAS_TOOLTIP:
      g_value_set_boolean (value, priv->has_tooltip);
      break;

    case PROP_TOOLTIP_TEXT:
      g_value_set_string (value, nbtk_tooltip_get_label (priv->tooltip));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
nbtk_widget_dispose (GObject *gobject)
{
  NbtkWidget *actor = NBTK_WIDGET (gobject);
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  if (priv->theme)
    {
      g_object_unref (priv->theme);
      priv->theme = NULL;
    }

  if (priv->border_image)
    {
      clutter_actor_unparent (priv->border_image);
      priv->border_image = NULL;
    }

  if (priv->tooltip)
    {
      ClutterContainer *parent;
      ClutterActor *tooltip = CLUTTER_ACTOR (priv->tooltip);

      /* this is just a little bit awkward because the tooltip is parented
       * on the stage, but we still want to "own" it */
      parent = CLUTTER_CONTAINER (clutter_actor_get_parent (tooltip));

      if (parent)
        clutter_container_remove_actor (parent, tooltip);

      priv->tooltip = NULL;
    }

  G_OBJECT_CLASS (nbtk_widget_parent_class)->dispose (gobject);
}

static void
nbtk_widget_finalize (GObject *gobject)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (gobject)->priv;

  g_free (priv->style_class);
  g_free (priv->pseudo_class);

  G_OBJECT_CLASS (nbtk_widget_parent_class)->finalize (gobject);
}

static void
nbtk_widget_allocate (ClutterActor          *actor,
                      const ClutterActorBox *box,
                      ClutterAllocationFlags flags)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  ClutterActorClass *klass;
  ClutterGeometry area;
  ClutterVertex in_v, out_v;

  klass = CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class);
  klass->allocate (actor, box, flags);

  /* update tooltip position */
  if (priv->tooltip)
    {
      in_v.x = in_v.y = in_v.z = 0;
      clutter_actor_apply_transform_to_point (actor, &in_v, &out_v);
      area.x = out_v.x;
      area.y = out_v.y;

      in_v.x = box->x2 - box->x1;
      in_v.y = box->y2 - box->y1;
      clutter_actor_apply_transform_to_point (actor, &in_v, &out_v);
      area.width = out_v.x - area.x;
      area.height = out_v.y - area.y;

      nbtk_tooltip_set_tip_area (priv->tooltip, &area);
    }



  if (priv->border_image)
    {
      ClutterActorBox frame_box = {
        0,
        0,
        box->x2 - box->x1,
        box->y2 - box->y1
      };

      clutter_actor_allocate (CLUTTER_ACTOR (priv->border_image),
                              &frame_box,
                              flags);
    }

  if (priv->background_image)
    {
      ClutterActorBox frame_box = {
          0, 0, box->x2 - box->x1, box->y2 - box->y1
      };
      gfloat w, h;

      clutter_actor_get_size (CLUTTER_ACTOR (priv->background_image), &w, &h);

      /* scale the background into the allocated bounds */
      if (w > frame_box.x2 || h > frame_box.y2)
        {
          gint new_h, new_w, offset;
          gint box_w, box_h;

          box_w = (int) frame_box.x2;
          box_h = (int) frame_box.y2;

          /* scale to fit */
          new_h = (int) ((h / w) * ((gfloat) box_w));
          new_w = (int) ((w / h) * ((gfloat) box_h));

          if (new_h > box_h)
            {
              /* center for new width */
              offset = ((box_w) - new_w) * 0.5;
              frame_box.x1 = offset;
              frame_box.x2 = offset + new_w;

              frame_box.y2 = box_h;
            }
          else
            {
              /* center for new height */
              offset = ((box_h) - new_h) * 0.5;
              frame_box.y1 = offset;
              frame_box.y2 = offset + new_h;

              frame_box.x2 = box_w;
            }

        }
      else
        {
          /* center the background on the widget */
          frame_box.x1 = (int) (((box->x2 - box->x1) / 2) - (w / 2));
          frame_box.y1 = (int) (((box->y2 - box->y1) / 2) - (h / 2));
          frame_box.x2 = frame_box.x1 + w;
          frame_box.y2 = frame_box.y1 + h;
        }

      clutter_actor_allocate (CLUTTER_ACTOR (priv->background_image),
                              &frame_box,
                              flags);
    }
}

static void
nbtk_widget_real_draw_background (NbtkWidget         *self,
                                  ClutterActor       *background,
                                  const ClutterColor *color)
{
  /* Default implementation just draws the background
   * colour and the image on top
   */
  if (color && color->alpha != 0)
    {
      ClutterActor *actor = CLUTTER_ACTOR (self);
      ClutterActorBox allocation = { 0, };
      ClutterColor bg_color = *color;
      gfloat w, h;

      bg_color.alpha = clutter_actor_get_paint_opacity (actor)
                     * bg_color.alpha
                     / 255;

      clutter_actor_get_allocation_box (actor, &allocation);

      w = allocation.x2 - allocation.x1;
      h = allocation.y2 - allocation.y1;

      cogl_set_source_color4ub (bg_color.red,
                                bg_color.green,
                                bg_color.blue,
                                bg_color.alpha);
      cogl_rectangle (0, 0, w, h);
    }

  if (background)
    clutter_actor_paint (background);
}

static void
nbtk_widget_paint (ClutterActor *self)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (self)->priv;
  NbtkWidgetClass *klass = NBTK_WIDGET_GET_CLASS (self);

  klass->draw_background (NBTK_WIDGET (self),
                          priv->border_image,
                          &priv->bg_color);

  if (priv->background_image != NULL)
    clutter_actor_paint (priv->background_image);
}

static void
nbtk_widget_parent_set (ClutterActor *widget,
                        ClutterActor *old_parent)
{
  ClutterActorClass *parent_class;
  ClutterActor *new_parent;

  parent_class = CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class);
  if (parent_class->parent_set)
    parent_class->parent_set (widget, old_parent);

  new_parent = clutter_actor_get_parent (widget);

  /* don't send the style changed signal if we no longer have a parent actor */
  if (new_parent)
    nbtk_widget_style_changed (NBTK_WIDGET (widget));
}

static void
nbtk_widget_map (ClutterActor *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->map (actor);

  nbtk_widget_ensure_style ((NbtkWidget*) actor);

  if (priv->border_image)
    clutter_actor_map (priv->border_image);

  if (priv->background_image)
    clutter_actor_map (priv->background_image);

  if (priv->tooltip)
    clutter_actor_map ((ClutterActor *) priv->tooltip);
}

static void
nbtk_widget_unmap (ClutterActor *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->unmap (actor);

  if (priv->border_image)
    clutter_actor_unmap (priv->border_image);

  if (priv->background_image)
    clutter_actor_unmap (priv->background_image);

  if (priv->tooltip)
    clutter_actor_unmap ((ClutterActor *) priv->tooltip);
}

static void notify_children_of_style_change (ClutterContainer *container);

static void
notify_children_of_style_change_foreach (ClutterActor *actor,
					 gpointer      user_data)
{
  if (NBTK_IS_WIDGET (actor))
    nbtk_widget_style_changed (NBTK_WIDGET (actor));
  else if (CLUTTER_IS_CONTAINER (actor))
    notify_children_of_style_change ((ClutterContainer *)actor);
}

static void
notify_children_of_style_change (ClutterContainer *container)
{
  /* notify our children that their parent stylable has changed */
  clutter_container_foreach (container,
			     notify_children_of_style_change_foreach,
			     NULL);
}

static void
nbtk_widget_real_style_changed (NbtkWidget *self)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (self)->priv;
  ShellThemeNode *theme_node;
  ShellThemeImage *theme_image;
  NbtkTextureCache *texture_cache;
  ClutterTexture *texture;
  const char *bg_file = NULL;
  gboolean relayout_needed = FALSE;
  gboolean has_changed = FALSE;
  ClutterColor color;

  /* application has request this widget is not stylable */
  if (!priv->is_stylable)
    return;

  theme_node = nbtk_widget_get_theme_node (self);

  shell_theme_node_get_background_color (theme_node, &color);
  if (!clutter_color_equal (&color, &priv->bg_color))
    {
      priv->bg_color = color;
      has_changed = TRUE;
    }

  if (priv->border_image)
    {
       clutter_actor_unparent (priv->border_image);
       priv->border_image = NULL;
    }

  if (priv->background_image)
    {
      clutter_actor_unparent (priv->background_image);
      priv->background_image = NULL;
    }

  texture_cache = nbtk_texture_cache_get_default ();

  theme_image = shell_theme_node_get_background_theme_image (theme_node);
  if (theme_image)
    {
      const char *filename;
      gint border_left, border_right, border_top, border_bottom;
      gint width, height;

      filename = shell_theme_image_get_filename (theme_image);

      /* `border-image' takes precedence over `background-image'.
       * Firefox lets the background-image shine thru when border-image has
       * alpha an channel, maybe that would be an option for the future. */
      texture = nbtk_texture_cache_get_texture (texture_cache,
                                                filename,
                                                FALSE);

      clutter_texture_get_base_size (CLUTTER_TEXTURE (texture),
                                     &width, &height);

      shell_theme_image_get_borders (theme_image,
				     &border_left, &border_right, &border_top, &border_bottom);

      priv->border_image = nbtk_texture_frame_new (texture,
                                                   border_top,
                                                   border_right,
                                                   border_bottom,
                                                   border_left);
      clutter_actor_set_parent (priv->border_image, CLUTTER_ACTOR (self));

      has_changed = TRUE;
      relayout_needed = TRUE;
    }

  bg_file = shell_theme_node_get_background_image (theme_node);
  if (bg_file != NULL)
    {
      texture = nbtk_texture_cache_get_texture (texture_cache,
                                                bg_file,
                                                FALSE);
      priv->background_image = (ClutterActor*) texture;

      if (priv->background_image != NULL)
        {
          clutter_actor_set_parent (priv->background_image,
                                    CLUTTER_ACTOR (self));
        }
      else
        g_warning ("Could not load %s", bg_file);

      has_changed = TRUE;
      relayout_needed = TRUE;
    }

  /* If there are any properties above that need to cause a relayout thay
   * should set this flag.
   */
  if (has_changed)
    {
      if (relayout_needed)
        clutter_actor_queue_relayout ((ClutterActor *) self);
      else
        clutter_actor_queue_redraw ((ClutterActor *) self);
    }

  if (CLUTTER_IS_CONTAINER (self))
    notify_children_of_style_change ((ClutterContainer *)self);
}

void
nbtk_widget_style_changed (NbtkWidget *widget)
{
  ShellThemeNode *old_theme_node = NULL;

  widget->priv->is_style_dirty = TRUE;
  if (widget->priv->theme_node)
    {
      old_theme_node = widget->priv->theme_node;
      widget->priv->theme_node = NULL;
    }

  /* update the style only if we are mapped */
  if (CLUTTER_ACTOR_IS_MAPPED (CLUTTER_ACTOR (widget)))
    nbtk_widget_recompute_style (widget, old_theme_node);

  if (old_theme_node)
    g_object_unref (old_theme_node);
}

static void
on_theme_context_changed (ShellThemeContext *context,
			  ClutterStage      *stage)
{
  notify_children_of_style_change (CLUTTER_CONTAINER (stage));
}

static ShellThemeNode *
get_root_theme_node (ClutterStage *stage)
{
  ShellThemeContext *context = shell_theme_context_get_for_stage (stage);

  if (!g_object_get_data (G_OBJECT (context), "nbtk-theme-initialized"))
    {
      g_object_set_data (G_OBJECT (context), "nbtk-theme-initialized", GUINT_TO_POINTER (1));
      g_signal_connect (G_OBJECT (context), "changed",
			G_CALLBACK (on_theme_context_changed), stage);
    }

  return shell_theme_context_get_root_node (context);
}

ShellThemeNode *
nbtk_widget_get_theme_node (NbtkWidget *widget)
{
  NbtkWidgetPrivate *priv = widget->priv;

  if (priv->theme_node == NULL)
    {
      ShellThemeNode *parent_node = NULL;
      ClutterStage *stage = NULL;
      ClutterActor *parent;

      parent = clutter_actor_get_parent (CLUTTER_ACTOR (widget));
      while (parent != NULL)
	{
	  if (parent_node == NULL && NBTK_IS_WIDGET (parent))
	    parent_node = nbtk_widget_get_theme_node (NBTK_WIDGET (parent));
	  else if (CLUTTER_IS_STAGE (parent))
	    stage = CLUTTER_STAGE (parent);

	  parent = clutter_actor_get_parent (parent);
	}

      if (stage == NULL)
	{
	  g_warning ("nbtk_widget_get_theme_node called on a widget not in a stage");
	  stage = CLUTTER_STAGE (clutter_stage_get_default ());
	}

      if (parent_node == NULL)
	parent_node = get_root_theme_node (CLUTTER_STAGE (stage));

      priv->theme_node = shell_theme_node_new (shell_theme_context_get_for_stage (stage),
					       parent_node, priv->theme,
					       G_OBJECT_TYPE (widget),
					       clutter_actor_get_name (CLUTTER_ACTOR (widget)),
					       priv->style_class,
					       priv->pseudo_class,
					       priv->inline_style);
    }

  return priv->theme_node;
}

static gboolean
nbtk_widget_enter (ClutterActor         *actor,
                   ClutterCrossingEvent *event)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;


  if (priv->has_tooltip)
      nbtk_widget_show_tooltip ((NbtkWidget*) actor);

  if (CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->enter_event)
    return CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->enter_event (actor, event);
  else
    return FALSE;
}

static gboolean
nbtk_widget_leave (ClutterActor         *actor,
                   ClutterCrossingEvent *event)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;

  if (priv->has_tooltip)
      nbtk_tooltip_hide (priv->tooltip);

  if (CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->leave_event)
    return CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->leave_event (actor, event);
  else
    return FALSE;
}

static void
nbtk_widget_hide (ClutterActor *actor)
{
  NbtkWidget *widget = (NbtkWidget *) actor;

  /* hide the tooltip, if there is one */
  if (widget->priv->tooltip)
    nbtk_tooltip_hide (NBTK_TOOLTIP (widget->priv->tooltip));

  CLUTTER_ACTOR_CLASS (nbtk_widget_parent_class)->hide (actor);
}



static void
nbtk_widget_class_init (NbtkWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (NbtkWidgetPrivate));

  gobject_class->set_property = nbtk_widget_set_property;
  gobject_class->get_property = nbtk_widget_get_property;
  gobject_class->dispose = nbtk_widget_dispose;
  gobject_class->finalize = nbtk_widget_finalize;

  actor_class->allocate = nbtk_widget_allocate;
  actor_class->paint = nbtk_widget_paint;
  actor_class->parent_set = nbtk_widget_parent_set;
  actor_class->map = nbtk_widget_map;
  actor_class->unmap = nbtk_widget_unmap;

  actor_class->enter_event = nbtk_widget_enter;
  actor_class->leave_event = nbtk_widget_leave;
  actor_class->hide = nbtk_widget_hide;

  klass->draw_background = nbtk_widget_real_draw_background;
  klass->style_changed = nbtk_widget_real_style_changed;

  /**
   * NbtkWidget:pseudo-class:
   *
   * The pseudo-class of the actor. Typical values include "hover", "active",
   * "focus".
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PSEUDO_CLASS,
                                   g_param_spec_string ("pseudo-class",
                                                        "Pseudo Class",
                                                        "Pseudo class for styling",
                                                        "",
                                                        NBTK_PARAM_READWRITE));
  /**
   * NbtkWidget:style-class:
   *
   * The style-class of the actor for use in styling.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STYLE_CLASS,
                                   g_param_spec_string ("style-class",
                                                        "Style Class",
                                                        "Style class for styling",
                                                        "",
                                                        NBTK_PARAM_READWRITE));

  /**
   * NbtkWidget:style:
   *
   * Inline style information for the actor as a ';'-separated list of
   * CSS properties.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STYLE,
                                   g_param_spec_string ("style",
                                                        "Style",
                                                        "Inline style string",
                                                        "",
                                                        NBTK_PARAM_READWRITE));

  /**
   * NbtkWidget:theme
   *
   * A theme set on this actor overriding the global theming for this actor
   * and its descendants
   */
  g_object_class_install_property (gobject_class,
                                   PROP_THEME,
                                   g_param_spec_object ("theme",
                                                        "Theme",
                                                        "Theme override",
                                                        SHELL_TYPE_THEME,
                                                        NBTK_PARAM_READWRITE));

  /**
   * NbtkWidget:stylable:
   *
   * Enable or disable styling of the widget
   */
  pspec = g_param_spec_boolean ("stylable",
                                "Stylable",
                                "Whether the table should be styled",
                                TRUE,
                                NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_STYLABLE,
                                   pspec);

  /**
   * NbtkWidget:has-tooltip:
   *
   * Determines whether the widget has a tooltip. If set to TRUE, causes the
   * widget to monitor enter and leave events (i.e. sets the widget reactive).
   */
  pspec = g_param_spec_boolean ("has-tooltip",
                                "Has Tooltip",
                                "Determines whether the widget has a tooltip",
                                FALSE,
                                NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_TOOLTIP,
                                   pspec);


  /**
   * NbtkWidget:tooltip-text:
   *
   * text displayed on the tooltip
   */
  pspec = g_param_spec_string ("tooltip-text",
                               "Tooltip Text",
                               "Text displayed on the tooltip",
                               "",
                               NBTK_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TOOLTIP_TEXT, pspec);

  /**
   * NbtkWidget::style-changed:
   *
   * Emitted when the style information that the widget derives from the
   * theme changes
   */
  signals[STYLE_CHANGED] =
    g_signal_new ("style-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NbtkWidgetClass, style_changed),
                  NULL, NULL,
                  _nbtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/**
 * nbtk_widget_set_theme:
 * @actor: a #NbtkWidget
 * @theme: a new style class string
 *
 * Overrides the theme that would be inherited from the actor's parent
 * or the stage with an entirely new theme (set of stylesheets).
 */
void
nbtk_widget_set_theme (NbtkWidget  *actor,
		       ShellTheme  *theme)
{
  NbtkWidgetPrivate *priv = actor->priv;

  g_return_if_fail (NBTK_IS_WIDGET (actor));

  priv = actor->priv;

  if (theme !=priv->theme)
    {
      if (priv->theme)
	g_object_unref (priv->theme);
      priv->theme = g_object_ref (priv->theme);

      nbtk_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "theme");
    }
}

/**
 * nbtk_widget_get_theme:
 * @actor: a #NbtkWidget
 *
 * Gets the overriding theme set on the actor. See nbtk_widget_set_theme()
 *
 * Return value: (transfer none): the overriding theme, or %NULL
 */
ShellTheme *
nbtk_widget_get_theme (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (actor), NULL);

  return actor->priv->theme;
}

/**
 * nbtk_widget_set_style_class_name:
 * @actor: a #NbtkWidget
 * @style_class: a new style class string
 *
 * Set the style class name
 */
void
nbtk_widget_set_style_class_name (NbtkWidget  *actor,
                                  const gchar *style_class)
{
  NbtkWidgetPrivate *priv = actor->priv;

  g_return_if_fail (NBTK_IS_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (style_class, priv->style_class))
    {
      g_free (priv->style_class);
      priv->style_class = g_strdup (style_class);

      nbtk_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "style-class");
    }
}


/**
 * nbtk_widget_get_style_class_name:
 * @actor: a #NbtkWidget
 *
 * Get the current style class name
 *
 * Returns: the class name string. The string is owned by the #NbtkWidget and
 * should not be modified or freed.
 */
const gchar*
nbtk_widget_get_style_class_name (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (actor), NULL);

  return actor->priv->style_class;
}

/**
 * nbtk_widget_get_style_pseudo_class:
 * @actor: a #NbtkWidget
 *
 * Get the current style pseudo class
 *
 * Returns: the pseudo class string. The string is owned by the #NbtkWidget and
 * should not be modified or freed.
 */
const gchar*
nbtk_widget_get_style_pseudo_class (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (actor), NULL);

  return actor->priv->pseudo_class;
}

/**
 * nbtk_widget_set_style_pseudo_class:
 * @actor: a #NbtkWidget
 * @pseudo_class: a new pseudo class string
 *
 * Set the style pseudo class
 */
void
nbtk_widget_set_style_pseudo_class (NbtkWidget  *actor,
                                    const gchar *pseudo_class)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_IS_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (pseudo_class, priv->pseudo_class))
    {
      g_free (priv->pseudo_class);
      priv->pseudo_class = g_strdup (pseudo_class);

      nbtk_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "pseudo-class");
    }
}

/**
 * nbtk_widget_set_style:
 * @actor: a #NbtkWidget
 * @style_class: (allow-none): a inline style string, or %NULL
 *
 * Set the inline style string for this widget. The inline style string is an
 * optional ';'-separated list of CSS properties that override the style as
 * determined from the stylesheets of the current theme.
 */
void
nbtk_widget_set_style (NbtkWidget  *actor,
		       const gchar *style)
{
  NbtkWidgetPrivate *priv = actor->priv;

  g_return_if_fail (NBTK_IS_WIDGET (actor));

  priv = actor->priv;

  if (g_strcmp0 (style, priv->inline_style))
    {
      g_free (priv->inline_style);
      priv->inline_style = g_strdup (style);

      nbtk_widget_style_changed (actor);

      g_object_notify (G_OBJECT (actor), "style");
    }
}

/**
 * nbtk_widget_get_style:
 * @actor: a #NbtkWidget
 *
 * Get the current inline style string. See nbtk_widget_set_style().
 *
 * Returns: The inline style string, or %NULL. The string is owned by the
 * #NbtkWidget and should not be modified or freed.
 */
const gchar*
nbtk_widget_get_style (NbtkWidget *actor)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (actor), NULL);

  return actor->priv->inline_style;
}

static void
nbtk_widget_name_notify (NbtkWidget *widget,
                         GParamSpec *pspec,
                         gpointer data)
{
  nbtk_widget_style_changed (widget);
}

static void
nbtk_widget_init (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv;

  actor->priv = priv = NBTK_WIDGET_GET_PRIVATE (actor);
  priv->is_stylable = TRUE;

  /* connect style changed */
  g_signal_connect (actor, "notify::name", G_CALLBACK (nbtk_widget_name_notify), NULL);
}

static void
nbtk_widget_recompute_style (NbtkWidget     *widget,
			     ShellThemeNode *old_theme_node)
{
  ShellThemeNode *new_theme_node = nbtk_widget_get_theme_node (widget);

  if (!old_theme_node ||
      !shell_theme_node_geometry_equal (old_theme_node, new_theme_node))
    clutter_actor_queue_relayout ((ClutterActor *) widget);

  g_signal_emit (widget, signals[STYLE_CHANGED], 0);
  widget->priv->is_style_dirty = FALSE;
}

/**
 * nbtk_widget_ensure_style:
 * @widget: A #NbtkWidget
 *
 * Ensures that @widget has read its style information.
 *
 */
void
nbtk_widget_ensure_style (NbtkWidget *widget)
{
  g_return_if_fail (NBTK_IS_WIDGET (widget));

  if (widget->priv->is_style_dirty)
    nbtk_widget_recompute_style (widget, NULL);
}

/**
 * nbtk_widget_get_border_image:
 * @actor: A #NbtkWidget
 *
 * Get the texture used as the border image. This is set using the
 * "border-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: (transfer none): #ClutterActor
 */
ClutterActor *
nbtk_widget_get_border_image (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  return priv->border_image;
}

/**
 * nbtk_widget_get_background_image:
 * @actor: A #NbtkWidget
 *
 * Get the texture used as the background image. This is set using the
 * "background-image" CSS property. This function should normally only be used
 * by subclasses.
 *
 * Returns: (transfer none): a #ClutterActor
 */
ClutterActor *
nbtk_widget_get_background_image (NbtkWidget *actor)
{
  NbtkWidgetPrivate *priv = NBTK_WIDGET (actor)->priv;
  return priv->background_image;
}

/**
 * nbtk_widget_set_has_tooltip:
 * @widget: A #NbtkWidget
 * @has_tooltip: #TRUE if the widget should display a tooltip
 *
 * Enables tooltip support on the #NbtkWidget.
 *
 * Note that setting has-tooltip to #TRUE will cause the widget to be set
 * reactive. If you no longer need tooltip support and do not need the widget
 * to be reactive, you need to set ClutterActor::reactive to FALSE.
 *
 */
void
nbtk_widget_set_has_tooltip (NbtkWidget *widget,
                             gboolean    has_tooltip)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_IS_WIDGET (widget));

  priv = widget->priv;

  priv->has_tooltip = has_tooltip;

  if (has_tooltip)
    {
      clutter_actor_set_reactive ((ClutterActor*) widget, TRUE);

      if (!priv->tooltip)
        {
          priv->tooltip = g_object_new (NBTK_TYPE_TOOLTIP, NULL);
          clutter_actor_set_parent ((ClutterActor *) priv->tooltip,
                                    (ClutterActor *) widget);
        }
    }
  else
    {
      if (priv->tooltip)
        {
          clutter_actor_unparent (CLUTTER_ACTOR (priv->tooltip));
          priv->tooltip = NULL;
        }
    }
}

/**
 * nbtk_widget_get_has_tooltip:
 * @widget: A #NbtkWidget
 *
 * Returns the current value of the has-tooltip property. See
 * nbtk_tooltip_set_has_tooltip() for more information.
 *
 * Returns: current value of has-tooltip on @widget
 */
gboolean
nbtk_widget_get_has_tooltip (NbtkWidget *widget)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_tooltip;
}

/**
 * nbtk_widget_set_tooltip_text:
 * @widget: A #NbtkWidget
 * @text: text to set as the tooltip
 *
 * Set the tooltip text of the widget. This will set NbtkWidget::has-tooltip to
 * #TRUE. A value of #NULL will unset the tooltip and set has-tooltip to #FALSE.
 *
 */
void
nbtk_widget_set_tooltip_text (NbtkWidget  *widget,
                              const gchar *text)
{
  NbtkWidgetPrivate *priv;

  g_return_if_fail (NBTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (text == NULL)
    nbtk_widget_set_has_tooltip (widget, FALSE);
  else
    nbtk_widget_set_has_tooltip (widget, TRUE);

  nbtk_tooltip_set_label (priv->tooltip, text);
}

/**
 * nbtk_widget_get_tooltip_text:
 * @widget: A #NbtkWidget
 *
 * Get the current tooltip string
 *
 * Returns: The current tooltip string, owned by the #NbtkWidget
 */
const gchar*
nbtk_widget_get_tooltip_text (NbtkWidget *widget)
{
  g_return_val_if_fail (NBTK_IS_WIDGET (widget), NULL);

  return nbtk_tooltip_get_label (widget->priv->tooltip);
}

/**
 * nbtk_widget_show_tooltip:
 * @widget: A #NbtkWidget
 *
 * Show the tooltip for @widget
 *
 */
void
nbtk_widget_show_tooltip (NbtkWidget *widget)
{
  gfloat x, y, width, height;
  ClutterGeometry area;

  g_return_if_fail (NBTK_IS_WIDGET (widget));

  /* XXX not necceary, but first allocate transform is wrong */

  clutter_actor_get_transformed_position ((ClutterActor*) widget,
                                          &x, &y);

  clutter_actor_get_size ((ClutterActor*) widget, &width, &height);

  area.x = x;
  area.y = y;
  area.width = width;
  area.height = height;


  if (widget->priv->tooltip)
    {
      nbtk_tooltip_set_tip_area (widget->priv->tooltip, &area);
      nbtk_tooltip_show (widget->priv->tooltip);
    }
}

/**
 * nbtk_widget_hide_tooltip:
 * @widget: A #NbtkWidget
 *
 * Hide the tooltip for @widget
 *
 */
void
nbtk_widget_hide_tooltip (NbtkWidget *widget)
{
  g_return_if_fail (NBTK_IS_WIDGET (widget));

  if (widget->priv->tooltip)
    nbtk_tooltip_hide (widget->priv->tooltip);
}

/**
 * nbtk_widget_draw_background:
 * @widget: a #NbtkWidget
 *
 * Invokes #NbtkWidget::draw_background() using the default background
 * image and/or color from the @widget style
 *
 * This function should be used by subclasses of #NbtkWidget that override
 * the paint() virtual function and cannot chain up
 */
void
nbtk_widget_draw_background (NbtkWidget *self)
{
  NbtkWidgetPrivate *priv;
  NbtkWidgetClass *klass;

  g_return_if_fail (NBTK_IS_WIDGET (self));

  priv = self->priv;

  klass = NBTK_WIDGET_GET_CLASS (self);
  klass->draw_background (NBTK_WIDGET (self),
                          priv->border_image,
                          &priv->bg_color);
}
