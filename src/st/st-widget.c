/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-widget.c: Base class for St actors
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2009 Abderrahim Kitouni
 * Copyright 2009, 2010 Florian Müllner
 * Copyright 2010 Adel Gadllah
 * Copyright 2012 Igalia, S.L.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <clutter/clutter.h>
#include <clutter/clutter-pango.h>

#include "st-widget.h"

#include "st-label.h"
#include "st-private.h"
#include "st-settings.h"
#include "st-texture-cache.h"
#include "st-theme-context.h"
#include "st-theme-node-transition.h"
#include "st-theme-node-private.h"
#include "st-drawing-area.h"

#include "st-widget-accessible.h"

#include <atk/atk-enum-types.h>

/* This is set in stone and also hard-coded in GDK. */
#define VIRTUAL_CORE_POINTER_ID 2

/*
 * Forward declaration for sake of StWidgetChild
 */
typedef struct _StWidgetPrivate        StWidgetPrivate;
struct _StWidgetPrivate
{
  StThemeNode  *theme_node;
  gchar        *pseudo_class;
  gchar        *style_class;
  gchar        *inline_style;

  StThemeNodeTransition *transition_animation;

  guint is_style_dirty : 1;
  guint first_child_dirty : 1;
  guint last_child_dirty : 1;
  guint draw_bg_color : 1;
  guint draw_border_internal : 1;
  guint track_hover : 1;
  guint hover : 1;
  guint can_focus : 1;

  gulong texture_file_changed_id;
  guint update_child_styles_id;

  int enter_count;

  ClutterActor *label_actor;

  StWidget *last_visible_child;
  StWidget *first_visible_child;

  StThemeNodePaintState paint_states[2];
  int current_paint_state : 2;
};

/**
 * StWidget:
 *
 * Base class for stylable actors
 *
 * #StWidget is a simple abstract class on top of #ClutterActor. It
 * provides basic theming properties.
 *
 * Actors in the St library should subclass #StWidget if they plan
 * to obey to a certain #StStyle.
 */

enum
{
  PROP_0,

  PROP_PSEUDO_CLASS,
  PROP_STYLE_CLASS,
  PROP_STYLE,
  PROP_TRACK_HOVER,
  PROP_HOVER,
  PROP_CAN_FOCUS,
  PROP_LABEL_ACTOR,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

enum
{
  STYLE_CHANGED,
  POPUP_MENU,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (StWidget, st_widget, CLUTTER_TYPE_ACTOR);
#define ST_WIDGET_PRIVATE(w) ((StWidgetPrivate *)st_widget_get_instance_private (w))

typedef enum {
  STYLE_CHANGE_FLAGS_NONE = 0,
  STYLE_CHANGE_FLAGS_NO_TRANSITIONS = 1 << 0,
} StyleChangeFlags;

static void st_widget_recompute_style (StWidget         *widget,
                                       StThemeNode      *old_theme_node,
                                       StyleChangeFlags  flags);

static gboolean st_widget_real_navigate_focus (StWidget         *widget,
                                               ClutterActor     *from,
                                               StDirectionType   direction);

static void check_pseudo_class (StWidget *widget);
static void check_labels (StWidget *widget);

static void
st_widget_update_insensitive (StWidget *widget)
{
  if (clutter_actor_get_reactive (CLUTTER_ACTOR (widget)))
    st_widget_remove_style_pseudo_class (widget, "insensitive");
  else
    st_widget_add_style_pseudo_class (widget, "insensitive");
}

static void
st_widget_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  StWidget *actor = ST_WIDGET (gobject);

  switch (prop_id)
    {
    case PROP_PSEUDO_CLASS:
      st_widget_set_style_pseudo_class (actor, g_value_get_string (value));
      break;

    case PROP_STYLE_CLASS:
      st_widget_set_style_class_name (actor, g_value_get_string (value));
      break;

    case PROP_STYLE:
      st_widget_set_style (actor, g_value_get_string (value));
      break;

    case PROP_TRACK_HOVER:
      st_widget_set_track_hover (actor, g_value_get_boolean (value));
      break;

    case PROP_HOVER:
      st_widget_set_hover (actor, g_value_get_boolean (value));
      break;

    case PROP_CAN_FOCUS:
      st_widget_set_can_focus (actor, g_value_get_boolean (value));
      break;

    case PROP_LABEL_ACTOR:
      st_widget_set_label_actor (actor, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_widget_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (ST_WIDGET (gobject));

  switch (prop_id)
    {
    case PROP_PSEUDO_CLASS:
      g_value_set_string (value, priv->pseudo_class);
      break;

    case PROP_STYLE_CLASS:
      g_value_set_string (value, priv->style_class);
      break;

    case PROP_STYLE:
      g_value_set_string (value, priv->inline_style);
      break;

    case PROP_TRACK_HOVER:
      g_value_set_boolean (value, priv->track_hover);
      break;

    case PROP_HOVER:
      g_value_set_boolean (value, priv->hover);
      break;

    case PROP_CAN_FOCUS:
      g_value_set_boolean (value, priv->can_focus);
      break;

    case PROP_LABEL_ACTOR:
      g_value_set_object (value, priv->label_actor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_widget_constructed (GObject *gobject)
{
  G_OBJECT_CLASS (st_widget_parent_class)->constructed (gobject);

  st_widget_update_insensitive (ST_WIDGET (gobject));
}

static void
st_widget_remove_transition (StWidget *widget)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  if (priv->transition_animation)
    {
      g_object_run_dispose (G_OBJECT (priv->transition_animation));
      g_object_unref (priv->transition_animation);
      priv->transition_animation = NULL;
    }
}

static void
next_paint_state (StWidget *widget)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  priv->current_paint_state = (priv->current_paint_state + 1) % G_N_ELEMENTS (priv->paint_states);
}

static StThemeNodePaintState *
current_paint_state (StWidget *widget)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  return &priv->paint_states[priv->current_paint_state];
}

static void
st_widget_texture_cache_changed (StTextureCache *cache,
                                 GFile          *file,
                                 gpointer        user_data)
{
  StWidget *actor = ST_WIDGET (user_data);
  StWidgetPrivate *priv = st_widget_get_instance_private (actor);
  gboolean changed = FALSE;
  int i;

  for (i = 0; i < G_N_ELEMENTS (priv->paint_states); i++)
    {
      StThemeNodePaintState *paint_state = &priv->paint_states[i];
      changed |= st_theme_node_paint_state_invalidate_for_file (paint_state, file);
    }

  if (changed && clutter_actor_is_mapped (CLUTTER_ACTOR (actor)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (actor));
}

static void
st_widget_dispose (GObject *gobject)
{
  StWidget *actor = ST_WIDGET (gobject);
  StWidgetPrivate *priv = st_widget_get_instance_private (actor);

  g_clear_pointer (&priv->theme_node, g_object_unref);

  st_widget_remove_transition (actor);

  g_clear_pointer (&priv->label_actor, g_object_unref);

  g_clear_signal_handler (&priv->texture_file_changed_id,
                          st_texture_cache_get_default ());

  g_clear_object (&priv->first_visible_child);
  g_clear_object (&priv->last_visible_child);

  G_OBJECT_CLASS (st_widget_parent_class)->dispose (gobject);

  g_clear_handle_id (&priv->update_child_styles_id, g_source_remove);
}

static void
st_widget_finalize (GObject *gobject)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (ST_WIDGET (gobject));
  guint i;

  g_free (priv->style_class);
  g_free (priv->pseudo_class);
  g_free (priv->inline_style);

  for (i = 0; i < G_N_ELEMENTS (priv->paint_states); i++)
    st_theme_node_paint_state_free (&priv->paint_states[i]);

  G_OBJECT_CLASS (st_widget_parent_class)->finalize (gobject);
}


static void
st_widget_get_preferred_width (ClutterActor *self,
                               gfloat        for_height,
                               gfloat       *min_width_p,
                               gfloat       *natural_width_p)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));

  st_theme_node_adjust_for_width (theme_node, &for_height);

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->get_preferred_width (self, for_height, min_width_p, natural_width_p);

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static void
st_widget_get_preferred_height (ClutterActor *self,
                                gfloat        for_width,
                                gfloat       *min_height_p,
                                gfloat       *natural_height_p)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));

  st_theme_node_adjust_for_width (theme_node, &for_width);

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->get_preferred_height (self, for_width, min_height_p, natural_height_p);

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
st_widget_allocate (ClutterActor          *actor,
                    const ClutterActorBox *box)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  ClutterActorBox content_box;

  /* Note that we can't just chain up to clutter_actor_real_allocate --
   * Clutter does some dirty tricks for backwards compatibility.
   * Clutter also passes the actor's allocation directly to the layout
   * manager, meaning that we can't modify it for children only.
   */

  clutter_actor_set_allocation (actor, box);

  st_theme_node_get_content_box (theme_node, box, &content_box);

  /* If we've chained up to here, we want to allocate the children using the
   * currently installed layout manager */
  clutter_layout_manager_allocate (clutter_actor_get_layout_manager (actor),
                                   actor,
                                   &content_box);
}

