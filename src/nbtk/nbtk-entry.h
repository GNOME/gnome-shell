/*
 * nbtk-entry.h: Plain entry actor
 *
 * Copyright 2008, 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Thomas Wood <thomas@linux.intel.com>
 *
 */

#if !defined(NBTK_H_INSIDE) && !defined(NBTK_COMPILATION)
#error "Only <nbtk/nbtk.h> can be included directly.h"
#endif

#ifndef __NBTK_ENTRY_H__
#define __NBTK_ENTRY_H__

G_BEGIN_DECLS

#include <nbtk/nbtk-widget.h>

#define NBTK_TYPE_ENTRY                (nbtk_entry_get_type ())
#define NBTK_ENTRY(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), NBTK_TYPE_ENTRY, NbtkEntry))
#define NBTK_IS_ENTRY(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NBTK_TYPE_ENTRY))
#define NBTK_ENTRY_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), NBTK_TYPE_ENTRY, NbtkEntryClass))
#define NBTK_IS_ENTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), NBTK_TYPE_ENTRY))
#define NBTK_ENTRY_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), NBTK_TYPE_ENTRY, NbtkEntryClass))

typedef struct _NbtkEntry              NbtkEntry;
typedef struct _NbtkEntryPrivate       NbtkEntryPrivate;
typedef struct _NbtkEntryClass         NbtkEntryClass;

/**
 * NbtkEntry:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */
struct _NbtkEntry
{
  /*< private >*/
  NbtkWidget parent_instance;

  NbtkEntryPrivate *priv;
};

struct _NbtkEntryClass
{
  NbtkWidgetClass parent_class;

  /* signals */
  void (*primary_icon_clicked) (NbtkEntry *entry);
  void (*secondary_icon_clicked) (NbtkEntry *entry);
};

GType nbtk_entry_get_type (void) G_GNUC_CONST;

NbtkWidget *          nbtk_entry_new              (const gchar *text);
G_CONST_RETURN gchar *nbtk_entry_get_text         (NbtkEntry   *entry);
void                  nbtk_entry_set_text         (NbtkEntry   *entry,
                                                   const gchar *text);
ClutterActor*         nbtk_entry_get_clutter_text (NbtkEntry   *entry);

void                  nbtk_entry_set_hint_text (NbtkEntry *entry,
                                                const gchar *text);
G_CONST_RETURN gchar *nbtk_entry_get_hint_text (NbtkEntry *entry);

void nbtk_entry_set_primary_icon_from_file (NbtkEntry   *entry,
                                            const gchar *filename);
void nbtk_entry_set_secondary_icon_from_file (NbtkEntry   *entry,
                                              const gchar *filename);

G_END_DECLS

#endif /* __NBTK_ENTRY_H__ */
