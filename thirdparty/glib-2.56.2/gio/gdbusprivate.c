/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "giotypes.h"
#include "gioenumtypes.h"
#include "gsocket.h"
#include "gdbusauthobserver.h"
#include "gdbusprivate.h"
#include "gdbusmessage.h"
#include "gdbusconnection.h"
#include "gdbusproxy.h"
#include "gdbuserror.h"
#include "gdbusintrospection.h"
#include "gtask.h"
#include "ginputstream.h"
#include "gmemoryinputstream.h"
#include "giostream.h"
#include "glib/gstdio.h"
#include "gsocketaddress.h"
#include "gsocketcontrolmessage.h"
#include "gsocketconnection.h"
#include "gsocketoutputstream.h"

#ifdef G_OS_UNIX
#include "gunixfdmessage.h"
#include "gunixconnection.h"
#include "gunixcredentialsmessage.h"
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "glibintl.h"

static gboolean _g_dbus_worker_do_initial_read (gpointer data);
static void schedule_pending_close (GDBusWorker *worker);

/* ---------------------------------------------------------------------------------------------------- */

gchar *
_g_dbus_hexdump (const gchar *data, gsize len, guint indent)
{
 guint n, m;
 GString *ret;

 ret = g_string_new (NULL);

 for (n = 0; n < len; n += 16)
   {
     g_string_append_printf (ret, "%*s%04x: ", indent, "", n);

     for (m = n; m < n + 16; m++)
       {
         if (m > n && (m%4) == 0)
           g_string_append_c (ret, ' ');
         if (m < len)
           g_string_append_printf (ret, "%02x ", (guchar) data[m]);
         else
           g_string_append (ret, "   ");
       }

     g_string_append (ret, "   ");

     for (m = n; m < len && m < n + 16; m++)
       g_string_append_c (ret, g_ascii_isprint (data[m]) ? data[m] : '.');

     g_string_append_c (ret, '\n');
   }

 return g_string_free (ret, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Unfortunately ancillary messages are discarded when reading from a
 * socket using the GSocketInputStream abstraction. So we provide a
 * very GInputStream-ish API that uses GSocket in this case (very
 * similar to GSocketInputStream).
 */

typedef struct
{
  void *buffer;
  gsize count;

  GSocketControlMessage ***messages;
  gint *num_messages;
} ReadWithControlData;

static void
read_with_control_data_free (ReadWithControlData *data)
{
  g_slice_free (ReadWithControlData, data);
}

static gboolean
_g_socket_read_with_control_messages_ready (GSocket      *socket,
                                            GIOCondition  condition,
                                            gpointer      user_data)
{
  GTask *task = user_data;
  ReadWithControlData *data = g_task_get_task_data (task);
  GError *error;
  gssize result;
  GInputVector vector;

  error = NULL;
  vector.buffer = data->buffer;
  vector.size = data->count;
  result = g_socket_receive_message (socket,
                                     NULL, /* address */
                                     &vector,
                                     1,
                                     data->messages,
                                     data->num_messages,
                                     NULL,
                                     g_task_get_cancellable (task),
                                     &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    {
      g_error_free (error);
      return TRUE;
    }

  g_assert (result >= 0 || error != NULL);
  if (result >= 0)
    g_task_return_int (task, result);
  else
    g_task_return_error (task, error);
  g_object_unref (task);

  return FALSE;
}

static void
_g_socket_read_with_control_messages (GSocket                 *socket,
                                      void                    *buffer,
                                      gsize                    count,
                                      GSocketControlMessage ***messages,
                                      gint                    *num_messages,
                                      gint                     io_priority,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  GTask *task;
  ReadWithControlData *data;
  GSource *source;

  data = g_slice_new0 (ReadWithControlData);
  data->buffer = buffer;
  data->count = count;
  data->messages = messages;
  data->num_messages = num_messages;

  task = g_task_new (socket, cancellable, callback, user_data);
  g_task_set_source_tag (task, _g_socket_read_with_control_messages);
  g_task_set_task_data (task, data, (GDestroyNotify) read_with_control_data_free);

  if (g_socket_condition_check (socket, G_IO_IN))
    {
      if (!_g_socket_read_with_control_messages_ready (socket, G_IO_IN, task))
        return;
    }

  source = g_socket_create_source (socket,
                                   G_IO_IN | G_IO_HUP | G_IO_ERR,
                                   cancellable);
  g_task_attach_source (task, source, (GSourceFunc) _g_socket_read_with_control_messages_ready);
  g_source_unref (source);
}

static gssize
_g_socket_read_with_control_messages_finish (GSocket       *socket,
                                             GAsyncResult  *result,
                                             GError       **error)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), -1);
  g_return_val_if_fail (g_task_is_valid (result, socket), -1);

  return g_task_propagate_int (G_TASK (result), error);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Work-around for https://bugzilla.gnome.org/show_bug.cgi?id=674885
   and see also the original https://bugzilla.gnome.org/show_bug.cgi?id=627724  */

static GPtrArray *ensured_classes = NULL;

static void
ensure_type (GType gtype)
{
  g_ptr_array_add (ensured_classes, g_type_class_ref (gtype));
}

static void
release_required_types (void)
{
  g_ptr_array_foreach (ensured_classes, (GFunc) g_type_class_unref, NULL);
  g_ptr_array_unref (ensured_classes);
  ensured_classes = NULL;
}

static void
ensure_required_types (void)
{
  g_assert (ensured_classes == NULL);
  ensured_classes = g_ptr_array_new ();
  /* Generally in this list, you should initialize types which are used as
   * properties first, then the class which has them. For example, GDBusProxy
   * has a type of GDBusConnection, so we initialize GDBusConnection first.
   * And because GDBusConnection has a property of type GDBusConnectionFlags,
   * we initialize that first.
   *
   * Similarly, GSocket has a type of GSocketAddress.
   *
   * We don't fill out the whole dependency tree right now because in practice
   * it tends to be just types that GDBus use that cause pain, and there
   * is work on a more general approach in https://bugzilla.gnome.org/show_bug.cgi?id=674885
   */
  ensure_type (G_TYPE_TASK);
  ensure_type (G_TYPE_MEMORY_INPUT_STREAM);
  ensure_type (G_TYPE_DBUS_CONNECTION_FLAGS);
  ensure_type (G_TYPE_DBUS_CAPABILITY_FLAGS);
  ensure_type (G_TYPE_DBUS_AUTH_OBSERVER);
  ensure_type (G_TYPE_DBUS_CONNECTION);
  ensure_type (G_TYPE_DBUS_PROXY);
  ensure_type (G_TYPE_SOCKET_FAMILY);
  ensure_type (G_TYPE_SOCKET_TYPE);
  ensure_type (G_TYPE_SOCKET_PROTOCOL);
  ensure_type (G_TYPE_SOCKET_ADDRESS);
  ensure_type (G_TYPE_SOCKET);
}
/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  volatile gint refcount;
  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;
} SharedThreadData;

