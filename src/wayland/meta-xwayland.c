/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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

#include "config.h"

#include "meta-xwayland-private.h"

#include <glib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "meta-window-actor-private.h"
#include "xserver-server-protocol.h"

static void
xserver_finished_init (MetaXWaylandManager *manager);

static void
associate_window_with_surface (MetaWindow         *window,
                               MetaWaylandSurface *surface)
{
  MetaDisplay *display = window->display;

  /* If the window has an existing surface, like if we're
   * undecorating or decorating the window, then we need
   * to detach the window from its old surface.
   */
  if (window->surface)
    window->surface->window = NULL;

  meta_wayland_surface_set_window (surface, window);
  window->surface = surface;

  meta_compositor_window_surface_changed (display->compositor, window);
}

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

  associate_window_with_surface (window, surface);
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
  MetaXWaylandManager *manager = data;

  /* If it's a different client than the xserver we launched,
   * just freeze up... */
  if (client != manager->client)
    return;

  manager->xserver_resource = wl_resource_create (client, &xserver_interface,
                                                  MIN (META_XSERVER_VERSION, version), id);
  wl_resource_set_implementation (manager->xserver_resource,
				  &xserver_implementation, manager, NULL);

  xserver_send_listen_socket (manager->xserver_resource, manager->abstract_fd);
  xserver_send_listen_socket (manager->xserver_resource, manager->unix_fd);

  /* Make sure xwayland will recieve the above sockets in a finite
   * time before unblocking the initialization mainloop since we are
   * then going to immediately try and connect to those as the window
   * manager. */
  wl_client_flush (client);

  xserver_finished_init (manager);
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
      g_warning ("failed to bind to @%s: %m", addr.sun_path + 1);
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
      g_warning ("failed to bind to %s: %m\n", addr.sun_path);
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

static gboolean
choose_xdisplay (MetaXWaylandManager *manager)
{
  int display = 0;
  char *lockfile = NULL;

  do
    {
      lockfile = create_lockfile (display, &display);
      if (!lockfile)
        {
          g_warning ("Failed to create an X lock file");
          return FALSE;
        }

      manager->abstract_fd = bind_to_abstract_socket (display);
      if (manager->abstract_fd < 0)
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

      manager->unix_fd = bind_to_unix_socket (display);
      if (manager->abstract_fd < 0)
        {
          unlink (lockfile);
          close (manager->abstract_fd);
          return FALSE;
        }

      break;
    }
  while (1);

  manager->display_index = display;
  manager->display_name = g_strdup_printf (":%d", manager->display_index);
  manager->lockfile = lockfile;

  return TRUE;
}

static void
xserver_finished_init (MetaXWaylandManager *manager)
{
  /* At this point xwayland is all setup to start accepting
   * connections so we can quit the transient initialization mainloop
   * and unblock meta_wayland_init() to continue initializing mutter.
   * */
  g_main_loop_quit (manager->init_loop);
  g_clear_pointer (&manager->init_loop, g_main_loop_unref);
}

gboolean
meta_xwayland_start (MetaXWaylandManager *manager,
                     struct wl_display   *wl_display)
{
  int sp[2];
  int fd;
  char *socket_fd;

  if (!choose_xdisplay (manager))
    return FALSE;

  wl_global_create (wl_display, &xserver_interface,
		    META_XSERVER_VERSION,
		    manager, bind_xserver);

  /* We want xwayland to be a wayland client so we make a socketpair to setup a
   * wayland protocol connection. */
  if (socketpair (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) < 0)
    {
      g_warning ("socketpair failed\n");
      unlink (manager->lockfile);
      return 1;
    }

  manager->pid = fork ();
  if (manager->pid == 0)
    {
      /* We passed SOCK_CLOEXEC, so dup the FD so it isn't
       * closed on exec.. */
      fd = dup (sp[1]);
      socket_fd = g_strdup_printf ("%d", fd);
      setenv ("WAYLAND_SOCKET", socket_fd, TRUE);
      g_free (socket_fd);

      /* xwayland, please. */
      if (g_getenv ("XWAYLAND_STFU"))
        {
          int dev_null;
          dev_null = open ("/dev/null", O_WRONLY);

          dup2 (dev_null, STDOUT_FILENO);
          dup2 (dev_null, STDERR_FILENO);
        }

      if (execl (XWAYLAND_PATH, XWAYLAND_PATH,
                 manager->display_name,
                 "-wayland",
                 "-rootless",
                 "-noreset",
                 "-nolisten", "all",
                 NULL) < 0)
        {
          g_warning ("Failed to spawn XWayland: %m");
          exit (EXIT_FAILURE);
        }
    }

  g_child_watch_add (manager->pid, xserver_died, NULL);
  manager->client = wl_client_create (wl_display, sp[0]);

  /* We need to run a mainloop until we know xwayland has a binding
   * for our xserver interface at which point we can assume it's
   * ready to start accepting connections. */
  manager->init_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (manager->init_loop);

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
meta_xwayland_stop (MetaXWaylandManager *manager)
{
  char path[256];

  snprintf (path, sizeof path, "/tmp/.X%d-lock", manager->display_index);
  unlink (path);
  snprintf (path, sizeof path, "/tmp/.X11-unix/X%d", manager->display_index);
  unlink (path);

  unlink (manager->lockfile);
}
