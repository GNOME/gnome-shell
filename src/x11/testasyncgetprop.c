/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2002 Havoc Pennington
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from The Open Group.
 */

#include "async-getprop.h"

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void*) 0)
#endif

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
static void
print_backtrace (void)
{
  void *bt[500];
  int bt_size;
  int i;
  char **syms;

  bt_size = backtrace (bt, 500);

  syms = backtrace_symbols (bt, bt_size);

  i = 0;
  while (i < bt_size)
    {
      fprintf (stderr, "  %s\n", syms[i]);
      ++i;
    }

  free (syms);
}
#else
static void
print_backtrace (void)
{
  fprintf (stderr, "Not compiled with backtrace support\n");
}
#endif

static int error_trap_depth = 0;

static int
x_error_handler (Display     *xdisplay,
                 XErrorEvent *error)
{
  char buf[64];

  XGetErrorText (xdisplay, error->error_code, buf, 63);

  if (error_trap_depth == 0)
    {
      print_backtrace ();

      fprintf (stderr, "Unexpected X error: %s serial %ld error_code %d request_code %d minor_code %d)\n",
               buf,
               error->serial,
               error->error_code,
               error->request_code,
               error->minor_code);

      exit (1);
    }

  return 1; /* return value is meaningless */
}

static void
error_trap_push (Display   *xdisplay)
{
  ++error_trap_depth;
}

static void
error_trap_pop (Display   *xdisplay)
{
  if (error_trap_depth == 0)
    {
      fprintf (stderr, "Error trap underflow!\n");
      exit (1);
    }

  XSync (xdisplay, False); /* get all errors out of the queue */
  --error_trap_depth;
}

static char*
my_strdup (const char *str)
{
  char *s;

  s = malloc (strlen (str) + 1);
  if (s == NULL)
    {
      fprintf (stderr, "malloc failed\n");
      exit (1);
    }
  strcpy (s, str);

  return s;
}

static char*
atom_name (Display *display,
           Atom     atom)
{
  if (atom == None)
    {
      return my_strdup ("None");
    }
  else
    {
      char *xname;
      char *ret;

      error_trap_push (display);
      xname = XGetAtomName (display, atom);
      error_trap_pop (display);
      if (xname == NULL)
        return my_strdup ("[unknown atom]");

      ret = my_strdup (xname);
      XFree (xname);

      return ret;
    }
}


#define ELAPSED(start_time, current_time) \
    (((((double)current_time.tv_sec - start_time.tv_sec) * 1000000 + \
       (current_time.tv_usec - start_time.tv_usec))) / 1000.0)

static struct timeval program_start_time;

static Bool
try_get_reply (Display           *xdisplay,
               AgGetPropertyTask *task)
{
  if (ag_task_have_reply (task))
    {
      int result;
      Atom actual_type;
      int actual_format;
      unsigned long n_items;
      unsigned long bytes_after;
      unsigned char *data;
      char *name;
      struct timeval current_time;

      gettimeofday (&current_time, NULL);

      printf (" %gms (we have a reply for property %ld)\n",
              ELAPSED (program_start_time, current_time),
              ag_task_get_property (task));

      data = NULL;

      name = atom_name (xdisplay,
                        ag_task_get_property (task));
      printf (" %s on 0x%lx:\n", name,
              ag_task_get_window (task));
      free (name);

      result = ag_task_get_reply_and_free (task,
                                           &actual_type,
                                           &actual_format,
                                           &n_items,
                                           &bytes_after,
                                           &data);
      task = NULL;

      if (result != Success)
        {
          fprintf (stderr, "  error code %d getting reply\n", result);
        }
      else
        {
          name = atom_name (xdisplay, actual_type);
          printf ("  actual_type = %s\n", name);
          free (name);

          printf ("  actual_format = %d\n", actual_format);

          printf ("  n_items = %lu\n", n_items);
          printf ("  bytes_after = %lu\n", bytes_after);

          printf ("  data = \"%s\"\n", data ? (char*) data : "NULL");
        }

      return True;
    }

  return False;
}

static void run_speed_comparison (Display *xdisplay,
                                  Window   window);

