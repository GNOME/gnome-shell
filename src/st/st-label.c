/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-label.c: Plain label actor
 *
 * Copyright 2008,2009 Intel Corporation
 * Copyright 2009 Red Hat, Inc.
 * Copyright 2010 Florian Müllner
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

/**
 * SECTION:st-label
 * @short_description: Widget for displaying text
 *
 * #StLabel is a simple widget for displaying text. It derives from
 * #StWidget to add extra style and placement functionality over
 * #ClutterText. The internal #ClutterText is publicly accessibly to allow
 * applications to set further properties.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>

#include "st-label.h"
#include "st-private.h"
#include "st-widget.h"

#include <st/st-widget-accessible.h>

enum
{
  PROP_0,

  PROP_CLUTTER_TEXT,
  PROP_TEXT,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

struct _StLabelPrivate
{
  ClutterActor *label;

  StShadow *shadow_spec;

  CoglPipeline *text_shadow_pipeline;
  float         shadow_width;
  float         shadow_height;
};

G_DEFINE_TYPE_WITH_PRIVATE (StLabel, st_label, ST_TYPE_WIDGET);

static GType st_label_accessible_get_type (void) G_GNUC_CONST;

static void
st_label_set_property (GObject      *gobject,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  StLabel *label = ST_LABEL (gobject);

  switch (prop_id)
    {
    case PROP_TEXT:
      st_label_set_text (label, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_label_get_property (GObject    *gobject,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  StLabelPrivate *priv = ST_LABEL (gobject)->priv;

  switch (prop_id)
    {
    case PROP_CLUTTER_TEXT:
      g_value_set_object (value, priv->label);
      break;

    case PROP_TEXT:
      g_value_set_string (value, clutter_text_get_text (CLUTTER_TEXT (priv->label)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_label_style_changed (StWidget *self)
{
  StLabelPrivate *priv = ST_LABEL(self)->priv;
  StThemeNode *theme_node;
  StShadow *shadow_spec;

  theme_node = st_widget_get_theme_node (self);

  shadow_spec = st_theme_node_get_text_shadow (theme_node);
  if (!priv->shadow_spec || !shadow_spec ||
      !st_shadow_equal (shadow_spec, priv->shadow_spec))
    {
      g_clear_pointer (&priv->text_shadow_pipeline, cogl_object_unref);

      g_clear_pointer (&priv->shadow_spec, st_shadow_unref);
      if (shadow_spec)
        priv->shadow_spec = st_shadow_ref (shadow_spec);
    }

  _st_set_text_from_style ((ClutterText *)priv->label, st_widget_get_theme_node (self));

  ST_WIDGET_CLASS (st_label_parent_class)->style_changed (self);
}

static void
st_label_get_preferred_width (ClutterActor *actor,
                              gfloat        for_height,
                              gfloat       *min_width_p,
                              gfloat       *natural_width_p)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));

  st_theme_node_adjust_for_height (theme_node, &for_height);

  clutter_actor_get_preferred_width (priv->label, for_height,
                                     min_width_p,
                                     natural_width_p);

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static void
st_label_get_preferred_height (ClutterActor *actor,
                               gfloat        for_width,
                               gfloat       *min_height_p,
                               gfloat       *natural_height_p)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));

  st_theme_node_adjust_for_width (theme_node, &for_width);

  clutter_actor_get_preferred_height (priv->label, for_width,
                                      min_height_p,
                                      natural_height_p);

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
st_label_allocate (ClutterActor          *actor,
                   const ClutterActorBox *box)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  ClutterActorBox content_box;

  clutter_actor_set_allocation (actor, box);

  st_theme_node_get_content_box (theme_node, box, &content_box);

  clutter_actor_allocate (priv->label, &content_box);
}

static void
st_label_dispose (GObject   *object)
{
  StLabelPrivate *priv = ST_LABEL (object)->priv;

  priv->label = NULL;
  g_clear_pointer (&priv->text_shadow_pipeline, cogl_object_unref);

  G_OBJECT_CLASS (st_label_parent_class)->dispose (object);
}

static void
st_label_paint (ClutterActor        *actor,
                ClutterPaintContext *paint_context)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;

  st_widget_paint_background (ST_WIDGET (actor), paint_context);

  if (priv->shadow_spec)
    {
      ClutterActorBox allocation;
      float width, height;
      float resource_scale;

      clutter_actor_get_allocation_box (priv->label, &allocation);
      clutter_actor_box_get_size (&allocation, &width, &height);

      resource_scale = clutter_actor_get_resource_scale (priv->label);

      width *= resource_scale;
      height *= resource_scale;

      if (priv->text_shadow_pipeline == NULL ||
          width != priv->shadow_width ||
          height != priv->shadow_height)
        {
          g_clear_pointer (&priv->text_shadow_pipeline, cogl_object_unref);

          priv->shadow_width = width;
          priv->shadow_height = height;
          priv->text_shadow_pipeline =
            _st_create_shadow_pipeline_from_actor (priv->shadow_spec,
                                                   priv->label);
        }

      if (priv->text_shadow_pipeline != NULL)
        {
          CoglFramebuffer *framebuffer;

          framebuffer =
            clutter_paint_context_get_framebuffer (paint_context);
          _st_paint_shadow_with_opacity (priv->shadow_spec,
                                         framebuffer,
                                         priv->text_shadow_pipeline,
                                         &allocation,
                                         clutter_actor_get_paint_opacity (priv->label));
        }
    }

  clutter_actor_paint (priv->label, paint_context);
}

static void
st_label_resource_scale_changed (ClutterActor *actor)
{
  StLabelPrivate *priv = ST_LABEL (actor)->priv;

  g_clear_pointer (&priv->text_shadow_pipeline, cogl_object_unref);

  if (CLUTTER_ACTOR_CLASS (st_label_parent_class)->resource_scale_changed)
    CLUTTER_ACTOR_CLASS (st_label_parent_class)->resource_scale_changed (actor);
}

static void
st_label_class_init (StLabelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  gobject_class->set_property = st_label_set_property;
  gobject_class->get_property = st_label_get_property;
  gobject_class->dispose = st_label_dispose;

  actor_class->paint = st_label_paint;
  actor_class->allocate = st_label_allocate;
  actor_class->get_preferred_width = st_label_get_preferred_width;
  actor_class->get_preferred_height = st_label_get_preferred_height;
  actor_class->resource_scale_changed = st_label_resource_scale_changed;

  widget_class->style_changed = st_label_style_changed;
  widget_class->get_accessible_type = st_label_accessible_get_type;

  /**
   * StLabel:clutter-text:
   *
   * The internal #ClutterText actor supporting the label
   */
  props[PROP_CLUTTER_TEXT] =
      g_param_spec_object ("clutter-text",
                           "Clutter Text",
                           "Internal ClutterText actor",
                           CLUTTER_TYPE_TEXT,
                           ST_PARAM_READABLE);

  /**
   * StLabel:text:
   *
   * The current text being display in the #StLabel.
   */
  props[PROP_TEXT] =
      g_param_spec_string ("text",
                           "Text",
                           "Text of the label",
                           NULL,
                           ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
}

