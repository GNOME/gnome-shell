/* 
 * tokentest.c - test for Metacity's tokeniser
 *
 * Copyright (C) 2008 Thomas Thurman
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* Still under heavy development. */
/* Especially: FIXME: GErrors need checking! */

#include <stdio.h>
#include <glib/gerror.h>
#include <ui/theme.h>

#define TOKENTEST_GROUP "tokentest0"

MetaTheme* meta_theme_load (const char *theme_name,
                            GError    **err) {
  // dummy
  return NULL;
}

GString *draw_spec_to_string(MetaDrawSpec *spec)
{
  GString *result;
  int i;

  if (spec == NULL)
    return g_string_new ("NONE");

  result = g_string_new ("");

  if (spec->constant)
    {
      g_string_append_printf (result, "{%d==}", spec->value);
    }

  for (i=0; i<spec->n_tokens; i++)
    {
      PosToken t = spec->tokens[i];

      switch (t.type)
        {
        case POS_TOKEN_INT:
          g_string_append_printf (result, "(int %d)", t.d.i.val);
          break;

        case POS_TOKEN_DOUBLE:
          g_string_append_printf (result, "(double %d)", t.d.d.val);
          break;

        case POS_TOKEN_OPERATOR:

          switch (t.d.o.op) {
            case POS_OP_NONE:
              g_string_append (result, "(no-op)");
              break;

            case POS_OP_ADD:
              g_string_append (result, "(add)");
              break;

            case POS_OP_SUBTRACT:
              g_string_append (result, "(subtract)");
              break;

            case POS_OP_MULTIPLY:
              g_string_append (result, "(multiply)");
              break;

            case POS_OP_DIVIDE:
              g_string_append (result, "(divide)");
              break;

            case POS_OP_MOD:
              g_string_append (result, "(mod)");
              break;

            case POS_OP_MAX:
              g_string_append (result, "(max)");
              break;

            case POS_OP_MIN:
              g_string_append (result, "(min)");
              break;

            default:
              g_string_append_printf (result, "(op %d)", t.d.o.op);
          }

          break;

        case POS_TOKEN_VARIABLE:
          g_string_append_printf (result, "(str %s)", t.d.v.name);
          break;

        case POS_TOKEN_OPEN_PAREN:
          g_string_append (result, "( ");
          break;

       case POS_TOKEN_CLOSE_PAREN:
          g_string_append (result, " )");
          break;

        default:
          g_string_append_printf (result, "(strange %d)", t.type);
        }

    }

  return result;
}

GKeyFile *keys;

void
load_keys ()
{
  GError* err = NULL;
  gchar** keys_of_file;
  gchar** cursor;
  keys = g_key_file_new ();

  g_key_file_load_from_file (keys,
        "tokentest.ini",
        G_KEY_FILE_NONE,
        &err);

  keys_of_file = g_key_file_get_keys (keys,
				      TOKENTEST_GROUP,
				      NULL,
				      &err);

  cursor = keys_of_file;

  while (*cursor)
    {
      gchar *desideratum = g_key_file_get_value (keys,
						 TOKENTEST_GROUP,
						 *cursor,
						 &err);
      MetaTheme *dummy = meta_theme_new ();
      MetaDrawSpec *spec;
      GString *str;

      spec = meta_draw_spec_new (dummy, *cursor, &err);

      str = draw_spec_to_string (spec);

      if (strcmp (str->str, desideratum)==0) {
        g_print("PASS: %s\n", *cursor);
      } else {
        g_warning ("FAIL: %s, wanted %s, got %s\n",
            *cursor, desideratum, str->str);
      }

      meta_theme_free (dummy);
      g_string_free (str, TRUE);

      cursor++;
    }

  g_strfreev (keys_of_file);
}

int
main ()
{
  load_keys ();
}
