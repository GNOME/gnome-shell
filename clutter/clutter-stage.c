/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * SECTION:clutter-stage
 * @short_description: Top level visual element to which actors are placed.
 *
 * #ClutterStage is a top level 'window' on which child actors are placed
 * and manipulated.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend.h"
#include "clutter-stage.h"
#include "clutter-main.h"
#include "clutter-color.h"
#include "clutter-util.h"
#include "clutter-marshal.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-version.h" 	/* For flavour */

#include "cogl.h"

G_DEFINE_ABSTRACT_TYPE (ClutterStage, clutter_stage, CLUTTER_TYPE_GROUP);

#define CLUTTER_STAGE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_STAGE, ClutterStagePrivate))

struct _ClutterStagePrivate
{
  ClutterColor        color;
  ClutterPerspective  perspective;
  ClutterFog          fog;

  guint is_fullscreen     : 1;
  guint is_offscreen      : 1;
  guint is_cursor_visible : 1;
  guint is_user_resizable : 1;
  guint use_fog           : 1;

  gchar              *title;
  ClutterActor       *key_focused_actor;
};

enum
{
  PROP_0,

  PROP_COLOR,
  PROP_FULLSCREEN,
  PROP_OFFSCREEN,
  PROP_CURSOR_VISIBLE,
  PROP_PERSPECTIVE,
  PROP_TITLE,
  PROP_USER_RESIZE,
  PROP_USE_FOG
};

enum
{
  FULLSCREEN,
  UNFULLSCREEN,
  ACTIVATE,
  DEACTIVATE,
  LAST_SIGNAL
};

static guint stage_signals[LAST_SIGNAL] = { 0, };

static void
clutter_stage_paint (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  cogl_paint_init (&priv->color);

  if (priv->use_fog)
    {
      cogl_fog_set (&priv->color,
                    priv->fog.density,
                    priv->fog.z_near,
                    priv->fog.z_far);
    }

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->paint (self);
}

static void
clutter_stage_pick (ClutterActor       *self,
		    const ClutterColor *color)
{
  /* Paint nothing, cogl_paint_init() effectively paints the stage
   * silhouette for us - see _clutter_do_pick().
   * Chain up to the groups paint howerer so our children get picked
   * - clutter_group_pick
   */
  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->paint (self);
}

