/* GDBus regression test - close a stream when a message remains to be written
 *
 * Copyright © 2006-2010 Red Hat, Inc.
 * Copyright © 2011 Nokia Corporation
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
 * Author: Simon McVittie <simon.mcvittie@collabora.co.uk>
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#ifdef G_OS_UNIX
# include <unistd.h>

# include <glib/glib-unix.h>
# include <gio/gunixinputstream.h>
# include <gio/gunixoutputstream.h>
# include <gio/gunixconnection.h>
#else
# error This test is currently Unix-specific due to use of g_unix_open_pipe()
#endif

#include "gdbus-tests.h"

#define CLOSE_TIME_MS 1
#define N_REPEATS_SLOW 5000
#define N_REPEATS 100

/* ---------- MyIOStream ------------------------------------------------- */

#define MY_TYPE_IO_STREAM  (my_io_stream_get_type ())
#define MY_IO_STREAM(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), MY_TYPE_IO_STREAM, MyIOStream))
#define MY_IS_IO_STREAM(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), MY_TYPE_IO_STREAM))

typedef struct
{
  GIOStream parent_instance;
  GInputStream *input_stream;
  GOutputStream *output_stream;
} MyIOStream;

typedef struct
{
  GIOStreamClass parent_class;
} MyIOStreamClass;

static GType my_io_stream_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MyIOStream, my_io_stream, G_TYPE_IO_STREAM)

static void
my_io_stream_finalize (GObject *object)
{
  MyIOStream *stream = MY_IO_STREAM (object);
  g_object_unref (stream->input_stream);
  g_object_unref (stream->output_stream);
  G_OBJECT_CLASS (my_io_stream_parent_class)->finalize (object);
}

static void
my_io_stream_init (MyIOStream *stream)
{
}

static GInputStream *
my_io_stream_get_input_stream (GIOStream *_stream)
{
  MyIOStream *stream = MY_IO_STREAM (_stream);
  return stream->input_stream;
}

static GOutputStream *
my_io_stream_get_output_stream (GIOStream *_stream)
{
  MyIOStream *stream = MY_IO_STREAM (_stream);
  return stream->output_stream;
}

static void
my_io_stream_class_init (MyIOStreamClass *klass)
{
  GObjectClass *gobject_class;
  GIOStreamClass *giostream_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = my_io_stream_finalize;

  giostream_class = G_IO_STREAM_CLASS (klass);
  giostream_class->get_input_stream  = my_io_stream_get_input_stream;
  giostream_class->get_output_stream = my_io_stream_get_output_stream;
}

static GIOStream *
my_io_stream_new (GInputStream  *input_stream,
                  GOutputStream *output_stream)
{
  MyIOStream *stream;
  g_return_val_if_fail (G_IS_INPUT_STREAM (input_stream), NULL);
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (output_stream), NULL);
  stream = MY_IO_STREAM (g_object_new (MY_TYPE_IO_STREAM, NULL));
  stream->input_stream = g_object_ref (input_stream);
  stream->output_stream = g_object_ref (output_stream);
  return G_IO_STREAM (stream);
}

/* ---------- MySlowCloseOutputStream ------------------------------------ */

typedef struct
{
  GFilterOutputStream parent_instance;
} MySlowCloseOutputStream;

typedef struct
{
  GFilterOutputStreamClass parent_class;
} MySlowCloseOutputStreamClass;

#define MY_TYPE_SLOW_CLOSE_OUTPUT_STREAM \
  (my_slow_close_output_stream_get_type ())
#define MY_OUTPUT_STREAM(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), MY_TYPE_SLOW_CLOSE_OUTPUT_STREAM, \
                               MySlowCloseOutputStream))
#define MY_IS_SLOW_CLOSE_OUTPUT_STREAM(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), MY_TYPE_SLOW_CLOSE_OUTPUT_STREAM))

static GType my_slow_close_output_stream_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MySlowCloseOutputStream, my_slow_close_output_stream,
               G_TYPE_FILTER_OUTPUT_STREAM)

static void
my_slow_close_output_stream_init (MySlowCloseOutputStream *stream)
{
}

static gboolean
my_slow_close_output_stream_close (GOutputStream  *stream,
                                   GCancellable   *cancellable,
                                   GError        **error)
{
  g_usleep (CLOSE_TIME_MS * 1000);
  return G_OUTPUT_STREAM_CLASS (my_slow_close_output_stream_parent_class)->
    close_fn (stream, cancellable, error);
}

typedef struct {
    GOutputStream *stream;
    gint io_priority;
    GCancellable *cancellable;
    GAsyncReadyCallback callback;
    gpointer user_data;
} DelayedClose;

static void
delayed_close_free (gpointer data)
{
  DelayedClose *df = data;

  g_object_unref (df->stream);
  if (df->cancellable)
    g_object_unref (df->cancellable);
  g_free (df);
}

static gboolean
delayed_close_cb (gpointer data)
{
  DelayedClose *df = data;

  G_OUTPUT_STREAM_CLASS (my_slow_close_output_stream_parent_class)->
    close_async (df->stream, df->io_priority, df->cancellable, df->callback,
                 df->user_data);

  return FALSE;
}

static void
my_slow_close_output_stream_close_async  (GOutputStream            *stream,
                                          int                       io_priority,
                                          GCancellable             *cancellable,
                                          GAsyncReadyCallback       callback,
                                          gpointer                  user_data)
{
  GSource *later;
  DelayedClose *df;

  df = g_new0 (DelayedClose, 1);
  df->stream = g_object_ref (stream);
  df->io_priority = io_priority;
  df->cancellable = (cancellable != NULL ? g_object_ref (cancellable) : NULL);
  df->callback = callback;
  df->user_data = user_data;

  later = g_timeout_source_new (CLOSE_TIME_MS);
  g_source_set_callback (later, delayed_close_cb, df, delayed_close_free);
  g_source_attach (later, g_main_context_get_thread_default ());
}

