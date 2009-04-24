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
 *
 * Clutter creates a default stage upon initialization, which can be retrieved
 * using clutter_stage_get_default(). Clutter always provides the default
 * stage, unless the backend is unable to create one. The stage returned
 * by clutter_stage_get_default() is guaranteed to always be the same.
 *
 * Backends might provide support for multiple stages. The support for this
 * feature can be checked at run-time using the clutter_feature_available()
 * function and the %CLUTTER_FEATURE_STAGE_MULTIPLE flag. If the backend used
 * supports multiple stages, new #ClutterStage instances can be created
 * using clutter_stage_new(). These stages must be managed by the developer
 * using clutter_actor_destroy(), which will take care of destroying all the
 * actors contained inside them.
 *
 * #ClutterStage is a proxy actor, wrapping the backend-specific
 * implementation of the windowing system. It is possible to subclass
 * #ClutterStage, as long as every overridden virtual function chains up to
 * the parent class corresponding function.
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
#include "clutter-stage-manager.h"
#include "clutter-stage-window.h"
#include "clutter-version.h" 	/* For flavour */
#include "clutter-id-pool.h"

#include "cogl/cogl.h"

G_DEFINE_TYPE (ClutterStage, clutter_stage, CLUTTER_TYPE_GROUP);

#define CLUTTER_STAGE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_STAGE, ClutterStagePrivate))

struct _ClutterStagePrivate
{
  /* the stage implementation */
  ClutterActor       *impl;

  ClutterColor        color;
  ClutterPerspective  perspective;
  ClutterFog          fog;

  gchar              *title;
  ClutterActor       *key_focused_actor;

  guint               update_idle;	       /* repaint idler id */

  guint is_fullscreen     : 1;
  guint is_offscreen      : 1;
  guint is_cursor_visible : 1;
  guint is_user_resizable : 1;
  guint use_fog           : 1;
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
  PROP_USE_FOG,
  PROP_FOG
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

static const ClutterColor default_stage_color = { 255, 255, 255, 255 };

static void
clutter_stage_get_preferred_width (ClutterActor *self,
                                   ClutterUnit   for_height,
                                   ClutterUnit  *min_width_p,
                                   ClutterUnit  *natural_width_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);

  CLUTTER_ACTOR_GET_CLASS (priv->impl)->get_preferred_width (priv->impl,
                                                             for_height,
                                                             min_width_p,
                                                             natural_width_p);
}

static void
clutter_stage_get_preferred_height (ClutterActor *self,
                                    ClutterUnit   for_width,
                                    ClutterUnit  *min_height_p,
                                    ClutterUnit  *natural_height_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);

  CLUTTER_ACTOR_GET_CLASS (priv->impl)->get_preferred_height (priv->impl,
                                                              for_width,
                                                              min_height_p,
                                                              natural_height_p);
}
static void
clutter_stage_allocate (ClutterActor          *self,
                        const ClutterActorBox *box,
                        gboolean               origin_changed)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);

  /* if the stage is fixed size (for instance, it's using a frame-buffer)
   * then we simply ignore any allocation request and override the
   * allocation chain.
   */
  if (G_LIKELY (!clutter_feature_available (CLUTTER_FEATURE_STAGE_STATIC)))
    {
      ClutterActorClass *klass;

      CLUTTER_NOTE (ACTOR, "Following allocation to %dx%d (origin %s)",
                    CLUTTER_UNITS_TO_DEVICE (box->x2 - box->x1),
                    CLUTTER_UNITS_TO_DEVICE (box->y2 - box->y1),
                    origin_changed ? "changed" : "not changed");

      klass = CLUTTER_ACTOR_CLASS (clutter_stage_parent_class);
      klass->allocate (self, box, origin_changed);

      klass = CLUTTER_ACTOR_GET_CLASS (priv->impl);
      klass->allocate (priv->impl, box, origin_changed);
    }
  else
    {
      ClutterActorBox override = { 0, };
      ClutterActorClass *klass;
      ClutterUnit natural_width, natural_height;

      /* propagate the allocation */
      klass = CLUTTER_ACTOR_GET_CLASS (priv->impl);
      klass->allocate (self, box, origin_changed);

      /* get the preferred size from the backend */
      clutter_actor_get_preferred_size (priv->impl,
                                        NULL, NULL,
                                        &natural_width, &natural_height);

      override.x1 = 0;
      override.y1 = 0;
      override.x2 = natural_width;
      override.y2 = natural_height;

      /* and store the overridden allocation */
      klass = CLUTTER_ACTOR_CLASS (clutter_stage_parent_class);
      klass->allocate (self, &override, origin_changed);
    }
}