static gpointer
gdbus_shared_thread_func (gpointer user_data)
{
  SharedThreadData *data = user_data;

  g_main_context_push_thread_default (data->context);
  g_main_loop_run (data->loop);
  g_main_context_pop_thread_default (data->context);

  release_required_types ();

  return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

static SharedThreadData *
_g_dbus_shared_thread_ref (void)
{
  static gsize shared_thread_data = 0;
  SharedThreadData *ret;

  if (g_once_init_enter (&shared_thread_data))
    {
      SharedThreadData *data;

      data = g_new0 (SharedThreadData, 1);
      data->refcount = 0;
      
      data->context = g_main_context_new ();
      data->loop = g_main_loop_new (data->context, FALSE);
      data->thread = g_thread_new ("gdbus",
                                   gdbus_shared_thread_func,
                                   data);
      /* We can cast between gsize and gpointer safely */
      g_once_init_leave (&shared_thread_data, (gsize) data);
    }

  ret = (SharedThreadData*) shared_thread_data;
  g_atomic_int_inc (&ret->refcount);
  return ret;
}

static void
_g_dbus_shared_thread_unref (SharedThreadData *data)
{
  /* TODO: actually destroy the shared thread here */
#if 0
  g_assert (data != NULL);
  if (g_atomic_int_dec_and_test (&data->refcount))
    {
      g_main_loop_quit (data->loop);
      //g_thread_join (data->thread);
      g_main_loop_unref (data->loop);
      g_main_context_unref (data->context);
    }
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

typedef enum {
    PENDING_NONE = 0,
    PENDING_WRITE,
    PENDING_FLUSH,
    PENDING_CLOSE
} OutputPending;

struct GDBusWorker
{
  volatile gint                       ref_count;

  SharedThreadData                   *shared_thread_data;

  /* really a boolean, but GLib 2.28 lacks atomic boolean ops */
  volatile gint                       stopped;

  /* TODO: frozen (e.g. G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING) currently
   * only affects messages received from the other peer (since GDBusServer is the
   * only user) - we might want it to affect messages sent to the other peer too?
   */
  gboolean                            frozen;
  GDBusCapabilityFlags                capabilities;
  GQueue                             *received_messages_while_frozen;

  GIOStream                          *stream;
  GCancellable                       *cancellable;
  GDBusWorkerMessageReceivedCallback  message_received_callback;
  GDBusWorkerMessageAboutToBeSentCallback message_about_to_be_sent_callback;
  GDBusWorkerDisconnectedCallback     disconnected_callback;
  gpointer                            user_data;

  /* if not NULL, stream is GSocketConnection */
  GSocket *socket;

  /* used for reading */
  GMutex                              read_lock;
  gchar                              *read_buffer;
  gsize                               read_buffer_allocated_size;
  gsize                               read_buffer_cur_size;
  gsize                               read_buffer_bytes_wanted;
  GUnixFDList                        *read_fd_list;
  GSocketControlMessage             **read_ancillary_messages;
  gint                                read_num_ancillary_messages;

  /* Whether an async write, flush or close, or none of those, is pending.
   * Only the worker thread may change its value, and only with the write_lock.
   * Other threads may read its value when holding the write_lock.
   * The worker thread may read its value at any time.
   */
  OutputPending                       output_pending;
  /* used for writing */
  GMutex                              write_lock;
  /* queue of MessageToWriteData, protected by write_lock */
  GQueue                             *write_queue;
  /* protected by write_lock */
  guint64                             write_num_messages_written;
  /* number of messages we'd written out last time we flushed;
   * protected by write_lock
   */
  guint64                             write_num_messages_flushed;
  /* list of FlushData, protected by write_lock */
  GList                              *write_pending_flushes;
  /* list of CloseData, protected by write_lock */
  GList                              *pending_close_attempts;
  /* no lock - only used from the worker thread */
  gboolean                            close_expected;
};

static void _g_dbus_worker_unref (GDBusWorker *worker);

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMutex  mutex;
  GCond   cond;
  guint64 number_to_wait_for;
  GError *error;
} FlushData;

struct _MessageToWriteData ;
typedef struct _MessageToWriteData MessageToWriteData;

static void message_to_write_data_free (MessageToWriteData *data);

static void read_message_print_transport_debug (gssize bytes_read,
                                                GDBusWorker *worker);

static void write_message_print_transport_debug (gssize bytes_written,
                                                 MessageToWriteData *data);

typedef struct {
    GDBusWorker *worker;
    GTask *task;
} CloseData;

static void close_data_free (CloseData *close_data)
{
  g_clear_object (&close_data->task);

  _g_dbus_worker_unref (close_data->worker);
  g_slice_free (CloseData, close_data);
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusWorker *
_g_dbus_worker_ref (GDBusWorker *worker)
{
  g_atomic_int_inc (&worker->ref_count);
  return worker;
}

static void
_g_dbus_worker_unref (GDBusWorker *worker)
{
  if (g_atomic_int_dec_and_test (&worker->ref_count))
    {
      g_assert (worker->write_pending_flushes == NULL);

      _g_dbus_shared_thread_unref (worker->shared_thread_data);

      g_object_unref (worker->stream);

      g_mutex_clear (&worker->read_lock);
      g_object_unref (worker->cancellable);
      if (worker->read_fd_list != NULL)
        g_object_unref (worker->read_fd_list);

      g_queue_free_full (worker->received_messages_while_frozen, (GDestroyNotify) g_object_unref);
      g_mutex_clear (&worker->write_lock);
      g_queue_free_full (worker->write_queue, (GDestroyNotify) message_to_write_data_free);
      g_free (worker->read_buffer);

      g_free (worker);
    }
}

static void
_g_dbus_worker_emit_disconnected (GDBusWorker  *worker,
                                  gboolean      remote_peer_vanished,
                                  GError       *error)
{
  if (!g_atomic_int_get (&worker->stopped))
    worker->disconnected_callback (worker, remote_peer_vanished, error, worker->user_data);
}

static void
_g_dbus_worker_emit_message_received (GDBusWorker  *worker,
                                      GDBusMessage *message)
{
  if (!g_atomic_int_get (&worker->stopped))
    worker->message_received_callback (worker, message, worker->user_data);
}

static GDBusMessage *
_g_dbus_worker_emit_message_about_to_be_sent (GDBusWorker  *worker,
                                              GDBusMessage *message)
{
  GDBusMessage *ret;
  if (!g_atomic_int_get (&worker->stopped))
    ret = worker->message_about_to_be_sent_callback (worker, message, worker->user_data);
  else
    ret = message;
  return ret;
}

/* can only be called from private thread with read-lock held - takes ownership of @message */
static void
_g_dbus_worker_queue_or_deliver_received_message (GDBusWorker  *worker,
                                                  GDBusMessage *message)
{
  if (worker->frozen || g_queue_get_length (worker->received_messages_while_frozen) > 0)
    {
      /* queue up */
      g_queue_push_tail (worker->received_messages_while_frozen, message);
    }
  else
    {
      /* not frozen, nor anything in queue */
      _g_dbus_worker_emit_message_received (worker, message);
      g_object_unref (message);
    }
}

/* called in private thread shared by all GDBusConnection instances (without read-lock held) */
static gboolean
unfreeze_in_idle_cb (gpointer user_data)
{
  GDBusWorker *worker = user_data;
  GDBusMessage *message;

  g_mutex_lock (&worker->read_lock);
  if (worker->frozen)
    {
      while ((message = g_queue_pop_head (worker->received_messages_while_frozen)) != NULL)
        {
          _g_dbus_worker_emit_message_received (worker, message);
          g_object_unref (message);
        }
      worker->frozen = FALSE;
    }
  else
    {
      g_assert (g_queue_get_length (worker->received_messages_while_frozen) == 0);
    }
  g_mutex_unlock (&worker->read_lock);
  return FALSE;
}

/* can be called from any thread */
void
_g_dbus_worker_unfreeze (GDBusWorker *worker)
{
  GSource *idle_source;
  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         unfreeze_in_idle_cb,
                         _g_dbus_worker_ref (worker),
                         (GDestroyNotify) _g_dbus_worker_unref);
  g_source_set_name (idle_source, "[gio] unfreeze_in_idle_cb");
  g_source_attach (idle_source, worker->shared_thread_data->context);
  g_source_unref (idle_source);
}

/* ---------------------------------------------------------------------------------------------------- */

static void _g_dbus_worker_do_read_unlocked (GDBusWorker *worker);

/* called in private thread shared by all GDBusConnection instances (without read-lock held) */
static void
_g_dbus_worker_do_read_cb (GInputStream  *input_stream,
                           GAsyncResult  *res,
                           gpointer       user_data)
{
  GDBusWorker *worker = user_data;
  GError *error;
  gssize bytes_read;

  g_mutex_lock (&worker->read_lock);

  /* If already stopped, don't even process the reply */
  if (g_atomic_int_get (&worker->stopped))
    goto out;

  error = NULL;
  if (worker->socket == NULL)
    bytes_read = g_input_stream_read_finish (g_io_stream_get_input_stream (worker->stream),
                                             res,
                                             &error);
  else
    bytes_read = _g_socket_read_with_control_messages_finish (worker->socket,
                                                              res,
                                                              &error);
  if (worker->read_num_ancillary_messages > 0)
    {
      gint n;
      for (n = 0; n < worker->read_num_ancillary_messages; n++)
        {
          GSocketControlMessage *control_message = G_SOCKET_CONTROL_MESSAGE (worker->read_ancillary_messages[n]);

          if (FALSE)
            {
            }
#ifdef G_OS_UNIX
          else if (G_IS_UNIX_FD_MESSAGE (control_message))
            {
              GUnixFDMessage *fd_message;
              gint *fds;
              gint num_fds;

              fd_message = G_UNIX_FD_MESSAGE (control_message);
              fds = g_unix_fd_message_steal_fds (fd_message, &num_fds);
              if (worker->read_fd_list == NULL)
                {
                  worker->read_fd_list = g_unix_fd_list_new_from_array (fds, num_fds);
                }
              else
                {
                  gint n;
                  for (n = 0; n < num_fds; n++)
                    {
                      /* TODO: really want a append_steal() */
                      g_unix_fd_list_append (worker->read_fd_list, fds[n], NULL);
                      (void) g_close (fds[n], NULL);
                    }
                }
              g_free (fds);
            }
          else if (G_IS_UNIX_CREDENTIALS_MESSAGE (control_message))
            {
              /* do nothing */
            }
#endif
          else
            {
              if (error == NULL)
                {
                  g_set_error (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Unexpected ancillary message of type %s received from peer",
                               g_type_name (G_TYPE_FROM_INSTANCE (control_message)));
                  _g_dbus_worker_emit_disconnected (worker, TRUE, error);
                  g_error_free (error);
                  g_object_unref (control_message);
                  n++;
                  while (n < worker->read_num_ancillary_messages)
                    g_object_unref (worker->read_ancillary_messages[n++]);
                  g_free (worker->read_ancillary_messages);
                  goto out;
                }
            }
          g_object_unref (control_message);
        }
      g_free (worker->read_ancillary_messages);
    }

  if (bytes_read == -1)
    {
      if (G_UNLIKELY (_g_dbus_debug_transport ()))
        {
          _g_dbus_debug_print_lock ();
          g_print ("========================================================================\n"
                   "GDBus-debug:Transport:\n"
                   "  ---- READ ERROR on stream of type %s:\n"
                   "  ---- %s %d: %s\n",
                   g_type_name (G_TYPE_FROM_INSTANCE (g_io_stream_get_input_stream (worker->stream))),
                   g_quark_to_string (error->domain), error->code,
                   error->message);
          _g_dbus_debug_print_unlock ();
        }

      /* Every async read that uses this callback uses worker->cancellable
       * as its GCancellable. worker->cancellable gets cancelled if and only
       * if the GDBusConnection tells us to close (either via
       * _g_dbus_worker_stop, which is called on last-unref, or directly),
       * so a cancelled read must mean our connection was closed locally.
       *
       * If we're closing, other errors are possible - notably,
       * G_IO_ERROR_CLOSED can be seen if we close the stream with an async
       * read in-flight. It seems sensible to treat all read errors during
       * closing as an expected thing that doesn't trip exit-on-close.
       *
       * Because close_expected can't be set until we get into the worker
       * thread, but the cancellable is signalled sooner (from another
       * thread), we do still need to check the error.
       */
      if (worker->close_expected ||
          g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        _g_dbus_worker_emit_disconnected (worker, FALSE, NULL);
      else
        _g_dbus_worker_emit_disconnected (worker, TRUE, error);

      g_error_free (error);
      goto out;
    }

#if 0
  g_debug ("read %d bytes (is_closed=%d blocking=%d condition=0x%02x) stream %p, %p",
           (gint) bytes_read,
           g_socket_is_closed (g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream))),
           g_socket_get_blocking (g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream))),
           g_socket_condition_check (g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream)),
                                     G_IO_IN | G_IO_OUT | G_IO_HUP),
           worker->stream,
           worker);
