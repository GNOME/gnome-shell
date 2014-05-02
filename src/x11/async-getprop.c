/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Asynchronous X property getting hack */

/*
 * Copyright (C) 2002 Havoc Pennington
 * Copyright (C) 1986, 1998  The Open Group
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

#include <assert.h>

#undef DEBUG_SPEW
#ifdef DEBUG_SPEW
#include <stdio.h>
#endif

#include "async-getprop.h"

#define NEED_REPLIES
#include <X11/Xlibint.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct _ListNode ListNode;
typedef struct _AgPerDisplayData AgPerDisplayData;

struct _ListNode
{
  ListNode *next;
};

struct _AgGetPropertyTask
{
  ListNode node;

  AgPerDisplayData *dd;
  Window window;
  Atom property;

  unsigned long request_seq;
  int error;

  Atom actual_type;
  int  actual_format;

  unsigned long  n_items;
  unsigned long  bytes_after;
  char          *data;

  Bool have_reply;
};

struct _AgPerDisplayData
{
  ListNode node;
  _XAsyncHandler async;

  Display *display;
  ListNode *pending_tasks;
  ListNode *pending_tasks_tail;
  ListNode *completed_tasks;
  ListNode *completed_tasks_tail;
  int n_tasks_pending;
  int n_tasks_completed;
};

static ListNode *display_datas = NULL;
static ListNode *display_datas_tail = NULL;

static void
append_to_list (ListNode **head,
                ListNode **tail,
                ListNode  *task)
{
  task->next = NULL;

  if (*tail == NULL)
    {
      assert (*head == NULL);
      *head = task;
      *tail = task;
    }
  else
    {
      (*tail)->next = task;
      *tail = task;
    }
}

static void
remove_from_list (ListNode **head,
                  ListNode **tail,
                  ListNode  *task)
{
  ListNode *prev;
  ListNode *node;

  prev = NULL;
  node = *head;
  while (node != NULL)
    {
      if (node == task)
        {
          if (prev)
            prev->next = node->next;
          else
            *head = node->next;

          if (node == *tail)
            *tail = prev;

          break;
        }

      prev = node;
      node = node->next;
    }

  /* can't remove what's not there */
  assert (node != NULL);

  node->next = NULL;
}

static void
move_to_completed (AgPerDisplayData  *dd,
                   AgGetPropertyTask *task)
{
  remove_from_list (&dd->pending_tasks,
                    &dd->pending_tasks_tail,
                    &task->node);

  append_to_list (&dd->completed_tasks,
                  &dd->completed_tasks_tail,
                  &task->node);

  dd->n_tasks_pending -= 1;
  dd->n_tasks_completed += 1;
}

static AgGetPropertyTask*
find_pending_by_request_sequence (AgPerDisplayData *dd,
                                  unsigned long     request_seq)
{
  ListNode *node;

  /* if the sequence is after our last pending task, we
   * aren't going to find a match
   */
  {
    AgGetPropertyTask *task = (AgGetPropertyTask*) dd->pending_tasks_tail;
    if (task != NULL)
      {
        if (task->request_seq < request_seq)
          return NULL;
        else if (task->request_seq == request_seq)
          return task; /* why not check this */
      }
  }

  /* Generally we should get replies in the order we sent
   * requests, so we should usually be using the task
   * at the head of the list, if we use any task at all.
   * I'm not sure this is 100% guaranteed, if it is,
   * it would be a big speedup.
   */

  node = dd->pending_tasks;
  while (node != NULL)
    {
      AgGetPropertyTask *task = (AgGetPropertyTask*) node;

      if (task->request_seq == request_seq)
        return task;

      node = node->next;
    }

  return NULL;
}