static void
clutter_stage_paint (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  CoglColor stage_color;

  CLUTTER_NOTE (PAINT, "Initializing stage paint");

  cogl_color_set_from_4ub (&stage_color,
                           priv->color.red,
                           priv->color.green,
                           priv->color.blue,
                           priv->color.alpha);
  cogl_clear (&stage_color,
	      COGL_BUFFER_BIT_COLOR |
	      COGL_BUFFER_BIT_DEPTH |
	      COGL_BUFFER_BIT_STENCIL);

  if (priv->use_fog)
    {
      /* we only expose the linear progression of the fog in
       * the ClutterStage API, and that ignores the fog density.
       * thus, we pass 1.0 as the density parameter
       */
      cogl_set_fog (&stage_color,
                    COGL_FOG_MODE_LINEAR,
                    1.0,
                    priv->fog.z_near,
                    priv->fog.z_far);
    }
  else
    cogl_disable_fog ();

  CLUTTER_NOTE (PAINT, "Proxying the paint to the stage implementation");
  clutter_actor_paint (priv->impl);

  /* this will take care of painting every child */
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
clutter_stage_realize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_REALIZED);

  /* Make sure the viewport and projection matrix are valid for the
     first paint (which will likely occur before the ConfigureNotify
     is received) */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_SYNC_MATRICES);

  g_assert (priv->impl != NULL);
  CLUTTER_ACTOR_GET_CLASS (priv->impl)->realize (priv->impl);

  /* ensure that the stage is using the context if the
   * realization sequence was successful
   */
  if (CLUTTER_ACTOR_IS_REALIZED (priv->impl))
    clutter_stage_ensure_current (CLUTTER_STAGE (self));
  else
    CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
}

static void
clutter_stage_unrealize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  /* unset the flag */
  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);

  /* and then unrealize the implementation */
  g_assert (priv->impl != NULL);
  CLUTTER_ACTOR_GET_CLASS (priv->impl)->unrealize (priv->impl);

  clutter_stage_ensure_current (CLUTTER_STAGE (self));
}

static void
clutter_stage_show (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);

  if (!CLUTTER_ACTOR_IS_REALIZED (priv->impl))
    clutter_actor_realize (priv->impl);

  clutter_actor_show (priv->impl);

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->show (self);
}

static void
clutter_stage_hide (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);
  clutter_actor_hide (priv->impl);

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->hide (self);
}

static void
clutter_stage_real_fullscreen (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;
  ClutterUnit natural_width, natural_height;
  ClutterActorBox box;

  /* we need to force an allocation here because the size
   * of the stage might have been changed by the backend
   *
   * this is a really bad solution to the issues caused by
   * the fact that fullscreening the stage on the X11 backends
   * is really an asynchronous operation
   */
  clutter_actor_get_preferred_size (CLUTTER_ACTOR (priv->impl),
                                    NULL, NULL,
                                    &natural_width, &natural_height);

  box.x1 = 0;
  box.y1 = 0;
  box.x2 = natural_width;
  box.y2 = natural_height;

  clutter_actor_allocate (CLUTTER_ACTOR (stage), &box, FALSE);
}

static gboolean
redraw_update_idle (gpointer user_data)
{
  ClutterStage *stage = user_data;
  ClutterStagePrivate *priv = stage->priv;

  /* clutter_redraw() will also call maybe_relayout(), but since a relayout can
   * queue a redraw, we want to do the relayout before we clear the update_idle
   * to avoid painting the stage twice. Calling maybe_relayout() twice in a row
   * is cheap because of caching of requested and allocated size.
   */
  _clutter_stage_maybe_relayout (CLUTTER_ACTOR (stage));

  if (priv->update_idle)
    {
      g_source_remove (priv->update_idle);
      priv->update_idle = 0;
    }

  CLUTTER_NOTE (MULTISTAGE, "redrawing via idle for stage:%p", stage);
  clutter_redraw (stage);

  return FALSE;
}

