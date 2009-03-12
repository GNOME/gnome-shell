/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file stack.h  Which windows cover which other windows
 *
 * There are two factors that determine window position.
 * 
 * One is window->stack_position, which is a unique integer
 * indicating how windows are ordered with respect to one
 * another. The ordering here transcends layers; it isn't changed
 * as the window is moved among layers. This allows us to move several
 * windows from one layer to another, while preserving the relative
 * order of the moved windows. Also, it allows us to restore
 * the stacking order from a saved session.
 * 
 * However when actually stacking windows on the screen, the
 * layer overrides the stack_position; windows are first sorted
 * by layer, then by stack_position within each layer.
 */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005 Elijah Newren
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

#ifndef META_STACK_H
#define META_STACK_H

#include "screen-private.h"

/**
 * A sorted list of windows bearing some level of resemblance to the stack of
 * windows on the X server.
 *
 * (This is only used as a field within a MetaScreen; we treat it as a separate
 * class for simplicity.)
 */
struct _MetaStack
{
  /** The MetaScreen containing this stack. */
  MetaScreen *screen;

  /**
   * A sequence of all the Windows (X handles, not MetaWindows) of the windows
   * we manage, sorted in order.  Suitable to be passed into _NET_CLIENT_LIST.
   */
  GArray *windows;

  /** The MetaWindows of the windows we manage, sorted in order. */
  GList *sorted;

  /**
   * MetaWindows waiting to be added to the "sorted" and "windows" list, after
   * being added by meta_stack_add() and before being assimilated by
   * stack_ensure_sorted().
   *
   * The order of the elements in this list is not important; what is important
   * is the stack_position element of each window.
   */
  GList *added;

  /**
   * Windows (X handles, not MetaWindows) waiting to be removed from the
   * "windows" list, after being removed by meta_stack_remove() and before
   * being assimilated by stack_ensure_sorted().  (We already removed them
   * from the "sorted" list.)
   *
   * The order of the elements in this list is not important.
   */
  GList *removed;
  
  /**
   * If this is zero, the local stack oughtn't to be brought up to date with
   * the X server's stack, because it is in the middle of being updated.
   * If it is positive, the local stack is said to be "frozen", and will need
   * to be thawed that many times before the stack can be brought up to date
   * again.  You may freeze the stack with meta_stack_freeze() and thaw it
   * with meta_stack_thaw().
   */
  int freeze_count;

  /**
   * The last-known stack of all windows, bottom to top.  We cache it here
   * so that subsequent times we'll be able to do incremental moves.
   */
  GArray *last_root_children_stacked;

  /**
   * Number of stack positions; same as the length of added, but
   * kept for quick reference.
   */
  gint n_positions;

  /** Is the stack in need of re-sorting? */
  unsigned int need_resort : 1;

  /**
   * Are the windows in the stack in need of having their
   * layers recalculated?
   */
  unsigned int need_relayer : 1;

  /**
   * Are the windows in the stack in need of having their positions
   * recalculated with respect to transiency (parent and child windows)?
   */
  unsigned int need_constrain : 1;
};

/**
 * Creates and initialises a MetaStack.
 *
 * \param screen  The MetaScreen which will be the parent of this stack.
 * \return The new screen.
 */
MetaStack *meta_stack_new       (MetaScreen     *screen);

/**
 * Destroys and frees a MetaStack.
 *
 * \param stack  The stack to destroy.
 */
void       meta_stack_free      (MetaStack      *stack);

/**
 * Adds a window to the local stack.  It is a fatal error to call this
 * function on a window which already exists on the stack of any screen.
 *
 * \param window  The window to add
 * \param stack  The stack to add it to
 */
void       meta_stack_add       (MetaStack      *stack,
                                 MetaWindow     *window);

/**
 * Removes a window from the local stack.  It is a fatal error to call this
 * function on a window which exists on the stack of any screen.
 *
 * \param window  The window to remove
 * \param stack   The stack to remove it from
 */
void       meta_stack_remove    (MetaStack      *stack,
                                 MetaWindow     *window);