static Bool
async_get_property_handler (Display *dpy,
                            xReply  *rep,
                            char    *buf,
                            int      len,
                            XPointer data)
{
  xGetPropertyReply  replbuf;
  xGetPropertyReply *reply;
  AgGetPropertyTask *task;
  AgPerDisplayData *dd;
  int bytes_read;

  dd = (AgPerDisplayData*) data;

#if 0
  printf ("%s: seeing request seq %ld buflen %d\n", __FUNCTION__,
          dpy->last_request_read, len);
#endif

  task = find_pending_by_request_sequence (dd, dpy->last_request_read);

  if (task == NULL)
    return False;

  assert (dpy->last_request_read == task->request_seq);

  task->have_reply = True;
  move_to_completed (dd, task);

  /* read bytes so far */
  bytes_read = SIZEOF (xReply);

  if (rep->generic.type == X_Error)
    {
      xError errbuf;

      task->error = rep->error.errorCode;

#ifdef DEBUG_SPEW
      printf ("%s: error code = %d (ignoring error, eating %d bytes, generic.length = %ld)\n",
              __FUNCTION__, task->error, (SIZEOF (xError) - bytes_read),
              rep->generic.length);
#endif

      /* We return True (meaning we consumed the reply)
       * because otherwise it would invoke the X error handler,
       * and an async API is useless if you have to synchronously
       * trap X errors. Also GetProperty can always fail, pretty
       * much, so trapping errors is always what you want.
       *
       * We have to eat all the error reply data here.
       * (kind of a charade as we know sizeof(xError) == sizeof(xReply))
       *
       * Passing discard = True seems to break things; I don't understand
       * why, because there should be no extra data in an error reply,
       * right?
       */
      _XGetAsyncReply (dpy, (char *)&errbuf, rep, buf, len,
                       (SIZEOF (xError) - bytes_read) >> 2, /* in 32-bit words */
                       False); /* really seems like it should be True */

      return True;
    }

#ifdef DEBUG_SPEW
  printf ("%s: already read %d bytes reading %d more for total of %d; generic.length = %ld\n",
          __FUNCTION__, bytes_read, (SIZEOF (xGetPropertyReply) - bytes_read) >> 2,
          SIZEOF (xGetPropertyReply), rep->generic.length);
#endif

  /* (kind of a silly as we know sizeof(xGetPropertyReply) == sizeof(xReply)) */
  reply = (xGetPropertyReply *)
    _XGetAsyncReply (dpy, (char *)&replbuf, rep, buf, len,
                     (SIZEOF (xGetPropertyReply) - bytes_read) >> 2, /* in 32-bit words */
                     False); /* False means expecting more data to follow,
                              * don't eat the rest of the reply
                              */

  bytes_read = SIZEOF (xGetPropertyReply);

#ifdef DEBUG_SPEW
  printf ("%s: have reply propertyType = %ld format = %d n_items = %ld\n",
          __FUNCTION__, reply->propertyType, reply->format, reply->nItems);
#endif

  assert (task->data == NULL);

  /* This is all copied from XGetWindowProperty().  Not sure we should
   * LockDisplay(). Not sure I'm passing the right args to
   * XGetAsyncData(). Not sure about a lot of things.
   */

  /* LockDisplay (dpy); */

  if (reply->propertyType != None)
    {
      long nbytes, netbytes;

      /* this alignment macro from orbit2 */
#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

      switch (reply->format)
        {
          /*
           * One extra byte is malloced than is needed to contain the property
           * data, but this last byte is null terminated and convenient for
           * returning string properties, so the client doesn't then have to
           * recopy the string to make it null terminated.
           */
        case 8:
          nbytes = reply->nItems;
          /* there's padding to word boundary */
          netbytes = ALIGN_VALUE (nbytes, 4);
          if (nbytes + 1 > 0 &&
              (task->data = (char *) Xmalloc ((unsigned)nbytes + 1)))
            {
#ifdef DEBUG_SPEW
              printf ("%s: already read %d bytes using %ld, more eating %ld more\n",
                      __FUNCTION__, bytes_read, nbytes, netbytes);
#endif
              /* _XReadPad (dpy, (char *) task->data, netbytes); */
              _XGetAsyncData (dpy, task->data, buf, len,
                              bytes_read, nbytes,
                              netbytes);
            }
          break;

        case 16:
          nbytes = reply->nItems * sizeof (short);
          netbytes = reply->nItems << 1;
          netbytes = ALIGN_VALUE (netbytes, 4); /* align to word boundary */
          if (nbytes + 1 > 0 &&
              (task->data = (char *) Xmalloc ((unsigned)nbytes + 1)))
            {
#ifdef DEBUG_SPEW
              printf ("%s: already read %d bytes using %ld more, eating %ld more\n",
                      __FUNCTION__, bytes_read, nbytes, netbytes);
#endif
              /* _XRead16Pad (dpy, (short *) task->data, netbytes); */
              _XGetAsyncData (dpy, task->data, buf, len,
                              bytes_read, nbytes, netbytes);
            }
          break;

        case 32:
          /* NOTE buffer is in longs to match XGetWindowProperty() */
          nbytes = reply->nItems * sizeof (long);
          netbytes = reply->nItems << 2; /* wire size is always 32 bits though */
          if (nbytes + 1 > 0 &&
              (task->data = (char *) Xmalloc ((unsigned)nbytes + 1)))
            {
#ifdef DEBUG_SPEW
              printf ("%s: already read %d bytes using %ld more, eating %ld more\n",
                      __FUNCTION__, bytes_read, nbytes, netbytes);
#endif

              /* We have to copy the XGetWindowProperty() crackrock
               * and get format 32 as long even on 64-bit platforms.
               */
              if (sizeof (long) == 8)
                {
                  char *netdata;
                  char *lptr;
                  char *end_lptr;

                  /* Store the 32-bit values in the end of the array */
                  netdata = task->data + nbytes / 2;

                  _XGetAsyncData (dpy, netdata, buf, len,
                                  bytes_read, netbytes,
                                  netbytes);

                  /* Now move the 32-bit values to the front */

                  lptr = task->data;
                  end_lptr = task->data + nbytes;
                  while (lptr != end_lptr)
                    {
                      *(long*) lptr = *(CARD32*) netdata;
                      lptr += sizeof (long);
                      netdata += sizeof (CARD32);
                    }
                }
              else
                {
                  /* Here the wire format matches our actual format */
                  _XGetAsyncData (dpy, task->data, buf, len,
                                  bytes_read, netbytes,
                                  netbytes);
                }
            }
          break;

        default:
          /*
           * This part of the code should never be reached.  If it is,
           * the server sent back a property with an invalid format.
           * This is a BadImplementation error.
           *
           * However this async GetProperty API doesn't report errors
           * via the standard X mechanism, so don't do anything about
           * it, other than store it in task->error.
           */
          {
#if 0
            xError error;
#endif

            task->error = BadImplementation;

#if 0
            error.sequenceNumber = task->request_seq;
            error.type = X_Error;
            error.majorCode = X_GetProperty;
            error.minorCode = 0;
            error.errorCode = BadImplementation;

            _XError (dpy, &error);
#endif
          }

          nbytes = netbytes = 0L;
          break;
        }

      if (task->data == NULL)
        {
          task->error = BadAlloc;

#ifdef DEBUG_SPEW
          printf ("%s: already read %d bytes eating %ld\n",
                  __FUNCTION__, bytes_read, netbytes);
#endif
          /* _XEatData (dpy, (unsigned long) netbytes); */
          _XGetAsyncData (dpy, NULL, buf, len,
                          bytes_read, 0, netbytes);

          /* UnlockDisplay (dpy); */
          return BadAlloc; /* not Success */
        }

      (task->data)[nbytes] = '\0';
    }

#ifdef DEBUG_SPEW
  printf ("%s: have data\n", __FUNCTION__);
#endif

  task->actual_type = reply->propertyType;
  task->actual_format = reply->format;
  task->n_items = reply->nItems;
  task->bytes_after = reply->bytesAfter;

  /* UnlockDisplay (dpy); */

  return True;
}

