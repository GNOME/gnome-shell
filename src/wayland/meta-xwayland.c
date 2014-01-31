/*
 * X Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
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

#include <glib.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "meta-xwayland-private.h"
#include "meta-window-actor-private.h"
#include "xserver-server-protocol.h"

static void
xserver_set_window_id (struct wl_client *client,
                       struct wl_resource *compositor_resource,
                       struct wl_resource *surface_resource,
                       guint32 xid)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaDisplay *display = meta_get_display ();
  MetaWindow *window;

  window = meta_display_lookup_x_window (display, xid);
  if (!window)
    return;

  surface->window = window;
  window->surface = surface;
}

static const struct xserver_interface xserver_implementation = {
  xserver_set_window_id
};

static void
bind_xserver (struct wl_client *client,
	      void *data,
              guint32 version,
              guint32 id)
{
  MetaWaylandCompositor *compositor = data;

  /* If it's a different client than the xserver we launched,
   * don't start the wm. */
  if (client != compositor->xwayland_client)
    return;

  compositor->xserver_resource = wl_resource_create (client, &xserver_interface,
						     MIN (META_XSERVER_VERSION, version), id);
  wl_resource_set_implementation (compositor->xserver_resource,
				  &xserver_implementation, compositor, NULL);

  xserver_send_listen_socket (compositor->xserver_resource, compositor->xwayland_abstract_fd);
  xserver_send_listen_socket (compositor->xserver_resource, compositor->xwayland_unix_fd);

  /* Make sure xwayland will recieve the above sockets in a finite
   * time before unblocking the initialization mainloop since we are
   * then going to immediately try and connect to those as the window
   * manager. */
  wl_client_flush (client);

  /* At this point xwayland is all setup to start accepting
   * connections so we can quit the transient initialization mainloop
   * and unblock meta_wayland_init() to continue initializing mutter.
   * */
  g_main_loop_quit (compositor->init_loop);
  g_clear_pointer (&compositor->init_loop, g_main_loop_unref);
}

static char *
create_lockfile (int display, int *display_out)
{
  char *filename;
  int size;
  char pid[11];
  int fd;

  do
    {
      char *end;
      pid_t other;

      filename = g_strdup_printf ("/tmp/.X%d-lock", display);
      fd = open (filename, O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL, 0444);

      if (fd < 0 && errno == EEXIST)
        {
          fd = open (filename, O_CLOEXEC, O_RDONLY);
          if (fd < 0 || read (fd, pid, 11) != 11)
            {
              const char *msg = strerror (errno);
              g_warning ("can't read lock file %s: %s", filename, msg);
              g_free (filename);

              /* ignore error and try the next display number */
              display++;
              continue;
            }
          close (fd);

          other = strtol (pid, &end, 0);
          if (end != pid + 10)
            {
              g_warning ("can't parse lock file %s", filename);
              g_free (filename);

              /* ignore error and try the next display number */
              display++;
              continue;
            }

          if (kill (other, 0) < 0 && errno == ESRCH)
            {
              g_warning ("unlinking stale lock file %s", filename);
              if (unlink (filename) < 0)
                {
                  const char *msg = strerror (errno);
                  g_warning ("failed to unlink stale lock file: %s", msg);
                  display++;
                }
              g_free (filename);
              continue;
            }

          g_free (filename);
          display++;
          continue;
        }
      else if (fd < 0)
        {
          const char *msg = strerror (errno);
          g_warning ("failed to create lock file %s: %s", filename , msg);
          g_free (filename);
          return NULL;
        }

      break;
    }
  while (1);

  /* Subtle detail: we use the pid of the wayland compositor, not the xserver
   * in the lock file. */
  size = snprintf (pid, 11, "%10d\n", getpid ());
  if (size != 11 || write (fd, pid, 11) != 11)
    {
      unlink (filename);
      close (fd);
      g_warning ("failed to write pid to lock file %s", filename);
      g_free (filename);
      return NULL;
    }

  close (fd);

  *display_out = display;
  return filename;
}

