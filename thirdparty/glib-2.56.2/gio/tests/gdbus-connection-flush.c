/* Test case for GNOME #662395
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
 * Copyright (C) 2011 Nokia Corporation
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

#include <unistd.h>
#include <string.h>

#include <gio/gio.h>

#include "test-io-stream.h"
#include "test-pipe-unix.h"

#define MY_TYPE_OUTPUT_STREAM \
        (my_output_stream_get_type ())
#define MY_OUTPUT_STREAM(o) \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                                     MY_TYPE_OUTPUT_STREAM, \
                                     MyOutputStream))
#define MY_IS_OUTPUT_STREAM(o) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MY_TYPE_OUTPUT_STREAM))

G_LOCK_DEFINE_STATIC (write);

typedef struct {
    GFilterOutputStream parent;

    volatile gint started;
    volatile gint finished;
    volatile gint flushed;

    GOutputStream *real_output;
} MyOutputStream;

typedef struct {
    GFilterOutputStreamClass parent;
} MyOutputStreamClass;

static GType my_output_stream_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MyOutputStream, my_output_stream, G_TYPE_FILTER_OUTPUT_STREAM)

/* Called from GDBusWorker thread */
static gssize
my_output_stream_write (GOutputStream  *os,
                        const void     *buffer,
                        gsize           count,
                        GCancellable   *cancellable,
                        GError        **error)
{
  MyOutputStream *self = MY_OUTPUT_STREAM (os);
  GFilterOutputStream *filter = G_FILTER_OUTPUT_STREAM (os);
  GOutputStream *real = g_filter_output_stream_get_base_stream (filter);
  gssize ret;

  g_atomic_int_add (&self->started, count);
  /* Other threads can make writing block forever by taking this lock */
  G_LOCK (write);
  ret = g_output_stream_write (real, buffer, count, cancellable, error);
  G_UNLOCK (write);
  g_atomic_int_add (&self->finished, count);
  return ret;
}

/* Called from GDBusWorker thread */
static gboolean
my_output_stream_flush (GOutputStream             *os,
                        GCancellable              *cancellable,
                        GError                   **error)
{
  MyOutputStream *self = MY_OUTPUT_STREAM (os);
  GFilterOutputStream *filter = G_FILTER_OUTPUT_STREAM (os);
  GOutputStream *real = g_filter_output_stream_get_base_stream (filter);
  gint started, finished;
  gboolean ret;

  /* These should be equal because you're not allowed to flush with a
   * write pending, and GOutputStream enforces that for its subclasses
   */
  started = g_atomic_int_get (&self->started);
  finished = g_atomic_int_get (&self->finished);
  g_assert_cmpint (started, ==, finished);

  ret = g_output_stream_flush (real, cancellable, error);

  /* As above, this shouldn't have changed during the flush */
  finished = g_atomic_int_get (&self->finished);
  g_assert_cmpint (started, ==, finished);

  /* Checkpoint reached */
  g_atomic_int_set (&self->flushed, finished);
  return ret;
}

/* Called from any thread; thread-safe */
static gint
my_output_stream_get_bytes_started (GOutputStream *os)
{
  MyOutputStream *self = MY_OUTPUT_STREAM (os);

  return g_atomic_int_get (&self->started);
}

/* Called from any thread; thread-safe */
static gint
my_output_stream_get_bytes_finished (GOutputStream *os)
{
  MyOutputStream *self = MY_OUTPUT_STREAM (os);

  return g_atomic_int_get (&self->finished);
}

/* Called from any thread; thread-safe */
static gint
my_output_stream_get_bytes_flushed (GOutputStream *os)
{
  MyOutputStream *self = MY_OUTPUT_STREAM (os);

  return g_atomic_int_get (&self->flushed);
}

static void
my_output_stream_init (MyOutputStream *self)
{
}

static void
my_output_stream_class_init (MyOutputStreamClass *cls)
{
  GOutputStreamClass *ostream_class = (GOutputStreamClass *) cls;

  ostream_class->write_fn = my_output_stream_write;
  ostream_class->flush = my_output_stream_flush;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct {
    GError *error;
    gchar *guid;
    gboolean flushed;

    GIOStream *client_stream;
    GInputStream *client_istream;
    GOutputStream *client_ostream;
    GOutputStream *client_real_ostream;
    GDBusConnection *client_conn;

    GIOStream *server_stream;
    GInputStream *server_istream;
    GOutputStream *server_ostream;
    GDBusConnection *server_conn;
} Fixture;

static void
setup_client_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  Fixture *f = user_data;

  f->client_conn = g_dbus_connection_new_finish (res, &f->error);
  g_assert_no_error (f->error);
  g_assert (G_IS_DBUS_CONNECTION (f->client_conn));
  g_assert (f->client_conn == G_DBUS_CONNECTION (source));
}

static void
setup_server_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  Fixture *f = user_data;

  f->server_conn = g_dbus_connection_new_finish (res, &f->error);
  g_assert_no_error (f->error);
  g_assert (G_IS_DBUS_CONNECTION (f->server_conn));
  g_assert (f->server_conn == G_DBUS_CONNECTION (source));
}