static void
clutter_stage_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;
  ClutterActor        *actor;

  stage = CLUTTER_STAGE (object);
  actor = CLUTTER_ACTOR (stage);
  priv = stage->priv;

  switch (prop_id)
    {
    case PROP_COLOR:
      clutter_stage_set_color (stage, g_value_get_boxed (value));
      break;
    case PROP_OFFSCREEN:
      if (priv->is_offscreen == g_value_get_boolean (value))
	return;

      if (CLUTTER_ACTOR_IS_REALIZED (actor))
        {
          /* Backend needs to check this prop and handle accordingly
           * in realise. 
           * FIXME: More 'obvious' implementation needed?
          */
          clutter_actor_unrealize (actor);
          priv->is_offscreen = g_value_get_boolean (value);
          clutter_actor_realize (actor);

	  if (!CLUTTER_ACTOR_IS_REALIZED (actor))
	    priv->is_offscreen = ~g_value_get_boolean (value);
        }
      else
        priv->is_offscreen = g_value_get_boolean (value);
      break;
    case PROP_FULLSCREEN:
      if (g_value_get_boolean (value))
        clutter_stage_fullscreen (stage);
      else
        clutter_stage_unfullscreen (stage);
      break;
    case PROP_CURSOR_VISIBLE:
      if (g_value_get_boolean (value))
        clutter_stage_show_cursor (stage);
      else
        clutter_stage_hide_cursor (stage);
      break;
    case PROP_PERSPECTIVE:
      clutter_stage_set_perspectivex (stage, g_value_get_boxed (value));
      break;
    case PROP_TITLE:
      clutter_stage_set_title (stage, g_value_get_string (value));
      break;
    case PROP_USER_RESIZE:
      clutter_stage_set_user_resizable (stage, g_value_get_boolean (value));
      break;
    case PROP_USE_FOG:
      clutter_stage_set_use_fog (stage, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_stage_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;
  ClutterColor         color;
  ClutterPerspective   perspective;

  stage = CLUTTER_STAGE(object);
  priv = stage->priv;

  switch (prop_id)
    {
    case PROP_COLOR:
      clutter_stage_get_color (stage, &color);
      g_value_set_boxed (value, &color);
      break;
    case PROP_OFFSCREEN:
      g_value_set_boolean (value, priv->is_offscreen);
      break;
    case PROP_FULLSCREEN:
      g_value_set_boolean (value, priv->is_fullscreen);
      break;
    case PROP_CURSOR_VISIBLE:
      g_value_set_boolean (value, priv->is_cursor_visible);
      break;
    case PROP_PERSPECTIVE:
      clutter_stage_get_perspectivex (stage, &perspective);
      g_value_set_boxed (value, &perspective);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_USER_RESIZE:
      g_value_set_boolean (value, priv->is_user_resizable);
      break;
    case PROP_USE_FOG:
      g_value_set_boolean (value, priv->use_fog);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_stage_class_init (ClutterStageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->set_property = clutter_stage_set_property;
  gobject_class->get_property = clutter_stage_get_property;

  actor_class->paint = clutter_stage_paint;
  actor_class->pick = clutter_stage_pick;

  /**
   * ClutterStage:fullscreen:
   *
   * Whether the stage should be fullscreen or not.
   */
  g_object_class_install_property
    (gobject_class, PROP_FULLSCREEN,
     g_param_spec_boolean ("fullscreen",
			   "Fullscreen",
			   "Whether the main stage is fullscreen",
			   FALSE,
			   G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));
  /**
   * ClutterStage:offscreen:
   *
   * Whether the stage should be rendered in an offscreen buffer.
   */
  g_object_class_install_property
    (gobject_class, PROP_OFFSCREEN,
     g_param_spec_boolean ("offscreen",
			   "Offscreen",
			   "Whether the main stage is renderer offscreen",
			   FALSE,
			   G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));
  /**
   * ClutterStage:cursor-visible:
   *
   * Whether the mouse pointer should be visible
   */
  g_object_class_install_property
    (gobject_class, PROP_CURSOR_VISIBLE,
     g_param_spec_boolean ("cursor-visible",
			   "Cursor Visible",
			   "Whether the mouse pointer is visible on the main stage ",
			   TRUE,
			   G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));
  /**
   * ClutterStage:user-resizable:
   *
   * Whether the stage is able to be resized via user interaction.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_USER_RESIZE,
     g_param_spec_boolean ("user-resizable",
			   "User Resizable",
			   "Whether the stage is able to be resized via "
			   "user interaction",
			   FALSE,
			   G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));
  /**
   * ClutterStage:color:
   *
   * The color of the main stage.
   */
  g_object_class_install_property
    (gobject_class, PROP_COLOR,
     g_param_spec_boxed ("color",
			 "Color",
			 "The color of the main stage",
			 CLUTTER_TYPE_COLOR,
			 CLUTTER_PARAM_READWRITE));
  /**
   * ClutterStage:title:
   *
   * The stages title - usually displayed in stage windows title decorations.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_TITLE,
     g_param_spec_string ("title",
			  "Title",
			  "Stage Title",
			  NULL,
			  CLUTTER_PARAM_READWRITE));
  /**
   * ClutterStage:use-fog:
   *
   * Whether the stage should use a linear GL "fog" for creating the
   * depth-cueing effect to enhance the perception of depth by fading
   * actors farther from the viewpoing.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_FOG,
                                   g_param_spec_boolean ("use-fog",
                                                         "Use Fog",
                                                         "Whether to enable depth cueing",
                                                         FALSE,
                                                         CLUTTER_PARAM_READWRITE));

  /**
   * ClutterStage::fullscreen
   * @stage: the stage which was fullscreened
   *
   * The ::fullscreen signal is emitted when the stage is made fullscreen.
   *
   * Since: 0.6
   */
  stage_signals[FULLSCREEN] =
    g_signal_new ("fullscreen",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, fullscreen),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::unfullscreen
   * @stage: the stage which has left a fullscreen state.
   *
   * The ::unfullscreen signal is emitted when the stage leaves a fullscreen
   * state.
   *
   * Since: 0.6
   */
  stage_signals[UNFULLSCREEN] =
    g_signal_new ("unfullscreen",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, unfullscreen),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::activate
   * @stage: the stage which was activated
   *
   * The ::activate signal is emitted when the stage recieves key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[ACTIVATE] =
    g_signal_new ("activate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, activate),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::deactivate
   * @stage: the stage which was deactivated
   *
   * The ::activate signal is emitted when the stage loses key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[DEACTIVATE] =
    g_signal_new ("deactivate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, deactivate),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ClutterStagePrivate));
}

static void
clutter_stage_init (ClutterStage *self)
{
  ClutterStagePrivate *priv;

  /* a stage is a top-level object */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IS_TOPLEVEL);

  self->priv = priv = CLUTTER_STAGE_GET_PRIVATE (self);

  priv->is_offscreen      = FALSE;
  priv->is_fullscreen     = FALSE;
  priv->is_user_resizable = FALSE;
  priv->is_cursor_visible = TRUE;
  priv->use_fog           = FALSE;

  priv->color.red   = 0xff;
  priv->color.green = 0xff;
  priv->color.blue  = 0xff;
  priv->color.alpha = 0xff;

  priv->perspective.fovy   = CFX_60; /* 60 Degrees */
  priv->perspective.aspect = CFX_ONE;
  priv->perspective.z_near = CLUTTER_FLOAT_TO_FIXED (0.1);
  priv->perspective.z_far  = CLUTTER_FLOAT_TO_FIXED (100.0);

  /* depth cueing */
  priv->fog.density = CLUTTER_FLOAT_TO_FIXED (0.1);
  priv->fog.z_near  = CLUTTER_FLOAT_TO_FIXED (1.0);
  priv->fog.z_far   = CLUTTER_FLOAT_TO_FIXED (2.0);

  clutter_actor_set_size (CLUTTER_ACTOR (self), 640, 480);
  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);
  clutter_stage_set_key_focus (self, NULL);
}

/**
 * clutter_stage_get_default:
 *
 * Returns the main stage.  #ClutterStage is a singleton, so
 * the stage will be created the first time this function is
 * called (typically, inside clutter_init()); all the subsequent
 * calls to clutter_stage_get_default() will return the same
 * instance.
 *
 * Return value: the main #ClutterStage.  You should never
 *   destroy or unref the returned actor.
 */
ClutterActor *
clutter_stage_get_default (void)
{
  ClutterMainContext *context;

  context = clutter_context_get_default ();
  g_assert (context != NULL);

  return _clutter_backend_get_stage (context->backend);
}

/**
 * clutter_stage_set_color
 * @stage: A #ClutterStage
 * @color: A #ClutterColor
 *
 * Set the stage color.
 **/
void
clutter_stage_set_color (ClutterStage       *stage,
			 const ClutterColor *color)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (color != NULL);

  priv = stage->priv;
  priv->color.red = color->red;
  priv->color.green = color->green;
  priv->color.blue = color->blue;
  priv->color.alpha = color->alpha;

  if (CLUTTER_ACTOR_IS_VISIBLE (stage))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  g_object_notify (G_OBJECT (stage), "color");
}

/**
 * clutter_stage_get_color
 * @stage: A #ClutterStage
 * @color: return location for a #ClutterColor
 *
 * Retrieves the stage color.
 */
void
clutter_stage_get_color (ClutterStage *stage,
			 ClutterColor *color)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (color != NULL);

  priv = stage->priv;

  color->red = priv->color.red;
  color->green = priv->color.green;
  color->blue = priv->color.blue;
  color->alpha = priv->color.alpha;
}

/**
 * clutter_stage_set_perspectivex
 * @stage: A #ClutterStage
 * @perspective: A #ClutterPerspective
 *
 * Set the stage perspective.
 **/
void
clutter_stage_set_perspectivex (ClutterStage       *stage,
				ClutterPerspective *perspective)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);

  priv = stage->priv;

  priv->perspective = *perspective;

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

