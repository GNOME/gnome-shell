/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <string.h>

#include "shell-perf-log.h"

typedef struct _ShellPerfEvent ShellPerfEvent;
typedef struct _ShellPerfBlock ShellPerfBlock;

/**
 * SECTION:shell-perf-log
 * @short_description: Event recorder for performance measurement
 *
 * ShellPerfLog provides a way for different parts of the code to
 * record information for subsequent analysis and interactive
 * exploration. Events exist of a timestamp, an event ID, and
 * arguments to the event.
 *
 * Emphasis is placed on storing recorded events in a compact
 * fashion so log recording disturbs the execution of the program
 * as little as possible, however events should not be recorded
 * at too fine a granularity - an event that is recorded once
 * per frame or once per user action is appropriate, an event that
 * occurs many times per frame is not.
 *
 * Arguments are identified by a D-Bus style signature; at the moment
 * only a limited number of event signatures are supported to
 * simplify the code.
 */
struct _ShellPerfLog
{
  GObject parent;

  GPtrArray *events;
  GHashTable *events_by_name;
  GQueue *blocks;

  gint64 start_time;
  gint64 last_time;

  guint enabled : 1;
};

struct _ShellPerfLogClass
{
  GObjectClass parent_class;
};

struct _ShellPerfEvent
{
  guint16 id;
  char *name;
  char *description;
  char *signature;
};

/* The events in the log are stored in a linked list of fixed size
 * blocks.
 *
 * Note that the power-of-two nature of BLOCK_SIZE here is superficial
 * since the allocated block has the 'bytes' field and malloc
 * overhead. The current value is well below the size that will
 * typically be independently mmapped by the malloc implementation so
 * it doesn't matter. If we switched to mmapping blocks manually
 * (perhaps to avoid polluting malloc statistics), we'd want to use a
 * different value of BLOCK_SIZE.
 */
#define BLOCK_SIZE 8192

struct _ShellPerfBlock
{
  guint32 bytes;
  guchar buffer[BLOCK_SIZE];
};

/* Builtin events */
enum {
  EVENT_SET_TIME
};

G_DEFINE_TYPE(ShellPerfLog, shell_perf_log, G_TYPE_OBJECT);

static gint64
get_time (void)
{
  GTimeVal timeval;

  g_get_current_time (&timeval);

  return timeval.tv_sec * 10000000LL + timeval.tv_usec;
}

static void
shell_perf_log_init (ShellPerfLog *perf_log)
{
  perf_log->events = g_ptr_array_new ();
  perf_log->events_by_name = g_hash_table_new (g_str_hash, g_str_equal);
  perf_log->blocks = g_queue_new ();

  /* This event is used when timestamp deltas are greater than
   * fits in a gint32. 0xffffffff microseconds is about 70 minutes, so this
   * is not going to happen in normal usage. It might happen if performance
   * logging is enabled some time after starting the shell */
  shell_perf_log_define_event (perf_log, "perf.setTime", "", "x");
  g_assert (perf_log->events->len == EVENT_SET_TIME + 1);

  perf_log->start_time = perf_log->last_time = get_time();
}

static void
shell_perf_log_class_init (ShellPerfLogClass *class)
{
}

/**
 * shell_perf_log_get_default:
 *
 * Gets the global singleton performance log. This is initially disabled
 * and must be explicitly enabled with shell_perf_log_set_enabled().
 *
 * Return value: (transfer none): the global singleton performance log
 */
ShellPerfLog *
shell_perf_log_get_default (void)
{
  static ShellPerfLog *perf_log;

  if (perf_log == NULL)
    perf_log = g_object_new (SHELL_TYPE_PERF_LOG, NULL);

  return perf_log;
}

/**
 * shell_perf_log_set_enabled:
 * @perf_log: a #ShellPerfLog
 * @enabled: whether to record events
 *
 * Sets whether events are currently being recorded.
 */
void
shell_perf_log_set_enabled (ShellPerfLog *perf_log,
                            gboolean      enabled)
{
  enabled = enabled != FALSE;

  perf_log->enabled = enabled;
}