#endif

  /* TODO: hmm, hmm... */
  if (bytes_read == 0)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Underlying GIOStream returned 0 bytes on an async read");
      _g_dbus_worker_emit_disconnected (worker, TRUE, error);
      g_error_free (error);
      goto out;
    }

  read_message_print_transport_debug (bytes_read, worker);

  worker->read_buffer_cur_size += bytes_read;
  if (worker->read_buffer_bytes_wanted == worker->read_buffer_cur_size)
    {
      /* OK, got what we asked for! */
      if (worker->read_buffer_bytes_wanted == 16)
        {
          gssize message_len;
          /* OK, got the header - determine how many more bytes are needed */
          error = NULL;
          message_len = g_dbus_message_bytes_needed ((guchar *) worker->read_buffer,
                                                     16,
                                                     &error);
          if (message_len == -1)
            {
              g_warning ("_g_dbus_worker_do_read_cb: error determining bytes needed: %s", error->message);
              _g_dbus_worker_emit_disconnected (worker, FALSE, error);
              g_error_free (error);
              goto out;
            }

          worker->read_buffer_bytes_wanted = message_len;
          _g_dbus_worker_do_read_unlocked (worker);
        }
      else
        {
          GDBusMessage *message;
          error = NULL;

          /* TODO: use connection->priv->auth to decode the message */

          message = g_dbus_message_new_from_blob ((guchar *) worker->read_buffer,
                                                  worker->read_buffer_cur_size,
                                                  worker->capabilities,
                                                  &error);
          if (message == NULL)
            {
              gchar *s;
              s = _g_dbus_hexdump (worker->read_buffer, worker->read_buffer_cur_size, 2);
              g_warning ("Error decoding D-Bus message of %" G_GSIZE_FORMAT " bytes\n"
                         "The error is: %s\n"
                         "The payload is as follows:\n"
                         "%s\n",
                         worker->read_buffer_cur_size,
                         error->message,
                         s);
              g_free (s);
              _g_dbus_worker_emit_disconnected (worker, FALSE, error);
              g_error_free (error);
              goto out;
            }

#ifdef G_OS_UNIX
          if (worker->read_fd_list != NULL)
            {
              g_dbus_message_set_unix_fd_list (message, worker->read_fd_list);
              g_object_unref (worker->read_fd_list);
              worker->read_fd_list = NULL;
            }
#endif

          if (G_UNLIKELY (_g_dbus_debug_message ()))
            {
              gchar *s;
              _g_dbus_debug_print_lock ();
              g_print ("========================================================================\n"
                       "GDBus-debug:Message:\n"
                       "  <<<< RECEIVED D-Bus message (%" G_GSIZE_FORMAT " bytes)\n",
                       worker->read_buffer_cur_size);
              s = g_dbus_message_print (message, 2);
              g_print ("%s", s);
              g_free (s);
              if (G_UNLIKELY (_g_dbus_debug_payload ()))
                {
                  s = _g_dbus_hexdump (worker->read_buffer, worker->read_buffer_cur_size, 2);
                  g_print ("%s\n", s);
                  g_free (s);
                }
              _g_dbus_debug_print_unlock ();
            }

          /* yay, got a message, go deliver it */
          _g_dbus_worker_queue_or_deliver_received_message (worker, message);

          /* start reading another message! */
          worker->read_buffer_bytes_wanted = 0;
          worker->read_buffer_cur_size = 0;
          _g_dbus_worker_do_read_unlocked (worker);
        }
    }
  else
    {
      /* didn't get all the bytes we requested - so repeat the request... */
      _g_dbus_worker_do_read_unlocked (worker);
    }

 out:
  g_mutex_unlock (&worker->read_lock);

  /* gives up the reference acquired when calling g_input_stream_read_async() */
  _g_dbus_worker_unref (worker);

  /* check if there is any pending close */
  schedule_pending_close (worker);
}