/**
 * clutter_stage_get_perspectivex
 * @stage: A #ClutterStage
 * @perspective: return location for a #ClutterPerspective
 *
 * Retrieves the stage perspective.
 */
void
clutter_stage_get_perspectivex (ClutterStage       *stage,
				ClutterPerspective *perspective)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);

  *perspective = stage->priv->perspective;
}

/**
 * clutter_stage_set_perspective
 * @stage: A #ClutterStage
 * @fovy: the field of view angle, in degrees, in the y direction
 * @aspect: the aspect ratio that determines the field of view in the x
 *   direction. The aspect ratio is the ratio of x (width) to y (height)
 * @z_near: the distance from the viewer to the near clipping
 *   plane (always positive)
 * @z_far: the  distance from the viewer to the far clipping
 *   plane (always positive)
 *
 * Set the stage perspective.
 *
 * Since: 0.4
 */
void
clutter_stage_set_perspective (ClutterStage *stage,
                               gfloat        fovy,
                               gfloat        aspect,
                               gfloat        z_near,
                               gfloat        z_far)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  priv->perspective.fovy   = CLUTTER_FLOAT_TO_FIXED(fovy);
  priv->perspective.aspect = CLUTTER_FLOAT_TO_FIXED(aspect);
  priv->perspective.z_near = CLUTTER_FLOAT_TO_FIXED(z_near);
  priv->perspective.z_far  = CLUTTER_FLOAT_TO_FIXED(z_far);

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