static void
invalidate_shadow_pipeline (GObject    *object,
                            GParamSpec *pspec,
                            StLabel    *label)
{
  StLabelPrivate *priv = st_label_get_instance_private (label);

  g_clear_pointer (&priv->text_shadow_pipeline, cogl_object_unref);
}

static void
st_label_init (StLabel *label)
{
  ClutterActor *actor = CLUTTER_ACTOR (label);
  StLabelPrivate *priv;

  label->priv = priv = st_label_get_instance_private (label);

  label->priv->label = g_object_new (CLUTTER_TYPE_TEXT,
                                     "ellipsize", PANGO_ELLIPSIZE_END,
                                     NULL);
  label->priv->text_shadow_pipeline = NULL;
  label->priv->shadow_width = -1.;
  label->priv->shadow_height = -1.;

  /* These properties might get set from CSS using _st_set_text_from_style */
  g_signal_connect (priv->label, "notify::font-description",
                    G_CALLBACK (invalidate_shadow_pipeline), label);

  g_signal_connect (priv->label, "notify::attributes",
                    G_CALLBACK (invalidate_shadow_pipeline), label);

  g_signal_connect (priv->label, "notify::justify",
                    G_CALLBACK (invalidate_shadow_pipeline), label);

  g_signal_connect (priv->label, "notify::line-alignment",
                    G_CALLBACK (invalidate_shadow_pipeline), label);

  clutter_actor_add_child (actor, priv->label);

  clutter_actor_set_offscreen_redirect (actor,
                                        CLUTTER_OFFSCREEN_REDIRECT_ALWAYS);
}

/**
 * st_label_new:
 * @text: (nullable): text to set the label to
 *
 * Create a new #StLabel with the label specified by @text.
 *
 * Returns: a new #StLabel
 */
StWidget *
st_label_new (const gchar *text)
{
  if (text == NULL || *text == '\0')
    return g_object_new (ST_TYPE_LABEL, NULL);
  else
    return g_object_new (ST_TYPE_LABEL,
                         "text", text,
                         NULL);
}