/* called in private thread shared by all GDBusConnection instances (with read-lock held) */
static void
_g_dbus_worker_do_read_unlocked (GDBusWorker *worker)
{
  /* Note that we do need to keep trying to read even if close_expected is
   * true, because only failing a read causes us to signal 'closed'.
   */

  /* if bytes_wanted is zero, it means start reading a message */
  if (worker->read_buffer_bytes_wanted == 0)
    {
      worker->read_buffer_cur_size = 0;
      worker->read_buffer_bytes_wanted = 16;
    }

  /* ensure we have a (big enough) buffer */
  if (worker->read_buffer == NULL || worker->read_buffer_bytes_wanted > worker->read_buffer_allocated_size)
    {
      /* TODO: 4096 is randomly chosen; might want a better chosen default minimum */
      worker->read_buffer_allocated_size = MAX (worker->read_buffer_bytes_wanted, 4096);
      worker->read_buffer = g_realloc (worker->read_buffer, worker->read_buffer_allocated_size);
    }

  if (worker->socket == NULL)
    g_input_stream_read_async (g_io_stream_get_input_stream (worker->stream),
                               worker->read_buffer + worker->read_buffer_cur_size,
                               worker->read_buffer_bytes_wanted - worker->read_buffer_cur_size,
                               G_PRIORITY_DEFAULT,
                               worker->cancellable,
                               (GAsyncReadyCallback) _g_dbus_worker_do_read_cb,
                               _g_dbus_worker_ref (worker));
  else
    {
      worker->read_ancillary_messages = NULL;
      worker->read_num_ancillary_messages = 0;
      _g_socket_read_with_control_messages (worker->socket,
                                            worker->read_buffer + worker->read_buffer_cur_size,
                                            worker->read_buffer_bytes_wanted - worker->read_buffer_cur_size,
                                            &worker->read_ancillary_messages,
                                            &worker->read_num_ancillary_messages,
                                            G_PRIORITY_DEFAULT,
                                            worker->cancellable,
                                            (GAsyncReadyCallback) _g_dbus_worker_do_read_cb,
                                            _g_dbus_worker_ref (worker));
    }
}

/* called in private thread shared by all GDBusConnection instances (without read-lock held) */
static gboolean
_g_dbus_worker_do_initial_read (gpointer data)
{
  GDBusWorker *worker = data;
  g_mutex_lock (&worker->read_lock);
  _g_dbus_worker_do_read_unlocked (worker);
  g_mutex_unlock (&worker->read_lock);
  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */

struct _MessageToWriteData
{
  GDBusWorker  *worker;
  GDBusMessage *message;
  gchar        *blob;
  gsize         blob_size;

  gsize         total_written;
  GTask        *task;
};

static void
message_to_write_data_free (MessageToWriteData *data)
{
  _g_dbus_worker_unref (data->worker);
  if (data->message)
    g_object_unref (data->message);
  g_free (data->blob);
  g_slice_free (MessageToWriteData, data);
}

/* ---------------------------------------------------------------------------------------------------- */

static void write_message_continue_writing (MessageToWriteData *data);

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_WRITE on entry
 */
static void
write_message_async_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  MessageToWriteData *data = user_data;
  GTask *task;
  gssize bytes_written;
  GError *error;

  /* Note: we can't access data->task after calling g_task_return_* () because the
   * callback can free @data and we're not completing in idle. So use a copy of the pointer.
   */
  task = data->task;

  error = NULL;
  bytes_written = g_output_stream_write_finish (G_OUTPUT_STREAM (source_object),
                                                res,
                                                &error);
  if (bytes_written == -1)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      goto out;
    }
  g_assert (bytes_written > 0); /* zero is never returned */

  write_message_print_transport_debug (bytes_written, data);

  data->total_written += bytes_written;
  g_assert (data->total_written <= data->blob_size);
  if (data->total_written == data->blob_size)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      goto out;
    }

  write_message_continue_writing (data);

 out:
  ;
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_WRITE on entry
 */
#ifdef G_OS_UNIX
static gboolean
on_socket_ready (GSocket      *socket,
                 GIOCondition  condition,
                 gpointer      user_data)
{
  MessageToWriteData *data = user_data;
  write_message_continue_writing (data);
  return FALSE; /* remove source */
}
#endif

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_WRITE on entry
 */
static void
write_message_continue_writing (MessageToWriteData *data)
{
  GOutputStream *ostream;
#ifdef G_OS_UNIX
  GTask *task;
  GUnixFDList *fd_list;
#endif

#ifdef G_OS_UNIX
  /* Note: we can't access data->task after calling g_task_return_* () because the
   * callback can free @data and we're not completing in idle. So use a copy of the pointer.
   */
  task = data->task;
#endif

  ostream = g_io_stream_get_output_stream (data->worker->stream);
#ifdef G_OS_UNIX
  fd_list = g_dbus_message_get_unix_fd_list (data->message);
#endif

  g_assert (!g_output_stream_has_pending (ostream));
  g_assert_cmpint (data->total_written, <, data->blob_size);

  if (FALSE)
    {
    }
#ifdef G_OS_UNIX
  else if (G_IS_SOCKET_OUTPUT_STREAM (ostream) && data->total_written == 0)
    {
      GOutputVector vector;
      GSocketControlMessage *control_message;
      gssize bytes_written;
      GError *error;

      vector.buffer = data->blob;
      vector.size = data->blob_size;

      control_message = NULL;
      if (fd_list != NULL && g_unix_fd_list_get_length (fd_list) > 0)
        {
          if (!(data->worker->capabilities & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING))
            {
              g_task_return_new_error (task,
                                       G_IO_ERROR,
                                       G_IO_ERROR_FAILED,
                                       "Tried sending a file descriptor but remote peer does not support this capability");
              g_object_unref (task);
              goto out;
            }
          control_message = g_unix_fd_message_new_with_fd_list (fd_list);
        }

      error = NULL;
      bytes_written = g_socket_send_message (data->worker->socket,
                                             NULL, /* address */
                                             &vector,
                                             1,
                                             control_message != NULL ? &control_message : NULL,
                                             control_message != NULL ? 1 : 0,
                                             G_SOCKET_MSG_NONE,
                                             data->worker->cancellable,
                                             &error);
      if (control_message != NULL)
        g_object_unref (control_message);

      if (bytes_written == -1)
        {
          /* Handle WOULD_BLOCK by waiting until there's room in the buffer */
          if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
            {
              GSource *source;
              source = g_socket_create_source (data->worker->socket,
                                               G_IO_OUT | G_IO_HUP | G_IO_ERR,
                                               data->worker->cancellable);
              g_source_set_callback (source,
                                     (GSourceFunc) on_socket_ready,
                                     data,
                                     NULL); /* GDestroyNotify */
              g_source_attach (source, g_main_context_get_thread_default ());
              g_source_unref (source);
              g_error_free (error);
              goto out;
            }
          g_task_return_error (task, error);
          g_object_unref (task);
          goto out;
        }
      g_assert (bytes_written > 0); /* zero is never returned */

      write_message_print_transport_debug (bytes_written, data);

      data->total_written += bytes_written;
      g_assert (data->total_written <= data->blob_size);
      if (data->total_written == data->blob_size)
        {
          g_task_return_boolean (task, TRUE);
          g_object_unref (task);
          goto out;
        }

      write_message_continue_writing (data);
    }
#endif
  else
    {
#ifdef G_OS_UNIX
      if (fd_list != NULL)
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "Tried sending a file descriptor on unsupported stream of type %s",
                                   g_type_name (G_TYPE_FROM_INSTANCE (ostream)));
          g_object_unref (task);
          goto out;
        }
