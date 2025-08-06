/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-FileCopyrightText: 2025 Florian Müllner <fmuellner@gnome.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <termios.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include "commands.h"
#include "common.h"
#include "config.h"

#define EGO_URL_BASE "https://extensions.gnome.org/api/v1"

static char *
get_cached_token_filename () {
  return g_build_filename (g_get_user_cache_dir (),
                           "gnome-extensions",
                           "auth-token",
                           NULL);
}

static char *
get_cached_token ()
{
  g_autoptr (GDateTime) expiry = NULL;
  g_autoptr (GDateTime) now = NULL;
  g_autoptr (GTimeZone) tz = NULL;
  g_autoptr (JsonParser) parser = NULL;
  g_autofree char *token_filename = NULL;
  JsonObject *root_obj = NULL;
  const char *date_str;

  token_filename = get_cached_token_filename ();
  parser = json_parser_new_immutable ();
  if (!json_parser_load_from_file (parser, token_filename, NULL))
    return NULL;

  root_obj = json_node_get_object (json_parser_get_root (parser));
  if (!json_object_has_member (root_obj, "token"))
    return NULL;

  if (!json_object_has_member (root_obj, "expiry"))
    return NULL;

  date_str = json_object_get_string_member (root_obj, "expiry");

  tz = g_time_zone_new_utc ();
  expiry = g_date_time_new_from_iso8601 (date_str, tz);
  now = g_date_time_new_now (tz);
  if (g_date_time_difference (expiry, now) <= 0)
    return NULL;

  return g_strdup (json_object_get_string_member (root_obj, "token"));
}

static gboolean
response_cache_token (JsonObject *object)
{
  g_autoptr (JsonGenerator) generator = NULL;
  JsonNode *token_node;
  g_autofree char *token_filename = NULL;
  g_autofree char *token_dirname = NULL;

  if (!json_object_has_member (object, "token"))
    return FALSE;

  token_node = json_object_get_member (object, "token");

  generator = json_generator_new ();
  json_generator_set_root (generator, token_node);

  token_filename = get_cached_token_filename ();
  token_dirname = g_path_get_dirname (token_filename);
  g_mkdir_with_parents (token_dirname, 0755);

  return json_generator_to_file (generator, token_filename, NULL);
}

static const char *
response_get_token (JsonObject  *object,
                    GError     **error)
{
  JsonObject *token_obj = NULL;
  const char *token = NULL;

  if (!json_object_has_member (object, "token"))
    goto out;

  token_obj = json_object_get_object_member (object, "token");

  if (!json_object_has_member (token_obj, "token"))
    goto out;

  token = json_object_get_string_member (token_obj, "token");

out:
  if (token == NULL)
    {
      const char *msg;

      if (token_obj && json_object_has_member (token_obj, "error"))
        msg = json_object_get_string_member (token_obj, "error");
      else
        msg = "Invalid token";

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", msg);

      return NULL;
    }

  return token;
}

static const char *
response_get_detail (JsonObject *object)
{
  if (!json_object_has_member (object, "detail"))
    return NULL;

  return json_object_get_string_member (object, "detail");
}

static JsonObject *
session_send_message (SoupSession  *session,
                      SoupMessage  *message,
                      GError      **error)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (JsonParser) parser = NULL;
  const char *data;
  gsize size;

  bytes = soup_session_send_and_read (session,
                                      message,
                                      NULL,
                                      error);

  if (bytes == NULL)
    return NULL;

  parser = json_parser_new_immutable ();
  data = g_bytes_get_data (bytes, &size);
  if (!json_parser_load_from_data (parser, data, size, error))
    return NULL;

  return json_node_dup_object (json_parser_get_root (parser));
}

