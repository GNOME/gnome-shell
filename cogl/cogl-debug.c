/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <glib/gi18n-lib.h>

#include "cogl-debug.h"

#ifdef COGL_ENABLE_DEBUG

/* XXX: If you add a debug option, please also scroll down to
 * cogl_arg_debug_cb() and add a "help" description of the option too.
 */

/* NB: Only these options get enabled if COGL_DEBUG=all is
 * used since they don't affect the behaviour of Cogl they
 * simply print out verbose information */
static const GDebugKey cogl_log_debug_keys[] = {
  { "handle", COGL_DEBUG_HANDLE },
  { "slicing", COGL_DEBUG_SLICING },
  { "atlas", COGL_DEBUG_ATLAS },
  { "blend-strings", COGL_DEBUG_BLEND_STRINGS },
  { "journal", COGL_DEBUG_JOURNAL },
  { "batching", COGL_DEBUG_BATCHING },
  { "matrices", COGL_DEBUG_MATRICES },
  { "draw", COGL_DEBUG_DRAW },
  { "opengl", COGL_DEBUG_OPENGL },
  { "pango", COGL_DEBUG_PANGO },
};
static const int n_cogl_log_debug_keys =
  G_N_ELEMENTS (cogl_log_debug_keys);

static const GDebugKey cogl_behavioural_debug_keys[] = {
  { "rectangles", COGL_DEBUG_RECTANGLES },
  { "disable-batching", COGL_DEBUG_DISABLE_BATCHING },
  { "disable-vbos", COGL_DEBUG_DISABLE_VBOS },
  { "disable-software-transform", COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM },
  { "force-scanline-paths", COGL_DEBUG_FORCE_SCANLINE_PATHS },
  { "dump-atlas-image", COGL_DEBUG_DUMP_ATLAS_IMAGE },
  { "disable-atlas", COGL_DEBUG_DISABLE_ATLAS }
};
static const int n_cogl_behavioural_debug_keys =
  G_N_ELEMENTS (cogl_behavioural_debug_keys);

#endif /* COGL_ENABLE_DEBUG */

unsigned int cogl_debug_flags = 0;

#ifdef COGL_ENABLE_DEBUG
static unsigned int
_cogl_parse_debug_string (const char *value,
                          gboolean ignore_help)
{
  unsigned int flags = 0;

  if (ignore_help && strcmp (value, "help") == 0)
    return 0;

  /* We don't want to let g_parse_debug_string handle "all" because
   * literally enabling all the debug options wouldn't be useful to
   * anyone; instead the all option enables all non behavioural
   * options.
   */
  if (strcmp (value, "all") == 0 ||
      strcmp (value, "verbose") == 0)
    {
      int i;
      for (i = 0; i < n_cogl_log_debug_keys; i++)
        flags |= cogl_log_debug_keys[i].value;
    }
  else if (strcmp (value, "help") == 0)
    {
      g_printerr ("\n\n%28s\n", "Supported debug values:");
#define OPT(NAME, HELP) \
      g_printerr ("%28s %s\n", NAME, HELP);
      OPT ("handle:", "debug ref counting issues for Cogl objects");
      OPT ("slicing:", "debug the creation of texture slices");
      OPT ("atlas:", "debug texture atlas management");
      OPT ("blend-strings:", "debug blend-string parsing");
      OPT ("journal:", "view all geometry passing through the journal");
      OPT ("batching:", "show how geometry is being batched in the journal");
      OPT ("matrices:", "trace all matrix manipulation");
      /* XXX: we should replace the "draw" option its very hand wavy... */
      OPT ("draw:", "misc tracing of some drawing operations");
      OPT ("pango:", "trace the pango renderer");
      OPT ("rectangles:", "add wire outlines for all rectangular geometry");
      OPT ("disable-batching:", "disable the journal batching");
      OPT ("disable-vbos:", "disable use of OpenGL vertex buffer objects");
      OPT ("disable-software-transform",
           "use the GPU to transform rectangular geometry");
      OPT ("force-scanline-paths:", "use a scanline based path rasterizer");
      OPT ("dump-atlas-image:", "dump atlas changes to an image file");
      OPT ("disable-atlas:", "disable texture atlasing");
      OPT ("opengl:", "traces some select OpenGL calls");
      g_printerr ("\n%28s\n", "Special debug values:");
      OPT ("all:", "Enables all non-behavioural debug options");
      OPT ("verbose:", "Enables all non-behavioural debug options");
#undef OPT
      exit (1);
    }
  else
    {
      flags |=
        g_parse_debug_string (value,
                              cogl_log_debug_keys,
                              n_cogl_log_debug_keys);
      flags |=
        g_parse_debug_string (value,
                              cogl_behavioural_debug_keys,
                              n_cogl_behavioural_debug_keys);
    }

  return flags;
}

static gboolean
cogl_arg_debug_cb (const char *key,
                   const char *value,
                   gpointer    user_data)
{
  cogl_debug_flags |= _cogl_parse_debug_string (value, FALSE);
  return TRUE;
}

static gboolean
cogl_arg_no_debug_cb (const char *key,
                      const char *value,
                      gpointer    user_data)
{
  cogl_debug_flags &= ~_cogl_parse_debug_string (value, TRUE);
  return TRUE;
}
#endif /* COGL_ENABLE_DEBUG */

static GOptionEntry cogl_args[] = {
  { "cogl-debug", 0, 0, G_OPTION_ARG_CALLBACK, cogl_arg_debug_cb,
    N_("COGL debugging flags to set"), "FLAGS" },
  { "cogl-no-debug", 0, 0, G_OPTION_ARG_CALLBACK, cogl_arg_no_debug_cb,
    N_("COGL debugging flags to unset"), "FLAGS" },
  { NULL, },
};

static gboolean
pre_parse_hook (GOptionContext  *context,
                GOptionGroup    *group,
                gpointer         data,
                GError         **error)
{
#ifdef COGL_ENABLE_DEBUG
  const char *env_string;

  env_string = g_getenv ("COGL_DEBUG");
  if (env_string != NULL)
    {
      cogl_debug_flags |= _cogl_parse_debug_string (env_string, FALSE);
      env_string = NULL;
    }
#endif /* COGL_ENABLE_DEBUG */

  return TRUE;
}

GOptionGroup *
cogl_get_option_group (void)
{
  GOptionGroup *group;

  group = g_option_group_new ("cogl",
                              _("COGL Options"),
                              _("Show COGL options"),
                              NULL, NULL);

  g_option_group_set_parse_hooks (group, pre_parse_hook, NULL);
  g_option_group_add_entries (group, cogl_args);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);

  return group;
}
