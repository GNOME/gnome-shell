/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2012, 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/* This list implementation is based on the Wayland source code */

#ifndef COGL_LIST_H
#define COGL_LIST_H

#include <stddef.h>

/**
 * CoglList - linked list
 *
 * The list head is of "CoglList" type, and must be initialized
 * using cogl_list_init().  All entries in the list must be of the same
 * type.  The item type must have a "CoglList" member. This
 * member will be initialized by cogl_list_insert(). There is no need to
 * call cogl_list_init() on the individual item. To query if the list is
 * empty in O(1), use cogl_list_empty().
 *
 * Let's call the list reference "CoglList foo_list", the item type as
 * "item_t", and the item member as "CoglList link". The following code
 *
 * The following code will initialize a list:
 *
 *      cogl_list_init (foo_list);
 *      cogl_list_insert (foo_list, item1);      Pushes item1 at the head
 *      cogl_list_insert (foo_list, item2);      Pushes item2 at the head
 *      cogl_list_insert (item2, item3);         Pushes item3 after item2
 *
 * The list now looks like [item2, item3, item1]
 *
 * Will iterate the list in ascending order:
 *
 *      item_t *item;
 *      cogl_list_for_each(item, foo_list, link) {
 *              Do_something_with_item(item);
 *      }
 */

typedef struct _CoglList CoglList;

struct _CoglList
{
  CoglList *prev;
  CoglList *next;
};

void
_cogl_list_init (CoglList *list);

void
_cogl_list_insert (CoglList *list,
                   CoglList *elm);

void
_cogl_list_remove (CoglList *elm);

int
_cogl_list_length (CoglList *list);

int
_cogl_list_empty (CoglList *list);

void
_cogl_list_insert_list (CoglList *list,
                        CoglList *other);

/* This assigns to iterator first so that taking a reference to it
 * later in the second step won't be an undefined operation. It
 * assigns the value of list_node rather than 0 so that it is possible
 * have list_node be based on the previous value of iterator. In that
 * respect iterator is just used as a convenient temporary variable.
 * The compiler optimises all of this down to a single subtraction by
 * a constant */
#define _cogl_list_set_iterator(list_node, iterator, member)    \
  ((iterator) = (void *) (list_node),                           \
   (iterator) = (void *) ((char *) (iterator) -                 \
                          (((char *) &(iterator)->member) -     \
                           (char *) (iterator))))

#define _cogl_container_of(ptr, type, member)           \
  (type *) ((char *) (ptr) - offsetof (type, member))

#define _cogl_list_for_each(pos, head, member)                          \
  for (_cogl_list_set_iterator ((head)->next, pos, member);             \
       &pos->member != (head);                                          \
       _cogl_list_set_iterator (pos->member.next, pos, member))

#define _cogl_list_for_each_safe(pos, tmp, head, member)                \
  for (_cogl_list_set_iterator ((head)->next, pos, member),             \
         _cogl_list_set_iterator ((pos)->member.next, tmp, member);     \
       &pos->member != (head);                                          \
       pos = tmp,                                                       \
         _cogl_list_set_iterator (pos->member.next, tmp, member))

#define _cogl_list_for_each_reverse(pos, head, member)                  \
  for (_cogl_list_set_iterator ((head)->prev, pos, member);             \
       &pos->member != (head);                                          \
       _cogl_list_set_iterator (pos->member.prev, pos, member))

#define _cogl_list_for_each_reverse_safe(pos, tmp, head, member)        \
  for (_cogl_list_set_iterator ((head)->prev, pos, member),             \
         _cogl_list_set_iterator ((pos)->member.prev, tmp, member);     \
       &pos->member != (head);                                          \
       pos = tmp,                                                       \
         _cogl_list_set_iterator (pos->member.prev, tmp, member))

#endif /* COGL_LIST_H */
