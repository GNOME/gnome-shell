#include "cb-button.h"

/*
 * convenience macro for GType implementations; see:
 * http://library.gnome.org/devel/gobject/2.27/gobject-Type-Information.html#G-DEFINE-TYPE:CAPS
 */
G_DEFINE_TYPE (CbButton, cb_button, CLUTTER_TYPE_ACTOR);

/* macro for accessing the object's private structure */
#define CB_BUTTON_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CB_TYPE_BUTTON, CbButtonPrivate))

/* private structure - should only be accessed through the public API */
struct _CbButtonPrivate
{
  ClutterActor  *child;
  ClutterActor  *label;
  ClutterAction *click_action;
  gchar         *text;
};

/* enumerates signal identifiers for this class */
enum
{
  CLICKED,
  LAST_SIGNAL
};

/* enumerates property identifiers for this class */
enum
{
  PROP_0,
  PROP_TEXT
};

/* signals */
static guint cb_button_signals[LAST_SIGNAL] = { 0, };

static void
cb_button_dispose (GObject *gobject)
{
  CbButtonPrivate *priv = CB_BUTTON (gobject)->priv;

  /* we just dispose of the child, and let its dispose()
   * function deal with its children
   */
  if (priv->child)
    {
      clutter_actor_unparent (priv->child);
      priv->child = NULL;
    }

  G_OBJECT_CLASS (cb_button_parent_class)->dispose (gobject);
}

static void
cb_button_finalize (GObject *gobject)
{
  CbButtonPrivate *priv = CB_BUTTON (gobject)->priv;

  g_free (priv->text);

  G_OBJECT_CLASS (cb_button_parent_class)->finalize (gobject);
}

static void
cb_button_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  CbButton *button = CB_BUTTON (gobject);

  switch (prop_id)
    {
    case PROP_TEXT:
      cb_button_set_text (button, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
cb_button_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  CbButtonPrivate *priv = CB_BUTTON (gobject)->priv;

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

/* ClutterActor implementation */

/* use the actor's allocation for the ClutterBox */
static void
cb_button_allocate (ClutterActor           *actor,
                    const ClutterActorBox  *box,
                    ClutterAllocationFlags  flags)
{
  CbButtonPrivate *priv = CB_BUTTON (actor)->priv;
  ClutterActorBox child_box = { 0, };

  /* set the allocation for the whole button */
  CLUTTER_ACTOR_CLASS (cb_button_parent_class)->allocate (actor, box, flags);

  /* make the child (the ClutterBox) fill the parent */
  child_box.x1 = 0.0;
  child_box.y1 = 0.0;
  child_box.x2 = clutter_actor_box_get_width (box);
  child_box.y2 = clutter_actor_box_get_height (box);

  clutter_actor_allocate (priv->child, &child_box, flags);
}

/* paint function implementation: just calls paint() on the ClutterBox */
static void
cb_button_paint (ClutterActor *actor)
{
  CbButtonPrivate *priv = CB_BUTTON (actor)->priv;

  clutter_actor_paint (priv->child);
}

/* proxy ClickAction signals so they become signals from the actor */
static void
cb_button_clicked (ClutterClickAction *action,
                   ClutterActor       *actor,
                   gpointer            user_data)
{
  g_signal_emit (actor, cb_button_signals[CLICKED], 0);
}

/* GObject class and instance init */
static void
cb_button_class_init (CbButtonClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->dispose = cb_button_dispose;
  gobject_class->finalize = cb_button_finalize;
  gobject_class->set_property = cb_button_set_property;
  gobject_class->get_property = cb_button_get_property;

  actor_class->allocate = cb_button_allocate;
  actor_class->paint = cb_button_paint;

  g_type_class_add_private (klass, sizeof (CbButtonPrivate));

  /**
   * CbButton:text:
   *
   * The text shown on the #CbButton
   */
  pspec = g_param_spec_string ("text",
                               "Text",
                               "Text of the button",
                               NULL,
                               G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TEXT, pspec);

  /**
   * CbButton::clicked:
   * @button: the #CbButton that emitted the signal
   *
   * The ::clicked signal is emitted when the internal #ClutterClickAction
   * associated with a #CbButton emits its own ::clicked signal
   */
  cb_button_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (CbButtonClass, clicked),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
cb_button_init (CbButton *self)
{
  CbButtonPrivate *priv;
  ClutterLayoutManager *layout;

  priv = self->priv = CB_BUTTON_GET_PRIVATE (self);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  priv->child = clutter_box_new (layout);

  /* set the parent of the ClutterBox to this instance */
  clutter_actor_set_parent (priv->child,
                            CLUTTER_ACTOR (self));

  /* add text label to the button */
  priv->label = g_object_new (CLUTTER_TYPE_TEXT,
                              "line-alignment", PANGO_ALIGN_CENTER,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (priv->child),
                               priv->label);

  /* add a ClutterClickAction on this actor, so we can proxy its
   * "clicked" signal through a signal from this actor
   */
  priv->click_action = clutter_click_action_new ();
  clutter_actor_add_action (CLUTTER_ACTOR (self), priv->click_action);

  g_signal_connect (priv->click_action,
                    "clicked",
                    G_CALLBACK (cb_button_clicked),
                    NULL);
}

/* public API */
/* examples of public API functions which wrap functions
 * on internal actors
 */

/**
 * cb_button_set_text:
 * @self: a #CbButton
 * @text: the text to display on the button
 *
 * Set the text on the button
 */
void
cb_button_set_text (CbButton    *self,
                    const gchar *text)
{
  CbButtonPrivate *priv;

  /* public API should check its arguments;
   * see also g_return_val_if_fail for functions which
   * return a value
   */
  g_return_if_fail (CB_IS_BUTTON (self));

  priv = self->priv;

  g_free (priv->text);

  if (text)
    priv->text = g_strdup (text);
  else
    priv->text = g_strdup ("");

  /* call a function on the ClutterText inside the layout */
  clutter_text_set_text (CLUTTER_TEXT (priv->label), priv->text);
}

/**
 * cb_button_set_background_color:
 * @self: a #CbButton
 * @color: the #ClutterColor to use for the button's background
 *
 * Set the color of the button's background
 */
void
cb_button_set_background_color (CbButton *self,
                                const ClutterColor *color)
{
  g_return_if_fail (CB_IS_BUTTON (self));

  clutter_box_set_color (CLUTTER_BOX (self->priv->child), color);
}

/**
 * cb_button_set_text_color:
 * @self: a #CbButton
 * @color: the #ClutterColor to use as the color for the button text
 *
 * Set the color of the text on the button
 */
void
cb_button_set_text_color (CbButton *self,
                          const ClutterColor *color)
{
  g_return_if_fail (CB_IS_BUTTON (self));

  clutter_text_set_color (CLUTTER_TEXT (self->priv->label), color);
}

/**
 * cb_button_get_text:
 * @self: a #CbButton
 *
 * Get the text displayed on the button
 *
 * Returns: the button's text. This must not be freed by the application.
 */
G_CONST_RETURN gchar *
cb_button_get_text (CbButton *self)
{
  g_return_val_if_fail (CB_IS_BUTTON (self), NULL);

  return self->priv->text;
}

/**
 * cb_button_new:
 *
 * Creates a new #CbButton instance
 *
 * Returns: a new #CbButton
 */
ClutterActor *
cb_button_new (void)
{
  return g_object_new (CB_TYPE_BUTTON, NULL);
}
