/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_STACK_H
#define META_STACK_H

/**
 * SECTION:stack
 * @short_description: Which windows cover which other windows
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
  GArray *xwindows;

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
  GArray *last_all_root_children_stacked;

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
 * meta_stack_new:
 * @screen: The MetaScreen which will be the parent of this stack.
 *
 * Creates and initialises a MetaStack.
 *
 * Returns: The new screen.
 */
MetaStack *meta_stack_new       (MetaScreen     *screen);

/**
 * meta_stack_free:
 * @stack: The stack to destroy.
 *
 * Destroys and frees a MetaStack.
 */
void       meta_stack_free      (MetaStack      *stack);

/**
 * meta_stack_add:
 * @stack: The stack to add it to
 * @window: The window to add
 *
 * Adds a window to the local stack.  It is a fatal error to call this
 * function on a window which already exists on the stack of any screen.
 */
void       meta_stack_add       (MetaStack      *stack,
                                 MetaWindow     *window);

/**
 * meta_stack_remove:
 * @stack: The stack to remove it from
 * @window: The window to remove
 *
 * Removes a window from the local stack.  It is a fatal error to call this
 * function on a window which exists on the stack of any screen.
 */
void       meta_stack_remove    (MetaStack      *stack,
                                 MetaWindow     *window);
/**
 * meta_stack_update_layer:
 * @stack: The stack to recalculate
 * @window: Dummy parameter
 *
 * Recalculates the correct layer for all windows in the stack,
 * and moves them about accordingly.
 *
 */
void       meta_stack_update_layer    (MetaStack      *stack,
                                       MetaWindow     *window);

/**
 * meta_stack_update_transient:
 * @stack: The stack to recalculate
 * @window: Dummy parameter
 *
 * Recalculates the correct stacking order for all windows in the stack
 * according to their transience, and moves them about accordingly.
 *
 * FIXME: What's with the dummy parameter?
 */
void       meta_stack_update_transient (MetaStack     *stack,
                                        MetaWindow    *window);

/**
 * meta_stack_raise:
 * @stack: The stack to modify.
 * @window: The window that's making an ascension.
 *              (Amulet of Yendor not required.)
 *
 * Move a window to the top of its layer.
 */
void       meta_stack_raise     (MetaStack      *stack,
                                 MetaWindow     *window);
/**
 * meta_stack_lower:
 * @stack: The stack to modify.
 * @window: The window that's on the way downwards.
 *
 * Move a window to the bottom of its layer.
 */
void       meta_stack_lower     (MetaStack      *stack,
                                 MetaWindow     *window);

/**
 * meta_stack_freeze:
 * @stack: The stack to freeze.
 *
 * Prevent syncing to server until the next call of meta_stack_thaw(),
 * so that we can carry out multiple operations in one go without having
 * everything halfway reflected on the X server.
 *
 * (Calls to meta_stack_freeze() nest, so that multiple calls to
 * meta_stack_freeze will require multiple calls to meta_stack_thaw().)
 */
void       meta_stack_freeze    (MetaStack      *stack);

/**
 * meta_stack_thaw:
 * @stack: The stack to thaw.
 *
 * Undoes a meta_stack_freeze(), and processes anything which has become
 * necessary during the freeze.  It is an error to call this function if
 * the stack has not been frozen.
 */
void       meta_stack_thaw      (MetaStack      *stack);

/**
 * meta_stack_get_top:
 * @stack: The stack to examine.
 *
 * Finds the top window on the stack.
 *
 * Returns: The top window on the stack, or %NULL in the vanishingly unlikely
 *          event that you have no windows on your screen whatsoever.
 */
MetaWindow* meta_stack_get_top    (MetaStack  *stack);

/**
 * meta_stack_get_bottom:
 * @stack: The stack to search
 *
 * Finds the window at the bottom of the stack.  Since that's pretty much
 * always the desktop, this isn't the most useful of functions, and nobody
 * actually calls it.  We should probably get rid of it.
 */
MetaWindow* meta_stack_get_bottom (MetaStack  *stack);

/**
 * meta_stack_get_above:
 * @stack: The stack to search.
 * @window: The window to look above.
 * @only_within_layer: If %TRUE, will return %NULL if @window is the
 *                     top window in its layer.
 *
 * Finds the window above a given window in the stack.
 * It is not an error to pass in a window which does not exist in
 * the stack; the function will merely return %NULL.
 *
 * Returns: %NULL if there is no such window;
 *          the window above @window otherwise.
 */
MetaWindow* meta_stack_get_above  (MetaStack  *stack,
                                   MetaWindow *window,
                                   gboolean    only_within_layer);

