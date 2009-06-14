/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file stack-tracker.h  Track stacking order for compositor
 *
 * MetaStackTracker maintains the most accurate view we have at a
 * given point of time of the ordering of the children of the root
 * window (including override-redirect windows.) This is used to order
 * the windows when the compositor draws them.
 *
 * By contrast, MetaStack is responsible for keeping track of how we
 * think that windows *should* be ordered.  For windows we manage
 * (non-override-redirect windows), the two stacking orders will be
 * the same.
 */

/*
 * Copyright (C) 2009 Red Hat, Inc.
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

#ifndef META_STACK_TRACKER_H
#define META_STACK_TRACKER_H

#include "screen.h"

typedef struct _MetaStackTracker MetaStackTracker;

MetaStackTracker *meta_stack_tracker_new  (MetaScreen       *screen);
void              meta_stack_tracker_free (MetaStackTracker *tracker);

/* These functions are called when we make an X call that changes the
 * stacking order; this allows MetaStackTracker to predict stacking
 * order before it receives events back from the X server */
void meta_stack_tracker_record_add             (MetaStackTracker *tracker,
						Window            window,
                                                gulong            serial);
void meta_stack_tracker_record_remove          (MetaStackTracker *tracker,
						Window            window,
                                                gulong            serial);
void meta_stack_tracker_record_restack_windows (MetaStackTracker *tracker,
						Window           *windows,
						int               n_windows,
                                                gulong            serial);
void meta_stack_tracker_record_raise_above     (MetaStackTracker *tracker,
						Window            window,
						Window            sibling,
                                                gulong            serial);
void meta_stack_tracker_record_lower_below    (MetaStackTracker *tracker,
						Window            window,
						Window            sibling,
                                                gulong            serial);
void meta_stack_tracker_record_lower           (MetaStackTracker *tracker,
						Window            window,
                                                gulong            serial);

/* These functions are used to update the stack when we get events
 * reflecting changes to the stacking order */
void meta_stack_tracker_create_event    (MetaStackTracker    *tracker,
					 XCreateWindowEvent  *event);
void meta_stack_tracker_destroy_event   (MetaStackTracker    *tracker,
					 XDestroyWindowEvent *event);
void meta_stack_tracker_reparent_event  (MetaStackTracker    *tracker,
					 XReparentEvent      *event);
void meta_stack_tracker_configure_event (MetaStackTracker    *tracker,
					 XConfigureEvent     *event);

void meta_stack_tracker_get_stack  (MetaStackTracker  *tracker,
                                    Window           **windows,
                                    int               *n_windows);

void meta_stack_tracker_sync_stack       (MetaStackTracker *tracker);
void meta_stack_tracker_queue_sync_stack (MetaStackTracker *tracker);

#endif /* META_STACK_TRACKER_H */