/**
 * clutter_stage_get_perspective
 * @stage: A #ClutterStage
 * @fovy: return location for the field of view, in degrees, or %NULL
 * @aspect: return location for the aspect ratio, or %NULL
 * @z_near: return location for the distance of the viewer from the
 *   near clipping plane, or %NULL
 * @z_far: return location for the distance of the viewer from the
 *   far clipping plane, or %NULL
 *
 * Retrieves the stage perspective.
 *
 * Since: 0.4
 */
void
clutter_stage_get_perspective (ClutterStage       *stage,
			       gfloat             *fovy,
			       gfloat             *aspect,
			       gfloat             *z_near,
			       gfloat             *z_far)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (fovy)
    *fovy   = CLUTTER_FIXED_TO_FLOAT (priv->perspective.fovy);

  if (aspect)
    *aspect = CLUTTER_FIXED_TO_FLOAT (priv->perspective.aspect);

  if (z_near)
    *z_near = CLUTTER_FIXED_TO_FLOAT (priv->perspective.z_near);

  if (z_far)
    *z_far  = CLUTTER_FIXED_TO_FLOAT (priv->perspective.z_far);
}

/**
 * clutter_stage_fullscreen:
 * @stage: a #ClutterStage
 *
 * Asks to place the stage window in the fullscreen state. Note that you
 * shouldn't assume the window is definitely full screen afterward, because
 * other entities (e.g. the user or window manager) could unfullscreen it
 * again, and not all window managers honor requests to fullscreen windows.
 */
void
clutter_stage_fullscreen (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (!priv->is_fullscreen)
    {
      /* Only set if backend implements.
       * Also see clutter_stage_event() for setting priv->is_fullscreen
       * on state change event.
      */
      if (CLUTTER_STAGE_GET_CLASS (stage)->set_fullscreen)
	CLUTTER_STAGE_GET_CLASS (stage)->set_fullscreen (stage, TRUE);
    }
}

/**
 * clutter_stage_unfullscreen:
 * @stage: a #ClutterStage
 *
 * Asks to toggle off the fullscreen state for the stage window. Note that
 * you shouldn't assume the window is definitely not full screen afterward,
 * because other entities (e.g. the user or window manager) could fullscreen
 * it again, and not all window managers honor requests to unfullscreen
 * windows.
 */