/**
 * Recalculates the correct layer for all windows in the stack,
 * and moves them about accordingly.
 *
 * \param window  Dummy parameter
 * \param stack   The stack to recalculate
 * \bug What's with the dummy parameter?
 */
void       meta_stack_update_layer    (MetaStack      *stack,
                                       MetaWindow     *window);

/**
 * Recalculates the correct stacking order for all windows in the stack
 * according to their transience, and moves them about accordingly.
 *
 * \param window  Dummy parameter
 * \param stack   The stack to recalculate
 * \bug What's with the dummy parameter?
 */
void       meta_stack_update_transient (MetaStack     *stack,
                                        MetaWindow    *window);

/**
 * Move a window to the top of its layer.
 *
 * \param stack  The stack to modify.
 * \param window  The window that's making an ascension.
 *                (Amulet of Yendor not required.)
 */
void       meta_stack_raise     (MetaStack      *stack,
                                 MetaWindow     *window);
/**
 * Move a window to the bottom of its layer.
 *
 * \param stack  The stack to modify.
 * \param window  The window that's on the way downwards.
 */
void       meta_stack_lower     (MetaStack      *stack,
                                 MetaWindow     *window);

/**
 * Prevent syncing to server until the next call of meta_stack_thaw(),
 * so that we can carry out multiple operations in one go without having
 * everything halfway reflected on the X server.
 *
 * (Calls to meta_stack_freeze() nest, so that multiple calls to
 * meta_stack_freeze will require multiple calls to meta_stack_thaw().)
 *
 * \param stack  The stack to freeze.
 */
void       meta_stack_freeze    (MetaStack      *stack);

/**
 * Undoes a meta_stack_freeze(), and processes anything which has become
 * necessary during the freeze.  It is an error to call this function if
 * the stack has not been frozen.
 *
 * \param stack  The stack to thaw.
 */
void       meta_stack_thaw      (MetaStack      *stack);

/**
 * Finds the top window on the stack.
 *
 * \param stack  The stack to examine.
 * \return The top window on the stack, or NULL in the vanishingly unlikely
 *         event that you have no windows on your screen whatsoever.
 */
MetaWindow* meta_stack_get_top    (MetaStack  *stack);

/**
 * Finds the window at the bottom of the stack.  Since that's pretty much
 * always the desktop, this isn't the most useful of functions, and nobody
 * actually calls it.  We should probably get rid of it.
 *
 * \param stack  The stack to search
 */
MetaWindow* meta_stack_get_bottom (MetaStack  *stack);

/**
 * Finds the window above a given window in the stack.
 * It is not an error to pass in a window which does not exist in
 * the stack; the function will merely return NULL.
 *
 * \param stack   The stack to search.
 * \param window  The window to look above.
 * \param only_within_layer  If true, will return NULL if "window" is the
 *                           top window in its layer.
 * \return NULL if there is no such window;
 *         the window above "window" otherwise.
 */
MetaWindow* meta_stack_get_above  (MetaStack  *stack,
                                   MetaWindow *window,
                                   gboolean    only_within_layer);

/**
 * Finds the window below a given window in the stack.
 * It is not an error to pass in a window which does not exist in
 * the stack; the function will merely return NULL.
 *
 * \param stack   The stack to search.
 * \param window  The window to look below.
 * \param only_within_layer  If true, will return NULL if "window" is the
 *                           bottom window in its layer.
 * \return NULL if there is no such window;
 *         the window below "window" otherwise.
 */
MetaWindow* meta_stack_get_below  (MetaStack  *stack,
                                   MetaWindow *window,
                                   gboolean    only_within_layer);

/**
 * Find the topmost, focusable, mapped, window in a stack.  If you supply
 * a window as "not_this_one", we won't return that one (presumably
 * because it's going to be going away).  But if you do supply "not_this_one"
 * and we find its parent, we'll return that; and if "not_this_one" is in
 * a group, we'll return the top window of that group.
 *
 * Also, we are prejudiced against dock windows.  Every kind of window, even
 * the desktop, will be returned in preference to a dock window.
 *
 * \param stack  The stack to search.
 * \param workspace  NULL to search all workspaces; otherwise only windows
 *                   from that workspace will be returned.
 * \param not_this_one  Window to ignore because it's being unfocussed or
 *                      going away.
 * \return The window matching all these constraints or NULL if none does.
 * 
 * \bug Never called!
  */