/**
 * st_widget_paint_background:
 * @widget: The #StWidget
 *
 * Paint the background of the widget. This is meant to be called by
 * subclasses of StWidget that need to paint the background without
 * painting children.
 */
void
st_widget_paint_background (StWidget            *widget,
                            ClutterPaintNode    *node,
                            ClutterPaintContext *paint_context)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);
  StThemeNode *theme_node;
  ClutterActorBox allocation;
  float resource_scale;
  guint8 opacity;
  ClutterContext *clutter_context;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;

  clutter_context = clutter_actor_get_context (CLUTTER_ACTOR (widget));
  clutter_backend = clutter_context_get_backend (clutter_context);
  cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  resource_scale = clutter_actor_get_resource_scale (CLUTTER_ACTOR (widget));

  theme_node = st_widget_get_theme_node (widget);

  clutter_actor_get_allocation_box (CLUTTER_ACTOR (widget), &allocation);

  opacity = clutter_actor_get_paint_opacity (CLUTTER_ACTOR (widget));

  if (priv->transition_animation)
    st_theme_node_transition_paint (priv->transition_animation,
                                    cogl_context,
                                    paint_context,
                                    node,
                                    &allocation,
                                    opacity,
                                    resource_scale);
  else
    st_theme_node_paint (theme_node,
                         current_paint_state (widget),
                         cogl_context,
                         paint_context,
                         node,
                         &allocation,
                         opacity,
                         resource_scale);
}

static void
st_widget_paint_node (ClutterActor        *actor,
                      ClutterPaintNode    *node,
                      ClutterPaintContext *paint_context)
{
  st_widget_paint_background (ST_WIDGET (actor), node, paint_context);
}

static void
st_widget_parent_set (ClutterActor *widget,
                      ClutterActor *old_parent)
{
  StWidget *self = ST_WIDGET (widget);
  ClutterActorClass *parent_class;

  parent_class = CLUTTER_ACTOR_CLASS (st_widget_parent_class);
  if (parent_class->parent_set)
    parent_class->parent_set (widget, old_parent);

  st_widget_style_changed (self);
}

static void
st_widget_map (ClutterActor *actor)
{
  StWidget *self = ST_WIDGET (actor);

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->map (actor);

  st_widget_ensure_style (self);
}

static void
st_widget_unmap (ClutterActor *actor)
{
  StWidget *self = ST_WIDGET (actor);
  StWidgetPrivate *priv = st_widget_get_instance_private (self);

  CLUTTER_ACTOR_CLASS (st_widget_parent_class)->unmap (actor);

  st_widget_remove_transition (self);

  if (priv->track_hover && priv->hover)
    st_widget_set_hover (self, FALSE);
}

static void
st_widget_style_changed_internal (StWidget         *widget,
                                  StyleChangeFlags  flags);

static void
notify_children_of_style_change (ClutterActor     *self,
                                 StyleChangeFlags  flags)
{
  ClutterActorIter iter;
  ClutterActor *actor;

  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &actor))
    {
      if (ST_IS_WIDGET (actor))
        st_widget_style_changed_internal (ST_WIDGET (actor), flags);
      else
        notify_children_of_style_change (actor, flags);
    }
}

static void
st_widget_real_style_changed (StWidget *self)
{
  clutter_actor_queue_redraw ((ClutterActor *) self);
}

static void
st_widget_style_changed_internal (StWidget         *widget,
                                  StyleChangeFlags  flags)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);
  StThemeNode *old_theme_node = NULL;

  priv->is_style_dirty = TRUE;
  if (priv->theme_node)
    {
      old_theme_node = priv->theme_node;
      priv->theme_node = NULL;
    }

  /* update the style only if we are mapped */
  if (clutter_actor_is_mapped (CLUTTER_ACTOR (widget)))
    st_widget_recompute_style (widget, old_theme_node, flags);

  /* Descend through all children. If the actor is not mapped,
   * children will clear their theme node without recomputing style.
   */
  notify_children_of_style_change (CLUTTER_ACTOR (widget), flags);

  if (old_theme_node)
    g_object_unref (old_theme_node);
}

void
st_widget_style_changed (StWidget *widget)
{
  st_widget_style_changed_internal (widget, STYLE_CHANGE_FLAGS_NONE);
}

static void
on_theme_context_changed (StThemeContext *context,
                          ClutterStage   *stage)
{
  notify_children_of_style_change (CLUTTER_ACTOR (stage), STYLE_CHANGE_FLAGS_NO_TRANSITIONS);
}

static StThemeNode *
get_root_theme_node (ClutterStage *stage)
{
  StThemeContext *context = st_theme_context_get_for_stage (stage);

  if (!g_object_get_data (G_OBJECT (context), "st-theme-initialized"))
    {
      g_object_set_data (G_OBJECT (context), "st-theme-initialized", GUINT_TO_POINTER (1));
      g_signal_connect (G_OBJECT (context), "changed",
                        G_CALLBACK (on_theme_context_changed), stage);
    }

  return st_theme_context_get_root_node (context);
}

/**
 * st_widget_get_theme_node:
 * @widget: a #StWidget
 *
 * Gets the theme node holding style information for the widget.
 * The theme node is used to access standard and custom CSS
 * properties of the widget.
 *
 * Note: it is a fatal error to call this on a widget that is
 *  not been added to a stage.
 *
 * Returns: (transfer none): the theme node for the widget.
 *   This is owned by the widget. When attributes of the widget
 *   or the environment that affect the styling change (for example
 *   the style_class property of the widget), it will be recreated,
 *   and the ::style-changed signal will be emitted on the widget.
 */
StThemeNode *
st_widget_get_theme_node (StWidget *widget)
{
  StWidgetPrivate *priv;

  g_return_val_if_fail (ST_IS_WIDGET (widget), NULL);

  priv = st_widget_get_instance_private (widget);

  if (priv->theme_node == NULL)
    {
      StThemeContext *context;
      StThemeNode *tmp_node;
      StThemeNode *parent_node = NULL;
      ClutterStage *stage = NULL;
      ClutterActor *parent;
      char *pseudo_class, *direction_pseudo_class;

      parent = clutter_actor_get_parent (CLUTTER_ACTOR (widget));
      while (parent != NULL)
        {
          if (parent_node == NULL && ST_IS_WIDGET (parent))
            parent_node = st_widget_get_theme_node (ST_WIDGET (parent));
          else if (CLUTTER_IS_STAGE (parent))
            stage = CLUTTER_STAGE (parent);

          parent = clutter_actor_get_parent (parent);
        }

      if (stage == NULL)
        {
          g_autofree char *desc = st_describe_actor (CLUTTER_ACTOR (widget));

          g_critical ("st_widget_get_theme_node called on the widget %s which is not in the stage.",
                      desc);

          return g_object_new (ST_TYPE_THEME_NODE, NULL);
        }

      if (parent_node == NULL)
        parent_node = get_root_theme_node (CLUTTER_STAGE (stage));

      /* Always append a "magic" pseudo class indicating the text
       * direction, to allow to adapt the CSS when necessary without
       * requiring separate style sheets.
       */
      if (clutter_actor_get_text_direction (CLUTTER_ACTOR (widget)) == CLUTTER_TEXT_DIRECTION_RTL)
        direction_pseudo_class = (char *)"rtl";
      else
        direction_pseudo_class = (char *)"ltr";

      if (priv->pseudo_class)
        pseudo_class = g_strconcat(priv->pseudo_class, " ",
                                   direction_pseudo_class, NULL);
      else
        pseudo_class = direction_pseudo_class;

      context = st_theme_context_get_for_stage (stage);
      tmp_node = st_theme_node_new (context, parent_node, NULL,
                                    G_OBJECT_TYPE (widget),
                                    clutter_actor_get_name (CLUTTER_ACTOR (widget)),
                                    priv->style_class,
                                    pseudo_class,
                                    priv->inline_style);

      if (pseudo_class != direction_pseudo_class)
        g_free (pseudo_class);

      priv->theme_node = g_object_ref (st_theme_context_intern_node (context,
                                                                     tmp_node));
      g_object_unref (tmp_node);
    }

  return priv->theme_node;
}

/**
 * st_widget_peek_theme_node:
 * @widget: a #StWidget
 *
 * Returns the theme node for the widget if it has already been
 * computed, %NULL if the widget hasn't been added to a  stage or the theme
 * node hasn't been computed. If %NULL is returned, then ::style-changed
 * will be reliably emitted before the widget is allocated or painted.
 *
 * Returns: (transfer none): the theme node for the widget.
 *   This is owned by the widget. When attributes of the widget
 *   or the environment that affect the styling change (for example
 *   the style_class property of the widget), it will be recreated,
 *   and the ::style-changed signal will be emitted on the widget.
 */