static void
clutter_stage_real_queue_redraw (ClutterActor *actor,
                                 ClutterActor *leaf)
{
  ClutterStage *stage = CLUTTER_STAGE (actor);
  ClutterStagePrivate *priv = stage->priv;

  if (priv->update_idle == 0)
    {
      CLUTTER_TIMESTAMP (SCHEDULER, "Adding idle source for stage: %p", stage);

      /* FIXME: weak_ref self in case we dissapear before paint? */
      priv->update_idle =
        clutter_threads_add_idle_full (CLUTTER_PRIORITY_REDRAW,
                                       redraw_update_idle,
                                       stage,
                                       NULL);
    }
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
      clutter_stage_set_color (stage, clutter_value_get_color (value));
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
      clutter_stage_set_perspective (stage, g_value_get_boxed (value));
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

    case PROP_FOG:
      clutter_stage_set_fog (stage, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_stage_get_property (GObject    *gobject,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_COLOR:
      clutter_value_set_color (value, &priv->color);
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
      g_value_set_boxed (value, &priv->perspective);
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

    case PROP_FOG:
      g_value_set_boxed (value, &priv->fog);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_stage_dispose (GObject *object)
{
  ClutterStage        *stage = CLUTTER_STAGE (object);
  ClutterStagePrivate *priv = stage->priv;
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();

  clutter_actor_unrealize (CLUTTER_ACTOR (object));

  if (priv->update_idle)
    {
      g_source_remove (priv->update_idle);
      priv->update_idle = 0;
    }

  _clutter_stage_manager_remove_stage (stage_manager, stage);

  if (priv->impl)
    {
      CLUTTER_NOTE (MISC, "Disposing of the stage implementation");
      g_object_unref (priv->impl);
      priv->impl = NULL;
    }

  G_OBJECT_CLASS (clutter_stage_parent_class)->dispose (object);
}

static void
clutter_stage_finalize (GObject *object)
{
  ClutterStage *stage = CLUTTER_STAGE (object);

  g_free (stage->priv->title);
  
  G_OBJECT_CLASS (clutter_stage_parent_class)->finalize (object);
}


static void
clutter_stage_class_init (ClutterStageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_stage_set_property;
  gobject_class->get_property = clutter_stage_get_property;
  gobject_class->dispose = clutter_stage_dispose;
  gobject_class->finalize = clutter_stage_finalize;

  actor_class->allocate = clutter_stage_allocate;
  actor_class->get_preferred_width = clutter_stage_get_preferred_width;
  actor_class->get_preferred_height = clutter_stage_get_preferred_height;
  actor_class->paint = clutter_stage_paint;
  actor_class->pick = clutter_stage_pick;
  actor_class->realize = clutter_stage_realize;
  actor_class->unrealize = clutter_stage_unrealize;
  actor_class->show = clutter_stage_show;
  actor_class->hide = clutter_stage_hide;
  actor_class->queue_redraw = clutter_stage_real_queue_redraw;

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
   * Whether the stage is resizable via user interaction.
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
  pspec = clutter_param_spec_color ("color",
                                    "Color",
                                    "The color of the stage",
                                    &default_stage_color,
                                    CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_COLOR, pspec);

  /**
   * ClutterStage:perspective:
   *
   * The parameters used for the perspective projection from 3D
   * coordinates to 2D
   *
   * Since: 0.8.2
   */
  g_object_class_install_property
    (gobject_class, PROP_PERSPECTIVE,
     g_param_spec_boxed ("perspective",
			 "Perspective",
			 "Perspective projection parameters",
			 CLUTTER_TYPE_PERSPECTIVE,
			 CLUTTER_PARAM_READWRITE));

  /**
   * ClutterStage:title:
   *
   * The stage's title - usually displayed in stage windows title decorations.
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
   * Whether the stage should use a linear GL "fog" in creating the
   * depth-cueing effect, to enhance the perception of depth by fading
   * actors farther from the viewpoint.
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
   * ClutterStage:fog:
   *
   * The settings for the GL "fog", used only if #ClutterStage:use-fog
   * is set to %TRUE
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boxed ("fog",
                              "Fog",
                              "Settings for the depth cueing",
                              CLUTTER_TYPE_FOG,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_FOG, pspec);

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
		  G_SIGNAL_RUN_FIRST,
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
   * The ::activate signal is emitted when the stage receives key focus
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

  klass->fullscreen = clutter_stage_real_fullscreen;

  g_type_class_add_private (gobject_class, sizeof (ClutterStagePrivate));
}

static void
clutter_stage_init (ClutterStage *self)
{
  ClutterStagePrivate *priv;
  ClutterBackend *backend;

  /* a stage is a top-level object */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IS_TOPLEVEL);

  self->priv = priv = CLUTTER_STAGE_GET_PRIVATE (self);

  CLUTTER_NOTE (BACKEND, "Creating stage from the default backend");
  backend = clutter_get_default_backend ();
  priv->impl = _clutter_backend_create_stage (backend, self, NULL);
  if (!priv->impl)
    {
      g_warning ("Unable to create a new stage, falling back to the "
                 "default stage.");
      priv->impl = CLUTTER_ACTOR (_clutter_stage_get_default_window ());

      /* at this point we must have a default stage, or we're screwed */
      g_assert (priv->impl != NULL);
    }
  else
    g_object_ref_sink (priv->impl);

  priv->is_offscreen      = FALSE;
  priv->is_fullscreen     = FALSE;
  priv->is_user_resizable = FALSE;
  priv->is_cursor_visible = TRUE;
  priv->use_fog           = FALSE;

  priv->color = default_stage_color;

  priv->perspective.fovy   = 60.0; /* 60 Degrees */
  priv->perspective.aspect = 1.0;
  priv->perspective.z_near = 0.1;
  priv->perspective.z_far  = 100.0;

  /* depth cueing */
  priv->fog.z_near = 1.0;
  priv->fog.z_far  = 2.0;

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);
  clutter_stage_set_key_focus (self, NULL);
}

/**
 * clutter_stage_get_default:
 *
 * Returns the main stage. The default #ClutterStage is a singleton,
 * so the stage will be created the first time this function is
 * called (typically, inside clutter_init()); all the subsequent
 * calls to clutter_stage_get_default() will return the same instance.
 *
 * Clutter guarantess the existence of the default stage.
 *
 * Return value: (transfer none): the main #ClutterStage.  You should never
 *   destroy or unref the returned actor.
 */
ClutterActor *
clutter_stage_get_default (void)
{
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();
  ClutterStage *stage;

  stage = clutter_stage_manager_get_default_stage (stage_manager);
  if (G_UNLIKELY (stage == NULL))
    /* This will take care of automatically adding the stage to the
     * stage manager and setting it as the default. Its floating
     * reference will be claimed by the stage manager.
     */
    stage = g_object_new (CLUTTER_TYPE_STAGE, NULL);

  return CLUTTER_ACTOR (stage);
}

/**
 * clutter_stage_set_color:
 * @stage: A #ClutterStage
 * @color: A #ClutterColor
 *
 * Sets the stage color.
 */
void
clutter_stage_set_color (ClutterStage       *stage,
			 const ClutterColor *color)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (color != NULL);

  priv = stage->priv;

  priv->color = *color;

  if (CLUTTER_ACTOR_IS_VISIBLE (stage))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  g_object_notify (G_OBJECT (stage), "color");
}