void
clutter_stage_unfullscreen (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (priv->is_fullscreen)
    {
      /* Only set if backend implements */
      if (CLUTTER_STAGE_GET_CLASS (stage)->set_fullscreen)
	CLUTTER_STAGE_GET_CLASS (stage)->set_fullscreen (stage, FALSE);
    }
}

/**
 * clutter_stage_set_user_resizable:
 * @stage: a #ClutterStage
 * @resizable: whether if the stage should be user resizable.
 *
 * Sets if the stage is able to be resized by user interaction (i.e via
 * window manager controls)
 *
 * Since: 0.4
 */
void
clutter_stage_set_user_resizable (ClutterStage *stage,
                                  gboolean      resizable)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (clutter_feature_available (CLUTTER_FEATURE_STAGE_USER_RESIZE)
      && priv->is_user_resizable != resizable
      && CLUTTER_STAGE_GET_CLASS (stage)->set_user_resize)
    {
      priv->is_user_resizable = resizable;

      CLUTTER_STAGE_GET_CLASS (stage)->set_user_resize (stage, resizable);

      g_object_notify (G_OBJECT (stage), "user-resizable");
    }
}

/**
 * clutter_stage_get_user_resizable:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set with clutter_stage_set_user_resizable().
 *
 * Return value: %TRUE is the stage is resizable by the user.
 *
 * Since: 0.4
 */
gboolean
clutter_stage_get_user_resizable (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->is_user_resizable;
}

/**
 * clutter_stage_show_cursor:
 * @stage: a #ClutterStage
 *
 * Shows the cursor on the stage window
 */
void
clutter_stage_show_cursor (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (!priv->is_cursor_visible)
    {
      priv->is_cursor_visible = TRUE;

      if (CLUTTER_STAGE_GET_CLASS (stage)->set_cursor_visible)
        CLUTTER_STAGE_GET_CLASS (stage)->set_cursor_visible (stage, TRUE);

      g_object_notify (G_OBJECT (stage), "cursor-visible");
    }
}

/**
 * clutter_stage_hide_cursor:
 * @stage: a #ClutterStage
 *
 * Makes the cursor invisible on the stage window
 *
 * Since: 0.4
 */
void
clutter_stage_hide_cursor (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (priv->is_cursor_visible)
    {
      priv->is_cursor_visible = FALSE;

      if (CLUTTER_STAGE_GET_CLASS (stage)->set_cursor_visible)
	CLUTTER_STAGE_GET_CLASS (stage)->set_cursor_visible (stage, FALSE);

      g_object_notify (G_OBJECT (stage), "cursor-visible");
    }
}

/**
 * clutter_stage_snapshot
 * @stage: A #ClutterStage
 * @x: x coordinate of the first pixel that is read from stage
 * @y: y coordinate of the first pixel that is read from stage
 * @width: Width dimention of pixels to be read, or -1 for the
 *   entire stage width
 * @height: Height dimention of pixels to be read, or -1 for the
 *   entire stage height
 *
 * Gets a pixel based representation of the current rendered stage.
 *
 * Return value: pixel representation as a  #GdkPixbuf
 **/
GdkPixbuf *
clutter_stage_snapshot (ClutterStage *stage,
			gint          x,
			gint          y,
			gint          width,
			gint          height)
{
  ClutterStageClass *klass;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);
  g_return_val_if_fail (x >= 0 && y >= 0, NULL);

  klass = CLUTTER_STAGE_GET_CLASS (stage);
  if (klass->draw_to_pixbuf)
    return klass->draw_to_pixbuf (stage, x, y, width, height);

  return NULL;
}

/**
 * clutter_stage_get_actor_at_pos:
 * @stage: a #ClutterStage
 * @x: X coordinate to check
 * @y: Y coordinate to check
 *
 * Checks the scene at the coordinates @x and @y and returns a pointer
 * to the #ClutterActor at those coordinates.
 *
 * Return value: the actor at the specified coordinates, if any
 */