StThemeNode *
st_widget_peek_theme_node (StWidget *widget)
{
  g_return_val_if_fail (ST_IS_WIDGET (widget), NULL);

  return ST_WIDGET_PRIVATE (widget)->theme_node;
}

static gboolean
st_widget_enter (ClutterActor *actor,
                 ClutterEvent *event)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (ST_WIDGET (actor));

  priv->enter_count++;

  if (priv->track_hover)
    {
      ClutterStage *stage;
      ClutterActor *target;

      stage = CLUTTER_STAGE (clutter_actor_get_stage (actor));
      target = clutter_stage_get_event_actor (stage, event);

      if (clutter_actor_contains (actor, target))
        st_widget_set_hover (ST_WIDGET (actor), TRUE);
      else
        {
          /* The widget has a grab and is being told about an
           * enter-event outside its hierarchy. Hopefully we already
           * got a leave-event, but if not, handle it now.
           */
          st_widget_set_hover (ST_WIDGET (actor), FALSE);
        }
    }

  if (CLUTTER_ACTOR_CLASS (st_widget_parent_class)->enter_event)
    return CLUTTER_ACTOR_CLASS (st_widget_parent_class)->enter_event (actor, event);
  else
    return FALSE;
}

static gboolean
st_widget_leave (ClutterActor *actor,
                 ClutterEvent *event)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (ST_WIDGET (actor));

  priv->enter_count--;

  if (priv->track_hover)
    {
      ClutterActor *related;

      related = clutter_event_get_related (event);

      if (!related || !clutter_actor_contains (actor, related))
        st_widget_set_hover (ST_WIDGET (actor), FALSE);
    }

  if (CLUTTER_ACTOR_CLASS (st_widget_parent_class)->leave_event)
    return CLUTTER_ACTOR_CLASS (st_widget_parent_class)->leave_event (actor, event);
  else
    return FALSE;
}

static void
st_widget_key_focus_in (ClutterActor *actor)
{
  StWidget *widget = ST_WIDGET (actor);

  st_widget_add_style_pseudo_class (widget, "focus");
}

static void
st_widget_key_focus_out (ClutterActor *actor)
{
  StWidget *widget = ST_WIDGET (actor);

  st_widget_remove_style_pseudo_class (widget, "focus");
}

static gboolean
st_widget_key_press_event (ClutterActor *actor,
                           ClutterEvent *event)
{
  ClutterModifierType state;
  uint32_t keyval;

  state = clutter_event_get_state (event);
  keyval = clutter_event_get_key_symbol (event);

  if (keyval == CLUTTER_KEY_Menu ||
      (keyval == CLUTTER_KEY_F10 &&
       (state & CLUTTER_SHIFT_MASK)))
    {
      st_widget_popup_menu (ST_WIDGET (actor));
      return TRUE;
    }

  return FALSE;
}

static gboolean
st_widget_get_paint_volume (ClutterActor *self,
                            ClutterPaintVolume *volume)
{
  ClutterActorBox paint_box, alloc_box;
  StThemeNode *theme_node;
  StWidgetPrivate *priv;
  graphene_point3d_t origin;

  /* Setting the paint volume does not make sense when we don't have any allocation */
  if (!clutter_actor_has_allocation (self))
    return FALSE;

  priv = st_widget_get_instance_private (ST_WIDGET (self));

  theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  clutter_actor_get_allocation_box (self, &alloc_box);

  if (priv->transition_animation)
    st_theme_node_transition_get_paint_box (priv->transition_animation,
                                            &alloc_box, &paint_box);
  else
    st_theme_node_get_paint_box (theme_node, &alloc_box, &paint_box);

  origin.x = paint_box.x1 - alloc_box.x1;
  origin.y = paint_box.y1 - alloc_box.y1;
  origin.z = 0.0f;

  clutter_paint_volume_set_origin (volume, &origin);
  clutter_paint_volume_set_width (volume, paint_box.x2 - paint_box.x1);
  clutter_paint_volume_set_height (volume, paint_box.y2 - paint_box.y1);

  if (!clutter_actor_get_clip_to_allocation (self))
    {
      ClutterActor *child;
      StShadow *shadow_spec = st_theme_node_get_text_shadow (theme_node);

      if (shadow_spec)
        {
          ClutterActorBox shadow_box;

          st_shadow_get_box (shadow_spec, &alloc_box, &shadow_box);
          clutter_paint_volume_union_box (volume, &shadow_box);
        }

      /* Based on ClutterGroup/ClutterBox; include the children's
       * paint volumes, since they may paint outside our allocation.
       */
      for (child = clutter_actor_get_first_child (self);
           child != NULL;
           child = clutter_actor_get_next_sibling (child))
        {
          g_autoptr (ClutterPaintVolume) child_volume = NULL;

          if (!clutter_actor_is_visible (child))
            continue;

          child_volume = clutter_actor_get_transformed_paint_volume (child, self);
          if (!child_volume)
            return FALSE;

          clutter_paint_volume_union (volume, child_volume);
        }
    }

  return TRUE;
}

static GList *
st_widget_real_get_focus_chain (StWidget *widget)
{
  GList *children, *l, *visible = NULL;

  children = clutter_actor_get_children (CLUTTER_ACTOR (widget));

  for (l = children; l; l = l->next)
    {
      if (clutter_actor_is_visible (CLUTTER_ACTOR (l->data)))
        visible = g_list_prepend (visible, l->data);
    }

  g_list_free (children);

  return g_list_reverse (visible);
}

static void
st_widget_resource_scale_changed (ClutterActor *actor)
{
  StWidget *widget = ST_WIDGET (actor);
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);
  int i;

  for (i = 0; i < G_N_ELEMENTS (priv->paint_states); i++)
    st_theme_node_paint_state_invalidate (&priv->paint_states[i]);

  if (CLUTTER_ACTOR_CLASS (st_widget_parent_class)->resource_scale_changed)
    CLUTTER_ACTOR_CLASS (st_widget_parent_class)->resource_scale_changed (actor);
}