static AgPerDisplayData*
get_display_data (Display *display,
                  Bool     create)
{
  ListNode *node;
  AgPerDisplayData *dd;

  node = display_datas;
  while (node != NULL)
    {
      dd = (AgPerDisplayData*) node;

      if (dd->display == display)
        return dd;

      node = node->next;
    }

  if (!create)
    return NULL;

  dd = Xcalloc (1, sizeof (AgPerDisplayData));
  if (dd == NULL)
    return NULL;

  dd->display = display;
  dd->async.next = display->async_handlers;
  dd->async.handler = async_get_property_handler;
  dd->async.data = (XPointer) dd;
  dd->display->async_handlers = &dd->async;

  append_to_list (&display_datas,
                  &display_datas_tail,
                  &dd->node);

  return dd;
}

static void
maybe_free_display_data (AgPerDisplayData *dd)
{
  if (dd->pending_tasks == NULL &&
      dd->completed_tasks == NULL)
    {
      DeqAsyncHandler (dd->display, &dd->async);
      remove_from_list (&display_datas, &display_datas_tail,
                        &dd->node);
      XFree (dd);
    }
}

AgGetPropertyTask*
ag_task_create (Display *dpy,
                Window   window,
                Atom     property,
                long     offset,
                long     length,
                Bool     delete,
                Atom     req_type)
{
  AgGetPropertyTask *task;
  xGetPropertyReq *req;
  AgPerDisplayData *dd;

  /* Fire up our request */
  LockDisplay (dpy);

  dd = get_display_data (dpy, True);
  if (dd == NULL)
    {
      UnlockDisplay (dpy);
      return NULL;
    }

  GetReq (GetProperty, req);
  req->window = window;
  req->property = property;
  req->type = req_type;
  req->delete = delete;
  req->longOffset = offset;
  req->longLength = length;

  /* Queue up our async task */
  task = Xcalloc (1, sizeof (AgGetPropertyTask));
  if (task == NULL)
    {
      UnlockDisplay (dpy);
      return NULL;
    }

  task->dd = dd;
  task->window = window;
  task->property = property;
  task->request_seq = dpy->request;

  append_to_list (&dd->pending_tasks,
                  &dd->pending_tasks_tail,
                  &task->node);
  dd->n_tasks_pending += 1;

  UnlockDisplay (dpy);

  SyncHandle ();

  return task;
}

