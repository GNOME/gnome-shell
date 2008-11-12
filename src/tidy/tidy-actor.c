/* tidy-actor.c: Base class for Tidy actors
 *
 * Copyright (C) 2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:tidy-actor
 * @short_description: Base class for stylable actors
 *
 * #TidyActor is a simple abstract class on top of #ClutterActor. It
 * provides basic themeing properties, support for padding and alignment.
 *
 * Actors in the Tidy library should subclass #TidyActor if they plan
 * to obey to a certain #TidyStyle or if they implement #ClutterContainer
 * and want to offer basic layout capabilities.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "tidy-actor.h"

#include "tidy-debug.h"
#include "tidy-marshal.h"
#include "tidy-private.h"
#include "tidy-stylable.h"

enum
{
  PROP_0,

  PROP_STYLE,
  PROP_PADDING,
  PROP_X_ALIGN,
  PROP_Y_ALIGN
};

enum
{
  LAST_SIGNAL
};

static void tidy_stylable_iface_init (TidyStylableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TidyActor, tidy_actor, CLUTTER_TYPE_ACTOR,
                                  G_IMPLEMENT_INTERFACE (TIDY_TYPE_STYLABLE,
                                                         tidy_stylable_iface_init));

#define TIDY_ACTOR_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_ACTOR, TidyActorPrivate))

struct _TidyActorPrivate
{
  TidyStyle *style;

  TidyPadding padding;

  ClutterFixed x_align;
  ClutterFixed y_align;
};

static void
tidy_actor_set_property (GObject      *gobject,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  TidyActor *actor = TIDY_ACTOR (gobject);

  switch (prop_id)
    {
    case PROP_PADDING:
      tidy_actor_set_padding (actor, g_value_get_boxed (value));
      break;

    case PROP_X_ALIGN:
      actor->priv->x_align =
        CLUTTER_FIXED_TO_FLOAT (g_value_get_double (value));
      break;

    case PROP_Y_ALIGN:
      actor->priv->y_align =
        CLUTTER_FIXED_TO_FLOAT (g_value_get_double (value));
      break;

    case PROP_STYLE:
      tidy_stylable_set_style (TIDY_STYLABLE (actor),
                               g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_actor_get_property (GObject    *gobject,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  TidyActor *actor = TIDY_ACTOR (gobject);
  TidyActorPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_PADDING:
      {
        TidyPadding padding = { 0, };

        tidy_actor_get_padding (actor, &padding);
        g_value_set_boxed (value, &padding);
      }
      break;

    case PROP_X_ALIGN:
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->x_align));
      break;

    case PROP_Y_ALIGN:
      g_value_set_double (value, CLUTTER_FIXED_TO_FLOAT (priv->y_align));
      break;

    case PROP_STYLE:
      g_value_set_object (value, priv->style);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_actor_dispose (GObject *gobject)
{
  TidyActor *actor = TIDY_ACTOR (gobject);

  if (actor->priv->style)
    {
      g_object_unref (actor->priv->style);
      actor->priv->style = NULL;
    }

  G_OBJECT_CLASS (tidy_actor_parent_class)->dispose (gobject);
}

static void
tidy_actor_class_init (TidyActorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyActorPrivate));

  gobject_class->set_property = tidy_actor_set_property;
  gobject_class->get_property = tidy_actor_get_property;
  gobject_class->dispose = tidy_actor_dispose;

  /**
   * TidyActor:padding:
   *
   * Padding around an actor, expressed in #ClutterUnit<!-- -->s. Padding
   * is the internal space between an actors bounding box and its internal
   * children.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PADDING,
                                   g_param_spec_boxed ("padding",
                                                       "Padding",
                                                       "Units of padding around an actor",
                                                       TIDY_TYPE_PADDING,
                                                       TIDY_PARAM_READWRITE));
  /**
   * TidyActor:x-align:
   *
   * Alignment of internal children along the X axis, relative to the
   * actor's bounding box origin, and in relative units (1.0 is the
   * current width of the actor).
   *
   * A value of 0.0 will left-align the children; 0.5 will align them at
   * the middle of the actor's width; 1.0 will right align the children.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_X_ALIGN,
                                   g_param_spec_double ("x-align",
                                                        "X Alignment",
                                                        "Alignment (between 0.0 and 1.0) on the X axis",
                                                        0.0, 1.0, 0.5,
                                                        TIDY_PARAM_READWRITE));
  /**
   * TidyActor:y-align:
   *
   * Alignment of internal children along the Y axis, relative to the
   * actor's bounding box origin, and in relative units (1.0 is the
   * current height of the actor).
   *
   * A value of 0.0 will top-align the children; 0.5 will align them at
   * the middle of the actor's height; 1.0 will bottom align the children.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_Y_ALIGN,
                                   g_param_spec_double ("y-align",
                                                        "Y Alignement",
                                                        "Alignment (between 0.0 and 1.0) on the Y axis",
                                                        0.0, 1.0, 0.5,
                                                        TIDY_PARAM_READWRITE));
  g_object_class_override_property (gobject_class, PROP_STYLE, "style");
}

static TidyStyle *
tidy_actor_get_style (TidyStylable *stylable)
{
  TidyActorPrivate *priv = TIDY_ACTOR (stylable)->priv;

  if (!priv->style)
    priv->style = g_object_ref (tidy_style_get_default ());

  return priv->style;
}

static void
tidy_actor_set_style (TidyStylable *stylable,
                      TidyStyle    *style)
{
  TidyActorPrivate *priv = TIDY_ACTOR (stylable)->priv;

  if (priv->style)
    g_object_unref (priv->style);

  priv->style = g_object_ref_sink (style);
}

static void
tidy_stylable_iface_init (TidyStylableIface *iface)
{
  static gboolean is_initialized = FALSE;

  if (!is_initialized)
    {
      GParamSpec *pspec;

      pspec = g_param_spec_string ("font-name",
                                   "Font Name",
                                   "The font to use for displaying text",
                                   "Sans 12px",
                                   G_PARAM_READWRITE);
      tidy_stylable_iface_install_property (iface, TIDY_TYPE_ACTOR, pspec);

      pspec = g_param_spec_boxed ("bg-color",
                                  "Background Color",
                                  "The background color of an actor",
                                  CLUTTER_TYPE_COLOR,
                                  G_PARAM_READWRITE);
      tidy_stylable_iface_install_property (iface, TIDY_TYPE_ACTOR, pspec);

      pspec = g_param_spec_boxed ("active-color",
                                  "Active Color",
                                  "The color of an active actor",
                                  CLUTTER_TYPE_COLOR,
                                  G_PARAM_READWRITE);
      tidy_stylable_iface_install_property (iface, TIDY_TYPE_ACTOR, pspec);

      pspec = g_param_spec_boxed ("text-color",
                                  "Text Color",
                                  "The color of the text of an actor",
                                  CLUTTER_TYPE_COLOR,
                                  G_PARAM_READWRITE);
      tidy_stylable_iface_install_property (iface, TIDY_TYPE_ACTOR, pspec);

      iface->get_style = tidy_actor_get_style;
      iface->set_style = tidy_actor_set_style;
    }
}

static void
tidy_actor_init (TidyActor *actor)
{
  TidyActorPrivate *priv;

  actor->priv = priv = TIDY_ACTOR_GET_PRIVATE (actor);

  /* no padding */
  priv->padding.top = priv->padding.bottom = 0;
  priv->padding.right = priv->padding.left = 0;

  /* middle align */
  priv->x_align = priv->y_align = CLUTTER_FLOAT_TO_FIXED (0.5);

  clutter_actor_set_reactive (CLUTTER_ACTOR (actor), TRUE);
}