static void
st_widget_class_init (StWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->set_property = st_widget_set_property;
  gobject_class->get_property = st_widget_get_property;
  gobject_class->constructed = st_widget_constructed;
  gobject_class->dispose = st_widget_dispose;
  gobject_class->finalize = st_widget_finalize;

  actor_class->get_accessible_type = st_widget_accessible_get_type;
  actor_class->get_preferred_width = st_widget_get_preferred_width;
  actor_class->get_preferred_height = st_widget_get_preferred_height;
  actor_class->allocate = st_widget_allocate;
  actor_class->paint_node = st_widget_paint_node;
  actor_class->get_paint_volume = st_widget_get_paint_volume;
  actor_class->parent_set = st_widget_parent_set;
  actor_class->map = st_widget_map;
  actor_class->unmap = st_widget_unmap;

  actor_class->enter_event = st_widget_enter;
  actor_class->leave_event = st_widget_leave;
  actor_class->key_focus_in = st_widget_key_focus_in;
  actor_class->key_focus_out = st_widget_key_focus_out;
  actor_class->key_press_event = st_widget_key_press_event;

  actor_class->resource_scale_changed = st_widget_resource_scale_changed;

  klass->style_changed = st_widget_real_style_changed;
  klass->navigate_focus = st_widget_real_navigate_focus;
  klass->get_focus_chain = st_widget_real_get_focus_chain;

  /**
   * StWidget:pseudo-class: (getter get_style_pseudo_class) (setter set_style_pseudo_class):
   *
   * The pseudo-class of the actor. Typical values include "hover", "active",
   * "focus".
   */
  props[PROP_PSEUDO_CLASS] =
    g_param_spec_string ("pseudo-class", NULL, NULL,
                         "",
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StWidget:style-class: (getter get_style_class_name) (setter set_style_class_name):
   *
   * The style-class of the actor for use in styling.
   */
  props[PROP_STYLE_CLASS] =
    g_param_spec_string ("style-class", NULL, NULL,
                         "",
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StWidget:style:
   *
   * Inline style information for the actor as a ';'-separated list of
   * CSS properties.
   */
  props[PROP_STYLE] =
     g_param_spec_string ("style", NULL, NULL,
                          "",
                          ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StWidget:track-hover:
   *
   * Determines whether the widget tracks pointer hover state. If
   * %TRUE (and the widget is visible and reactive), the
   * #StWidget:hover property and "hover" style pseudo class will be
   * adjusted automatically as the pointer moves in and out of the
   * widget.
   */
  props[PROP_TRACK_HOVER] =
     g_param_spec_boolean ("track-hover", NULL, NULL,
                           FALSE,
                           ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StWidget:hover:
   *
   * Whether or not the pointer is currently hovering over the widget. This is
   * only tracked automatically if #StWidget:track-hover is %TRUE, but you can
   * adjust it manually in any case.
   */
  props[PROP_HOVER] =
     g_param_spec_boolean ("hover", NULL, NULL,
                           FALSE,
                           ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StWidget:can-focus:
   *
   * Whether or not the widget can be focused via keyboard navigation.
   */
  props[PROP_CAN_FOCUS] =
     g_param_spec_boolean ("can-focus", NULL, NULL,
                           FALSE,
                           ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StWidget:label-actor:
   *
   * An actor that labels this widget.
   */
  props[PROP_LABEL_ACTOR] =
     g_param_spec_object ("label-actor", NULL, NULL,
                          CLUTTER_TYPE_ACTOR,
                          ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);

  /**
   * StWidget::style-changed:
   * @widget: the #StWidget
   *
   * Emitted when the style information that the widget derives from the
   * theme changes
   */
  signals[STYLE_CHANGED] =
    g_signal_new ("style-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StWidgetClass, style_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * StWidget::popup-menu:
   * @widget: the #StWidget
   *
   * Emitted when the user has requested a context menu (eg, via a keybinding)
   */
  signals[POPUP_MENU] =
    g_signal_new ("popup-menu",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StWidgetClass, popup_menu),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static const gchar *
find_class_name (const gchar *class_list,
                 const gchar *class_name)
{
  gint len = strlen (class_name);
  const gchar *match;

  if (!class_list)
    return NULL;

  for (match = strstr (class_list, class_name); match; match = strstr (match + 1, class_name))
    {
      if ((match == class_list || g_ascii_isspace (match[-1])) &&
          (match[len] == '\0' || g_ascii_isspace (match[len])))
        return match;
    }

  return NULL;
}

static gboolean
set_class_list (gchar       **class_list,
                const gchar  *new_class_list)
{
  if (g_strcmp0 (*class_list, new_class_list) != 0)
    {
      g_free (*class_list);
      *class_list = g_strdup (new_class_list);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
add_class_name (gchar       **class_list,
                const gchar  *class_name)
{
  gchar *new_class_list;

  if (*class_list)
    {
      if (find_class_name (*class_list, class_name))
        return FALSE;

      new_class_list = g_strdup_printf ("%s %s", *class_list, class_name);
      g_free (*class_list);
      *class_list = new_class_list;
    }
  else
    *class_list = g_strdup (class_name);

  return TRUE;
}

static gboolean
remove_class_name (gchar       **class_list,
                   const gchar  *class_name)
{
  const gchar *match, *end;
  gchar *new_class_list;

  if (!*class_list)
    return FALSE;

  if (strcmp (*class_list, class_name) == 0)
    {
      g_free (*class_list);
      *class_list = NULL;
      return TRUE;
    }

  match = find_class_name (*class_list, class_name);
  if (!match)
    return FALSE;
  end = match + strlen (class_name);

  /* Adjust either match or end to include a space as well.
   * (One or the other must be possible at this point.)
   */
  if (match != *class_list)
    match--;
  else
    end++;

  new_class_list = g_strdup_printf ("%.*s%s", (int)(match - *class_list),
                                    *class_list, end);
  g_free (*class_list);
  *class_list = new_class_list;

  return TRUE;
}

/**
 * st_widget_set_style_class_name:
 * @actor: a #StWidget
 * @style_class_list: (nullable): a new style class list string
 *
 * Set the style class name list. @style_class_list can either be
 * %NULL, for no classes, or a space-separated list of style class
 * names. See also st_widget_add_style_class_name() and
 * st_widget_remove_style_class_name().
 */
void
st_widget_set_style_class_name (StWidget    *actor,
                                const gchar *style_class_list)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));

  priv = st_widget_get_instance_private (actor);

  if (set_class_list (&priv->style_class, style_class_list))
    {
      st_widget_style_changed (actor);
      g_object_notify_by_pspec (G_OBJECT (actor), props[PROP_STYLE_CLASS]);
    }
}

/**
 * st_widget_add_style_class_name:
 * @actor: a #StWidget
 * @style_class: a style class name string
 *
 * Adds @style_class to @actor's style class name list, if it is not
 * already present.
 */
void
st_widget_add_style_class_name (StWidget    *actor,
                                const gchar *style_class)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));
  g_return_if_fail (style_class != NULL);
  g_return_if_fail (style_class[0] != '\0');

  priv = st_widget_get_instance_private (actor);

  if (add_class_name (&priv->style_class, style_class))
    {
      st_widget_style_changed (actor);
      g_object_notify_by_pspec (G_OBJECT (actor), props[PROP_STYLE_CLASS]);
    }
}

/**
 * st_widget_remove_style_class_name:
 * @actor: a #StWidget
 * @style_class: a style class name string
 *
 * Removes @style_class from @actor's style class name, if it is
 * present.
 */
void
st_widget_remove_style_class_name (StWidget    *actor,
                                   const gchar *style_class)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));
  g_return_if_fail (style_class != NULL);
  g_return_if_fail (style_class[0] != '\0');

  priv = st_widget_get_instance_private (actor);

  if (remove_class_name (&priv->style_class, style_class))
    {
      st_widget_style_changed (actor);
      g_object_notify_by_pspec (G_OBJECT (actor), props[PROP_STYLE_CLASS]);
    }
}

/**
 * st_widget_get_style_class_name:
 * @actor: a #StWidget
 *
 * Get the current style class name
 *
 * Returns: the class name string. The string is owned by the #StWidget and
 * should not be modified or freed.
 */
const gchar*
st_widget_get_style_class_name (StWidget *actor)
{
  g_return_val_if_fail (ST_IS_WIDGET (actor), NULL);

  return ST_WIDGET_PRIVATE (actor)->style_class;
}

/**
 * st_widget_has_style_class_name:
 * @actor: a #StWidget
 * @style_class: a style class string
 *
 * Tests if @actor's style class list includes @style_class.
 *
 * Returns: whether or not @actor's style class list includes
 * @style_class.
 */
gboolean
st_widget_has_style_class_name (StWidget    *actor,
                                const gchar *style_class)
{
  StWidgetPrivate *priv;

  g_return_val_if_fail (ST_IS_WIDGET (actor), FALSE);
  g_return_val_if_fail (style_class != NULL, FALSE);
  g_return_val_if_fail (style_class[0] != '\0', FALSE);

  priv = st_widget_get_instance_private (actor);

  return find_class_name (priv->style_class, style_class) != NULL;
}

/**
 * st_widget_get_style_pseudo_class:
 * @actor: a #StWidget
 *
 * Get the current style pseudo class list.
 *
 * Note that an actor can have multiple pseudo classes; if you just
 * want to test for the presence of a specific pseudo class, use
 * st_widget_has_style_pseudo_class().
 *
 * Returns: the pseudo class list string. The string is owned by the
 * #StWidget and should not be modified or freed.
 */
const gchar*
st_widget_get_style_pseudo_class (StWidget *actor)
{
  g_return_val_if_fail (ST_IS_WIDGET (actor), NULL);

  return ST_WIDGET_PRIVATE (actor)->pseudo_class;
}

/**
 * st_widget_has_style_pseudo_class:
 * @actor: a #StWidget
 * @pseudo_class: a pseudo class string
 *
 * Tests if @actor's pseudo class list includes @pseudo_class.
 *
 * Returns: whether or not @actor's pseudo class list includes
 * @pseudo_class.
 */
gboolean
st_widget_has_style_pseudo_class (StWidget    *actor,
                                  const gchar *pseudo_class)
{
  StWidgetPrivate *priv;

  g_return_val_if_fail (ST_IS_WIDGET (actor), FALSE);
  g_return_val_if_fail (pseudo_class != NULL, FALSE);
  g_return_val_if_fail (pseudo_class[0] != '\0', FALSE);

  priv = st_widget_get_instance_private (actor);

  return find_class_name (priv->pseudo_class, pseudo_class) != NULL;
}

/**
 * st_widget_set_style_pseudo_class:
 * @actor: a #StWidget
 * @pseudo_class_list: (nullable): a new pseudo class list string
 *
 * Set the style pseudo class list. @pseudo_class_list can either be
 * %NULL, for no classes, or a space-separated list of pseudo class
 * names. See also st_widget_add_style_pseudo_class() and
 * st_widget_remove_style_pseudo_class().
 */
void
st_widget_set_style_pseudo_class (StWidget    *actor,
                                  const gchar *pseudo_class_list)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));

  priv = st_widget_get_instance_private (actor);

  if (set_class_list (&priv->pseudo_class, pseudo_class_list))
    {
      st_widget_style_changed (actor);
      g_object_notify_by_pspec (G_OBJECT (actor), props[PROP_PSEUDO_CLASS]);
      check_pseudo_class (actor);
    }
}

/**
 * st_widget_add_style_pseudo_class:
 * @actor: a #StWidget
 * @pseudo_class: a pseudo class string
 *
 * Adds @pseudo_class to @actor's pseudo class list, if it is not
 * already present.
 */