static int
bind_to_abstract_socket (int display)
{
  struct sockaddr_un addr;
  socklen_t size, name_size;
  int fd;

  fd = socket (PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;

  addr.sun_family = AF_LOCAL;
  name_size = snprintf (addr.sun_path, sizeof addr.sun_path,
                        "%c/tmp/.X11-unix/X%d", 0, display);
  size = offsetof (struct sockaddr_un, sun_path) + name_size;
  if (bind (fd, (struct sockaddr *) &addr, size) < 0)
    {
      g_warning ("failed to bind to @%s: %s\n",
                 addr.sun_path + 1, strerror (errno));
      close (fd);
      return -1;
    }

  if (listen (fd, 1) < 0)
    {
      close (fd);
      return -1;
    }

  return fd;
}

static int
bind_to_unix_socket (int display)
{
  struct sockaddr_un addr;
  socklen_t size, name_size;
  int fd;

  fd = socket (PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;

  addr.sun_family = AF_LOCAL;
  name_size = snprintf (addr.sun_path, sizeof addr.sun_path,
                        "/tmp/.X11-unix/X%d", display) + 1;
  size = offsetof (struct sockaddr_un, sun_path) + name_size;
  unlink (addr.sun_path);
  if (bind (fd, (struct sockaddr *) &addr, size) < 0)
    {
      char *msg = strerror (errno);
      g_warning ("failed to bind to %s (%s)\n", addr.sun_path, msg);
      close (fd);
      return -1;
    }

  if (listen (fd, 1) < 0)
    {
      unlink (addr.sun_path);
      close (fd);
      return -1;
    }

  return fd;
}

static void
uncloexec (gpointer user_data)
{
  int fd = GPOINTER_TO_INT (user_data);

  /* Make sure the client end of the socket pair doesn't get closed
   * when we exec xwayland. */
  int flags = fcntl (fd, F_GETFD);
  if (flags != -1)
    fcntl (fd, F_SETFD, flags & ~FD_CLOEXEC);
}

static void
xserver_died (GPid     pid,
              gint     status,
              gpointer user_data)
{
  if (!WIFEXITED (status))
    g_error ("X Wayland crashed; aborting");
  else
    {
      /* For now we simply abort if we see the server exit.
       *
       * In the future X will only be loaded lazily for legacy X support
       * but for now it's a hard requirement. */
      g_error ("Spurious exit of X Wayland server");
    }
}

static int
x_io_error (Display *display)
{
  g_error ("Connection to xwayland lost");

  return 0;
}

gboolean
meta_xwayland_start (MetaWaylandCompositor *compositor)
{
  int display = 0;
  char *lockfile = NULL;
  int sp[2];
  pid_t pid;
  char **env;
  char *fd_string;
  char *display_name;
  char *log_path;
  char *args[11];
  GError *error;

  wl_global_create (compositor->wayland_display,
		    &xserver_interface,
		    META_XSERVER_VERSION,
		    compositor, bind_xserver);

  do
    {
      lockfile = create_lockfile (display, &display);
      if (!lockfile)
        {
          g_warning ("Failed to create an X lock file");
          return FALSE;
        }

      compositor->xwayland_abstract_fd = bind_to_abstract_socket (display);
      if (compositor->xwayland_abstract_fd < 0)
        {
          unlink (lockfile);

          if (errno == EADDRINUSE)
            {
              display++;
              continue;
            }
          else
            return FALSE;
        }

      compositor->xwayland_unix_fd = bind_to_unix_socket (display);
      if (compositor->xwayland_abstract_fd < 0)
        {
          unlink (lockfile);
          close (compositor->xwayland_abstract_fd);
          return FALSE;
        }

      break;
    }
  while (1);

  compositor->xwayland_display_index = display;
  compositor->xwayland_lockfile = lockfile;

  /* We want xwayland to be a wayland client so we make a socketpair to setup a
   * wayland protocol connection. */
  if (socketpair (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) < 0)
    {
      g_warning ("socketpair failed\n");
      unlink (lockfile);
      return 1;
    }

  env = g_get_environ ();
  fd_string = g_strdup_printf ("%d", sp[1]);
  env = g_environ_setenv (env, "WAYLAND_SOCKET", fd_string, TRUE);
  g_free (fd_string);

  display_name = g_strdup_printf (":%d",
                                  compositor->xwayland_display_index);
  log_path = g_build_filename (g_get_user_cache_dir (), "xwayland.log", NULL);

  args[0] = XWAYLAND_PATH;
  args[1] = display_name;
  args[2] = "-wayland";
  args[3] = "-rootless";
  args[4] = "-retro";
  args[5] = "-noreset";
  args[6] = "-logfile";
  args[7] = log_path;
  args[8] = "-nolisten";
  args[9] = "all";
  args[10] = NULL;

  error = NULL;
  if (g_spawn_async (NULL, /* cwd */
                     args,
                     env,
                     G_SPAWN_LEAVE_DESCRIPTORS_OPEN |
                     G_SPAWN_DO_NOT_REAP_CHILD |
                     G_SPAWN_STDOUT_TO_DEV_NULL |
                     G_SPAWN_STDERR_TO_DEV_NULL,
                     uncloexec,
                     GINT_TO_POINTER (sp[1]),
                     &pid,
                     &error))
    {
      g_message ("forked X server, pid %d\n", pid);

      close (sp[1]);
      compositor->xwayland_client =
        wl_client_create (compositor->wayland_display, sp[0]);

      compositor->xwayland_pid = pid;
      g_child_watch_add (pid, xserver_died, NULL);
    }
  else
    {
      g_error ("Failed to fork for xwayland server: %s", error->message);
    }

  g_strfreev (env);
  g_free (display_name);
  g_free (log_path);

  /* We need to run a mainloop until we know xwayland has a binding
   * for our xserver interface at which point we can assume it's
   * ready to start accepting connections. */
  compositor->init_loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (compositor->init_loop);

  return TRUE;
}

/* To be called right after connecting */
void
meta_xwayland_complete_init (void)
{
  /* We install an X IO error handler in addition to the child watch,
     because after Xlib connects our child watch may not be called soon
     enough, and therefore we won't crash when X exits (and most important
     we won't reset the tty).
  */
  XSetIOErrorHandler (x_io_error);
}

void
meta_xwayland_stop (MetaWaylandCompositor *compositor)
{
  char path[256];

  snprintf (path, sizeof path, "/tmp/.X%d-lock",
            compositor->xwayland_display_index);
  unlink (path);
  snprintf (path, sizeof path, "/tmp/.X11-unix/X%d",
            compositor->xwayland_display_index);
  unlink (path);

  unlink (compositor->xwayland_lockfile);
}