/**
 * clutter_stage_get_color:
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

  *color = priv->color;
}

/**
 * clutter_stage_set_perspective:
 * @stage: A #ClutterStage
 * @perspective: A #ClutterPerspective
 *
 * Sets the stage perspective.
 */
void
clutter_stage_set_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);
  g_return_if_fail (perspective->z_far - perspective->z_near != 0);

  priv = stage->priv;

  priv->perspective = *perspective;

  /* this will cause the viewport to be reset; see
   * clutter_maybe_setup_viewport() inside clutter-main.c
   */
  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

/**
 * clutter_stage_get_perspective:
 * @stage: A #ClutterStage
 * @perspective: return location for a #ClutterPerspective
 *
 * Retrieves the stage perspective.
 */
void
clutter_stage_get_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);

  *perspective = stage->priv->perspective;
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
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);

      /* Only set if backend implements.
       * Also see clutter_stage_event() for setting priv->is_fullscreen
       * on state change event.
      */
      if (iface->set_fullscreen)
	iface->set_fullscreen (impl, TRUE);
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
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);

      /* Only set if backend implements.
       * Also see clutter_stage_event() for setting priv->is_fullscreen
       * on state change event.
      */
      if (iface->set_fullscreen)
	iface->set_fullscreen (impl, FALSE);
    }
}