void
st_widget_add_style_pseudo_class (StWidget    *actor,
                                  const gchar *pseudo_class)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));
  g_return_if_fail (pseudo_class != NULL);
  g_return_if_fail (pseudo_class[0] != '\0');

  priv = st_widget_get_instance_private (actor);

  if (add_class_name (&priv->pseudo_class, pseudo_class))
    {
      st_widget_style_changed (actor);
      g_object_notify_by_pspec (G_OBJECT (actor), props[PROP_PSEUDO_CLASS]);
      check_pseudo_class (actor);
    }
}

/**
 * st_widget_remove_style_pseudo_class:
 * @actor: a #StWidget
 * @pseudo_class: a pseudo class string
 *
 * Removes @pseudo_class from @actor's pseudo class, if it is present.
 */
void
st_widget_remove_style_pseudo_class (StWidget    *actor,
                                     const gchar *pseudo_class)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));
  g_return_if_fail (pseudo_class != NULL);
  g_return_if_fail (pseudo_class[0] != '\0');

  priv = st_widget_get_instance_private (actor);

  if (remove_class_name (&priv->pseudo_class, pseudo_class))
    {
      st_widget_style_changed (actor);
      g_object_notify_by_pspec (G_OBJECT (actor), props[PROP_PSEUDO_CLASS]);
      check_pseudo_class (actor);
    }
}

/**
 * st_widget_set_style:
 * @actor: a #StWidget
 * @style: (nullable): a inline style string, or %NULL
 *
 * Set the inline style string for this widget. The inline style string is an
 * optional ';'-separated list of CSS properties that override the style as
 * determined from the stylesheets of the current theme.
 */
void
st_widget_set_style (StWidget  *actor,
                     const gchar *style)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (actor));

  priv = st_widget_get_instance_private (actor);

  if (g_strcmp0 (style, priv->inline_style))
    {
      g_free (priv->inline_style);
      priv->inline_style = g_strdup (style);

      st_widget_style_changed (actor);

      g_object_notify_by_pspec (G_OBJECT (actor), props[PROP_STYLE]);
    }
}

/**
 * st_widget_get_style:
 * @actor: a #StWidget
 *
 * Get the current inline style string. See st_widget_set_style().
 *
 * Returns: (transfer none) (nullable): The inline style string, or %NULL. The
 *   string is owned by the #StWidget and should not be modified or freed.
 */
const gchar*
st_widget_get_style (StWidget *actor)
{
  g_return_val_if_fail (ST_IS_WIDGET (actor), NULL);

  return ST_WIDGET_PRIVATE (actor)->inline_style;
}

static void
st_widget_set_first_visible_child (StWidget     *widget,
                                   ClutterActor *actor)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  if (priv->first_visible_child == NULL && actor == NULL)
    return;

  if (priv->first_visible_child != NULL &&
      CLUTTER_ACTOR (priv->first_visible_child) == actor)
    return;

  if (priv->first_visible_child != NULL)
    {
      st_widget_remove_style_pseudo_class (priv->first_visible_child, "first-child");
      g_clear_object (&priv->first_visible_child);
    }

  if (actor == NULL)
    return;

  if (ST_IS_WIDGET (actor))
    {
      st_widget_add_style_pseudo_class (ST_WIDGET (actor), "first-child");
      priv->first_visible_child = g_object_ref (ST_WIDGET (actor));
    }
}

static void
st_widget_set_last_visible_child (StWidget     *widget,
                                  ClutterActor *actor)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  if (priv->last_visible_child == NULL && actor == NULL)
    return;

  if (priv->last_visible_child != NULL &&
      CLUTTER_ACTOR (priv->last_visible_child) == actor)
    return;

  if (priv->last_visible_child != NULL)
    {
      st_widget_remove_style_pseudo_class (priv->last_visible_child, "last-child");
      g_clear_object (&priv->last_visible_child);
    }

  if (actor == NULL)
    return;

  if (ST_IS_WIDGET (actor))
    {
      st_widget_add_style_pseudo_class (ST_WIDGET (actor), "last-child");
      priv->last_visible_child = g_object_ref (ST_WIDGET (actor));
    }
}

static void
st_widget_name_notify (StWidget   *widget,
                       GParamSpec *pspec,
                       gpointer    data)
{
  st_widget_style_changed (widget);
}

static void
st_widget_reactive_notify (StWidget   *widget,
                           GParamSpec *pspec,
                           gpointer    data)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  st_widget_update_insensitive (widget);

  if (priv->track_hover)
    st_widget_sync_hover(widget);
}

static ClutterActor *
find_nearest_visible_backwards (ClutterActor *actor)
{
  ClutterActor *prev = actor;

  while (prev != NULL && !clutter_actor_is_visible (prev))
    prev = clutter_actor_get_previous_sibling (prev);
  return prev;
}

static ClutterActor *
find_nearest_visible_forward (ClutterActor *actor)
{
  ClutterActor *next = actor;

  while (next != NULL && !clutter_actor_is_visible (next))
    next = clutter_actor_get_next_sibling (next);
  return next;
}

static gboolean
st_widget_update_child_styles (StWidget *widget)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  if (priv->first_child_dirty)
    {
      ClutterActor *first_child;

      priv->first_child_dirty = FALSE;

      first_child = clutter_actor_get_first_child (CLUTTER_ACTOR (widget));
      st_widget_set_first_visible_child (widget,
                                         find_nearest_visible_forward (first_child));
    }

  if (priv->last_child_dirty)
    {
      ClutterActor *last_child;

      priv->last_child_dirty = FALSE;

      last_child = clutter_actor_get_last_child (CLUTTER_ACTOR (widget));
      st_widget_set_last_visible_child (widget,
                                        find_nearest_visible_backwards (last_child));
    }

  priv->update_child_styles_id = 0;
  return G_SOURCE_REMOVE;
}

static void
st_widget_queue_child_styles_update (StWidget *widget)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  if (priv->update_child_styles_id != 0)
    return;

  priv->update_child_styles_id = g_idle_add ((GSourceFunc) st_widget_update_child_styles, widget);
}

static void
st_widget_visible_notify (StWidget   *widget,
                          GParamSpec *pspec,
                          gpointer    data)
{
  StWidgetPrivate *parent_priv;
  ClutterActor *actor = CLUTTER_ACTOR (widget);
  ClutterActor *parent = clutter_actor_get_parent (actor);

  if (parent == NULL || !ST_IS_WIDGET (parent))
    return;

  parent_priv = st_widget_get_instance_private (ST_WIDGET (parent));

  if (clutter_actor_is_visible (actor))
    {
      ClutterActor *before, *after;

      before = clutter_actor_get_previous_sibling (actor);
      if (find_nearest_visible_backwards (before) == NULL)
        parent_priv->first_child_dirty = TRUE;

      after = clutter_actor_get_next_sibling (actor);
      if (find_nearest_visible_forward (after) == NULL)
        parent_priv->last_child_dirty = TRUE;
    }
  else
    {
      if (st_widget_has_style_pseudo_class (widget, "first-child"))
        parent_priv->first_child_dirty = TRUE;

      if (st_widget_has_style_pseudo_class (widget, "last-child"))
        parent_priv->last_child_dirty = TRUE;
    }

  if (parent_priv->first_child_dirty || parent_priv->last_child_dirty)
    st_widget_queue_child_styles_update (ST_WIDGET (parent));
}

static void
st_widget_first_child_notify (StWidget   *widget,
                              GParamSpec *pspec,
                              gpointer    data)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  priv->first_child_dirty = TRUE;
  st_widget_queue_child_styles_update (widget);
}

static void
st_widget_last_child_notify (StWidget   *widget,
                             GParamSpec *pspec,
                             gpointer    data)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);

  priv->last_child_dirty = TRUE;
  st_widget_queue_child_styles_update (widget);
}

static void
st_widget_init (StWidget *actor)
{
  StWidgetPrivate *priv;
  guint i;

  priv = st_widget_get_instance_private (actor);
  priv->transition_animation = NULL;

  /* connect style changed */
  g_signal_connect (actor, "notify::name", G_CALLBACK (st_widget_name_notify), NULL);
  g_signal_connect (actor, "notify::reactive", G_CALLBACK (st_widget_reactive_notify), NULL);

  g_signal_connect (actor, "notify::visible", G_CALLBACK (st_widget_visible_notify), NULL);
  g_signal_connect (actor, "notify::first-child", G_CALLBACK (st_widget_first_child_notify), NULL);
  g_signal_connect (actor, "notify::last-child", G_CALLBACK (st_widget_last_child_notify), NULL);
  priv->texture_file_changed_id = g_signal_connect (st_texture_cache_get_default (), "texture-file-changed",
                                                    G_CALLBACK (st_widget_texture_cache_changed), actor);

  for (i = 0; i < G_N_ELEMENTS (priv->paint_states); i++)
    st_theme_node_paint_state_init (&priv->paint_states[i]);
}

static void
on_transition_completed (StThemeNodeTransition *transition,
                         StWidget              *widget)
{
  next_paint_state (widget);

  st_theme_node_paint_state_copy (current_paint_state (widget),
                                  st_theme_node_transition_get_new_paint_state (transition));

  st_widget_remove_transition (widget);
}