static ShellPerfEvent *
define_event (ShellPerfLog *perf_log,
              const char   *name,
              const char   *description,
              const char   *signature)
{
  ShellPerfEvent *event;

  if (strcmp (signature, "") != 0 &&
      strcmp (signature, "s") != 0 &&
      strcmp (signature, "i") != 0 &&
      strcmp (signature, "x") != 0)
    {
      g_warning ("Only supported event signatures are '', 's', 'i', and 'x'\n");
      return NULL;
    }

  if (perf_log->events->len == 65536)
    {
      g_warning ("Maximum number of events defined\n");
      return NULL;
    }

  if (g_hash_table_lookup (perf_log->events_by_name, name) != NULL)
    {
      g_warning ("Duplicate event event for '%s'\n", name);
      return NULL;
    }

  event = g_slice_new (ShellPerfEvent);

  event->id = perf_log->events->len;
  event->name = g_strdup (name);
  event->signature = g_strdup (signature);
  event->description = g_strdup (description);

  g_ptr_array_add (perf_log->events, event);
  g_hash_table_insert (perf_log->events_by_name, event->name, event);

  return event;
}

/**
 * shell_perf_log_define_event:
 * @perf_log: a #ShellPerfLog
 * @name: name of the event. This should of the form
 *   '<namespace>.<specific eventf'>, for example
 *   'clutter.stagePaintDone'.
 * @description: human readable description of the event.
 * @signature: signature defining the arguments that event takes.
 *   This is a string of type characters, using the same characters
 *   as D-Bus or GVariant. Only a very limited number of signatures
 *   are supported: , '', 's', 'i', and 'x'. This mean respectively:
 *   no arguments, one string, one 32-bit integer, and one 64-bit
 *   integer.
 *
 * Defines a performance event for later recording.
 */
void
shell_perf_log_define_event (ShellPerfLog *perf_log,
                             const char   *name,
                             const char   *description,
                             const char   *signature)
{
  define_event (perf_log, name, description, signature);
}

static ShellPerfEvent *
lookup_event (ShellPerfLog *perf_log,
              const char   *name,
              const char   *signature)
{
  ShellPerfEvent *event = g_hash_table_lookup (perf_log->events_by_name, name);

  if (G_UNLIKELY (event == NULL))
    {
      g_warning ("Discarding unknown event '%s'\n", name);
      return NULL;
    }

  if (G_UNLIKELY (strcmp (event->signature, signature) != 0))
    {
      g_warning ("Event '%s'; defined with signature '%s', used with '%s'\n",
                 name, event->signature, signature);
      return NULL;
    }

  return event;
}

static void
record_event (ShellPerfLog   *perf_log,
              gint64          event_time,
              ShellPerfEvent *event,
              const guchar   *bytes,
              size_t          bytes_len)
{
  ShellPerfBlock *block;
  size_t total_bytes;
  guint32 time_delta;
  guint32 pos;

  if (!perf_log->enabled)
    return;

  total_bytes = sizeof (gint32) + sizeof (gint16) + bytes_len;
  if (G_UNLIKELY (bytes_len > BLOCK_SIZE || total_bytes > BLOCK_SIZE))
    {
      g_warning ("Discarding oversize event '%s'\n", event->name);
      return;
    }

  if (event_time > perf_log->last_time + G_GINT64_CONSTANT(0xffffffff))
    {
      perf_log->last_time = event_time;
      record_event (perf_log, event_time,
                    lookup_event (perf_log, "perf.setTime", "x"),
                    (const guchar *)&event_time, sizeof(gint64));
      time_delta = 0;
    }
  else if (event_time < perf_log->last_time)
    time_delta = 0;
  else
    time_delta = (guint32)(event_time - perf_log->last_time);

  perf_log->last_time = event_time;

  if (perf_log->blocks->tail == NULL ||
      total_bytes + ((ShellPerfBlock *)perf_log->blocks->tail->data)->bytes > BLOCK_SIZE)
    {
      block = g_new (ShellPerfBlock, 1);
      block->bytes = 0;
      g_queue_push_tail (perf_log->blocks, block);
    }
  else
    {
      block = (ShellPerfBlock *)perf_log->blocks->tail->data;
    }

  pos = block->bytes;

  memcpy (block->buffer + pos, &time_delta, sizeof (guint32));
  pos += sizeof (guint32);
  memcpy (block->buffer + pos, &event->id, sizeof (guint16));
  pos += sizeof (guint16);
  memcpy (block->buffer + pos, bytes, bytes_len);
  pos += bytes_len;

  block->bytes = pos;
}

/**
 * shell_perf_log_event:
 * @perf_log: a #ShellPerfLog
 * @name: name of the event
 *
 * Records a performance event with no arguments.
 */
