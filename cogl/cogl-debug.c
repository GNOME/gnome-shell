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

#include "cogl-i18n-private.h"
#include "cogl-private.h"
#include "cogl-debug.h"
#include "cogl1-context.h"

/* XXX: If you add a debug option, please also add an option
 * definition to cogl-debug-options.h. This will enable us - for
 * example - to emit a "help" description for the option.
 */

/* NB: Only these options get enabled if COGL_DEBUG=all is
 * used since they don't affect the behaviour of Cogl they
 * simply print out verbose information */
static const GDebugKey cogl_log_debug_keys[] = {
  { "object", COGL_DEBUG_OBJECT },
  { "slicing", COGL_DEBUG_SLICING },
  { "atlas", COGL_DEBUG_ATLAS },
  { "blend-strings", COGL_DEBUG_BLEND_STRINGS },
  { "journal", COGL_DEBUG_JOURNAL },
  { "batching", COGL_DEBUG_BATCHING },
  { "matrices", COGL_DEBUG_MATRICES },
  { "draw", COGL_DEBUG_DRAW },
  { "opengl", COGL_DEBUG_OPENGL },
  { "pango", COGL_DEBUG_PANGO },
  { "show-source", COGL_DEBUG_SHOW_SOURCE},
  { "offscreen", COGL_DEBUG_OFFSCREEN },
  { "texture-pixmap", COGL_DEBUG_TEXTURE_PIXMAP },
  { "bitmap", COGL_DEBUG_BITMAP },
  { "clipping", COGL_DEBUG_CLIPPING },
  { "winsys", COGL_DEBUG_WINSYS },
  { "performance", COGL_DEBUG_PERFORMANCE }
};
static const int n_cogl_log_debug_keys =
  G_N_ELEMENTS (cogl_log_debug_keys);

static const GDebugKey cogl_behavioural_debug_keys[] = {
  { "rectangles", COGL_DEBUG_RECTANGLES },
  { "disable-batching", COGL_DEBUG_DISABLE_BATCHING },
  { "disable-vbos", COGL_DEBUG_DISABLE_VBOS },
  { "disable-pbos", COGL_DEBUG_DISABLE_PBOS },
  { "disable-software-transform", COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM },
  { "dump-atlas-image", COGL_DEBUG_DUMP_ATLAS_IMAGE },
  { "disable-atlas", COGL_DEBUG_DISABLE_ATLAS },
  { "disable-shared-atlas", COGL_DEBUG_DISABLE_SHARED_ATLAS },
  { "disable-texturing", COGL_DEBUG_DISABLE_TEXTURING},
  { "disable-arbfp", COGL_DEBUG_DISABLE_ARBFP},
  { "disable-fixed", COGL_DEBUG_DISABLE_FIXED},
  { "disable-glsl", COGL_DEBUG_DISABLE_GLSL},
  { "disable-blending", COGL_DEBUG_DISABLE_BLENDING},
  { "disable-npot-textures", COGL_DEBUG_DISABLE_NPOT_TEXTURES},
  { "wireframe", COGL_DEBUG_WIREFRAME},
  { "disable-software-clip", COGL_DEBUG_DISABLE_SOFTWARE_CLIP},
  { "disable-program-caches", COGL_DEBUG_DISABLE_PROGRAM_CACHES},
  { "disable-fast-read-pixel", COGL_DEBUG_DISABLE_FAST_READ_PIXEL}
};
static const int n_cogl_behavioural_debug_keys =
  G_N_ELEMENTS (cogl_behavioural_debug_keys);

unsigned long _cogl_debug_flags[COGL_DEBUG_N_LONGS];
GHashTable *_cogl_debug_instances;

static void
_cogl_parse_debug_string_for_keys (const char *value,
                                   CoglBool enable,
                                   const GDebugKey *keys,
                                   unsigned int nkeys)
{
  int long_num, key_num;

  /* g_parse_debug_string expects the value field in GDebugKey to be a
     mask in an unsigned int but the flags are stored in an array of
     multiple longs so we need to build a separate array for each
     possible unsigned int */

  for (long_num = 0; long_num < COGL_DEBUG_N_LONGS; long_num++)
    {
      int int_num;

      for (int_num = 0;
           int_num < sizeof (unsigned long) / sizeof (unsigned int);
           int_num++)
        {
          GDebugKey keys_for_int[sizeof (unsigned int) * 8];
          int nkeys_for_int = 0;

          for (key_num = 0; key_num < nkeys; key_num++)
            {
              int long_index = COGL_FLAGS_GET_INDEX (keys[key_num].value);
              int int_index = (keys[key_num].value %
                               (sizeof (unsigned long) * 8) /
                               (sizeof (unsigned int) * 8));

              if (long_index == long_num && int_index == int_num)
                {
                  keys_for_int[nkeys_for_int] = keys[key_num];
                  keys_for_int[nkeys_for_int].value =
                    COGL_FLAGS_GET_MASK (keys[key_num].value) >>
                    (int_num * sizeof (unsigned int) * 8);
                  nkeys_for_int++;
                }
            }

          if (nkeys_for_int > 0)
            {
              unsigned long mask =
                ((unsigned long) g_parse_debug_string (value,
                                                       keys_for_int,
                                                       nkeys_for_int)) <<
                (int_num * sizeof (unsigned int) * 8);

              if (enable)
                _cogl_debug_flags[long_num] |= mask;
              else
                _cogl_debug_flags[long_num] &= ~mask;
            }
        }
    }
}