static gboolean
my_slow_close_output_stream_close_finish  (GOutputStream  *stream,
                                GAsyncResult   *result,
                                GError        **error)
{
  return G_OUTPUT_STREAM_CLASS (my_slow_close_output_stream_parent_class)->
    close_finish (stream, result, error);
}

static void
my_slow_close_output_stream_class_init (MySlowCloseOutputStreamClass *klass)
{
  GOutputStreamClass *ostream_class;

  ostream_class = G_OUTPUT_STREAM_CLASS (klass);
  ostream_class->close_fn = my_slow_close_output_stream_close;
  ostream_class->close_async = my_slow_close_output_stream_close_async;
  ostream_class->close_finish = my_slow_close_output_stream_close_finish;
}

static GIOStream *
my_io_stream_new_for_fds (gint fd_in, gint fd_out)
{
  GIOStream *stream;
  GInputStream *input_stream;
  GOutputStream *real_output_stream;
  GOutputStream *output_stream;

  input_stream = g_unix_input_stream_new (fd_in, TRUE);
  real_output_stream = g_unix_output_stream_new (fd_out, TRUE);
  output_stream = g_object_new (MY_TYPE_SLOW_CLOSE_OUTPUT_STREAM,
                                "base-stream", real_output_stream,
                                NULL);
  stream = my_io_stream_new (input_stream, output_stream);
  g_object_unref (input_stream);
  g_object_unref (output_stream);
  g_object_unref (real_output_stream);
  return stream;
}

/* ---------- Tests ------------------------------------------------------ */

typedef struct {
  gint server_to_client[2];
  gint client_to_server[2];
  GIOStream *server_iostream;
  GDBusConnection *server_conn;
  GIOStream *iostream;
  GDBusConnection *connection;
  gchar *guid;
  GError *error;
} Fixture;

static void
setup (Fixture       *f,
       gconstpointer  context)
{
  f->guid = g_dbus_generate_guid ();
}

static void
teardown (Fixture       *f,
          gconstpointer  context)
{
  g_clear_object (&f->server_iostream);
  g_clear_object (&f->server_conn);
  g_clear_object (&f->iostream);
  g_clear_object (&f->connection);
  g_clear_error (&f->error);
  g_free (f->guid);
}

static void
on_new_conn (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  GDBusConnection **connection = user_data;
  GError *error = NULL;

  *connection = g_dbus_connection_new_for_address_finish (res, &error);
  g_assert_no_error (error);
}

static void
test_once (Fixture       *f,
           gconstpointer  context)
{
  GDBusMessage *message;
  gboolean pipe_res;

  pipe_res = g_unix_open_pipe (f->server_to_client, FD_CLOEXEC, &f->error);
  g_assert (pipe_res);
  pipe_res = g_unix_open_pipe (f->client_to_server, FD_CLOEXEC, &f->error);
  g_assert (pipe_res);

  f->server_iostream = my_io_stream_new_for_fds (f->client_to_server[0],
                                                 f->server_to_client[1]);
  f->iostream = my_io_stream_new_for_fds (f->server_to_client[0],
                                          f->client_to_server[1]);

  g_dbus_connection_new (f->server_iostream,
                         f->guid,
                         (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER |
                          G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS),
                         NULL /* auth observer */,
                         NULL /* cancellable */,
                         on_new_conn, &f->server_conn);

  g_dbus_connection_new (f->iostream,
                         NULL,
                         G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                         NULL /* auth observer */,
                         NULL /* cancellable */,
                         on_new_conn, &f->connection);

  while (f->server_conn == NULL || f->connection == NULL)
    g_main_context_iteration (NULL, TRUE);

  /*
   * queue a message - it'll sometimes be sent while the close is pending,
   * triggering the bug
   */
  message = g_dbus_message_new_signal ("/", "com.example.Foo", "Bar");
  g_dbus_connection_send_message (f->connection, message, 0, NULL, &f->error);
  g_assert_no_error (f->error);
  g_object_unref (message);

  /* close the connection (deliberately or via last-unref) */
  if (g_strcmp0 (context, "unref") == 0)
    {
      g_clear_object (&f->connection);
    }
  else
    {
      g_dbus_connection_close_sync (f->connection, NULL, &f->error);
      g_assert_no_error (f->error);
    }

  /* either way, wait for the connection to close */
  while (!g_dbus_connection_is_closed (f->server_conn))
    g_main_context_iteration (NULL, TRUE);

  /* clean up before the next run */
  g_clear_object (&f->iostream);
  g_clear_object (&f->server_iostream);
  g_clear_object (&f->connection);
  g_clear_object (&f->server_conn);
  g_clear_error (&f->error);
}

static void
test_many_times (Fixture       *f,
                 gconstpointer  context)
{
  guint i, n_repeats;

  if (g_test_slow ())
    n_repeats = N_REPEATS_SLOW;
  else
    n_repeats = N_REPEATS;

  for (i = 0; i < n_repeats; i++)
    test_once (f, context);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/gdbus/close-pending", Fixture, "close",
      setup, test_many_times, teardown);
  g_test_add ("/gdbus/unref-pending", Fixture, "unref",
      setup, test_many_times, teardown);

  return g_test_run();
}