ClutterActor *
clutter_stage_get_actor_at_pos (ClutterStage *stage,
                                gint          x,
                                gint          y)
{
  return _clutter_do_pick (stage, x, y, CLUTTER_PICK_ALL);
}

/**
 * clutter_stage_event:
 * @stage: a #ClutterStage
 * @event: a #ClutterEvent
 *
 * This function is used to emit an event on the main stage.
 * You should rarely need to use this function, except for
 * synthetising events.
 *
 * Return value: the return value from the signal emission
 *
 * Since: 0.4
 */
gboolean
clutter_stage_event (ClutterStage *stage,
                     ClutterEvent *event)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  priv = stage->priv;

  if (event->type == CLUTTER_DELETE)
    {
      gboolean retval = FALSE;

      g_signal_emit_by_name (stage, "event", event, &retval);

      return retval;
    }

  if (event->type != CLUTTER_STAGE_STATE)
    return FALSE;

  /* emit raw event */
  if (clutter_actor_event (CLUTTER_ACTOR (stage), event, FALSE))
    return TRUE;

  if (event->stage_state.changed_mask & CLUTTER_STAGE_STATE_FULLSCREEN)
    {
      if (event->stage_state.new_state & CLUTTER_STAGE_STATE_FULLSCREEN)
	{
	  priv->is_fullscreen = TRUE;
	  g_signal_emit (stage, stage_signals[FULLSCREEN], 0);

          g_object_notify (G_OBJECT (stage), "fullscreen");
	}
      else
	{
	  priv->is_fullscreen = FALSE;
	  g_signal_emit (stage, stage_signals[UNFULLSCREEN], 0);

          g_object_notify (G_OBJECT (stage), "fullscreen");
	}
    }

  if (event->stage_state.changed_mask & CLUTTER_STAGE_STATE_ACTIVATED)
    {
      if (event->stage_state.new_state & CLUTTER_STAGE_STATE_ACTIVATED)
	g_signal_emit (stage, stage_signals[ACTIVATE], 0);
      else
	g_signal_emit (stage, stage_signals[DEACTIVATE], 0);
    }

  return TRUE;
}

/**
 * clutter_stage_set_title
 * @stage: A #ClutterStage
 * @title: A utf8 string for the stage windows title.
 *
 * Sets the stage title.
 *
 * Since 0.4
 **/
void
clutter_stage_set_title (ClutterStage       *stage,
			 const gchar        *title)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  g_free (priv->title);
  priv->title = g_strdup (title);

  if (CLUTTER_STAGE_GET_CLASS (stage)->set_title)
    CLUTTER_STAGE_GET_CLASS (stage)->set_title (stage, priv->title);

  g_object_notify (G_OBJECT (stage), "title");
}

/**
 * clutter_stage_get_title
 * @stage: A #ClutterStage
 *
 * Gets the stage title.
 *
 * Return value: pointer to the title string for the stage. The
 * returned string is owned by the actor and should not
 * be modified or freed.
 *
 * Since: 0.4
 **/
G_CONST_RETURN gchar *
clutter_stage_get_title (ClutterStage       *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return stage->priv->title;
}

static void
on_key_focused_weak_notify (gpointer data,
			    GObject *where_the_object_was)
{
  ClutterStagePrivate *priv;
  ClutterStage        *stage = CLUTTER_STAGE (data);

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  priv->key_focused_actor = NULL;

  /* focused actor has dissapeared - fall back to stage
   * FIXME: need some kind of signal dance/block here.
  */
  clutter_stage_set_key_focus (stage, NULL);
}

/**
 * clutter_stage_set_key_focus:
 * @stage: the #ClutterStage
 * @actor: the actor to set key focus to, or %NULL
 *
 * Sets the key focus on @actor. An actor with key focus will receive
 * all the key events. If @actor is %NULL, the stage will receive
 * focus.
 *
 * Since: 0.6
 */