/**
 * clutter_stage_set_user_resizable:
 * @stage: a #ClutterStage
 * @resizable: whether the stage should be user resizable.
 *
 * Sets if the stage is resizable by user interaction (e.g. via
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
      && priv->is_user_resizable != resizable)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_user_resizable)
        {
          priv->is_user_resizable = resizable;

          iface->set_user_resizable (impl, resizable);

          g_object_notify (G_OBJECT (stage), "user-resizable");
        }
    }
}

/**
 * clutter_stage_get_user_resizable:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set with clutter_stage_set_user_resizable().
 *
 * Return value: %TRUE if the stage is resizable by the user.
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
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_cursor_visible)
        {
          priv->is_cursor_visible = TRUE;

          iface->set_cursor_visible (impl, TRUE);

          g_object_notify (G_OBJECT (stage), "cursor-visible");
        }
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
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_cursor_visible)
        {
          priv->is_cursor_visible = FALSE;

          iface->set_cursor_visible (impl, FALSE);

          g_object_notify (G_OBJECT (stage), "cursor-visible");
        }
    }
}

/**
 * clutter_stage_read_pixels:
 * @stage: A #ClutterStage
 * @x: x coordinate of the first pixel that is read from stage
 * @y: y coordinate of the first pixel that is read from stage
 * @width: Width dimention of pixels to be read, or -1 for the
 *   entire stage width
 * @height: Height dimention of pixels to be read, or -1 for the
 *   entire stage height
 *
 * Makes a screenshot of the stage in RGBA 8bit data, returns a
 * linear buffer with @width * 4 as rowstride.
 *
 * The alpha data contained in the returned buffer is driver-dependent,
 * and not guaranteed to hold any sensible value.
 *
 * Return value: a pointer to newly allocated memory with the buffer
 *   or %NULL if the read failed. Use g_free() on the returned data
 *   to release the resources it has allocated.
 */