/**
 * tidy_actor_set_padding:
 * @actor: a #TidyActor
 * @padding: padding for internal children
 *
 * Sets @padding around @actor.
 */
void
tidy_actor_set_padding (TidyActor         *actor,
                        const TidyPadding *padding)
{
  g_return_if_fail (TIDY_IS_ACTOR (actor));
  g_return_if_fail (padding != NULL);

  actor->priv->padding = *padding;

  g_object_notify (G_OBJECT (actor), "padding");

  if (CLUTTER_ACTOR_IS_VISIBLE (actor))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (actor));
}

/**
 * tidy_actor_get_padding:
 * @actor: a #TidyActor
 * @padding: return location for the padding
 *
 * Retrieves the padding aound @actor.
 */
void
tidy_actor_get_padding (TidyActor   *actor,
                        TidyPadding *padding)
{
  g_return_if_fail (TIDY_IS_ACTOR (actor));
  g_return_if_fail (padding != NULL);

  *padding = actor->priv->padding;
}

/**
 * tidy_actor_set_alignment:
 * @actor: a #TidyActor
 * @x_align: relative alignment on the X axis
 * @y_align: relative alignment on the Y axis
 *
 * Sets the alignment, relative to the @actor's width and height, of
 * the internal children.
 */
void
tidy_actor_set_alignment (TidyActor *actor,
                          gdouble    x_align,
                          gdouble    y_align)
{
  TidyActorPrivate *priv;

  g_return_if_fail (TIDY_IS_ACTOR (actor));

  g_object_ref (actor);
  g_object_freeze_notify (G_OBJECT (actor));

  priv = actor->priv;

  x_align = CLAMP (x_align, 0.0, 1.0);
  y_align = CLAMP (y_align, 0.0, 1.0);

  priv->x_align = CLUTTER_FLOAT_TO_FIXED (x_align);
  g_object_notify (G_OBJECT (actor), "x-align");
  
  priv->y_align = CLUTTER_FLOAT_TO_FIXED (y_align);
  g_object_notify (G_OBJECT (actor), "y-align");

  if (CLUTTER_ACTOR_IS_VISIBLE (actor))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (actor));

  g_object_thaw_notify (G_OBJECT (actor));
  g_object_unref (actor);
}