void
clutter_stage_set_key_focus (ClutterStage *stage,
			     ClutterActor *actor)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  priv = stage->priv;

  if (priv->key_focused_actor == actor)
    return;

  if (priv->key_focused_actor)
    {
      g_object_weak_unref (G_OBJECT (priv->key_focused_actor),
			   on_key_focused_weak_notify,
			   stage);
      g_signal_emit_by_name (priv->key_focused_actor, "focus-out");

      priv->key_focused_actor = NULL;
    }
  else
    g_signal_emit_by_name (stage, "focus-out");

  if (actor)
    {
      priv->key_focused_actor = actor;

      g_object_weak_ref (G_OBJECT (actor),
			 on_key_focused_weak_notify,
			 stage);
      g_signal_emit_by_name (priv->key_focused_actor, "focus-in");
    }
  else
    g_signal_emit_by_name (stage, "focus-in");
}

/**
 * clutter_stage_get_key_focus:
 * @stage: the #ClutterStage
 *
 * Retrieves the actor that is currently under key focus.
 *
 * Return value: the actor with key focus, or the stage
 *
 * Since: 0.6
 */
ClutterActor *
clutter_stage_get_key_focus (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  if (stage->priv->key_focused_actor)
    return stage->priv->key_focused_actor;

  return CLUTTER_ACTOR (stage);
}

/**
 * clutter_stage_get_use_fog:
 * @stage: the #ClutterStage
 *
 * Gets whether the depth cueing effect is enabled on @stage.
 *
 * Return value: %TRUE if the the depth cueing effect is enabled
 *
 * Since: 0.6
 */
gboolean
clutter_stage_get_use_fog (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->use_fog;
}

/**
 * clutter_stage_set_use_fog:
 * @stage: the #ClutterStage
 * @fog: %TRUE for enabling the depth cueing effect
 *
 * Sets whether the depth cueing effect on the stage should be enabled
 * or not.
 *
 * Depth cueing is a 3D effect that makes actors farther away from the
 * viewing point less opaque, by fading them with the stage color.

 * The parameters of the GL fog used can be changed using the
 * clutter_stage_set_fog() function.
 *
 * Since: 0.6
 */
void
clutter_stage_set_use_fog (ClutterStage *stage,
                           gboolean      fog)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->use_fog != fog)
    {
      priv->use_fog = fog;

      CLUTTER_NOTE (MISC, "%s depth-cueing inside stage",
                    priv->use_fog ? "enabling" : "disabling");

      if (CLUTTER_ACTOR_IS_VISIBLE (stage))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

      g_object_notify (G_OBJECT (stage), "use-fog");
    }
}

/**
 * clutter_stage_get_fog:
 * @stage: a #ClutterStage
 * @density: return location for the intensity dampening
 * @z_near: return location for the starting point of the depth cueing
 * @z_far: return location for the ending point of the depth cueing
 *
 * Retrieves the settings used by the GL fog to create the
 * depth cueing effect on the @stage.
 *
 * Since: 0.6
 */
void
clutter_stage_get_fog (ClutterStage *stage,
                       gdouble      *density,
                       gdouble      *z_near,
                       gdouble      *z_far)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (density)
    *density = CLUTTER_FIXED_TO_FLOAT (priv->fog.density);
  if (z_near)
    *z_near = CLUTTER_FIXED_TO_FLOAT (priv->fog.z_near);
  if (z_far)
    *z_far = CLUTTER_FIXED_TO_FLOAT (priv->fog.z_far);
}

/**
 * clutter_stage_set_fog:
 * @stage: the #ClutterStage
 * @density: density of the intensity dampening
 * @z_near: starting point of the depth cueing
 * @z_far: ending point of the depth cueing
 *
 * Sets the GL fog settings used to create the depth cueing effect
 * on the @stage.
 *
 * If the actors are all near the view point you will need a higher @density
 * and a smaller interval between @z_near and @z_far. On the other hand, if
 * actors are placed far away from the view point you will need a lower
 * @density but a bigger interval between @z_near and @z_far.
 *
 * Since: 0.6
 */