MetaWindow* meta_stack_get_default_focus_window          (MetaStack     *stack,
                                                          MetaWorkspace *workspace,
                                                          MetaWindow    *not_this_one);

/**
 * Find the topmost, focusable, mapped, window in a stack.  If you supply
 * a window as "not_this_one", we won't return that one (presumably
 * because it's going to be going away).  But if you do supply "not_this_one"
 * and we find its parent, we'll return that; and if "not_this_one" is in
 * a group, we'll return the top window of that group.
 *
 * Also, we are prejudiced against dock windows.  Every kind of window, even
 * the desktop, will be returned in preference to a dock window.
 *
 * \param stack  The stack to search.
 * \param workspace  NULL to search all workspaces; otherwise only windows
 *                   from that workspace will be returned.
 * \param not_this_one  Window to ignore because it's being unfocussed or
 *                      going away.
 * \param root_x  The returned window must contain this point,
 *                unless it's a dock.
 * \param root_y  See root_x.
 * \return The window matching all these constraints or NULL if none does.
 */
MetaWindow* meta_stack_get_default_focus_window_at_point (MetaStack     *stack,
                                                          MetaWorkspace *workspace,
                                                          MetaWindow    *not_this_one,
                                                          int            root_x,
                                                          int            root_y);

/**
 * Finds all the windows in the stack, in order.
 *
 * \param stack  The stack to examine.
 * \param workspace  If non-NULL, only windows on this workspace will be
 *                   returned; otherwise all windows in the stack will be
 *                   returned.
 * \return A list of windows, in stacking order, honouring layers.
 */
GList*      meta_stack_list_windows (MetaStack *stack,
                                     MetaWorkspace *workspace);

/**
 * Comparison function for windows within a stack.  This is not directly
 * suitable for use within a standard comparison routine, because it takes
 * an extra parameter; you will need to wrap it.
 *
 * (FIXME: We could remove the stack parameter and use the stack of
 * the screen of window A, and complain if the stack of the screen of
 * window B differed; then this would be a usable general comparison function.)
 *
 * (FIXME: Apparently identical to compare_window_position(). Merge them.)
 *
 * \param stack  A stack containing both window_a and window_b
 * \param window_a  A window
 * \param window_b  Another window
 * \return -1 if window_a is below window_b, honouring layers; 1 if it's
 *         above it; 0 if you passed in the same window twice!
 */
int         meta_stack_windows_cmp  (MetaStack  *stack,
                                     MetaWindow *window_a,
                                     MetaWindow *window_b);

/**
 * Sets the position of a window within the stack.  This will only move it
 * up or down within its layer.  It is an error to attempt to move this
 * below position zero or above the last position in the stack (however, since
 * we don't provide a simple way to tell the number of windows in the stack,
 * this requirement may not be easy to fulfil).
 *
 * \param window  The window which is moving.
 * \param position  Where it should move to (0 is the bottom).
 */
void meta_window_set_stack_position (MetaWindow *window,
                                     int         position);

/**
 * Returns the current stack state, allowing rudimentary transactions.
 *
 * \param stack  The stack to examine.
 * \return An opaque GList representing the current stack sort order;
 *         it is the caller's responsibility to free it.
 *         Pass this to meta_stack_set_positions() later if you want to restore
 *         the state to where it was when you called this function.
 */
GList* meta_stack_get_positions (MetaStack *stack);

/**
 * Rolls back a transaction, given the list returned from
 * meta_stack_get_positions().
 *
 * \param stack  The stack to roll back.
 * \param windows  The list returned from meta_stack_get_positions().
 */
void   meta_stack_set_positions (MetaStack *stack,
                                 GList     *windows);

#endif