static void
st_widget_recompute_style (StWidget         *widget,
                           StThemeNode      *old_theme_node,
                           StyleChangeFlags  flags)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);
  StThemeNode *new_theme_node = st_widget_get_theme_node (widget);
  int transition_duration;
  StSettings *settings;
  gboolean paint_equal, geometry_equal = FALSE;
  gboolean animations_enabled;

  if (new_theme_node == old_theme_node)
    {
      priv->is_style_dirty = FALSE;
      return;
    }

  _st_theme_node_apply_margins (new_theme_node, CLUTTER_ACTOR (widget));

  if (old_theme_node)
    geometry_equal = st_theme_node_geometry_equal (old_theme_node, new_theme_node);
  if (!geometry_equal)
    clutter_actor_queue_relayout ((ClutterActor *) widget);

  transition_duration = (flags & STYLE_CHANGE_FLAGS_NO_TRANSITIONS) != 0
    ? 0
    : st_theme_node_get_transition_duration (new_theme_node);

  paint_equal = st_theme_node_paint_equal (old_theme_node, new_theme_node);

  settings = st_settings_get ();
  g_object_get (settings, "enable-animations", &animations_enabled, NULL);

  if (animations_enabled && transition_duration > 0)
    {
      if (priv->transition_animation != NULL)
        {
          st_theme_node_transition_update (priv->transition_animation,
                                           new_theme_node);
        }
      else if (old_theme_node && !paint_equal)
        {
          /* Since our transitions are only of the painting done by StThemeNode, we
           * only want to start a transition when what is painted changes; if
           * other visual aspects like the foreground color of a label change,
           * we can't animate that anyways.
           */

          priv->transition_animation =
            st_theme_node_transition_new (CLUTTER_ACTOR (widget),
                                          old_theme_node,
                                          new_theme_node,
                                          current_paint_state (widget),
                                          transition_duration);

          g_signal_connect (priv->transition_animation, "completed",
                            G_CALLBACK (on_transition_completed), widget);
          g_signal_connect_swapped (priv->transition_animation,
                                    "new-frame",
                                    G_CALLBACK (clutter_actor_queue_redraw),
                                    widget);
        }
    }
  else if (priv->transition_animation)
    {
      st_widget_remove_transition (widget);
    }

  if (!paint_equal)
    {
      clutter_actor_invalidate_paint_volume (CLUTTER_ACTOR (widget));

      next_paint_state (widget);

      if (!st_theme_node_paint_equal (new_theme_node, current_paint_state (widget)->node))
        st_theme_node_paint_state_invalidate (current_paint_state (widget));
    }

  g_signal_emit (widget, signals[STYLE_CHANGED], 0);

  priv->is_style_dirty = FALSE;
}

/**
 * st_widget_ensure_style:
 * @widget: A #StWidget
 *
 * Ensures that @widget has read its style information and propagated any
 * changes to its children.
 */
void
st_widget_ensure_style (StWidget *widget)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = st_widget_get_instance_private (widget);

  if (priv->is_style_dirty)
    {
      st_widget_recompute_style (widget, NULL, STYLE_CHANGE_FLAGS_NONE);
      notify_children_of_style_change (CLUTTER_ACTOR (widget), STYLE_CHANGE_FLAGS_NONE);
    }
}

/**
 * st_widget_set_track_hover:
 * @widget: A #StWidget
 * @track_hover: %TRUE if the widget should track the pointer hover state
 *
 * Enables hover tracking on the #StWidget.
 *
 * If hover tracking is enabled, and the widget is visible and
 * reactive, then @widget's #StWidget:hover property will be updated
 * automatically to reflect whether the pointer is in @widget (or one
 * of its children), and @widget's #StWidget:pseudo-class will have
 * the "hover" class added and removed from it accordingly.
 *
 * Note that currently it is not possible to correctly track the hover
 * state when another actor has a pointer grab. You can use
 * st_widget_sync_hover() to update the property manually in this
 * case.
 */
void
st_widget_set_track_hover (StWidget *widget,
                           gboolean  track_hover)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = st_widget_get_instance_private (widget);

  if (priv->track_hover != track_hover)
    {
      priv->track_hover = track_hover;
      g_object_notify_by_pspec (G_OBJECT (widget), props[PROP_TRACK_HOVER]);

      if (priv->track_hover)
        st_widget_sync_hover (widget);
      else
        st_widget_set_hover (widget, FALSE);
    }
}

/**
 * st_widget_get_track_hover:
 * @widget: A #StWidget
 *
 * Returns the current value of the #StWidget:track-hover property. See
 * st_widget_set_track_hover() for more information.
 *
 * Returns: current value of track-hover on @widget
 */
gboolean
st_widget_get_track_hover (StWidget *widget)
{
  g_return_val_if_fail (ST_IS_WIDGET (widget), FALSE);

  return ST_WIDGET_PRIVATE (widget)->track_hover;
}

/**
 * st_widget_set_hover:
 * @widget: A #StWidget
 * @hover: whether the pointer is hovering over the widget
 *
 * Sets @widget's hover property and adds or removes "hover" from its
 * pseudo class accordingly.
 *
 * If you have set #StWidget:track-hover, you should not need to call
 * this directly. You can call st_widget_sync_hover() if the hover
 * state might be out of sync due to another actor's pointer grab.
 */
void
st_widget_set_hover (StWidget *widget,
                     gboolean  hover)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = st_widget_get_instance_private (widget);

  if (priv->hover != hover)
    {
      priv->hover = hover;
      if (priv->hover)
        st_widget_add_style_pseudo_class (widget, "hover");
      else
        st_widget_remove_style_pseudo_class (widget, "hover");
      g_object_notify_by_pspec (G_OBJECT (widget), props[PROP_HOVER]);
    }
}

/**
 * st_widget_sync_hover:
 * @widget: A #StWidget
 *
 * Sets @widget's hover state according to the current pointer
 * position. This can be used to ensure that it is correct after
 * (or during) a pointer grab.
 */
void
st_widget_sync_hover (StWidget *widget)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = st_widget_get_instance_private (widget);
  st_widget_set_hover (widget, priv->enter_count > 0);
}

/**
 * st_widget_get_hover:
 * @widget: A #StWidget
 *
 * If #StWidget:track-hover is set, this returns whether the pointer
 * is currently over the widget.
 *
 * Returns: current value of hover on @widget
 */
gboolean
st_widget_get_hover (StWidget *widget)
{
  g_return_val_if_fail (ST_IS_WIDGET (widget), FALSE);

  return ST_WIDGET_PRIVATE (widget)->hover;
}

/**
 * st_widget_set_can_focus:
 * @widget: A #StWidget
 * @can_focus: %TRUE if the widget can receive keyboard focus
 *   via keyboard navigation
 *
 * Marks @widget as being able to receive keyboard focus via
 * keyboard navigation.
 */
void
st_widget_set_can_focus (StWidget *widget,
                         gboolean  can_focus)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = st_widget_get_instance_private (widget);

  if (priv->can_focus != can_focus)
    {
      priv->can_focus = can_focus;
      g_object_notify_by_pspec (G_OBJECT (widget), props[PROP_CAN_FOCUS]);

      if (can_focus)
        clutter_actor_add_accessible_state (CLUTTER_ACTOR (widget),
                                            ATK_STATE_FOCUSABLE);
      else
        clutter_actor_remove_accessible_state (CLUTTER_ACTOR (widget),
                                               ATK_STATE_FOCUSABLE);
    }
}

/**
 * st_widget_get_can_focus:
 * @widget: A #StWidget
 *
 * Returns the current value of the can-focus property. See
 * st_widget_set_can_focus() for more information.
 *
 * Returns: current value of can-focus on @widget
 */
gboolean
st_widget_get_can_focus (StWidget *widget)
{
  g_return_val_if_fail (ST_IS_WIDGET (widget), FALSE);

  return ST_WIDGET_PRIVATE (widget)->can_focus;
}

/**
 * st_widget_popup_menu:
 * @self: A #StWidget
 *
 * Asks the widget to pop-up a context menu by emitting #StWidget::popup-menu.
 */
void
st_widget_popup_menu (StWidget *self)
{
  g_signal_emit (self, signals[POPUP_MENU], 0);
}

/* filter @children to contain only only actors that overlap @rbox
 * when moving in @direction. (Assuming no transformations.)
 */