void
clutter_stage_set_fog (ClutterStage *stage,
                       gdouble       density,
                       gdouble       z_near,
                       gdouble       z_far)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  priv->fog.density = CLUTTER_FLOAT_TO_FIXED (density);
  priv->fog.z_near  = CLUTTER_FLOAT_TO_FIXED (z_near);
  priv->fog.z_far   = CLUTTER_FLOAT_TO_FIXED (z_far);

  if (priv->use_fog && CLUTTER_ACTOR_IS_VISIBLE (stage))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_set_fogx:
 * @stage: the #ClutterStage
 * @fog: a #ClutterFog structure
 *
 * Sets the depth cueing settings for the @stage. This is the fixed point
 * version of clutter_stage_set_fog().
 *
 * Since: 0.6
 */
void
clutter_stage_set_fogx (ClutterStage *stage,
                        ClutterFog   *fog)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (fog != NULL);

  priv = stage->priv;

  priv->fog = *fog;

  if (priv->use_fog && CLUTTER_ACTOR_IS_VISIBLE (stage))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_get_fogx:
 * @stage: the #ClutterStage
 * @fog: return location for a #ClutterFog structure
 *
 * Retrieves the current depth cueing settings from the stage. This is the
 * fixed point version of clutter_stage_get_fog().
 *
 * Since: 0.6
 */
void
clutter_stage_get_fogx (ClutterStage *stage,
                        ClutterFog   *fog)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (fog != NULL);

  *fog = stage->priv->fog;
}

/**
 * clutter_stage_get_resolution:
 * @stage: the #ClutterStage
 *
 * Retrieves the resolution (in DPI) of the stage from the default
 * backend.
 *
 * Return value: the resolution of the stage
 *
 * Since: 0.6
 */
gdouble
clutter_stage_get_resolution (ClutterStage *stage)
{
  ClutterMainContext *context;

  context = clutter_context_get_default ();
  g_assert (context != NULL);

  return clutter_backend_get_resolution (context->backend);
}

/**
 * clutter_stage_get_resolutionx:
 * @stage: the #ClutterStage
 *
 * Fixed point version of clutter_stage_get_resolution().
 *
 * Return value: the resolution of the stage
 *
 * Since: 0.6
 */
ClutterFixed
clutter_stage_get_resolutionx (ClutterStage *stage)
{
  ClutterFixed res;
  ClutterMainContext *context;

  context = clutter_context_get_default ();
  g_assert (context != NULL);

  res = clutter_backend_get_resolution (context->backend);

  return CLUTTER_FLOAT_TO_FIXED (res);
}

/*** Perspective boxed type ******/

static ClutterPerspective *
clutter_perspective_copy (const ClutterPerspective *perspective)
{
  ClutterPerspective *result;

  g_return_val_if_fail (perspective != NULL, NULL);

  result = g_slice_new (ClutterPerspective);
  *result = *perspective;

  return result;
}

static void
clutter_perspective_free (ClutterPerspective *perspective)
{
  if (G_LIKELY (perspective))
    g_slice_free (ClutterPerspective, perspective);
}

GType
clutter_perspective_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    our_type =
      g_boxed_type_register_static (I_("ClutterPerspective"),
                                    (GBoxedCopyFunc) clutter_perspective_copy,
                                    (GBoxedFreeFunc) clutter_perspective_free);
  return our_type;
}

static ClutterFog *
clutter_fog_copy (const ClutterFog *fog)
{
  ClutterFog *copy;

  g_return_val_if_fail (fog != NULL, NULL);

  copy = g_slice_new0 (ClutterFog);
  *copy = *fog;

  return copy;
}

static void
clutter_fog_free (ClutterFog *fog)
{
  if (G_LIKELY (fog))
    g_slice_free (ClutterFog, fog);
}

GType
clutter_fog_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("ClutterFog"),
                                    (GBoxedCopyFunc) clutter_fog_copy,
                                    (GBoxedFreeFunc) clutter_fog_free);

  return our_type;
}
