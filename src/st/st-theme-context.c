/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include "st-theme.h"
#include "st-theme-context.h"

struct _StThemeContext {
  GObject parent;

  double resolution;
  PangoFontDescription *font;
  StThemeNode *root_node;
  StTheme *theme;
};

struct _StThemeContextClass {
  GObjectClass parent_class;
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (StThemeContext, st_theme_context, G_TYPE_OBJECT)

static void
st_theme_context_finalize (GObject *object)
{
  StThemeContext *context = ST_THEME_CONTEXT (object);

  if (context->root_node)
    g_object_unref (context->root_node);
  if (context->theme)
    g_object_unref (context->theme);

  pango_font_description_free (context->font);

  G_OBJECT_CLASS (st_theme_context_parent_class)->finalize (object);
}

static void
st_theme_context_class_init (StThemeContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = st_theme_context_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, /* no default handler slot */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
st_theme_context_init (StThemeContext *context)
{
  context->resolution = 96.;
  context->font = pango_font_description_from_string ("sans-serif 10");
}

StThemeContext *
st_theme_context_new (void)
{
  StThemeContext *context;

  context = g_object_new (ST_TYPE_THEME_CONTEXT, NULL);

  return context;
}

static void
on_stage_destroy (ClutterStage *stage)
{
  StThemeContext *context = st_theme_context_get_for_stage (stage);

  g_object_set_data (G_OBJECT (stage), "st-theme-context", NULL);
  g_object_unref (context);
}

/**
 * st_theme_context_get_for_stage:
 * @stage: a #ClutterStage
 *
 * Gets a singleton theme context associated with the stage.
 *
 * Return value: (transfer none): the singleton theme context for the stage
 */
StThemeContext *
st_theme_context_get_for_stage (ClutterStage *stage)
{
  StThemeContext *context;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  context = g_object_get_data (G_OBJECT (stage), "st-theme-context");
  if (context)
    return context;

  context = st_theme_context_new ();
  g_object_set_data (G_OBJECT (stage), "st-theme-context", context);
  g_signal_connect (stage, "destroy",
                    G_CALLBACK (on_stage_destroy), NULL);

  return context;
}

/**
 * st_theme_context_set_theme:
 * @context: a #StThemeContext
 *
 * Sets the default set of theme stylesheets for the context. This theme will
 * be used for the root node and for nodes descending from it, unless some other
 * style is explicitely specified.
 */
void
st_theme_context_set_theme (StThemeContext          *context,
                            StTheme                 *theme)
{
  g_return_if_fail (ST_IS_THEME_CONTEXT (context));
  g_return_if_fail (theme == NULL || ST_IS_THEME (theme));

  if (context->theme != theme)
    {
      if (context->theme)
        g_object_unref (context->theme);

      context->theme = theme;

      if (context->theme)
        g_object_ref (context->theme);

      g_signal_emit (context, signals[CHANGED], 0);
    }
}

/**
 * st_theme_context_get_theme:
 * @context: a #StThemeContext
 *
 * Gets the default theme for the context. See st_theme_context_set_theme()
 *
 * Return value: (transfer none): the default theme for the context
 */
StTheme *
st_theme_context_get_theme (StThemeContext *context)
{
  g_return_val_if_fail (ST_IS_THEME_CONTEXT (context), NULL);

  return context->theme;
}

void
st_theme_context_set_resolution (StThemeContext *context,
                                 double          resolution)
{
  g_return_if_fail (ST_IS_THEME_CONTEXT (context));

  context->resolution = resolution;
}

double
st_theme_context_get_resolution (StThemeContext *context)
{
  g_return_val_if_fail (ST_IS_THEME_CONTEXT (context), 96.);

  return context->resolution;
}

void
st_theme_context_set_font (StThemeContext             *context,
                           const PangoFontDescription *font)
{
  g_return_if_fail (ST_IS_THEME_CONTEXT (context));

  if (context->font == font)
    return;

  pango_font_description_free (context->font);
  context->font = pango_font_description_copy (font);
}

const PangoFontDescription *
st_theme_context_get_font (StThemeContext *context)
{
  g_return_val_if_fail (ST_IS_THEME_CONTEXT (context), NULL);

  return context->font;
}

/**
 * st_theme_context_get_root_node:
 * @context: a #StThemeContext
 *
 * Gets the root node of the tree of theme style nodes that associated with this
 * context. For the node tree associated with a stage, this node represents
 * styles applied to the stage itself.
 *
 * Return value: (transfer none): the root node of the context's style tree
 */
StThemeNode *
st_theme_context_get_root_node (StThemeContext *context)
{
  if (context->root_node == NULL)
    context->root_node = st_theme_node_new (context, NULL, context->theme,
                                            G_TYPE_NONE, NULL, NULL, NULL, NULL);

  return context->root_node;
}