guchar *
clutter_stage_read_pixels (ClutterStage *stage,
                           gint          x,
                           gint          y,
                           gint          width,
                           gint          height)
{
  guchar *pixels;
  guchar *temprow;
  GLint   viewport[4];
  gint    rowstride;
  gint    stage_x, stage_y, stage_width, stage_height;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  /* according to glReadPixels documentation pixels outside the viewport are
   * undefined, but no error should be provoked, thus this is probably unnneed.
   */
  g_return_val_if_fail (x >= 0 && y >= 0, NULL);

  /* Force a redraw of the stage before reading back pixels */
  clutter_redraw (stage);
  clutter_stage_ensure_current (stage);

  glGetIntegerv (GL_VIEWPORT, viewport);
  stage_x      = viewport[0];
  stage_y      = viewport[1];
  stage_width  = viewport[2];
  stage_height = viewport[3];

  if (width < 0 || width > stage_width)
    width = stage_width;

  if (height < 0 || height > stage_height)
    height = stage_height;

  rowstride = width * 4;

  pixels  = g_malloc (height * rowstride);
  temprow = g_malloc (rowstride);

  /* Setup the pixel store parameters that may have been changed by
     Cogl */
  glPixelStorei (GL_PACK_ALIGNMENT, 4);
#ifdef HAVE_COGL_GL
  glPixelStorei (GL_PACK_ROW_LENGTH, 0);
  glPixelStorei (GL_PACK_SKIP_PIXELS, 0);
  glPixelStorei (GL_PACK_SKIP_ROWS, 0);
#endif /* HAVE_COGL_GL */

  /* The y co-ordinate should be given in OpenGL's coordinate system
     so 0 is the bottom row */
  y = stage_height - y - height;

  glFinish ();

  /* check whether we need to read into a smaller temporary buffer */
  glReadPixels (x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  /* vertically flip the buffer in-place */
  for (y = 0; y < height / 2; y++)
    {
      if (y != height - y - 1) /* skip center row */
        {
          memcpy (temprow,
                  pixels + y * rowstride, rowstride);
          memcpy (pixels + y * rowstride,
                  pixels + (height - y - 1) * rowstride, rowstride);
          memcpy (pixels + (height - y - 1) * rowstride,
                  temprow,
                  rowstride);
        }
    }

  g_free (temprow);

  return pixels;
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
 * Return value: (transfer none): the actor at the specified coordinates, if any
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
 *
 * You should rarely need to use this function, except for
 * synthetised events.
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
 * Since: 0.4
 **/
void
clutter_stage_set_title (ClutterStage       *stage,
			 const gchar        *title)
{
  ClutterStagePrivate *priv;
  ClutterStageWindow *impl;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  g_free (priv->title);
  priv->title = g_strdup (title);

  impl = CLUTTER_STAGE_WINDOW (priv->impl);
  if (CLUTTER_STAGE_WINDOW_GET_IFACE(impl)->set_title != NULL)
    CLUTTER_STAGE_WINDOW_GET_IFACE (impl)->set_title (impl, priv->title);

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
 * Return value: (transfer none): the actor with key focus, or the stage
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
 * clutter_stage_set_fog:
 * @stage: the #ClutterStage
 * @fog: a #ClutterFog structure
 *
 * Sets the fog (also known as "depth cueing") settings for the @stage.
 *
 * A #ClutterStage will only use a linear fog progression, which
 * depends solely on the distance from the viewer. The cogl_set_fog()
 * function in COGL exposes more of the underlying implementation,
 * and allows changing the for progression function. It can be directly
 * used by disabling the #ClutterStage:use-fog property and connecting
 * a signal handler to the #ClutterActor::paint signal on the @stage,
 * like:
 *
 * |[
 *   clutter_stage_set_use_fog (stage, FALSE);
 *   g_signal_connect (stage, "paint", G_CALLBACK (on_stage_paint), NULL);
 * ]|
 *
 * The paint signal handler will call cogl_set_fog() with the
 * desired settings:
 *
 * |[
 *   static void
 *   on_stage_paint (ClutterActor *actor)
 *   {
 *     ClutterColor stage_color = { 0, };
 *     CoglColor fog_color = { 0, };
 *
 *     /&ast; set the fog color to the stage background color &ast;/
 *     clutter_stage_get_color (CLUTTER_STAGE (actor), &amp;stage_color);
 *     cogl_color_set_from_4ub (&amp;fog_color,
 *                              stage_color.red,
 *                              stage_color.green,
 *                              stage_color.blue,
 *                              stage_color.alpha);
 *
 *     /&ast; enable fog &ast;/
 *     cogl_set_fog (&amp;fog_color,
 *                   COGL_FOG_MODE_EXPONENTIAL, /&ast; mode &ast;/
 *                   0.5,                       /&ast; density &ast;/
 *                   5.0, 30.0);                /&ast; z_near and z_far &ast;/
 *   }
 * ]|
 *
 * Since: 0.6
 */
void
clutter_stage_set_fog (ClutterStage *stage,
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
 * clutter_stage_get_fog:
 * @stage: the #ClutterStage
 * @fog: return location for a #ClutterFog structure
 *
 * Retrieves the current depth cueing settings from the stage.
 *
 * Since: 0.6
 */
void
clutter_stage_get_fog (ClutterStage *stage,
                        ClutterFog   *fog)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (fog != NULL);

  *fog = stage->priv->fog;
}

/*** Perspective boxed type ******/

static gpointer
clutter_perspective_copy (gpointer data)
{
  if (G_LIKELY (data))
    return g_slice_dup (ClutterPerspective, data);

  return NULL;
}

static void
clutter_perspective_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (ClutterPerspective, data);
}

GType
clutter_perspective_get_type (void)
{
  static GType our_type = 0;

  if (!our_type)
    our_type = g_boxed_type_register_static (I_("ClutterPerspective"),
                                             clutter_perspective_copy,
                                             clutter_perspective_free);
  return our_type;
}

static gpointer
clutter_fog_copy (gpointer data)
{
  if (G_LIKELY (data))
    return g_slice_dup (ClutterFog, data);

  return NULL;
}

static void
clutter_fog_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (ClutterFog, data);
}

GType
clutter_fog_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type = g_boxed_type_register_static (I_("ClutterFog"),
                                             clutter_fog_copy,
                                             clutter_fog_free);

  return our_type;
}