/**
 * tidy_actor_get_alignment:
 * @actor: a #TidyActor
 * @x_align: return location for the relative alignment on the X axis,
 *   or %NULL
 * @y_align: return location for the relative alignment on the Y axis,
 *   or %NULL
 *
 * Retrieves the alignment, relative to the @actor's width and height, of
 * the internal children.
 */
void
tidy_actor_get_alignment (TidyActor *actor,
                          gdouble   *x_align,
                          gdouble   *y_align)
{
  TidyActorPrivate *priv;

  g_return_if_fail (TIDY_IS_ACTOR (actor));

  priv = actor->priv;

  if (x_align)
    *x_align = CLUTTER_FIXED_TO_FLOAT (priv->x_align);

  if (y_align)
    *y_align = CLUTTER_FIXED_TO_FLOAT (priv->y_align);
}

/**
 * tidy_actor_set_alignmentx:
 * @actor: a #TidyActor
 * @x_align: relative alignment on the X axis
 * @y_align: relative alignment on the Y axis
 *
 * Fixed point version of tidy_actor_set_alignment().
 *
 * Sets the alignment, relative to the @actor's width and height, of
 * the internal children.
 */
void
tidy_actor_set_alignmentx (TidyActor    *actor,
                           ClutterFixed  x_align,
                           ClutterFixed  y_align)
{
  TidyActorPrivate *priv;

  g_return_if_fail (TIDY_IS_ACTOR (actor));

  g_object_ref (actor);
  g_object_freeze_notify (G_OBJECT (actor));

  priv = actor->priv;

  x_align = CLAMP (x_align, 0, CFX_ONE);
  y_align = CLAMP (y_align, 0, CFX_ONE);

  if (priv->x_align != x_align)
    {
      priv->x_align = x_align;
      g_object_notify (G_OBJECT (actor), "x-align");
    }

  if (priv->y_align != y_align)
    {
      priv->y_align = y_align;
      g_object_notify (G_OBJECT (actor), "y-align");
    }

  if (CLUTTER_ACTOR_IS_VISIBLE (actor))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (actor));

  g_object_thaw_notify (G_OBJECT (actor));
  g_object_unref (actor);
}

/**
 * tidy_actor_get_alignmentx:
 * @actor: a #TidyActor
 * @x_align: return location for the relative alignment on the X axis,
 *   or %NULL
 * @y_align: return location for the relative alignment on the Y axis,
 *   or %NULL
 *
 * Fixed point version of tidy_actor_get_alignment().
 *
 * Retrieves the alignment, relative to the @actor's width and height, of
 * the internal children.
 */
void
tidy_actor_get_alignmentx (TidyActor    *actor,
                           ClutterFixed *x_align,
                           ClutterFixed *y_align)
{
  TidyActorPrivate *priv;

  g_return_if_fail (TIDY_IS_ACTOR (actor));

  priv = actor->priv;

  if (x_align)
    *x_align = priv->x_align;

  if (y_align)
    *y_align = priv->y_align;
}

static TidyPadding *
tidy_padding_copy (const TidyPadding *padding)
{
  TidyPadding *copy;

  g_return_val_if_fail (padding != NULL, NULL);

  copy = g_slice_new (TidyPadding);
  *copy = *padding;

  return copy;
}

static void
tidy_padding_free (TidyPadding *padding)
{
  if (G_LIKELY (padding))
    g_slice_free (TidyPadding, padding);
}

GType
tidy_padding_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("TidyPadding"),
                                    (GBoxedCopyFunc) tidy_padding_copy,
                                    (GBoxedFreeFunc) tidy_padding_free);

  return our_type;
}