static void
setup (Fixture       *f,
       gconstpointer  test_data G_GNUC_UNUSED)
{
  gboolean ok;

  f->guid = g_dbus_generate_guid ();

  ok = test_pipe (&f->server_istream, &f->client_real_ostream, &f->error);
  g_assert_no_error (f->error);
  g_assert (G_IS_OUTPUT_STREAM (f->client_real_ostream));
  g_assert (G_IS_INPUT_STREAM (f->server_istream));
  g_assert (ok);

  f->client_ostream = g_object_new (MY_TYPE_OUTPUT_STREAM,
                                    "base-stream", f->client_real_ostream,
                                    "close-base-stream", TRUE,
                                    NULL);
  g_assert (G_IS_OUTPUT_STREAM (f->client_ostream));

  ok = test_pipe (&f->client_istream, &f->server_ostream, &f->error);
  g_assert_no_error (f->error);
  g_assert (G_IS_OUTPUT_STREAM (f->server_ostream));
  g_assert (G_IS_INPUT_STREAM (f->client_istream));
  g_assert (ok);

  f->client_stream = test_io_stream_new (f->client_istream, f->client_ostream);
  f->server_stream = test_io_stream_new (f->server_istream, f->server_ostream);

  g_dbus_connection_new (f->client_stream, NULL,
                         G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                         NULL, NULL, setup_client_cb, f);
  g_dbus_connection_new (f->server_stream, f->guid,
                         G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER,
                         NULL, NULL, setup_server_cb, f);

  while (f->client_conn == NULL || f->server_conn == NULL)
    g_main_context_iteration (NULL, TRUE);
}

static void
flush_cb (GObject      *source,
          GAsyncResult *res,
          gpointer      user_data)
{
  Fixture *f = user_data;
  gboolean ok;

  g_assert (G_IS_DBUS_CONNECTION (source));
  g_assert (G_IS_DBUS_CONNECTION (f->client_conn));
  g_assert_cmpuint ((guintptr) f->client_conn, ==, (guintptr) G_DBUS_CONNECTION (source));

  ok = g_dbus_connection_flush_finish (f->client_conn, res, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  f->flushed = TRUE;
}

static void
test_flush_busy (Fixture       *f,
                 gconstpointer  test_data G_GNUC_UNUSED)
{
  gint initial, started;
  gboolean ok;

  initial = my_output_stream_get_bytes_started (f->client_ostream);
  /* make sure the actual write will block */
  G_LOCK (write);

  ok = g_dbus_connection_emit_signal (f->client_conn, NULL, "/",
                                      "com.example.Foo", "SomeSignal", NULL,
                                      &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* wait for at least part of the message to have started writing -
   * the write will block indefinitely in the worker thread
   */
  do {
    started = my_output_stream_get_bytes_started (f->client_ostream);
    g_thread_yield ();
  } while (initial >= started);

  /* we haven't flushed anything */
  g_assert_cmpint (my_output_stream_get_bytes_flushed (f->client_ostream),
                   <=, initial);

  /* start to flush: it can't happen til the write finishes */
  g_dbus_connection_flush (f->client_conn, NULL, flush_cb, f);

  /* we still haven't actually flushed anything */
  g_assert_cmpint (my_output_stream_get_bytes_flushed (f->client_ostream),
                   <=, initial);

  /* let the write finish */
  G_UNLOCK (write);

  /* wait for the flush to happen */
  while (!f->flushed)
    g_main_context_iteration (NULL, TRUE);

  /* now we have flushed at least what we'd written - but before fixing
   * GNOME#662395 this assertion would fail
   */
  g_assert_cmpint (my_output_stream_get_bytes_flushed (f->client_ostream),
                   >=, started);
}

static void
test_flush_idle (Fixture       *f,
                 gconstpointer  test_data G_GNUC_UNUSED)
{
  gint initial, finished;
  gboolean ok;

  initial = my_output_stream_get_bytes_finished (f->client_ostream);

  ok = g_dbus_connection_emit_signal (f->client_conn, NULL, "/",
                                      "com.example.Foo", "SomeSignal", NULL,
                                      &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* wait for at least part of the message to have been written */
  do {
    finished = my_output_stream_get_bytes_finished (f->client_ostream);
    g_thread_yield ();
  } while (initial >= finished);

  /* we haven't flushed anything */
  g_assert_cmpint (my_output_stream_get_bytes_flushed (f->client_ostream),
                   <=, initial);

  /* flush with fully-written, but unflushed, messages */
  ok = g_dbus_connection_flush_sync (f->client_conn, NULL, &f->error);

  /* now we have flushed at least what we'd written - but before fixing
   * GNOME#662395 this assertion would fail
   */
  g_assert_cmpint (my_output_stream_get_bytes_flushed (f->client_ostream),
                   >=, finished);
}

static void
teardown (Fixture       *f,
          gconstpointer  test_data G_GNUC_UNUSED)
{
  g_clear_error (&f->error);

  g_clear_object (&f->client_stream);
  g_clear_object (&f->client_istream);
  g_clear_object (&f->client_ostream);
  g_clear_object (&f->client_real_ostream);
  g_clear_object (&f->client_conn);

  g_clear_object (&f->server_stream);
  g_clear_object (&f->server_istream);
  g_clear_object (&f->server_ostream);
  g_clear_object (&f->server_conn);

  g_free (f->guid);
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/gdbus/connection/flush/busy", Fixture, NULL,
              setup, test_flush_busy, teardown);
  g_test_add ("/gdbus/connection/flush/idle", Fixture, NULL,
              setup, test_flush_idle, teardown);

  ret = g_test_run();

  return ret;
}