/**
 * clutter_stage_new:
 *
 * Creates a new, non-default stage. A non-default stage is a new
 * top-level actor which can be used as another container. It works
 * exactly like the default stage, but while clutter_stage_get_default()
 * will always return the same instance, you will have to keep a pointer
 * to any #ClutterStage returned by clutter_stage_create().
 *
 * The ability to support multiple stages depends on the current
 * backend. Use clutter_feature_available() and
 * %CLUTTER_FEATURE_STAGE_MULTIPLE to check at runtime whether a
 * backend supports multiple stages.
 *
 * Return value: a new stage, or %NULL if the default backend does
 *   not support multiple stages. Use clutter_actor_destroy() to
 *   programmatically close the returned stage.
 *
 * Since: 0.8
 */
ClutterActor *
clutter_stage_new (void)
{
  if (!clutter_feature_available (CLUTTER_FEATURE_STAGE_MULTIPLE))
    {
      g_warning ("Unable to create a new stage: the %s backend does not "
                 "support multiple stages.",
                 CLUTTER_FLAVOUR);
      return NULL;
    }

  /* The stage manager will grab the floating reference when the stage
     is added to it in the constructor */
  return g_object_new (CLUTTER_TYPE_STAGE, NULL);
}

/**
 * clutter_stage_ensure_current:
 * @stage: the #ClutterStage
 *
 * This function essentially makes sure the right GL context is
 * current for the passed stage. It is not intended to
 * be used by applications.
 *
 * Since: 0.8
 */
void
clutter_stage_ensure_current (ClutterStage *stage)
{
  ClutterMainContext *ctx = clutter_context_get_default ();

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  _clutter_backend_ensure_context (ctx->backend, stage);
}

/**
 * clutter_stage_ensure_viewport:
 * @stage: a #ClutterStage
 *
 * Ensures that the GL viewport is updated with the current
 * stage window size.
 *
 * This function will queue a redraw of @stage.
 *
 * This function should not be called by applications; it is used
 * when embedding a #ClutterStage into a toolkit with another
 * windowing system, like GTK+.
 *
 * Since: 1.0
 */
void
clutter_stage_ensure_viewport (ClutterStage *stage)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_queue_redraw:
 * @stage: the #ClutterStage
 *
 * Queues a redraw for the passed stage.
 *
 * <note>Applications should call clutter_actor_queue_redraw() and not
 * this function.</note>
 *
 * <note>This function is just a wrapper for clutter_actor_queue_redraw()
 * and should probably go away.</note>
 *
 * Since: 0.8
 */
void
clutter_stage_queue_redraw (ClutterStage *stage)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_is_default:
 * @stage: a #ClutterStage
 *
 * Checks if @stage is the default stage, or an instance created using
 * clutter_stage_new() but internally using the same implementation.
 *
 * Return value: %TRUE if the passed stage is the default one
 *
 * Since: 0.8
 */
gboolean
clutter_stage_is_default (ClutterStage *stage)
{
  ClutterStageWindow *impl;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  if (CLUTTER_ACTOR (stage) == clutter_stage_get_default ())
    return TRUE;

  impl = _clutter_stage_get_window (stage);
  if (impl == _clutter_stage_get_default_window ())
    return TRUE;

  return FALSE;
}

void
_clutter_stage_set_window (ClutterStage       *stage,
                           ClutterStageWindow *stage_window)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (stage_window));

  if (stage->priv->impl)
    g_object_unref (stage->priv->impl);

  stage->priv->impl = CLUTTER_ACTOR (stage_window);
}

ClutterStageWindow *
_clutter_stage_get_window (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return CLUTTER_STAGE_WINDOW (stage->priv->impl);
}

ClutterStageWindow *
_clutter_stage_get_default_window (void)
{
  ClutterActor *stage = clutter_stage_get_default ();

  return _clutter_stage_get_window (CLUTTER_STAGE (stage));
}