static GList *
filter_by_position (GList            *children,
                    ClutterActorBox  *rbox,
                    StDirectionType   direction)
{
  ClutterActorBox cbox;
  graphene_point3d_t abs_vertices[4];
  GList *l, *ret;
  ClutterActor *child;

  for (l = children, ret = NULL; l; l = l->next)
    {
      child = l->data;
      clutter_actor_get_abs_allocation_vertices (child, abs_vertices);
      clutter_actor_box_from_vertices (&cbox, abs_vertices);

      /* Filter out children if they are in the wrong direction from
       * @rbox, or if they don't overlap it. To account for floating-
       * point imprecision, an actor is "down" (etc.) from an another
       * actor even if it overlaps it by up to 0.1 pixels.
       */
      switch (direction)
        {
        case ST_DIR_UP:
          if (cbox.y2 > rbox->y1 + 0.1)
            continue;
          break;

        case ST_DIR_DOWN:
          if (cbox.y1 < rbox->y2 - 0.1)
            continue;
          break;

        case ST_DIR_LEFT:
          if (cbox.x2 > rbox->x1 + 0.1)
            continue;
          break;

        case ST_DIR_RIGHT:
          if (cbox.x1 < rbox->x2 - 0.1)
            continue;
          break;

        case ST_DIR_TAB_BACKWARD:
        case ST_DIR_TAB_FORWARD:
        default:
          g_return_val_if_reached (NULL);
        }

      ret = g_list_prepend (ret, child);
    }

  g_list_free (children);
  return ret;
}


static void
get_midpoint (ClutterActorBox *box,
              int             *x,
              int             *y)
{
  *x = (box->x1 + box->x2) / 2;
  *y = (box->y1 + box->y2) / 2;
}

static double
get_distance (ClutterActor    *actor,
              ClutterActorBox *bbox)
{
  int ax, ay, bx, by, dx, dy;
  ClutterActorBox abox;
  graphene_point3d_t abs_vertices[4];

  clutter_actor_get_abs_allocation_vertices (actor, abs_vertices);
  clutter_actor_box_from_vertices (&abox, abs_vertices);

  get_midpoint (&abox, &ax, &ay);
  get_midpoint (bbox, &bx, &by);
  dx = ax - bx;
  dy = ay - by;

  /* Not the exact distance, but good enough to sort by. */
  return dx*dx + dy*dy;
}

static int
sort_by_distance (gconstpointer  a,
                  gconstpointer  b,
                  gpointer       user_data)
{
  ClutterActor *actor_a = (ClutterActor *)a;
  ClutterActor *actor_b = (ClutterActor *)b;
  ClutterActorBox *box = user_data;

  return get_distance (actor_a, box) - get_distance (actor_b, box);
}

static gboolean
st_widget_real_navigate_focus (StWidget         *widget,
                               ClutterActor     *from,
                               StDirectionType   direction)
{
  StWidgetPrivate *priv = st_widget_get_instance_private (widget);
  ClutterActor *widget_actor, *focus_child;
  GList *children, *l;

  widget_actor = CLUTTER_ACTOR (widget);
  if (from == widget_actor)
    return FALSE;

  /* Figure out if @from is a descendant of @widget, and if so,
   * set @focus_child to the immediate child of @widget that
   * contains (or *is*) @from.
   */
  focus_child = from;
  while (focus_child && clutter_actor_get_parent (focus_child) != widget_actor)
    focus_child = clutter_actor_get_parent (focus_child);

  if (priv->can_focus)
    {
      if (!focus_child)
        {
          if (clutter_actor_is_mapped (widget_actor))
            {
              /* Accept focus from outside */
              clutter_actor_grab_key_focus (widget_actor);
              return TRUE;
            }
          else
            {
              /* Refuse to set focus on hidden actors */
              return FALSE;
            }
        }
      else
        {
          /* Yield focus from within: since @widget itself is
           * focusable we don't allow the focus to be navigated
           * within @widget.
           */
          return FALSE;
        }
    }

  /* See if we can navigate within @focus_child */
  if (focus_child && ST_IS_WIDGET (focus_child))
    {
      if (st_widget_navigate_focus (ST_WIDGET (focus_child), from, direction, FALSE))
        return TRUE;
    }

  children = st_widget_get_focus_chain (widget);
  if (direction == ST_DIR_TAB_FORWARD ||
      direction == ST_DIR_TAB_BACKWARD)
    {
      /* At this point we know that we want to navigate focus to one of
       * @widget's immediate children; the next one after @focus_child, or the
       * first one if @focus_child is %NULL. (With "next" and "first" being
       * determined by @direction.)
       */
      if (direction == ST_DIR_TAB_BACKWARD)
        children = g_list_reverse (children);

      if (focus_child)
        {
          /* Remove focus_child and any earlier children */
          while (children && children->data != focus_child)
            children = g_list_delete_link (children, children);
          if (children)
            children = g_list_delete_link (children, children);
        }
    }
  else /* direction is an arrow key, not tab */
    {
      ClutterActorBox sort_box;
      graphene_point3d_t abs_vertices[4];

      /* Compute the allocation box of the previous focused actor. If there
       * was no previous focus, use the coordinates of the appropriate edge of
       * @widget.
       *
       * Note that all of this code assumes the actors are not
       * transformed (or at most, they are all scaled by the same
       * amount). If @widget or any of its children is rotated, or
       * any child is inconsistently scaled, then the focus chain will
       * probably be unpredictable.
       */
      if (from)
        {
          clutter_actor_get_abs_allocation_vertices (from, abs_vertices);
          clutter_actor_box_from_vertices (&sort_box, abs_vertices);
        }
      else
        {
          clutter_actor_get_abs_allocation_vertices (widget_actor, abs_vertices);
          clutter_actor_box_from_vertices (&sort_box, abs_vertices);
          switch (direction)
            {
            case ST_DIR_UP:
              sort_box.y1 = sort_box.y2;
              break;
            case ST_DIR_DOWN:
              sort_box.y2 = sort_box.y1;
              break;
            case ST_DIR_LEFT:
              sort_box.x1 = sort_box.x2;
              break;
            case ST_DIR_RIGHT:
              sort_box.x2 = sort_box.x1;
              break;
            case ST_DIR_TAB_FORWARD:
            case ST_DIR_TAB_BACKWARD:
            default:
              g_warn_if_reached ();
            }
        }

      if (from)
        children = filter_by_position (children, &sort_box, direction);
      if (children)
        children = g_list_sort_with_data (children, sort_by_distance, &sort_box);
    }

  /* Now try each child in turn */
  for (l = children; l; l = l->next)
    {
      if (ST_IS_WIDGET (l->data))
        {
          if (st_widget_navigate_focus (l->data, from, direction, FALSE))
            {
              g_list_free (children);
              return TRUE;
            }
        }
    }

  g_list_free (children);
  return FALSE;
}


/**
 * st_widget_navigate_focus:
 * @widget: the "top level" container
 * @from: (nullable): the actor that the focus is coming from
 * @direction: the direction focus is moving in
 * @wrap_around: whether focus should wrap around
 *
 * Tries to update the keyboard focus within @widget in response to a
 * keyboard event.
 *
 * If @from is a descendant of @widget, this attempts to move the
 * keyboard focus to the next descendant of @widget (in the order
 * implied by @direction) that has the #StWidget:can-focus property
 * set. If @from is %NULL, this attempts to focus either @widget
 * itself, or its first descendant in the order implied by
 * @direction. If @from is outside of @widget, it behaves as if it was
 * a descendant if @direction is one of the directional arrows and as
 * if it was %NULL otherwise.
 *
 * If a container type is marked #StWidget:can-focus, the expected
 * behavior is that it will only take up a single slot on the focus
 * chain as a whole, rather than allowing navigation between its child
 * actors (or having a distinction between itself being focused and
 * one of its children being focused).
 *
 * Some widget classes might have slightly different behavior from the
 * above, where that would make more sense.
 *
 * If @wrap_around is %TRUE and @from is a child of @widget, but the
 * widget has no further children that can accept the focus in the
 * given direction, then st_widget_navigate_focus() will try a second
 * time, using a %NULL @from, which should cause it to reset the focus
 * to the first available widget in the given direction.
 *
 * Returns: %TRUE if clutter_actor_grab_key_focus() has been
 * called on an actor. %FALSE if not.
 */
gboolean
st_widget_navigate_focus (StWidget         *widget,
                          ClutterActor     *from,
                          StDirectionType   direction,
                          gboolean          wrap_around)
{
  g_return_val_if_fail (ST_IS_WIDGET (widget), FALSE);

  if (ST_WIDGET_GET_CLASS (widget)->navigate_focus (widget, from, direction))
    return TRUE;
  if (wrap_around && from && clutter_actor_contains (CLUTTER_ACTOR (widget), from))
    return ST_WIDGET_GET_CLASS (widget)->navigate_focus (widget, NULL, direction);
  return FALSE;
}