/**
 * meta_stack_get_below:
 * @stack: The stack to search.
 * @window: The window to look below.
 * @only_within_layer: If %TRUE, will return %NULL if window is the
 *                     bottom window in its layer.
 *
 * Finds the window below a given window in the stack.
 * It is not an error to pass in a window which does not exist in
 * the stack; the function will merely return %NULL.
 *
 *
 * Returns: %NULL if there is no such window;
 *          the window below @window otherwise.
 */
MetaWindow* meta_stack_get_below  (MetaStack  *stack,
                                   MetaWindow *window,
                                   gboolean    only_within_layer);

/**
 * meta_stack_get_default_focus_window:
 * @stack: The stack to search.
 * @workspace: %NULL to search all workspaces; otherwise only windows
 *             from that workspace will be returned.
 * @not_this_one: Window to ignore because it's being unfocussed or
 *                going away.
 *
 * Find the topmost, focusable, mapped, window in a stack. If you supply
 * a window as @not_this_one, we won't return that one (presumably
 * because it's going to be going away).  But if you do supply @not_this_one
 * and we find its parent, we'll return that; and if @not_this_one is in
 * a group, we'll return the top window of that group.
 *
 * Also, we are prejudiced against dock windows.  Every kind of window, even
 * the desktop, will be returned in preference to a dock window.
 *
 * Returns: The window matching all these constraints or %NULL if none does.
  */
MetaWindow* meta_stack_get_default_focus_window          (MetaStack     *stack,
                                                          MetaWorkspace *workspace,
                                                          MetaWindow    *not_this_one);

/**
 * meta_stack_get_default_focus_window_at_point:
 * @stack: The stack to search.
 * @workspace: %NULL to search all workspaces; otherwise only windows
 *             from that workspace will be returned.
 * @not_this_one: Window to ignore because it's being unfocussed or
 *                going away.
 * @root_x: The returned window must contain this point,
 *          unless it's a dock.
 * @root_y: See root_x.
 *
 * Find the topmost, focusable, mapped, window in a stack.  If you supply
 * a window as @not_this_one, we won't return that one (presumably
 * because it's going to be going away).  But if you do supply @not_this_one
 * and we find its parent, we'll return that; and if @not_this_one is in
 * a group, we'll return the top window of that group.
 *
 * Also, we are prejudiced against dock windows.  Every kind of window, even
 * the desktop, will be returned in preference to a dock window.
 *
 * Returns: The window matching all these constraints or %NULL if none does.
 */
MetaWindow* meta_stack_get_default_focus_window_at_point (MetaStack     *stack,
                                                          MetaWorkspace *workspace,
                                                          MetaWindow    *not_this_one,
                                                          int            root_x,
                                                          int            root_y);

/**
 * meta_stack_list_windows:
 * @stack: The stack to examine.
 * @workspace: If not %NULL, only windows on this workspace will be
 *             returned; otherwise all windows in the stack will be
 *             returned.
 *
 * Finds all the windows in the stack, in order.
 *
 * Returns: A list of windows, in stacking order, honouring layers.
 */
GList*      meta_stack_list_windows (MetaStack *stack,
                                     MetaWorkspace *workspace);

/**
 * meta_stack_windows_cmp:
 * @stack: A stack containing both window_a and window_b
 * @window_a: A window
 * @window_b  Another window
 *
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
 * \return -1 if window_a is below window_b, honouring layers; 1 if it's
 *         above it; 0 if you passed in the same window twice!
 */
int         meta_stack_windows_cmp  (MetaStack  *stack,
                                     MetaWindow *window_a,
                                     MetaWindow *window_b);

/**
 * meta_window_set_stack_position:
 * @window: The window which is moving.
 * @position:  Where it should move to (0 is the bottom).
 *
 * Sets the position of a window within the stack.  This will only move it
 * up or down within its layer.  It is an error to attempt to move this
 * below position zero or above the last position in the stack (however, since
 * we don't provide a simple way to tell the number of windows in the stack,
 * this requirement may not be easy to fulfil).
 */
void meta_window_set_stack_position (MetaWindow *window,
                                     int         position);

/**
 * meta_stack_get_positions:
 * @stack: The stack to examine.
 *
 * Returns the current stack state, allowing rudimentary transactions.
 *
 * Returns: An opaque GList representing the current stack sort order;
 *          it is the caller's responsibility to free it.
 *          Pass this to meta_stack_set_positions() later if you want to restore
 *          the state to where it was when you called this function.
 */
GList* meta_stack_get_positions (MetaStack *stack);

/**
 * meta_stack_set_positions:
 * @stack:  The stack to roll back.
 * @windows:  The list returned from meta_stack_get_positions().
 *
 * Rolls back a transaction, given the list returned from
 * meta_stack_get_positions().
 *
 */
void   meta_stack_set_positions (MetaStack *stack,
                                 GList     *windows);

void meta_stack_update_window_tile_matches (MetaStack     *stack,
                                            MetaWorkspace *workspace);
#endif
