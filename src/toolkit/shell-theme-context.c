/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include "shell-theme.h"
#include "shell-theme-context.h"

struct _ShellThemeContext {
  GObject parent;

  double resolution;
  PangoFontDescription *font;
  ShellThemeNode *root_node;
  ShellTheme *theme;
};

struct _ShellThemeContextClass {
  GObjectClass parent_class;
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (ShellThemeContext, shell_theme_context, G_TYPE_OBJECT)

static void
shell_theme_context_finalize (GObject *object)
{
  ShellThemeContext *context = SHELL_THEME_CONTEXT (object);

  if (context->root_node)
    g_object_unref (context->root_node);
  if (context->theme)
    g_object_unref (context->theme);
  
  pango_font_description_free (context->font);

  G_OBJECT_CLASS (shell_theme_context_parent_class)->finalize (object);
}

static void
shell_theme_context_class_init (ShellThemeContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = shell_theme_context_finalize;

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
shell_theme_context_init (ShellThemeContext *context)
{
  context->resolution = 96.;
  context->font = pango_font_description_from_string ("sans-serif 10");
}

ShellThemeContext *
shell_theme_context_new (void)
{
  ShellThemeContext *context;

  context = g_object_new (SHELL_TYPE_THEME_CONTEXT, NULL);

  return context;
}

static void
on_stage_destroy (ClutterStage *stage)
{
  ShellThemeContext *context = shell_theme_context_get_for_stage (stage);

  g_object_set_data (G_OBJECT (stage), "shell-theme-context", NULL);
  g_object_unref (context);
}

/**
 * shell_theme_context_get_for_stage:
 * @stage: a #ClutterStage
 *
 * Gets a singleton theme context associated with the stage.
 *
 * Return value: (transfer none): the singleton theme context for the stage
 */
ShellThemeContext *
shell_theme_context_get_for_stage (ClutterStage *stage)
{
  ShellThemeContext *context;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  context = g_object_get_data (G_OBJECT (stage), "shell-theme-context");
  if (context)
    return context;

  context = shell_theme_context_new ();
  g_object_set_data (G_OBJECT (stage), "shell-theme-context", context);
  g_signal_connect (stage, "destroy",
                    G_CALLBACK (on_stage_destroy), NULL);

  return context;
}

/**
 * shell_theme_context_set_theme:
 * @context: a #ShellThemeContext
 *
 * Sets the default set of theme stylesheets for the context. This theme will
 * be used for the root node and for nodes descending from it, unless some other
 * style is explicitely specified.
 */
void
shell_theme_context_set_theme (ShellThemeContext          *context,
                               ShellTheme                 *theme)
{
  g_return_if_fail (SHELL_IS_THEME_CONTEXT (context));
  g_return_if_fail (theme == NULL || SHELL_IS_THEME (theme));

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
 * shell_theme_context_get_theme:
 * @context: a #ShellThemeContext
 *
 * Gets the default theme for the context. See shell_theme_context_set_theme()
 *
 * Return value: (transfer none): the default theme for the context
 */
ShellTheme *
shell_theme_context_get_theme (ShellThemeContext *context)
{
  g_return_val_if_fail (SHELL_IS_THEME_CONTEXT (context), NULL);

  return context->theme;
}

void
shell_theme_context_set_resolution (ShellThemeContext *context,
                                    double             resolution)
{
  g_return_if_fail (SHELL_IS_THEME_CONTEXT (context));

  context->resolution = resolution;
}

double
shell_theme_context_get_resolution (ShellThemeContext *context)
{
  g_return_val_if_fail (SHELL_IS_THEME_CONTEXT (context), 96.);

  return context->resolution;
}

void
shell_theme_context_set_font (ShellThemeContext          *context,
                              const PangoFontDescription *font)
{
  g_return_if_fail (SHELL_IS_THEME_CONTEXT (context));

  if (context->font == font)
    return;

  pango_font_description_free (context->font);
  context->font = pango_font_description_copy (font);
}

const PangoFontDescription *
shell_theme_context_get_font (ShellThemeContext *context)
{
  g_return_val_if_fail (SHELL_IS_THEME_CONTEXT (context), NULL);

  return context->font;
}

/**
 * shell_theme_context_get_root_node:
 * @context: a #ShellThemeContext
 *
 * Gets the root node of the tree of theme style nodes that associated with this
 * context. For the node tree associated with a stage, this node represents
 * styles applied to the stage itself.
 *
 * Return value: (transfer none): the root node of the context's style tree
 */
ShellThemeNode *
shell_theme_context_get_root_node (ShellThemeContext *context)
{
  if (context->root_node == NULL)
    context->root_node = shell_theme_node_new (context, NULL, context->theme,
                                               G_TYPE_NONE, NULL, NULL, NULL, NULL);

  return context->root_node;
}