#endif

      g_output_stream_write_async (ostream,
                                   (const gchar *) data->blob + data->total_written,
                                   data->blob_size - data->total_written,
                                   G_PRIORITY_DEFAULT,
                                   data->worker->cancellable,
                                   write_message_async_cb,
                                   data);
    }
#ifdef G_OS_UNIX
 out:
#endif
  ;
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_WRITE on entry
 */
static void
write_message_async (GDBusWorker         *worker,
                     MessageToWriteData  *data,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  data->task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_source_tag (data->task, write_message_async);
  data->total_written = 0;
  write_message_continue_writing (data);
}

/* called in private thread shared by all GDBusConnection instances (with write-lock held) */
static gboolean
write_message_finish (GAsyncResult   *res,
                      GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (res, NULL), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}
/* ---------------------------------------------------------------------------------------------------- */

static void continue_writing (GDBusWorker *worker);

typedef struct
{
  GDBusWorker *worker;
  GList *flushers;
} FlushAsyncData;

static void
flush_data_list_complete (const GList  *flushers,
                          const GError *error)
{
  const GList *l;

  for (l = flushers; l != NULL; l = l->next)
    {
      FlushData *f = l->data;

      f->error = error != NULL ? g_error_copy (error) : NULL;

      g_mutex_lock (&f->mutex);
      g_cond_signal (&f->cond);
      g_mutex_unlock (&f->mutex);
    }
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_FLUSH on entry
 */
static void
ostream_flush_cb (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  FlushAsyncData *data = user_data;
  GError *error;

  error = NULL;
  g_output_stream_flush_finish (G_OUTPUT_STREAM (source_object),
                                res,
                                &error);

  if (error == NULL)
    {
      if (G_UNLIKELY (_g_dbus_debug_transport ()))
        {
          _g_dbus_debug_print_lock ();
          g_print ("========================================================================\n"
                   "GDBus-debug:Transport:\n"
                   "  ---- FLUSHED stream of type %s\n",
                   g_type_name (G_TYPE_FROM_INSTANCE (g_io_stream_get_output_stream (data->worker->stream))));
          _g_dbus_debug_print_unlock ();
        }
    }

  g_assert (data->flushers != NULL);
  flush_data_list_complete (data->flushers, error);
  g_list_free (data->flushers);

  if (error != NULL)
    g_error_free (error);

  /* Make sure we tell folks that we don't have additional
     flushes pending */
  g_mutex_lock (&data->worker->write_lock);
  data->worker->write_num_messages_flushed = data->worker->write_num_messages_written;
  g_assert (data->worker->output_pending == PENDING_FLUSH);
  data->worker->output_pending = PENDING_NONE;
  g_mutex_unlock (&data->worker->write_lock);

  /* OK, cool, finally kick off the next write */
  continue_writing (data->worker);

  _g_dbus_worker_unref (data->worker);
  g_free (data);
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_FLUSH on entry
 */
static void
start_flush (FlushAsyncData *data)
{
  g_output_stream_flush_async (g_io_stream_get_output_stream (data->worker->stream),
                               G_PRIORITY_DEFAULT,
                               data->worker->cancellable,
                               ostream_flush_cb,
                               data);
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is held on entry
 * output_pending is PENDING_NONE on entry
 */
static void
message_written_unlocked (GDBusWorker *worker,
                          MessageToWriteData *message_data)
{
  if (G_UNLIKELY (_g_dbus_debug_message ()))
    {
      gchar *s;
      _g_dbus_debug_print_lock ();
      g_print ("========================================================================\n"
               "GDBus-debug:Message:\n"
               "  >>>> SENT D-Bus message (%" G_GSIZE_FORMAT " bytes)\n",
               message_data->blob_size);
      s = g_dbus_message_print (message_data->message, 2);
      g_print ("%s", s);
      g_free (s);
      if (G_UNLIKELY (_g_dbus_debug_payload ()))
        {
          s = _g_dbus_hexdump (message_data->blob, message_data->blob_size, 2);
          g_print ("%s\n", s);
          g_free (s);
        }
      _g_dbus_debug_print_unlock ();
    }

  worker->write_num_messages_written += 1;
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is held on entry
 * output_pending is PENDING_NONE on entry
 *
 * Returns: non-%NULL, setting @output_pending, if we need to flush now
 */
static FlushAsyncData *
prepare_flush_unlocked (GDBusWorker *worker)
{
  GList *l;
  GList *ll;
  GList *flushers;

  flushers = NULL;
  for (l = worker->write_pending_flushes; l != NULL; l = ll)
    {
      FlushData *f = l->data;
      ll = l->next;

      if (f->number_to_wait_for == worker->write_num_messages_written)
        {
          flushers = g_list_append (flushers, f);
          worker->write_pending_flushes = g_list_delete_link (worker->write_pending_flushes, l);
        }
    }
  if (flushers != NULL)
    {
      g_assert (worker->output_pending == PENDING_NONE);
      worker->output_pending = PENDING_FLUSH;
    }

  if (flushers != NULL)
    {
      FlushAsyncData *data;

      data = g_new0 (FlushAsyncData, 1);
      data->worker = _g_dbus_worker_ref (worker);
      data->flushers = flushers;
      return data;
    }

  return NULL;
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_WRITE on entry
 */
static void
write_message_cb (GObject       *source_object,
                  GAsyncResult  *res,
                  gpointer       user_data)
{
  MessageToWriteData *data = user_data;
  GError *error;

  g_mutex_lock (&data->worker->write_lock);
  g_assert (data->worker->output_pending == PENDING_WRITE);
  data->worker->output_pending = PENDING_NONE;

  error = NULL;
  if (!write_message_finish (res, &error))
    {
      g_mutex_unlock (&data->worker->write_lock);

      /* TODO: handle */
      _g_dbus_worker_emit_disconnected (data->worker, TRUE, error);
      g_error_free (error);

      g_mutex_lock (&data->worker->write_lock);
    }

  message_written_unlocked (data->worker, data);

  g_mutex_unlock (&data->worker->write_lock);

  continue_writing (data->worker);

  message_to_write_data_free (data);
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending is PENDING_CLOSE on entry
 */
static void
iostream_close_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GDBusWorker *worker = user_data;
  GError *error = NULL;
  GList *pending_close_attempts, *pending_flush_attempts;
  GQueue *send_queue;

  g_io_stream_close_finish (worker->stream, res, &error);

  g_mutex_lock (&worker->write_lock);

  pending_close_attempts = worker->pending_close_attempts;
  worker->pending_close_attempts = NULL;

  pending_flush_attempts = worker->write_pending_flushes;
  worker->write_pending_flushes = NULL;

  send_queue = worker->write_queue;
  worker->write_queue = g_queue_new ();

  g_assert (worker->output_pending == PENDING_CLOSE);
  worker->output_pending = PENDING_NONE;

  g_mutex_unlock (&worker->write_lock);

  while (pending_close_attempts != NULL)
    {
      CloseData *close_data = pending_close_attempts->data;

      pending_close_attempts = g_list_delete_link (pending_close_attempts,
                                                   pending_close_attempts);

      if (close_data->task != NULL)
        {
          if (error != NULL)
            g_task_return_error (close_data->task, g_error_copy (error));
          else
            g_task_return_boolean (close_data->task, TRUE);
        }

      close_data_free (close_data);
    }

  g_clear_error (&error);

  /* all messages queued for sending are discarded */
  g_queue_free_full (send_queue, (GDestroyNotify) message_to_write_data_free);
  /* all queued flushes fail */
  error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED,
                       _("Operation was cancelled"));
  flush_data_list_complete (pending_flush_attempts, error);
  g_list_free (pending_flush_attempts);
  g_clear_error (&error);

  _g_dbus_worker_unref (worker);
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending must be PENDING_NONE on entry
 */
static void
continue_writing (GDBusWorker *worker)
{
  MessageToWriteData *data;
  FlushAsyncData *flush_async_data;

 write_next:
  /* we mustn't try to write two things at once */
  g_assert (worker->output_pending == PENDING_NONE);

  g_mutex_lock (&worker->write_lock);

  data = NULL;
  flush_async_data = NULL;

  /* if we want to close the connection, that takes precedence */
  if (worker->pending_close_attempts != NULL)
    {
      GInputStream *input = g_io_stream_get_input_stream (worker->stream);

      if (!g_input_stream_has_pending (input))
        {
          worker->close_expected = TRUE;
          worker->output_pending = PENDING_CLOSE;

          g_io_stream_close_async (worker->stream, G_PRIORITY_DEFAULT,
                                   NULL, iostream_close_cb,
                                   _g_dbus_worker_ref (worker));
        }
    }
  else
    {
      flush_async_data = prepare_flush_unlocked (worker);

      if (flush_async_data == NULL)
        {
          data = g_queue_pop_head (worker->write_queue);

          if (data != NULL)
            worker->output_pending = PENDING_WRITE;
        }
    }

  g_mutex_unlock (&worker->write_lock);

  /* Note that write_lock is only used for protecting the @write_queue
   * and @output_pending fields of the GDBusWorker struct ... which we
   * need to modify from arbitrary threads in _g_dbus_worker_send_message().
   *
   * Therefore, it's fine to drop it here when calling back into user
   * code and then writing the message out onto the GIOStream since this
   * function only runs on the worker thread.
   */

  if (flush_async_data != NULL)
    {
      start_flush (flush_async_data);
      g_assert (data == NULL);
    }
  else if (data != NULL)
    {
      GDBusMessage *old_message;
      guchar *new_blob;
      gsize new_blob_size;
      GError *error;

      old_message = data->message;
      data->message = _g_dbus_worker_emit_message_about_to_be_sent (worker, data->message);
      if (data->message == old_message)
        {
          /* filters had no effect - do nothing */
        }
      else if (data->message == NULL)
        {
          /* filters dropped message */
          g_mutex_lock (&worker->write_lock);
          worker->output_pending = PENDING_NONE;
          g_mutex_unlock (&worker->write_lock);
          message_to_write_data_free (data);
          goto write_next;
        }
      else
        {
          /* filters altered the message -> reencode */
          error = NULL;
          new_blob = g_dbus_message_to_blob (data->message,
                                             &new_blob_size,
                                             worker->capabilities,
                                             &error);
          if (new_blob == NULL)
            {
              /* if filter make the GDBusMessage unencodeable, just complain on stderr and send
               * the old message instead
               */
              g_warning ("Error encoding GDBusMessage with serial %d altered by filter function: %s",
                         g_dbus_message_get_serial (data->message),
                         error->message);
              g_error_free (error);
            }
          else
            {
              g_free (data->blob);
              data->blob = (gchar *) new_blob;
              data->blob_size = new_blob_size;
            }
        }

      write_message_async (worker,
                           data,
                           write_message_cb,
                           data);
    }
}

/* called in private thread shared by all GDBusConnection instances
 *
 * write-lock is not held on entry
 * output_pending may be anything
 */
static gboolean
continue_writing_in_idle_cb (gpointer user_data)
{
  GDBusWorker *worker = user_data;

  /* Because this is the worker thread, we can read this struct member
   * without holding the lock: no other thread ever modifies it.
   */
  if (worker->output_pending == PENDING_NONE)
    continue_writing (worker);

  return FALSE;
}

/*
 * @write_data: (transfer full) (nullable):
 * @flush_data: (transfer full) (nullable):
 * @close_data: (transfer full) (nullable):
 *
 * Can be called from any thread
 *
 * write_lock is held on entry
 * output_pending may be anything
 */
static void
schedule_writing_unlocked (GDBusWorker        *worker,
                           MessageToWriteData *write_data,
                           FlushData          *flush_data,
                           CloseData          *close_data)
{
  if (write_data != NULL)
    g_queue_push_tail (worker->write_queue, write_data);

  if (flush_data != NULL)
    worker->write_pending_flushes = g_list_prepend (worker->write_pending_flushes, flush_data);

  if (close_data != NULL)
    worker->pending_close_attempts = g_list_prepend (worker->pending_close_attempts,
                                                     close_data);

  /* If we had output pending, the next bit of output will happen
   * automatically when it finishes, so we only need to do this
   * if nothing was pending.
   *
   * The idle callback will re-check that output_pending is still
   * PENDING_NONE, to guard against output starting before the idle.
   */
  if (worker->output_pending == PENDING_NONE)
    {
      GSource *idle_source;
      idle_source = g_idle_source_new ();
      g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
      g_source_set_callback (idle_source,
                             continue_writing_in_idle_cb,
                             _g_dbus_worker_ref (worker),
                             (GDestroyNotify) _g_dbus_worker_unref);
      g_source_set_name (idle_source, "[gio] continue_writing_in_idle_cb");
      g_source_attach (idle_source, worker->shared_thread_data->context);
      g_source_unref (idle_source);
    }
}

static void
schedule_pending_close (GDBusWorker *worker)
{
  g_mutex_lock (&worker->write_lock);
  if (worker->pending_close_attempts)
    schedule_writing_unlocked (worker, NULL, NULL, NULL);
  g_mutex_unlock (&worker->write_lock);
}

/* ---------------------------------------------------------------------------------------------------- */

/* can be called from any thread - steals blob
 *
 * write_lock is not held on entry
 * output_pending may be anything
 */
void
_g_dbus_worker_send_message (GDBusWorker    *worker,
                             GDBusMessage   *message,
                             gchar          *blob,
                             gsize           blob_len)
{
  MessageToWriteData *data;

  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (blob != NULL);
  g_return_if_fail (blob_len > 16);

  data = g_slice_new0 (MessageToWriteData);
  data->worker = _g_dbus_worker_ref (worker);
  data->message = g_object_ref (message);
  data->blob = blob; /* steal! */
  data->blob_size = blob_len;

  g_mutex_lock (&worker->write_lock);
  schedule_writing_unlocked (worker, data, NULL, NULL);
  g_mutex_unlock (&worker->write_lock);
}

/* ---------------------------------------------------------------------------------------------------- */

GDBusWorker *
_g_dbus_worker_new (GIOStream                              *stream,
                    GDBusCapabilityFlags                    capabilities,
                    gboolean                                initially_frozen,
                    GDBusWorkerMessageReceivedCallback      message_received_callback,
                    GDBusWorkerMessageAboutToBeSentCallback message_about_to_be_sent_callback,
                    GDBusWorkerDisconnectedCallback         disconnected_callback,
                    gpointer                                user_data)
{
  GDBusWorker *worker;
  GSource *idle_source;

  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);
  g_return_val_if_fail (message_received_callback != NULL, NULL);
  g_return_val_if_fail (message_about_to_be_sent_callback != NULL, NULL);
  g_return_val_if_fail (disconnected_callback != NULL, NULL);

  worker = g_new0 (GDBusWorker, 1);
  worker->ref_count = 1;

  g_mutex_init (&worker->read_lock);
  worker->message_received_callback = message_received_callback;
  worker->message_about_to_be_sent_callback = message_about_to_be_sent_callback;
  worker->disconnected_callback = disconnected_callback;
  worker->user_data = user_data;
  worker->stream = g_object_ref (stream);
  worker->capabilities = capabilities;
  worker->cancellable = g_cancellable_new ();
  worker->output_pending = PENDING_NONE;

  worker->frozen = initially_frozen;
  worker->received_messages_while_frozen = g_queue_new ();

  g_mutex_init (&worker->write_lock);
  worker->write_queue = g_queue_new ();

  if (G_IS_SOCKET_CONNECTION (worker->stream))
    worker->socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream));

  worker->shared_thread_data = _g_dbus_shared_thread_ref ();

  /* begin reading */
  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         _g_dbus_worker_do_initial_read,
                         _g_dbus_worker_ref (worker),
                         (GDestroyNotify) _g_dbus_worker_unref);
  g_source_set_name (idle_source, "[gio] _g_dbus_worker_do_initial_read");
  g_source_attach (idle_source, worker->shared_thread_data->context);
  g_source_unref (idle_source);

  return worker;
}

/* ---------------------------------------------------------------------------------------------------- */

/* can be called from any thread
 *
 * write_lock is not held on entry
 * output_pending may be anything
 */
void
_g_dbus_worker_close (GDBusWorker         *worker,
                      GTask               *task)
{
  CloseData *close_data;

  close_data = g_slice_new0 (CloseData);
  close_data->worker = _g_dbus_worker_ref (worker);
  close_data->task = (task == NULL ? NULL : g_object_ref (task));

  /* Don't set worker->close_expected here - we're in the wrong thread.
   * It'll be set before the actual close happens.
   */
  g_cancellable_cancel (worker->cancellable);
  g_mutex_lock (&worker->write_lock);
  schedule_writing_unlocked (worker, NULL, NULL, close_data);
  g_mutex_unlock (&worker->write_lock);
}

/* This can be called from any thread - frees worker. Note that
 * callbacks might still happen if called from another thread than the
 * worker - use your own synchronization primitive in the callbacks.
 *
 * write_lock is not held on entry
 * output_pending may be anything
 */
void
_g_dbus_worker_stop (GDBusWorker *worker)
{
  g_atomic_int_set (&worker->stopped, TRUE);

  /* Cancel any pending operations and schedule a close of the underlying I/O
   * stream in the worker thread
   */
  _g_dbus_worker_close (worker, NULL);

  /* _g_dbus_worker_close holds a ref until after an idle in the worker
   * thread has run, so we no longer need to unref in an idle like in
   * commit 322e25b535
   */
  _g_dbus_worker_unref (worker);
}

/* ---------------------------------------------------------------------------------------------------- */

/* can be called from any thread (except the worker thread) - blocks
 * calling thread until all queued outgoing messages are written and
 * the transport has been flushed
 *
 * write_lock is not held on entry
 * output_pending may be anything
 */
gboolean
_g_dbus_worker_flush_sync (GDBusWorker    *worker,
                           GCancellable   *cancellable,
                           GError        **error)
{
  gboolean ret;
  FlushData *data;
  guint64 pending_writes;

  data = NULL;
  ret = TRUE;

  g_mutex_lock (&worker->write_lock);

  /* if the queue is empty, no write is in-flight and we haven't written
   * anything since the last flush, then there's nothing to wait for
   */
  pending_writes = g_queue_get_length (worker->write_queue);

  /* if a write is in-flight, we shouldn't be satisfied until the first
   * flush operation that follows it
   */
  if (worker->output_pending == PENDING_WRITE)
    pending_writes += 1;

  if (pending_writes > 0 ||
      worker->write_num_messages_written != worker->write_num_messages_flushed)
    {
      data = g_new0 (FlushData, 1);
      g_mutex_init (&data->mutex);
      g_cond_init (&data->cond);
      data->number_to_wait_for = worker->write_num_messages_written + pending_writes;
      g_mutex_lock (&data->mutex);

      schedule_writing_unlocked (worker, NULL, data, NULL);
    }
  g_mutex_unlock (&worker->write_lock);

  if (data != NULL)
    {
      g_cond_wait (&data->cond, &data->mutex);
      g_mutex_unlock (&data->mutex);

      /* note:the element is removed from worker->write_pending_flushes in flush_cb() above */
      g_cond_clear (&data->cond);
      g_mutex_clear (&data->mutex);
      if (data->error != NULL)
        {
          ret = FALSE;
          g_propagate_error (error, data->error);
        }
      g_free (data);
    }

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

#define G_DBUS_DEBUG_AUTHENTICATION (1<<0)
#define G_DBUS_DEBUG_TRANSPORT      (1<<1)
#define G_DBUS_DEBUG_MESSAGE        (1<<2)
#define G_DBUS_DEBUG_PAYLOAD        (1<<3)
#define G_DBUS_DEBUG_CALL           (1<<4)
#define G_DBUS_DEBUG_SIGNAL         (1<<5)
#define G_DBUS_DEBUG_INCOMING       (1<<6)
#define G_DBUS_DEBUG_RETURN         (1<<7)
#define G_DBUS_DEBUG_EMISSION       (1<<8)
#define G_DBUS_DEBUG_ADDRESS        (1<<9)

static gint _gdbus_debug_flags = 0;

gboolean
_g_dbus_debug_authentication (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_AUTHENTICATION) != 0;
}

gboolean
_g_dbus_debug_transport (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_TRANSPORT) != 0;
}