/**
 * st_label_get_text:
 * @label: a #StLabel
 *
 * Get the text displayed on the label.
 *
 * Returns: (transfer none): the text for the label. This must not be freed by
 * the application
 */
const gchar *
st_label_get_text (StLabel *label)
{
  g_return_val_if_fail (ST_IS_LABEL (label), NULL);

  return clutter_text_get_text (CLUTTER_TEXT (label->priv->label));
}

/**
 * st_label_set_text:
 * @label: a #StLabel
 * @text: (nullable): text to set the label to
 *
 * Sets the text displayed by the label.
 */
void
st_label_set_text (StLabel     *label,
                   const gchar *text)
{
  StLabelPrivate *priv;
  ClutterText *ctext;

  g_return_if_fail (ST_IS_LABEL (label));

  priv = label->priv;
  ctext = CLUTTER_TEXT (priv->label);

  if (clutter_text_get_editable (ctext) ||
      g_strcmp0 (clutter_text_get_text (ctext), text) != 0)
    {
      g_clear_pointer (&priv->text_shadow_pipeline, cogl_object_unref);

      clutter_text_set_text (ctext, text);

      g_object_notify_by_pspec (G_OBJECT (label), props[PROP_TEXT]);
    }
}

/**
 * st_label_get_clutter_text:
 * @label: a #StLabel
 *
 * Retrieve the internal #ClutterText used by @label so that extra parameters
 * can be set.
 *
 * Returns: (transfer none): the #ClutterText used by #StLabel. The actor
 * is owned by the #StLabel and should not be destroyed by the application.
 */
ClutterActor*
st_label_get_clutter_text (StLabel *label)
{
  g_return_val_if_fail (ST_LABEL (label), NULL);

  return label->priv->label;
}


/******************************************************************************/
/*************************** ACCESSIBILITY SUPPORT ****************************/
/******************************************************************************/

#define ST_TYPE_LABEL_ACCESSIBLE st_label_accessible_get_type ()

#define ST_LABEL_ACCESSIBLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  ST_TYPE_LABEL_ACCESSIBLE, StLabelAccessible))

#define ST_IS_LABEL_ACCESSIBLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  ST_TYPE_LABEL_ACCESSIBLE))

#define ST_LABEL_ACCESSIBLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  ST_TYPE_LABEL_ACCESSIBLE, StLabelAccessibleClass))

#define ST_IS_LABEL_ACCESSIBLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  ST_TYPE_LABEL_ACCESSIBLE))

#define ST_LABEL_ACCESSIBLE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  ST_TYPE_LABEL_ACCESSIBLE, StLabelAccessibleClass))

typedef struct _StLabelAccessible  StLabelAccessible;
typedef struct _StLabelAccessibleClass  StLabelAccessibleClass;

struct _StLabelAccessible
{
  StWidgetAccessible parent;
};

struct _StLabelAccessibleClass
{
  StWidgetAccessibleClass parent_class;
};

/* AtkObject */
static void          st_label_accessible_initialize (AtkObject *obj,
                                                     gpointer   data);
static const gchar * st_label_accessible_get_name   (AtkObject *obj);

G_DEFINE_TYPE (StLabelAccessible, st_label_accessible, ST_TYPE_WIDGET_ACCESSIBLE)

static void
st_label_accessible_class_init (StLabelAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->initialize = st_label_accessible_initialize;
  atk_class->get_name = st_label_accessible_get_name;
}

static void
st_label_accessible_init (StLabelAccessible *self)
{
  /* initialization done on AtkObject->initialize */
}

static void
label_text_notify_cb (StLabel    *label,
                      GParamSpec *pspec,
                      AtkObject  *accessible)
{
  g_object_notify (G_OBJECT (accessible), "accessible-name");
}

static void
st_label_accessible_initialize (AtkObject *obj,
                                gpointer   data)
{
  ATK_OBJECT_CLASS (st_label_accessible_parent_class)->initialize (obj, data);

  g_signal_connect (data, "notify::text",
                    G_CALLBACK (label_text_notify_cb),
                    obj);

  obj->role = ATK_ROLE_LABEL;
}

static const gchar *
st_label_accessible_get_name (AtkObject *obj)
{
  const gchar *name = NULL;

  g_return_val_if_fail (ST_IS_LABEL_ACCESSIBLE (obj), NULL);

  name = ATK_OBJECT_CLASS (st_label_accessible_parent_class)->get_name (obj);
  if (name == NULL)
    {
      ClutterActor *actor = NULL;

      actor = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));

      if (actor == NULL || st_widget_has_style_class_name (ST_WIDGET (actor), "hidden"))
        name = NULL;
      else
        name = st_label_get_text (ST_LABEL (actor));
    }

  return name;
}