void
_cogl_parse_debug_string (const char *value,
                          CoglBool enable,
                          CoglBool ignore_help)
{
  if (ignore_help && strcmp (value, "help") == 0)
    return;

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
        if (enable)
          COGL_DEBUG_SET_FLAG (cogl_log_debug_keys[i].value);
        else
          COGL_DEBUG_CLEAR_FLAG (cogl_log_debug_keys[i].value);
    }
  else if (g_ascii_strcasecmp (value, "help") == 0)
    {
      g_printerr ("\n\n%28s\n", _("Supported debug values:"));
#define OPT(MASK_NAME, GROUP, NAME, NAME_FORMATTED, DESCRIPTION) \
      g_printerr ("%28s %s\n", NAME ":", g_dgettext (GETTEXT_PACKAGE, \
                                                     DESCRIPTION));
#include "cogl-debug-options.h"
      g_printerr ("\n%28s\n", _("Special debug values:"));
      OPT (IGNORED, "ignored", "all", "ignored", \
           N_("Enables all non-behavioural debug options"));
      OPT (IGNORED, "ignored", "verbose", "ignored", \
           N_("Enables all non-behavioural debug options"));
#undef OPT

      g_printerr ("\n"
                  "%28s\n"
                  " COGL_DISABLE_GL_EXTENSIONS: %s\n"
                  "   COGL_OVERRIDE_GL_VERSION: %s\n",
                  _("Additional environment variables:"),
                  _("Comma-separated list of GL extensions to pretend are "
                    "disabled"),
                  _("Override the GL version that Cogl will assume the driver "
                    "supports"));
      exit (1);
    }
  else
    {
      _cogl_parse_debug_string_for_keys (value,
                                         enable,
                                         cogl_log_debug_keys,
                                         n_cogl_log_debug_keys);
      _cogl_parse_debug_string_for_keys (value,
                                         enable,
                                         cogl_behavioural_debug_keys,
                                         n_cogl_behavioural_debug_keys);
    }
}

#ifdef COGL_ENABLE_DEBUG
static CoglBool
cogl_arg_debug_cb (const char *key,
                   const char *value,
                   void *user_data)
{
  _cogl_parse_debug_string (value,
                            TRUE /* enable the flags */,
                            FALSE /* don't ignore help */);
  return TRUE;
}

static CoglBool
cogl_arg_no_debug_cb (const char *key,
                      const char *value,
                      void *user_data)
{
  _cogl_parse_debug_string (value,
                            FALSE, /* disable the flags */
                            TRUE /* ignore help */);
  return TRUE;
}
#endif /* COGL_ENABLE_DEBUG */

static GOptionEntry cogl_args[] = {
#ifdef COGL_ENABLE_DEBUG
  { "cogl-debug", 0, 0, G_OPTION_ARG_CALLBACK, cogl_arg_debug_cb,
    N_("Cogl debugging flags to set"), "FLAGS" },
  { "cogl-no-debug", 0, 0, G_OPTION_ARG_CALLBACK, cogl_arg_no_debug_cb,
    N_("Cogl debugging flags to unset"), "FLAGS" },
#endif /* COGL_ENABLE_DEBUG */
  { NULL, },
};

void
_cogl_debug_check_environment (void)
{
  const char *env_string;

  env_string = g_getenv ("COGL_DEBUG");
  if (env_string != NULL)
    {
      _cogl_parse_debug_string (env_string,
                                TRUE /* enable the flags */,
                                FALSE /* don't ignore help */);
      env_string = NULL;
    }

  env_string = g_getenv ("COGL_NO_DEBUG");
  if (env_string != NULL)
    {
      _cogl_parse_debug_string (env_string,
                                FALSE /* disable the flags */,
                                FALSE /* don't ignore help */);
      env_string = NULL;
    }
}

static CoglBool
pre_parse_hook (GOptionContext *context,
                GOptionGroup *group,
                void *data,
                GError **error)
{
  _cogl_init ();

  return TRUE;
}

/* XXX: GOption based library initialization is not reliable because the
 * GOption API has no way to represent dependencies between libraries.
 */
GOptionGroup *
cogl_get_option_group (void)
{
  GOptionGroup *group;

  group = g_option_group_new ("cogl",
                              _("Cogl Options"),
                              _("Show Cogl options"),
                              NULL, NULL);

  g_option_group_set_parse_hooks (group, pre_parse_hook, NULL);
  g_option_group_add_entries (group, cogl_args);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);

  return group;
}