static gboolean
append_actor_text (GString      *desc,
                   ClutterActor *actor)
{
  if (CLUTTER_IS_TEXT (actor))
    {
      g_string_append_printf (desc, " (\"%s\")",
                              clutter_text_get_text (CLUTTER_TEXT (actor)));
      return TRUE;
    }
  else if (ST_IS_LABEL (actor))
    {
      g_string_append_printf (desc, " (\"%s\")",
                              st_label_get_text (ST_LABEL (actor)));
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * st_describe_actor:
 * @actor: a #ClutterActor
 *
 * Creates a string describing @actor, for use in debugging. This
 * includes the class name and actor name (if any), plus if @actor
 * is an #StWidget, its style class and pseudo class names.
 *
 * Returns: the debug name.
 */
char *
st_describe_actor (ClutterActor *actor)
{
  GString *desc;
  const char *name;
  int i;

  if (!actor)
    return g_strdup ("[null]");

  desc = g_string_new (NULL);
  g_string_append_printf (desc, "[%p %s", actor,
                          G_OBJECT_TYPE_NAME (actor));

  if (ST_IS_WIDGET (actor))
    {
      const char *style_class = st_widget_get_style_class_name (ST_WIDGET (actor));
      const char *pseudo_class = st_widget_get_style_pseudo_class (ST_WIDGET (actor));
      char **classes;

      if (style_class)
        {
          classes = g_strsplit (style_class, ",", -1);
          for (i = 0; classes[i]; i++)
            {
              g_strchug (classes[i]);
              g_string_append_printf (desc, ".%s", classes[i]);
            }
          g_strfreev (classes);
        }

      if (pseudo_class)
        {
          classes = g_strsplit (pseudo_class, ",", -1);
          for (i = 0; classes[i]; i++)
            {
              g_strchug (classes[i]);
              g_string_append_printf (desc, ":%s", classes[i]);
            }
          g_strfreev (classes);
        }
    }

  name = clutter_actor_get_name (actor);
  if (name)
    g_string_append_printf (desc, " \"%s\"", name);

  if (!append_actor_text (desc, actor))
    {
      GList *children, *l;

      /* Do a limited search of @actor's children looking for a label */
      children = clutter_actor_get_children (actor);
      for (l = children, i = 0; l && i < 20; l = l->next, i++)
        {
          if (append_actor_text (desc, l->data))
            break;
          children = g_list_concat (children, clutter_actor_get_children (l->data));
        }
      g_list_free (children);
    }

  g_string_append_c (desc, ']');

  return g_string_free (desc, FALSE);
}

/**
 * st_widget_get_label_actor:
 * @widget: a #StWidget
 *
 * Gets the label that identifies @widget if it is defined
 *
 * Returns: (transfer none): the label that identifies the widget
 */
ClutterActor *
st_widget_get_label_actor (StWidget *widget)
{
  g_return_val_if_fail (ST_IS_WIDGET (widget), NULL);

  return ST_WIDGET_PRIVATE (widget)->label_actor;
}

/**
 * st_widget_set_label_actor:
 * @widget: a #StWidget
 * @label: a #ClutterActor
 *
 * Sets @label as the #ClutterActor that identifies (labels)
 * @widget. @label can be %NULL to indicate that @widget is not
 * labelled any more
 */

void
st_widget_set_label_actor (StWidget     *widget,
                           ClutterActor *label)
{
  StWidgetPrivate *priv;

  g_return_if_fail (ST_IS_WIDGET (widget));

  priv = st_widget_get_instance_private (widget);

  if (priv->label_actor != label)
    {
      if (priv->label_actor)
        g_object_unref (priv->label_actor);

      if (label != NULL)
        priv->label_actor = g_object_ref (label);
      else
        priv->label_actor = NULL;

      g_object_notify_by_pspec (G_OBJECT (widget), props[PROP_LABEL_ACTOR]);
      check_labels (widget);
    }
}


/******************************************************************************/
/*************************** ACCESSIBILITY SUPPORT ****************************/
/******************************************************************************/

/* GObject */

static void st_widget_accessible_dispose    (GObject *gobject);

/* AtkObject */
static void         st_widget_accessible_initialize    (AtkObject *obj,
                                                        gpointer   data);

typedef struct _StWidgetAccessiblePrivate
{
  /* Cached values (used to avoid extra notifications) */
  gboolean selected;
  gboolean checked;

  /* The current_label. Right now there are the proper atk
   * relationships between this object and the label
   */
  AtkObject *current_label;
} StWidgetAccessiblePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (StWidgetAccessible, st_widget_accessible, CLUTTER_TYPE_ACTOR_ACCESSIBLE)

static void
st_widget_accessible_class_init (StWidgetAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  gobject_class->dispose = st_widget_accessible_dispose;

  atk_class->initialize = st_widget_accessible_initialize;
}

static void
st_widget_accessible_init (StWidgetAccessible *self)
{
}

static void
st_widget_accessible_dispose (GObject *gobject)
{
  StWidgetAccessible *self = ST_WIDGET_ACCESSIBLE (gobject);
  StWidgetAccessiblePrivate *priv =
    st_widget_accessible_get_instance_private (self);

  g_clear_object (&priv->current_label);

  G_OBJECT_CLASS (st_widget_accessible_parent_class)->dispose (gobject);
}

static void
st_widget_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (st_widget_accessible_parent_class)->initialize (obj, data);

  /* Check the cached selected state and notify the first selection.
   * Ie: it is required to ensure a first notification when Alt+Tab
   * popup appears
   */
  check_pseudo_class (ST_WIDGET (data));
  check_labels (ST_WIDGET (data));
}

/*
 * In some cases the only way to check some states are checking the
 * pseudo-class. Like if the object is selected (see bug 637830) or if
 * the object is toggled. This method also notifies a state change if
 * the value is different to the one cached.
 *
 * We also assume that if the object uses that pseudo-class, it makes
 * sense to notify that state change. It would be possible to refine
 * that behaviour checking the role (ie: notify CHECKED changes only
 * for CHECK_BUTTON roles).
 *
 * In a ideal world we would have a more standard way to get the
 * state, like the widget-context (as in the case of
 * gtktreeview-cells), or something like the property "can-focus". But
 * for the moment this is enough, and we can update that in the future
 * if required.
 */
static void
check_pseudo_class (StWidget *widget)
{
  gboolean found = FALSE;
  AtkObject *accessible =
    clutter_actor_get_accessible (CLUTTER_ACTOR (widget));
  StWidgetAccessiblePrivate *priv;

  if (!accessible)
    return;

  priv = st_widget_accessible_get_instance_private (ST_WIDGET_ACCESSIBLE (accessible));
  found = st_widget_has_style_pseudo_class (widget,
                                            "selected");

  if (found != priv->selected)
    {
      priv->selected = found;
      if (priv->selected)
        clutter_actor_add_accessible_state (CLUTTER_ACTOR (widget),
                                            ATK_STATE_SELECTED);
      else
        clutter_actor_remove_accessible_state (CLUTTER_ACTOR (widget),
                                               ATK_STATE_SELECTED);
    }

  found = st_widget_has_style_pseudo_class (widget,
                                            "checked");
  if (found != priv->checked)
    {
      priv->checked = found;
      if (priv->checked)
        clutter_actor_add_accessible_state (CLUTTER_ACTOR (widget),
                                            ATK_STATE_CHECKED);
      else
        clutter_actor_remove_accessible_state (CLUTTER_ACTOR (widget),
                                               ATK_STATE_CHECKED);
    }
}

static void
check_labels (StWidget *widget)
{
  AtkObject *accessible =
    clutter_actor_get_accessible (CLUTTER_ACTOR (widget));
  StWidgetAccessiblePrivate *priv;
  ClutterActor *label = NULL;
  AtkObject *label_accessible = NULL;

  if (!accessible)
    return;

  priv = st_widget_accessible_get_instance_private (ST_WIDGET_ACCESSIBLE (accessible));

  /* We only call this method at startup, and when the label changes,
   * so it is fine to remove the previous relationships if we have the
   * current_label by default
   */
  if (priv->current_label != NULL)
    {
      AtkObject *previous_label = priv->current_label;

      atk_object_remove_relationship (accessible,
                                      ATK_RELATION_LABELLED_BY,
                                      previous_label);

      atk_object_remove_relationship (previous_label,
                                      ATK_RELATION_LABEL_FOR,
                                      accessible);

      g_object_unref (previous_label);
    }

  label = st_widget_get_label_actor (widget);
  if (label == NULL)
    {
      priv->current_label = NULL;
    }
  else
    {
      label_accessible = clutter_actor_get_accessible (label);
      priv->current_label = g_object_ref (label_accessible);

      atk_object_add_relationship (accessible,
                                   ATK_RELATION_LABELLED_BY,
                                   label_accessible);

      atk_object_add_relationship (label_accessible,
                                   ATK_RELATION_LABEL_FOR,
                                   accessible);
    }
}

/**
 * st_widget_get_focus_chain:
 * @widget: An #StWidget
 *
 * Gets a list of the focusable children of @widget, in "Tab"
 * order. By default, this returns all visible
 * (as in [method@Clutter.Actor.is_visible]) children of @widget.
 *
 * Returns: (element-type Clutter.Actor) (transfer container):
 *   @widget's focusable children
 */
GList *
st_widget_get_focus_chain (StWidget *widget)
{
  return ST_WIDGET_GET_CLASS (widget)->get_focus_chain (widget);
}
