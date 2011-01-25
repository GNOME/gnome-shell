/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_EVOLUTION_EVENT_SOURCE_H__
#define __SHELL_EVOLUTION_EVENT_SOURCE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ShellEvolutionEvent            ShellEvolutionEvent;

struct _ShellEvolutionEvent
{
  gchar    *summary;
  gboolean  all_day;
  gint64    date;
};

GType                shell_evolution_event_get_type (void) G_GNUC_CONST;
ShellEvolutionEvent *shell_evolution_event_new      (const gchar *summary,
                                                     gboolean     all_day,
                                                     gint64       date);
ShellEvolutionEvent *shell_evolution_event_copy     (ShellEvolutionEvent *event);
void                 shell_evolution_event_free     (ShellEvolutionEvent *event);

typedef struct _ShellEvolutionEventSource      ShellEvolutionEventSource;
typedef struct _ShellEvolutionEventSourceClass ShellEvolutionEventSourceClass;

#define SHELL_TYPE_EVOLUTION_EVENT_SOURCE              (shell_evolution_event_source_get_type ())
#define SHELL_EVOLUTION_EVENT_SOURCE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_EVOLUTION_EVENT_SOURCE, ShellEvolutionEventSource))
#define SHELL_EVOLUTION_EVENT_SOURCE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_EVOLUTION_EVENT_SOURCE, ShellEvolutionEventSourceClass))
#define SHELL_IS_EVOLUTION_EVENT_SOURCE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_EVOLUTION_EVENT_SOURCE))
#define SHELL_IS_EVOLUTION_EVENT_SOURCE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_EVOLUTION_EVENT_SOURCE))
#define SHELL_EVOLUTION_EVENT_SOURCE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_EVOLUTION_EVENT_SOURCE, ShellEvolutionEventSourceClass))

GType                      shell_evolution_event_source_get_type    (void) G_GNUC_CONST;
ShellEvolutionEventSource *shell_evolution_event_source_new         (void);
GList                     *shell_evolution_event_source_get_events  (ShellEvolutionEventSource *source,
                                                                     gint64                     date_begin,
                                                                     gint64                     date_end);
G_END_DECLS

#endif /* __SHELL_EVOLUTION_EVENT_SOURCE_H__ */