int
main (int argc, char **argv)
{
  Display *xdisplay;
  int i;
  int n_left;
  int n_props;
  Window window;
  const char *window_str;
  char *end;
  Atom *props;
  struct timeval current_time;

  if (argc < 2)
    {
      fprintf (stderr, "specify window ID\n");
      return 1;
    }

  window_str = argv[1];

  end = NULL;
  window = strtoul (window_str, &end, 0);
  if (end == NULL || *end != '\0')
    {
      fprintf (stderr, "\"%s\" does not parse as a window ID\n", window_str);
      return 1;
    }

  xdisplay = XOpenDisplay (NULL);
  if (xdisplay == NULL)
    {
      fprintf (stderr, "Could not open display\n");
      return 1;
    }

  if (getenv ("MUTTER_SYNC") != NULL)
    XSynchronize (xdisplay, True);

  XSetErrorHandler (x_error_handler);

  n_props = 0;
  props = XListProperties (xdisplay, window, &n_props);
  if (n_props == 0 || props == NULL)
    {
      fprintf (stderr, "Window has no properties\n");
      return 1;
    }

  gettimeofday (&program_start_time, NULL);

  i = 0;
  while (i < n_props)
    {
      gettimeofday (&current_time, NULL);
      printf (" %gms (sending request for property %ld)\n",
              ELAPSED (program_start_time, current_time),
              props[i]);
      if (ag_task_create (xdisplay,
                          window, props[i],
                          0, 0xffffffff,
                          False,
                          AnyPropertyType) == NULL)
        {
          fprintf (stderr, "Failed to send request\n");
          return 1;
        }

      ++i;
    }

  XFree (props);
  props = NULL;

  n_left = n_props;

  while (TRUE)
    {
      XEvent xevent;
      int connection;
      fd_set set;
      AgGetPropertyTask *task;

      /* Mop up event queue */
      while (XPending (xdisplay) > 0)
        {
          XNextEvent (xdisplay, &xevent);
          gettimeofday (&current_time, NULL);
          printf (" %gms (processing event type %d)\n",
                  ELAPSED (program_start_time, current_time),
                  xevent.xany.type);
        }

      while ((task = ag_get_next_completed_task (xdisplay)))
        {
          try_get_reply (xdisplay, task);
          n_left -= 1;
        }

      if (n_left == 0)
        {
          printf ("All %d replies received.\n", n_props);
          break;
        }

      /* Wake up if we may have a reply */
      connection = ConnectionNumber (xdisplay);

      FD_ZERO (&set);
      FD_SET (connection, &set);

      gettimeofday (&current_time, NULL);
      printf (" %gms (blocking for data %d left)\n",
              ELAPSED (program_start_time, current_time), n_left);
      select (connection + 1, &set, NULL, NULL, NULL);
    }

  run_speed_comparison (xdisplay, window);

  return 0;
}

/* This function doesn't have all the printf's
 * and other noise, it just compares async to sync
 */
static void
run_speed_comparison (Display *xdisplay,
                      Window   window)
{
  int i;
  int n_props;
  struct timeval start, end;
  int n_left;

  /* We just use atom values (0 to n_props) % 200, many are probably
   * BadAtom, that's fine, but the %200 keeps most of them valid. The
   * async case is about twice as advantageous when using valid atoms
   * (or the issue may be that it's more advantageous when the
   * properties are present and data is transmitted).
   */
  n_props = 4000;
  printf ("Timing with %d property requests\n", n_props);

  gettimeofday (&start, NULL);

  i = 0;
  while (i < n_props)
    {
      if (ag_task_create (xdisplay,
                          window, (Atom) i % 200,
                          0, 0xffffffff,
                          False,
                          AnyPropertyType) == NULL)
        {
          fprintf (stderr, "Failed to send request\n");
          exit (1);
        }

      ++i;
    }

  n_left = n_props;

  while (TRUE)
    {
      int connection;
      fd_set set;
      XEvent xevent;
      AgGetPropertyTask *task;

      /* Mop up event queue */
      while (XPending (xdisplay) > 0)
        XNextEvent (xdisplay, &xevent);

      while ((task = ag_get_next_completed_task (xdisplay)))
        {
          Atom actual_type;
          int actual_format;
          unsigned long n_items;
          unsigned long bytes_after;
          unsigned char *data;

          assert (ag_task_have_reply (task));

          data = NULL;
          ag_task_get_reply_and_free (task,
                                      &actual_type,
                                      &actual_format,
                                      &n_items,
                                      &bytes_after,
                                      &data);

          if (data)
            XFree (data);

          n_left -= 1;
        }

      if (n_left == 0)
        break;

      /* Wake up if we may have a reply */
      connection = ConnectionNumber (xdisplay);

      FD_ZERO (&set);
      FD_SET (connection, &set);

      select (connection + 1, &set, NULL, NULL, NULL);
    }

  gettimeofday (&end, NULL);

  printf ("Async time: %gms\n",
          ELAPSED (start, end));

  gettimeofday (&start, NULL);

  error_trap_push (xdisplay);

  i = 0;
  while (i < n_props)
    {
      Atom actual_type;
      int actual_format;
      unsigned long n_items;
      unsigned long bytes_after;
      unsigned char *data;

      data = NULL;
      if (XGetWindowProperty (xdisplay, window,
                              (Atom) i % 200,
                              0, 0xffffffff,
                              False,
                              AnyPropertyType,
                              &actual_type,
                              &actual_format,
                              &n_items,
                              &bytes_after,
                              &data) == Success)
        {
          if (data)
            XFree (data);
        }

      ++i;
    }

  error_trap_pop (xdisplay);

  gettimeofday (&end, NULL);

  printf ("Sync time:  %gms\n",
          ELAPSED (start, end));
}