static gboolean
read_password_file (const char  *filename,
                    char       **password,
                    GError     **error)
{
  g_autoptr (GDataInputStream) istream = NULL;
  char *line = NULL;

  if (strcmp (filename, "-") == 0)
    {
      g_autoptr (GInputStream) stdin = NULL;

      stdin = g_unix_input_stream_new (STDIN_FILENO, FALSE);
      istream = g_data_input_stream_new (stdin);
    }
  else
    {
      g_autoptr (GFile) file = NULL;
      g_autoptr (GFileInputStream) fstream = NULL;

      file = g_file_new_for_commandline_arg (filename);
      fstream = g_file_read (file, NULL, error);
      if (fstream == NULL)
        return FALSE;

      istream = g_data_input_stream_new (G_INPUT_STREAM (fstream));
    }

  line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, error);
  if (line == NULL)
    {
      if (error != NULL && *error == NULL)
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "File is empty");
      return FALSE;
    }

  *password = g_strdelimit (line, "\n", '\0');

  return TRUE;
}

static void
ensure_credentials (char   **user,
                    char   **password)
{
  g_autoptr (GInputStream) stdin = NULL;
  g_autoptr (GDataInputStream) istream = NULL;

  g_return_if_fail (user != NULL);
  g_return_if_fail (password != NULL);

  if (*user != NULL && *password != NULL)
    return;

  stdin = g_unix_input_stream_new (STDIN_FILENO, FALSE);
  istream = g_data_input_stream_new (stdin);

  g_print (_("Login to extensions.gnome.org\n"));

  if (*user == NULL)
    {
      char *line = NULL;

      while (line == NULL)
        {
          g_print ("%s: ", _("Username"));

          line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);
        }
      *user = g_strdelimit (line, "\n", '\0');
    }

  if (*password == NULL)
    {
      struct termios term_attr;
      int old_flags;
      char *line = NULL;

      tcgetattr (STDIN_FILENO, &term_attr);
      old_flags = term_attr.c_lflag;

      while (line == NULL)
        {
          g_print ("%s: ", _("Password"));

          term_attr.c_lflag &= ~ECHO;
          if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &term_attr) != 0)
            g_print (_("Warning! Password will be echoed"));

          line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);

          term_attr.c_lflag = old_flags;
          tcsetattr (STDIN_FILENO, TCSAFLUSH, &term_attr);

          g_print ("\n");
        }
      *password = g_strdelimit (line, "\n", '\0');
    }
}

static char *
session_get_login_token (SoupSession  *session,
                         char         *user,
                         char         *password,
                         GError      **error)
{
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (JsonObject) response = NULL;
  const char *token = NULL;
  SoupStatus status;

  message = soup_message_new_from_encoded_form ("POST",
                                                EGO_URL_BASE "/accounts/login/",
                                                soup_form_encode ("login", user,
                                                                  "password", password,
                                                                  NULL));

  response = session_send_message (session, message, error);

  status = soup_message_get_status (message);
  if (status != SOUP_STATUS_OK)
    {
      const char *detail = response_get_detail (response);

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "%s", detail ? detail : soup_status_get_phrase (status));
      return NULL;
    }

  token = response_get_token (response, error);

  if (token == NULL)
    return NULL;

  if (!response_cache_token (response))
    g_warning ("Failed to cache login token");

  return g_strdup (token);
}

static gboolean
session_upload_file (SoupSession  *session,
                     GFile        *file,
                     const char   *token,
                     GError      **error)
{
  g_autoptr (SoupMessage) message = NULL;
  g_autoptr (SoupMultipart) multipart = NULL;
  g_autoptr (JsonObject) response = NULL;
  g_autoptr (GBytes) contents = NULL;
  g_autofree char *path = NULL;
  g_autofree char *auth_token = NULL;
  SoupStatus status;

  contents = g_file_load_bytes (file, NULL, NULL, error);
  if (contents == NULL)
    return FALSE;

  path = g_file_get_path (file);

  multipart = soup_multipart_new ("multipart/form-data");
  soup_multipart_append_form_file (multipart,
                                   "source", path,
                                   "application/zip",
                                   contents);
  soup_multipart_append_form_string (multipart,
                                     "shell_license_compliant", "true");
  soup_multipart_append_form_string (multipart,
                                     "tos_compliant", "true");

  message = soup_message_new_from_multipart (EGO_URL_BASE "/extensions",
                                             multipart);

  auth_token = g_strdup_printf ("Token %s", token);
  soup_message_headers_append (soup_message_get_request_headers (message),
                               "Authorization",
                               auth_token);

  response = session_send_message (session, message, error);
  if (response == NULL)
    return FALSE;

  status = soup_message_get_status (message);
  if (status != SOUP_STATUS_CREATED)
    {
      const char *detail = response_get_detail (response);

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "%s", detail ? detail : soup_status_get_phrase (status));
      return FALSE;
    }

  return TRUE;
}