static void
free_task (AgGetPropertyTask *task)
{
  remove_from_list (&task->dd->completed_tasks,
                    &task->dd->completed_tasks_tail,
                    &task->node);
  task->dd->n_tasks_completed -= 1;
  maybe_free_display_data (task->dd);
  XFree (task);
}

Status
ag_task_get_reply_and_free (AgGetPropertyTask  *task,
                            Atom               *actual_type,
                            int                *actual_format,
                            unsigned long      *nitems,
                            unsigned long      *bytesafter,
                            unsigned char     **prop)
{
  Display *dpy;

  *prop = NULL;

  dpy = task->dd->display; /* Xlib macros require a variable named "dpy" */

  if (task->error != Success)
    {
      Status s = task->error;

      free_task (task);

      return s;
    }

  if (!task->have_reply)
    {
      free_task (task);

      return BadAlloc; /* not Success */
    }

  *actual_type = task->actual_type;
  *actual_format = task->actual_format;
  *nitems = task->n_items;
  *bytesafter = task->bytes_after;

  *prop = (unsigned char*) task->data; /* pass out ownership of task->data */

  SyncHandle ();

  free_task (task);

  return Success;
}

Bool
ag_task_have_reply (AgGetPropertyTask *task)
{
  return task->have_reply;
}

Atom
ag_task_get_property (AgGetPropertyTask *task)
{
  return task->property;
}

Window
ag_task_get_window (AgGetPropertyTask *task)
{
  return task->window;
}

Display*
ag_task_get_display (AgGetPropertyTask *task)
{
  return task->dd->display;
}

AgGetPropertyTask*
ag_get_next_completed_task (Display *display)
{
  AgPerDisplayData *dd;

  dd = get_display_data (display, False);

  if (dd == NULL)
    return NULL;

#ifdef DEBUG_SPEW
  printf ("%d pending %d completed\n",
          dd->n_tasks_pending,
          dd->n_tasks_completed);
#endif

  return (AgGetPropertyTask*) dd->completed_tasks;
}

void*
ag_Xmalloc (unsigned long bytes)
{
  return (void*) Xmalloc (bytes);
}

void*
ag_Xmalloc0 (unsigned long bytes)
{
  return (void*) Xcalloc (bytes, 1);
}
