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
#include <X11/Xproto.h>
#include <X11/Xlibint.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

struct _AgGetPropertyTask
{
  Display *display;
  Window window;
  Atom property;

  _XAsyncHandler async;

  unsigned long request_seq;
  int error;

  Atom actual_type;
  int  actual_format;

  unsigned long  n_items;
  unsigned long  bytes_after;
  unsigned char *data;

  Bool have_reply;

  AgGetPropertyTask *next;
};

static AgGetPropertyTask *pending_tasks = NULL;
static AgGetPropertyTask *pending_tasks_tail = NULL;
static AgGetPropertyTask *completed_tasks = NULL;
static AgGetPropertyTask *completed_tasks_tail = NULL;
static int n_tasks_pending = 0;
static int n_tasks_completed = 0;

static void
append_to_list (AgGetPropertyTask **head,
                AgGetPropertyTask **tail,
                AgGetPropertyTask  *task)
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
remove_from_list (AgGetPropertyTask **head,
                  AgGetPropertyTask **tail,
                  AgGetPropertyTask  *task)
{
  AgGetPropertyTask *prev;
  AgGetPropertyTask *node;

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
move_to_completed (AgGetPropertyTask *task)
{
  remove_from_list (&pending_tasks,
                    &pending_tasks_tail,
                    task);
  
  append_to_list (&completed_tasks,
                  &completed_tasks_tail,
                  task);

  --n_tasks_pending;
  ++n_tasks_completed;
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
  int bytes_read;

  task = (AgGetPropertyTask*) data;

#if 0
  printf ("%s: waiting for %ld seeing %ld buflen %d\n", __FUNCTION__,
          task->request_seq, dpy->last_request_read, len);
#endif

  if (dpy->last_request_read != task->request_seq)
    return False;

  task->have_reply = True;
  move_to_completed (task);
  
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
              (task->data = (unsigned char *) Xmalloc ((unsigned)nbytes + 1)))
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
              (task->data = (unsigned char *) Xmalloc ((unsigned)nbytes + 1)))
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
          nbytes = reply->nItems * sizeof (long);
          netbytes = reply->nItems << 2;
          if (nbytes + 1 > 0 &&
              (task->data = (unsigned char *) Xmalloc ((unsigned)nbytes + 1)))
            {
#ifdef DEBUG_SPEW
              printf ("%s: already read %d bytes using %ld more, eating %ld more\n",
                      __FUNCTION__, bytes_read, nbytes, netbytes);
#endif
              /* _XRead32 (dpy, (long *) task->data, netbytes); */
              _XGetAsyncData (dpy, task->data, buf, len,
                              bytes_read, nbytes,
                              netbytes);
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
  xError error;

  /* Fire up our request */
  LockDisplay (dpy);
  GetReq (GetProperty, req);
  req->window = window;
  req->property = property;
  req->type = req_type;
  req->delete = delete;
  req->longOffset = offset;
  req->longLength = length;

  error.sequenceNumber = dpy->request;

  /* Queue up our async task */
  task = Xcalloc (1, sizeof (AgGetPropertyTask));
  if (task == NULL)
    {
      UnlockDisplay (dpy);
      return NULL;
    }

  task->display = dpy;
  task->window = window;
  task->property = property;
  task->request_seq = dpy->request;
  task->async.next = dpy->async_handlers;
  task->async.handler = async_get_property_handler;
  task->async.data = (XPointer) task;
  dpy->async_handlers = &task->async;

  append_to_list (&pending_tasks,
                  &pending_tasks_tail,
                  task);
  ++n_tasks_pending;
  
  UnlockDisplay (dpy);

  SyncHandle ();

  return task;
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

  dpy = task->display; /* Xlib macros require a variable named "dpy" */
  
  if (task->error != Success)
    {
      Status s = task->error;
      DeqAsyncHandler (task->display, &task->async);
      remove_from_list (&completed_tasks, &completed_tasks_tail, task);
      --n_tasks_completed;
      XFree (task);
      return s;
    }

  if (!task->have_reply)
    {
      DeqAsyncHandler (task->display, &task->async);
      remove_from_list (&completed_tasks, &completed_tasks_tail, task);
      --n_tasks_completed;
      XFree (task);
      return BadAlloc; /* not Success */
    }

  *actual_type = task->actual_type;
  *actual_format = task->actual_format;
  *nitems = task->n_items;
  *bytesafter = task->bytes_after;

  *prop = task->data; /* pass out ownership of task->data */

  SyncHandle ();

  DeqAsyncHandler (dpy, &task->async);
  remove_from_list (&completed_tasks, &completed_tasks_tail, task);
  --n_tasks_completed;
  XFree (task);
  
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
  return task->display;
}

AgGetPropertyTask*
ag_get_next_completed_task (Display *display)
{
  AgGetPropertyTask *node;

#ifdef DEBUG_SPEW
  printf ("%d pending %d completed\n", n_tasks_pending, n_tasks_completed);
#endif
  
  node = completed_tasks;
  while (node != NULL)
    {
      if (node->display == display)
        return node;
      
      node = node->next;
    }

  return NULL;
}