void
shell_perf_log_event (ShellPerfLog *perf_log,
                      const char   *name)
{
  ShellPerfEvent *event = lookup_event (perf_log, name, "");
  if (G_UNLIKELY (event == NULL))
    return;

  record_event (perf_log, get_time(), event, NULL, 0);
}

/**
 * shell_perf_log_event_i:
 * @perf_log: a #ShellPerfLog
 * @name: name of the event
 * @arg: the argument
 *
 * Records a performance event with one 32-bit integer argument.
 */
void
shell_perf_log_event_i (ShellPerfLog *perf_log,
                        const char   *name,
                        gint32        arg)
{
  ShellPerfEvent *event = lookup_event (perf_log, name, "i");
  if (G_UNLIKELY (event == NULL))
    return;

  record_event (perf_log, get_time(), event,
                (const guchar *)&arg, sizeof (arg));
}

/**
 * shell_perf_log_event_x:
 * @perf_log: a #ShellPerfLog
 * @name: name of the event
 * @arg: the argument
 *
 * Records a performance event with one 64-bit integer argument.
 */
void
shell_perf_log_event_x (ShellPerfLog *perf_log,
                        const char   *name,
                        gint64        arg)
{
  ShellPerfEvent *event = lookup_event (perf_log, name, "x");
  if (G_UNLIKELY (event == NULL))
    return;

  record_event (perf_log, get_time(), event,
                (const guchar *)&arg, sizeof (arg));
}

/**
 * shell_perf_log_event_s:
 * @perf_log: a #ShellPerfLog
 * @name: name of the event
 * @arg: the argument
 *
 * Records a performance event with one string argument.
 */
void
shell_perf_log_event_s (ShellPerfLog *perf_log,
                         const char   *name,
                         const char   *arg)
{
  ShellPerfEvent *event = lookup_event (perf_log, name, "s");
  if (G_UNLIKELY (event == NULL))
    return;

  record_event (perf_log, get_time(), event,
                (const guchar *)arg, strlen (arg) + 1);
}

/**
 * shell_perf_log_replay:
 * @perf_log: a #ShellPerfLog
 * @replay_function: function to call for each event in the log
 *
 * Replays the log by calling the given function for each event
 * in the log.
 */
void
shell_perf_log_replay (ShellPerfLog            *perf_log,
                       ShellPerfReplayFunction  replay_function)
{
  gint64 event_time = perf_log->start_time;
  GList *iter;

  for (iter = perf_log->blocks->head; iter; iter = iter->next)
    {
      ShellPerfBlock *block = iter->data;
      guint32 pos = 0;

      while (pos < block->bytes)
        {
          ShellPerfEvent *event;
          guint16 id;
          guint32 time_delta;
          GValue arg = { 0, };

          memcpy (&time_delta, block->buffer + pos, sizeof (guint32));
          pos += sizeof (guint32);
          memcpy (&id, block->buffer + pos, sizeof (guint16));
          pos += sizeof (guint16);

          if (id == EVENT_SET_TIME)
            {
              /* Internal, we don't include in the replay */
              memcpy (&event_time, block->buffer + pos, sizeof (gint64));
              pos += sizeof (gint64);
              continue;
            }
          else
            {
              event_time += time_delta;
            }

          event = g_ptr_array_index (perf_log->events, id);

          if (strcmp (event->signature, "") == 0)
            {
              /* We need to pass something, so pass an empty string */
              g_value_init (&arg, G_TYPE_STRING);
            }
          else if (strcmp (event->signature, "i") == 0)
            {
              gint32 l;

              memcpy (&l, block->buffer + pos, sizeof (gint32));
              pos += sizeof (gint32);

              g_value_init (&arg, G_TYPE_INT);
              g_value_set_int (&arg, l);
            }
          else if (strcmp (event->signature, "x") == 0)
            {
              gint64 l;

              memcpy (&l, block->buffer + pos, sizeof (gint64));
              pos += sizeof (gint64);

              g_value_init (&arg, G_TYPE_INT64);
              g_value_set_int64 (&arg, l);
            }
          else if (strcmp (event->signature, "s") == 0)
            {
              g_value_init (&arg, G_TYPE_STRING);
              g_value_set_string (&arg, (char *)block->buffer + pos);
              pos += strlen ((char *)(block->buffer + pos)) + 1;
            }

          replay_function (event_time, event->name, event->signature, &arg);
          g_value_unset (&arg);
        }
    }
}