static gboolean
upload_extensions (char        *user,
                   char        *password,
                   const char **filenames)
{
  g_autoptr (SoupSession) session = NULL;
  g_autofree char *token = NULL;
  g_autoptr (GError) error = NULL;
  gboolean success = TRUE;

  session = soup_session_new ();

  token = get_cached_token ();
  if (token == NULL)
    {
      ensure_credentials (&user, &password);
      token = session_get_login_token (session, user, password, &error);
    }

  if (token == NULL)
    {
      if (error != NULL)
        g_printerr ("Failed to get login token: %s\n", error->message);
      return FALSE;
    }

  for (const char **f = filenames; *f; f++)
    {
      const char *filename = *f;
      g_autoptr (GFile) file = NULL;
      g_autoptr (GError) local_error = NULL;

      file = g_file_new_for_commandline_arg (filename);
      if (!session_upload_file (session, file, token, &local_error))
        {
          success = FALSE;
          g_printerr ("Failed to upload %s: %s\n", filename, local_error->message);
        }
    }

  return success;
}

int
handle_upload (int argc, char *argv[], gboolean do_help)
{
  g_autoptr (GOptionContext) context = NULL;
  g_autoptr (GError) error = NULL;
  g_auto(GStrv) filenames = NULL;
  g_autofree char *user = NULL;
  g_autofree char *password = NULL;
  g_autofree char *password_file = NULL;
  gboolean tos_accepted = FALSE;
  GOptionEntry entries[] = {
    { .long_name = "user", .short_name = 'u',
      .arg_description = _("USERNAME"),
      .arg = G_OPTION_ARG_STRING, .arg_data = &user,
      .description = _("Username to log into https://extensions.gnome.org") },
    { .long_name = "password", .short_name = 'p',
      .arg_description = _("PASSWORD"),
      .arg = G_OPTION_ARG_STRING, .arg_data = &password,
      .description = _("Password to log into https://extensions.gnome.org") },
    { .long_name = "password-file", .short_name = 'P',
      .arg_description = _("FILE"),
      .arg = G_OPTION_ARG_FILENAME, .arg_data = &password_file,
      .description = _("Read https://extensions.gnome.org password from file, use \"-\" for stdin") },
    { .long_name = "accept-tos",
      .arg = G_OPTION_ARG_NONE, .arg_data = &tos_accepted,
      .description = _("Accept the terms of service at https://extensions.gnome.org/upload/") },
    { .long_name = G_OPTION_REMAINING,
      .arg_description = _("FILE…"),
      .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = &filenames },
    { NULL }
  };

  g_set_prgname ("gnome-extensions upload");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Upload new extension versions"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, get_option_group());

  if (do_help)
    {
      show_help (context, NULL);
      return 0;
    }

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      show_help (context, error->message);
      return 1;
    }

  if (password != NULL && password_file != NULL)
    {
      show_help (context, _("Only one of --password and --password-file can be used"));
      return 1;
    }

  if (!tos_accepted)
    {
      show_help (context, _("You must accept the terms of service to upload extensions"));
      return 1;
    }

  if (filenames == NULL)
    {
      show_help (context, _("No files given"));
      return 1;
    }

  if (password_file != NULL)
    {
      if (!read_password_file (password_file, &password, &error))
        {
          g_printerr ("Failed to read %s: %s\n", password_file, error->message);
          return 1;
        }
    }

  return upload_extensions (user, password, (const char **)filenames) ? 0 : 2;
}