gboolean
_g_dbus_debug_message (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_MESSAGE) != 0;
}

gboolean
_g_dbus_debug_payload (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_PAYLOAD) != 0;
}

gboolean
_g_dbus_debug_call (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_CALL) != 0;
}

gboolean
_g_dbus_debug_signal (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_SIGNAL) != 0;
}

gboolean
_g_dbus_debug_incoming (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_INCOMING) != 0;
}

gboolean
_g_dbus_debug_return (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_RETURN) != 0;
}

gboolean
_g_dbus_debug_emission (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_EMISSION) != 0;
}

gboolean
_g_dbus_debug_address (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_ADDRESS) != 0;
}

G_LOCK_DEFINE_STATIC (print_lock);

void
_g_dbus_debug_print_lock (void)
{
  G_LOCK (print_lock);
}

void
_g_dbus_debug_print_unlock (void)
{
  G_UNLOCK (print_lock);
}

/**
 * _g_dbus_initialize:
 *
 * Does various one-time init things such as
 *
 *  - registering the G_DBUS_ERROR error domain
 *  - parses the G_DBUS_DEBUG environment variable
 */
void
_g_dbus_initialize (void)
{
  static volatile gsize initialized = 0;

  if (g_once_init_enter (&initialized))
    {
      volatile GQuark g_dbus_error_domain;
      const gchar *debug;

      g_dbus_error_domain = G_DBUS_ERROR;
      (g_dbus_error_domain); /* To avoid -Wunused-but-set-variable */

      debug = g_getenv ("G_DBUS_DEBUG");
      if (debug != NULL)
        {
          const GDebugKey keys[] = {
            { "authentication", G_DBUS_DEBUG_AUTHENTICATION },
            { "transport",      G_DBUS_DEBUG_TRANSPORT      },
            { "message",        G_DBUS_DEBUG_MESSAGE        },
            { "payload",        G_DBUS_DEBUG_PAYLOAD        },
            { "call",           G_DBUS_DEBUG_CALL           },
            { "signal",         G_DBUS_DEBUG_SIGNAL         },
            { "incoming",       G_DBUS_DEBUG_INCOMING       },
            { "return",         G_DBUS_DEBUG_RETURN         },
            { "emission",       G_DBUS_DEBUG_EMISSION       },
            { "address",        G_DBUS_DEBUG_ADDRESS        }
          };

          _gdbus_debug_flags = g_parse_debug_string (debug, keys, G_N_ELEMENTS (keys));
          if (_gdbus_debug_flags & G_DBUS_DEBUG_PAYLOAD)
            _gdbus_debug_flags |= G_DBUS_DEBUG_MESSAGE;
        }

      /* Work-around for https://bugzilla.gnome.org/show_bug.cgi?id=627724 */
      ensure_required_types ();

      g_once_init_leave (&initialized, 1);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

GVariantType *
_g_dbus_compute_complete_signature (GDBusArgInfo **args)
{
  const GVariantType *arg_types[256];
  guint n;

  if (args)
    for (n = 0; args[n] != NULL; n++)
      {
        /* DBus places a hard limit of 255 on signature length.
         * therefore number of args must be less than 256.
         */
        g_assert (n < 256);

        arg_types[n] = G_VARIANT_TYPE (args[n]->signature);

        if G_UNLIKELY (arg_types[n] == NULL)
          return NULL;
      }
  else
    n = 0;

  return g_variant_type_new_tuple (arg_types, n);
}

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_WIN32

extern BOOL WINAPI ConvertSidToStringSidA (PSID Sid, LPSTR *StringSid);

gchar *
_g_dbus_win32_get_user_sid (void)
{
  HANDLE h;
  TOKEN_USER *user;
  DWORD token_information_len;
  PSID psid;
  gchar *sid;
  gchar *ret;

  ret = NULL;
  user = NULL;
  h = INVALID_HANDLE_VALUE;

  if (!OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &h))
    {
      g_warning ("OpenProcessToken failed with error code %d", (gint) GetLastError ());
      goto out;
    }

  /* Get length of buffer */
  token_information_len = 0;
  if (!GetTokenInformation (h, TokenUser, NULL, 0, &token_information_len))
    {
      if (GetLastError () != ERROR_INSUFFICIENT_BUFFER)
        {
          g_warning ("GetTokenInformation() failed with error code %d", (gint) GetLastError ());
          goto out;
        }
    }
  user = g_malloc (token_information_len);
  if (!GetTokenInformation (h, TokenUser, user, token_information_len, &token_information_len))
    {
      g_warning ("GetTokenInformation() failed with error code %d", (gint) GetLastError ());
      goto out;
    }

  psid = user->User.Sid;
  if (!IsValidSid (psid))
    {
      g_warning ("Invalid SID");
      goto out;
    }

  if (!ConvertSidToStringSidA (psid, &sid))
    {
      g_warning ("Invalid SID");
      goto out;
    }

  ret = g_strdup (sid);
  LocalFree (sid);

out:
  g_free (user);
  if (h != INVALID_HANDLE_VALUE)
    CloseHandle (h);
  return ret;
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

gchar *
_g_dbus_get_machine_id (GError **error)
{
#ifdef G_OS_WIN32
  HW_PROFILE_INFOA info;
  char *src, *dest, *res;
  int i;

  if (!GetCurrentHwProfileA (&info))
    {
      char *message = g_win32_error_message (GetLastError ());
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_FAILED,
		   _("Unable to get Hardware profile: %s"), message);
      g_free (message);
      return NULL;
    }

  /* Form: {12340001-4980-1920-6788-123456789012} */
  src = &info.szHwProfileGuid[0];

  res = g_malloc (32+1);
  dest = res;

  src++; /* Skip { */
  for (i = 0; i < 8; i++)
    *dest++ = *src++;
  src++; /* Skip - */
  for (i = 0; i < 4; i++)
    *dest++ = *src++;
  src++; /* Skip - */
  for (i = 0; i < 4; i++)
    *dest++ = *src++;
  src++; /* Skip - */
  for (i = 0; i < 4; i++)
    *dest++ = *src++;
  src++; /* Skip - */
  for (i = 0; i < 12; i++)
    *dest++ = *src++;
  *dest = 0;

  return res;
#else
  gchar *ret;
  GError *first_error;
  /* TODO: use PACKAGE_LOCALSTATEDIR ? */
  ret = NULL;
  first_error = NULL;
  if (!g_file_get_contents ("/var/lib/dbus/machine-id",
                            &ret,
                            NULL,
                            &first_error) &&
      !g_file_get_contents ("/etc/machine-id",
                            &ret,
                            NULL,
                            NULL))
    {
      g_propagate_prefixed_error (error, first_error,
                                  _("Unable to load /var/lib/dbus/machine-id or /etc/machine-id: "));
    }
  else
    {
      /* ignore the error from the first try, if any */
      g_clear_error (&first_error);
      /* TODO: validate value */
      g_strstrip (ret);
    }
  return ret;
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

gchar *
_g_dbus_enum_to_string (GType enum_type, gint value)
{
  gchar *ret;
  GEnumClass *klass;
  GEnumValue *enum_value;

  klass = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (klass, value);
  if (enum_value != NULL)
    ret = g_strdup (enum_value->value_nick);
  else
    ret = g_strdup_printf ("unknown (value %d)", value);
  g_type_class_unref (klass);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
write_message_print_transport_debug (gssize bytes_written,
                                     MessageToWriteData *data)
{
  if (G_LIKELY (!_g_dbus_debug_transport ()))
    goto out;

  _g_dbus_debug_print_lock ();
  g_print ("========================================================================\n"
           "GDBus-debug:Transport:\n"
           "  >>>> WROTE %" G_GSSIZE_FORMAT " bytes of message with serial %d and\n"
           "       size %" G_GSIZE_FORMAT " from offset %" G_GSIZE_FORMAT " on a %s\n",
           bytes_written,
           g_dbus_message_get_serial (data->message),
           data->blob_size,
           data->total_written,
           g_type_name (G_TYPE_FROM_INSTANCE (g_io_stream_get_output_stream (data->worker->stream))));
  _g_dbus_debug_print_unlock ();
 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
read_message_print_transport_debug (gssize bytes_read,
                                    GDBusWorker *worker)
{
  gsize size;
  gint32 serial;
  gint32 message_length;

  if (G_LIKELY (!_g_dbus_debug_transport ()))
    goto out;

  size = bytes_read + worker->read_buffer_cur_size;
  serial = 0;
  message_length = 0;
  if (size >= 16)
    message_length = g_dbus_message_bytes_needed ((guchar *) worker->read_buffer, size, NULL);
  if (size >= 1)
    {
      switch (worker->read_buffer[0])
        {
        case 'l':
          if (size >= 12)
            serial = GUINT32_FROM_LE (((guint32 *) worker->read_buffer)[2]);
          break;
        case 'B':
          if (size >= 12)
            serial = GUINT32_FROM_BE (((guint32 *) worker->read_buffer)[2]);
          break;
        default:
          /* an error will be set elsewhere if this happens */
          goto out;
        }
    }

    _g_dbus_debug_print_lock ();
  g_print ("========================================================================\n"
           "GDBus-debug:Transport:\n"
           "  <<<< READ %" G_GSSIZE_FORMAT " bytes of message with serial %d and\n"
           "       size %d to offset %" G_GSIZE_FORMAT " from a %s\n",
           bytes_read,
           serial,
           message_length,
           worker->read_buffer_cur_size,
           g_type_name (G_TYPE_FROM_INSTANCE (g_io_stream_get_input_stream (worker->stream))));
  _g_dbus_debug_print_unlock ();
 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

gboolean
_g_signal_accumulator_false_handled (GSignalInvocationHint *ihint,
                                     GValue                *return_accu,
                                     const GValue          *handler_return,
                                     gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_return;

  signal_return = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_return);
  continue_emission = signal_return;

  return continue_emission;
}
