/* GLib testing framework examples and tests
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

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

/* for open(2) */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

/* for g_unlink() */
#include <glib/gstdio.h>

#include <gio/gnetworking.h>
#include <gio/gunixsocketaddress.h>
#include <gio/gunixfdlist.h>
#include <gio/gcredentialsprivate.h>

#ifdef G_OS_UNIX
#include <gio/gunixconnection.h>
#include <errno.h>
#endif

#include "gdbus-tests.h"

#include "gdbus-object-manager-example/gdbus-example-objectmanager-generated.h"

#ifdef G_OS_UNIX
static gboolean is_unix = TRUE;
#else
static gboolean is_unix = FALSE;
#endif

static gchar *tmp_address = NULL;
static gchar *test_guid = NULL;
static GMutex service_loop_lock;
static GCond service_loop_cond;
static GMainLoop *service_loop = NULL;
static GDBusServer *server = NULL;
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that peer-to-peer connections work */
/* ---------------------------------------------------------------------------------------------------- */


typedef struct
{
  gboolean accept_connection;
  gint num_connection_attempts;
  GPtrArray *current_connections;
  guint num_method_calls;
  gboolean signal_received;
} PeerData;

static const gchar *test_interface_introspection_xml =
  "<node>"
  "  <interface name='org.gtk.GDBus.PeerTestInterface'>"
  "    <method name='HelloPeer'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <method name='EmitSignal'/>"
  "    <method name='EmitSignalWithNameSet'/>"
  "    <method name='OpenFile'>"
  "      <arg type='s' name='path' direction='in'/>"
  "    </method>"
  "    <signal name='PeerSignal'>"
  "      <arg type='s' name='a_string'/>"
  "    </signal>"
  "    <property type='s' name='PeerProperty' access='read'/>"
  "  </interface>"
  "</node>";
static GDBusInterfaceInfo *test_interface_introspection_data = NULL;

static void
test_interface_method_call (GDBusConnection       *connection,
                            const gchar           *sender,
                            const gchar           *object_path,
                            const gchar           *interface_name,
                            const gchar           *method_name,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer               user_data)
{
  PeerData *data = user_data;
  const GDBusMethodInfo *info;

  data->num_method_calls++;

  g_assert_cmpstr (object_path, ==, "/org/gtk/GDBus/PeerTestObject");
  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.PeerTestInterface");

  info = g_dbus_method_invocation_get_method_info (invocation);
  g_assert_cmpstr (info->name, ==, method_name);

  if (g_strcmp0 (method_name, "HelloPeer") == 0)
    {
      const gchar *greeting;
      gchar *response;

      g_variant_get (parameters, "(&s)", &greeting);

      response = g_strdup_printf ("You greeted me with '%s'.",
                                  greeting);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", response));
      g_free (response);
    }
  else if (g_strcmp0 (method_name, "EmitSignal") == 0)
    {
      GError *error;

      error = NULL;
      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     "/org/gtk/GDBus/PeerTestObject",
                                     "org.gtk.GDBus.PeerTestInterface",
                                     "PeerSignal",
                                     NULL,
                                     &error);
      g_assert_no_error (error);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "EmitSignalWithNameSet") == 0)
    {
      GError *error;
      gboolean ret;
      GDBusMessage *message;

      message = g_dbus_message_new_signal ("/org/gtk/GDBus/PeerTestObject",
                                           "org.gtk.GDBus.PeerTestInterface",
                                           "PeerSignalWithNameSet");
      g_dbus_message_set_sender (message, ":1.42");

      error = NULL;
      ret = g_dbus_connection_send_message (connection, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, &error);
      g_assert_no_error (error);
      g_assert (ret);
      g_object_unref (message);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "OpenFile") == 0)
    {
#ifdef G_OS_UNIX
      const gchar *path;
      GDBusMessage *reply;
      GError *error;
      gint fd;
      GUnixFDList *fd_list;

      g_variant_get (parameters, "(&s)", &path);

      fd_list = g_unix_fd_list_new ();

      error = NULL;

      fd = g_open (path, O_RDONLY, 0);
      g_assert (fd != -1);
      g_unix_fd_list_append (fd_list, fd, &error);
      g_assert_no_error (error);
      close (fd);

      reply = g_dbus_message_new_method_reply (g_dbus_method_invocation_get_message (invocation));
      g_dbus_message_set_unix_fd_list (reply, fd_list);
      g_object_unref (fd_list);
      g_object_unref (invocation);

      error = NULL;
      g_dbus_connection_send_message (connection,
                                      reply,
                                      G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                      NULL, /* out_serial */
                                      &error);
      g_assert_no_error (error);
      g_object_unref (reply);
#else
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.gtk.GDBus.NotOnUnix",
                                                  "Your OS does not support file descriptor passing");
#endif
    }
  else
    {
      g_assert_not_reached ();
    }
}

static GVariant *
test_interface_get_property (GDBusConnection  *connection,
                             const gchar      *sender,
                             const gchar      *object_path,
                             const gchar      *interface_name,
                             const gchar      *property_name,
                             GError          **error,
                             gpointer          user_data)
{
  g_assert_cmpstr (object_path, ==, "/org/gtk/GDBus/PeerTestObject");
  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.PeerTestInterface");
  g_assert_cmpstr (property_name, ==, "PeerProperty");

  return g_variant_new_string ("ThePropertyValue");
}


static const GDBusInterfaceVTable test_interface_vtable =
{
  test_interface_method_call,
  test_interface_get_property,
  NULL  /* set_property */
};

static void
on_proxy_signal_received (GDBusProxy *proxy,
                          gchar      *sender_name,
                          gchar      *signal_name,
                          GVariant   *parameters,
                          gpointer    user_data)
{
  PeerData *data = user_data;

  data->signal_received = TRUE;

  g_assert (sender_name == NULL);
  g_assert_cmpstr (signal_name, ==, "PeerSignal");
  g_main_loop_quit (loop);
}

static void
on_proxy_signal_received_with_name_set (GDBusProxy *proxy,
                                        gchar      *sender_name,
                                        gchar      *signal_name,
                                        GVariant   *parameters,
                                        gpointer    user_data)
{
  PeerData *data = user_data;

  data->signal_received = TRUE;

  g_assert_cmpstr (sender_name, ==, ":1.42");
  g_assert_cmpstr (signal_name, ==, "PeerSignalWithNameSet");
  g_main_loop_quit (loop);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
on_authorize_authenticated_peer (GDBusAuthObserver *observer,
                                 GIOStream         *stream,
                                 GCredentials      *credentials,
                                 gpointer           user_data)
{
  PeerData *data = user_data;
  gboolean authorized;

  data->num_connection_attempts++;

  authorized = TRUE;
  if (!data->accept_connection)
    {
      authorized = FALSE;
      g_main_loop_quit (loop);
    }

  return authorized;
}

/* Runs in thread we created GDBusServer in (since we didn't pass G_DBUS_SERVER_FLAGS_RUN_IN_THREAD) */
static gboolean
on_new_connection (GDBusServer *server,
                   GDBusConnection *connection,
                   gpointer user_data)
{
  PeerData *data = user_data;
  GError *error;
  guint reg_id;

  //g_printerr ("Client connected.\n"
  //         "Negotiated capabilities: unix-fd-passing=%d\n",
  //         g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING);

  g_ptr_array_add (data->current_connections, g_object_ref (connection));

#if G_CREDENTIALS_SUPPORTED
    {
      GCredentials *credentials;

      credentials = g_dbus_connection_get_peer_credentials (connection);

      g_assert (credentials != NULL);
      g_assert_cmpuint (g_credentials_get_unix_user (credentials, NULL), ==,
                        getuid ());
      g_assert_cmpuint (g_credentials_get_unix_pid (credentials, NULL), ==,
                        getpid ());
    }
#endif

  /* export object on the newly established connection */
  error = NULL;
  reg_id = g_dbus_connection_register_object (connection,
                                              "/org/gtk/GDBus/PeerTestObject",
                                              test_interface_introspection_data,
                                              &test_interface_vtable,
                                              data,
                                              NULL, /* GDestroyNotify for data */
                                              &error);
  g_assert_no_error (error);
  g_assert (reg_id > 0);

  g_main_loop_quit (loop);

  return TRUE;
}

/* We don't tell the main thread about the new GDBusServer until it has
 * had a chance to start listening. */
static gboolean
idle_in_service_loop (gpointer loop)
{
  g_assert (service_loop == NULL);
  g_mutex_lock (&service_loop_lock);
  service_loop = loop;
  g_cond_broadcast (&service_loop_cond);
  g_mutex_unlock (&service_loop_lock);

  return G_SOURCE_REMOVE;
}

static void
run_service_loop (GMainContext *service_context)
{
  GMainLoop *loop;
  GSource *source;

  g_assert (service_loop == NULL);

  loop = g_main_loop_new (service_context, FALSE);
  source = g_idle_source_new ();
  g_source_set_callback (source, idle_in_service_loop, loop, NULL);
  g_source_attach (source, service_context);
  g_source_unref (source);
  g_main_loop_run (loop);
}

static void
teardown_service_loop (void)
{
  g_mutex_lock (&service_loop_lock);
  g_clear_pointer (&service_loop, g_main_loop_unref);
  g_mutex_unlock (&service_loop_lock);
}

static void
await_service_loop (void)
{
  g_mutex_lock (&service_loop_lock);
  while (service_loop == NULL)
    g_cond_wait (&service_loop_cond, &service_loop_lock);
  g_mutex_unlock (&service_loop_lock);
}

static gpointer
service_thread_func (gpointer user_data)
{
  PeerData *data = user_data;
  GMainContext *service_context;
  GDBusAuthObserver *observer, *o;
  GError *error;
  GDBusServerFlags f;
  gchar *a, *g;
  gboolean b;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  error = NULL;
  observer = g_dbus_auth_observer_new ();
  server = g_dbus_server_new_sync (tmp_address,
                                   G_DBUS_SERVER_FLAGS_NONE,
                                   test_guid,
                                   observer,
                                   NULL, /* cancellable */
                                   &error);
  g_assert_no_error (error);

  g_signal_connect (server,
                    "new-connection",
                    G_CALLBACK (on_new_connection),
                    data);
  g_signal_connect (observer,
                    "authorize-authenticated-peer",
                    G_CALLBACK (on_authorize_authenticated_peer),
                    data);

  g_assert_cmpint (g_dbus_server_get_flags (server), ==, G_DBUS_SERVER_FLAGS_NONE);
  g_assert_cmpstr (g_dbus_server_get_guid (server), ==, test_guid);
  g_object_get (server,
                "flags", &f,
                "address", &a,
                "guid", &g,
                "active", &b,
                "authentication-observer", &o,
                NULL);
  g_assert_cmpint (f, ==, G_DBUS_SERVER_FLAGS_NONE);
  g_assert_cmpstr (a, ==, tmp_address);
  g_assert_cmpstr (g, ==, test_guid);
  g_assert (!b);
  g_assert (o == observer);
  g_free (a);
  g_free (g);
  g_object_unref (o);

  g_object_unref (observer);

  g_dbus_server_start (server);

  run_service_loop (service_context);

  g_main_context_pop_thread_default (service_context);

  teardown_service_loop ();
  g_main_context_unref (service_context);

  /* test code specifically unrefs the server - see below */
  g_assert (server == NULL);

  return NULL;
}

#if 0
static gboolean
on_incoming_connection (GSocketService     *service,
                        GSocketConnection  *socket_connection,
                        GObject            *source_object,
                        gpointer           user_data)
{
  PeerData *data = user_data;

  if (data->accept_connection)
    {
      GError *error;
      guint reg_id;
      GDBusConnection *connection;

      error = NULL;
      connection = g_dbus_connection_new_sync (G_IO_STREAM (socket_connection),
                                               test_guid,
                                               G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER,
                                               NULL, /* cancellable */
                                               &error);
      g_assert_no_error (error);

      g_ptr_array_add (data->current_connections, connection);

      /* export object on the newly established connection */
      error = NULL;
      reg_id = g_dbus_connection_register_object (connection,
                                                  "/org/gtk/GDBus/PeerTestObject",
                                                  &test_interface_introspection_data,
                                                  &test_interface_vtable,
                                                  data,
                                                  NULL, /* GDestroyNotify for data */
                                                  &error);
      g_assert_no_error (error);
      g_assert (reg_id > 0);

    }
  else
    {
      /* don't do anything */
    }

  data->num_connection_attempts++;

  g_main_loop_quit (loop);

  /* stops other signal handlers from being invoked */
  return TRUE;
}

static gpointer
service_thread_func (gpointer data)
{
  GMainContext *service_context;
  gchar *socket_path;
  GSocketAddress *address;
  GError *error;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  socket_path = g_strdup_printf ("/tmp/gdbus-test-pid-%d", getpid ());
  address = g_unix_socket_address_new (socket_path);

  service = g_socket_service_new ();
  error = NULL;
  g_socket_listener_add_address (G_SOCKET_LISTENER (service),
                                 address,
                                 G_SOCKET_TYPE_STREAM,
                                 G_SOCKET_PROTOCOL_DEFAULT,
                                 NULL, /* source_object */
                                 NULL, /* effective_address */
                                 &error);
  g_assert_no_error (error);
  g_signal_connect (service,
                    "incoming",
                    G_CALLBACK (on_incoming_connection),
                    data);
  g_socket_service_start (service);

  run_service_loop (service_context);

  g_main_context_pop_thread_default (service_context);

  teardown_service_loop ();
  g_main_context_unref (service_context);

  g_object_unref (address);
  g_free (socket_path);
  return NULL;
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

#if 0
static gboolean
check_connection (gpointer user_data)
{
  PeerData *data = user_data;
  guint n;

  for (n = 0; n < data->current_connections->len; n++)
    {
      GDBusConnection *c;
      GIOStream *stream;

      c = G_DBUS_CONNECTION (data->current_connections->pdata[n]);
      stream = g_dbus_connection_get_stream (c);

      g_debug ("In check_connection for %d: connection %p, stream %p", n, c, stream);
      g_debug ("closed = %d", g_io_stream_is_closed (stream));

      GSocket *socket;
      socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (stream));
      g_debug ("socket_closed = %d", g_socket_is_closed (socket));
      g_debug ("socket_condition_check = %d", g_socket_condition_check (socket, G_IO_IN|G_IO_OUT|G_IO_ERR|G_IO_HUP));

      gchar buf[128];
      GError *error;
      gssize num_read;
      error = NULL;
      num_read = g_input_stream_read (g_io_stream_get_input_stream (stream),
                                      buf,
                                      128,
                                      NULL,
                                      &error);
      if (num_read < 0)
        {
          g_debug ("error: %s", error->message);
          g_error_free (error);
        }
      else
        {
          g_debug ("no error, read %d bytes", (gint) num_read);
        }
    }

  return FALSE;
}

static gboolean
on_do_disconnect_in_idle (gpointer data)
{
  GDBusConnection *c = G_DBUS_CONNECTION (data);
  g_debug ("GDC %p has ref_count %d", c, G_OBJECT (c)->ref_count);
  g_dbus_connection_disconnect (c);
  g_object_unref (c);
  return FALSE;
}
#endif

#ifdef G_OS_UNIX
static gchar *
read_all_from_fd (gint fd, gsize *out_len, GError **error)
{
  GString *str;
  gchar buf[64];
  gssize num_read;

  str = g_string_new (NULL);

  do
    {
      int errsv;

      num_read = read (fd, buf, sizeof (buf));
      errsv = errno;
      if (num_read == -1)
        {
          if (errsv == EAGAIN || errsv == EWOULDBLOCK)
            continue;
          g_set_error (error,
                       G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       "Failed reading %d bytes into offset %d: %s",
                       (gint) sizeof (buf),
                       (gint) str->len,
                       g_strerror (errsv));
          goto error;
        }
      else if (num_read > 0)
        {
          g_string_append_len (str, buf, num_read);
        }
      else if (num_read == 0)
        {
          break;
        }
    }
  while (TRUE);

  if (out_len != NULL)
    *out_len = str->len;
  return g_string_free (str, FALSE);

 error:
  if (out_len != NULL)
    *out_len = 0;
  g_string_free (str, TRUE);
  return NULL;
}
#endif

static void
test_peer (void)
{
  GDBusConnection *c;
  GDBusConnection *c2;
  GDBusProxy *proxy;
  GError *error;
  PeerData data;
  GVariant *value;
  GVariant *result;
  const gchar *s;
  GThread *service_thread;
  gulong signal_handler_id;

  memset (&data, '\0', sizeof (PeerData));
  data.current_connections = g_ptr_array_new_with_free_func (g_object_unref);

  /* first try to connect when there is no server */
  error = NULL;
  c = g_dbus_connection_new_for_address_sync (is_unix ? "unix:path=/tmp/gdbus-test-does-not-exist-pid" :
                                              /* NOTE: Even if something is listening on port 12345 the connection
                                               * will fail because the nonce file doesn't exist */
                                              "nonce-tcp:host=localhost,port=12345,noncefile=this-does-not-exist-gdbus",
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* GDBusAuthObserver */
                                              NULL, /* cancellable */
                                              &error);
  _g_assert_error_domain (error, G_IO_ERROR);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_clear_error (&error);
  g_assert (c == NULL);

  /* bring up a server - we run the server in a different thread to avoid deadlocks */
  service_thread = g_thread_new ("test_peer",
                                 service_thread_func,
                                 &data);
  await_service_loop ();
  g_assert (server != NULL);

  /* bring up a connection and accept it */
  data.accept_connection = TRUE;
  error = NULL;
  c = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* GDBusAuthObserver */
                                              NULL, /* cancellable */
                                              &error);
  g_assert_no_error (error);
  g_assert (c != NULL);
  while (data.current_connections->len < 1)
    g_main_loop_run (loop);
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert_cmpint (data.num_connection_attempts, ==, 1);
  g_assert (g_dbus_connection_get_unique_name (c) == NULL);
  g_assert_cmpstr (g_dbus_connection_get_guid (c), ==, test_guid);

  /* check that we create a proxy, read properties, receive signals and invoke
   * the HelloPeer() method. Since the server runs in another thread it's fine
   * to use synchronous blocking API here.
   */
  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,
                                 NULL, /* bus_name */
                                 "/org/gtk/GDBus/PeerTestObject",
                                 "org.gtk.GDBus.PeerTestInterface",
                                 NULL, /* GCancellable */
                                 &error);
  g_assert_no_error (error);
  g_assert (proxy != NULL);
  error = NULL;
  value = g_dbus_proxy_get_cached_property (proxy, "PeerProperty");
  g_assert_cmpstr (g_variant_get_string (value, NULL), ==, "ThePropertyValue");

  /* try invoking a method */
  error = NULL;
  result = g_dbus_proxy_call_sync (proxy,
                                   "HelloPeer",
                                   g_variant_new ("(s)", "Hey Peer!"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,  /* GCancellable */
                                   &error);
  g_assert_no_error (error);
  g_variant_get (result, "(&s)", &s);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hey Peer!'.");
  g_variant_unref (result);
  g_assert_cmpint (data.num_method_calls, ==, 1);

  /* make the other peer emit a signal - catch it */
  signal_handler_id = g_signal_connect (proxy,
                                        "g-signal",
                                        G_CALLBACK (on_proxy_signal_received),
                                        &data);
  g_assert (!data.signal_received);
  g_dbus_proxy_call (proxy,
                     "EmitSignal",
                     NULL,  /* no arguments */
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,  /* GCancellable */
                     NULL,  /* GAsyncReadyCallback - we don't care about the result */
                     NULL); /* user_data */
  g_main_loop_run (loop);
  g_assert (data.signal_received);
  g_assert_cmpint (data.num_method_calls, ==, 2);
  g_signal_handler_disconnect (proxy, signal_handler_id);

  /* Also ensure that messages with the sender header-field set gets
   * delivered to the proxy - note that this doesn't really make sense
   * e.g. names are meaning-less in a peer-to-peer case... but we
   * support it because it makes sense in certain bridging
   * applications - see e.g. #623815.
   */
  signal_handler_id = g_signal_connect (proxy,
                                        "g-signal",
                                        G_CALLBACK (on_proxy_signal_received_with_name_set),
                                        &data);
  data.signal_received = FALSE;
  g_dbus_proxy_call (proxy,
                     "EmitSignalWithNameSet",
                     NULL,  /* no arguments */
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,  /* GCancellable */
                     NULL,  /* GAsyncReadyCallback - we don't care about the result */
                     NULL); /* user_data */
  g_main_loop_run (loop);
  g_assert (data.signal_received);
  g_assert_cmpint (data.num_method_calls, ==, 3);
  g_signal_handler_disconnect (proxy, signal_handler_id);

  /* check for UNIX fd passing */
#ifdef G_OS_UNIX
  {
    GDBusMessage *method_call_message;
    GDBusMessage *method_reply_message;
    GUnixFDList *fd_list;
    gint fd;
    gchar *buf;
    gsize len;
    gchar *buf2;
    gsize len2;
    const char *testfile = g_test_get_filename (G_TEST_DIST, "file.c", NULL);

    method_call_message = g_dbus_message_new_method_call (NULL, /* name */
                                                          "/org/gtk/GDBus/PeerTestObject",
                                                          "org.gtk.GDBus.PeerTestInterface",
                                                          "OpenFile");
    g_dbus_message_set_body (method_call_message, g_variant_new ("(s)", testfile));
    error = NULL;
    method_reply_message = g_dbus_connection_send_message_with_reply_sync (c,
                                                                           method_call_message,
                                                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                                                           -1,
                                                                           NULL, /* out_serial */
                                                                           NULL, /* cancellable */
                                                                           &error);
    g_assert_no_error (error);
    g_assert (g_dbus_message_get_message_type (method_reply_message) == G_DBUS_MESSAGE_TYPE_METHOD_RETURN);
    fd_list = g_dbus_message_get_unix_fd_list (method_reply_message);
    g_assert (fd_list != NULL);
    g_assert_cmpint (g_unix_fd_list_get_length (fd_list), ==, 1);
    error = NULL;
    fd = g_unix_fd_list_get (fd_list, 0, &error);
    g_assert_no_error (error);
    g_object_unref (method_call_message);
    g_object_unref (method_reply_message);

    error = NULL;
    len = 0;
    buf = read_all_from_fd (fd, &len, &error);
    g_assert_no_error (error);
    g_assert (buf != NULL);
    close (fd);

    error = NULL;
    g_file_get_contents (testfile,
                         &buf2,
                         &len2,
                         &error);
    g_assert_no_error (error);
    g_assert_cmpmem (buf, len, buf2, len2);
    g_free (buf2);
    g_free (buf);
  }
#else
  error = NULL;
  result = g_dbus_proxy_call_sync (proxy,
                                   "OpenFile",
                                   g_variant_new ("(s)", "boo"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,  /* GCancellable */
                                   &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR);
  g_assert (result == NULL);
  g_error_free (error);
#endif /* G_OS_UNIX */

  /* Check that g_socket_get_credentials() work - (though this really
   * should be in socket.c)
   */
  {
    GSocket *socket;
    GCredentials *credentials;
    socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (g_dbus_connection_get_stream (c)));
    g_assert (G_IS_SOCKET (socket));
    error = NULL;
    credentials = g_socket_get_credentials (socket, &error);

#if G_CREDENTIALS_SOCKET_GET_CREDENTIALS_SUPPORTED
    g_assert_no_error (error);
    g_assert (G_IS_CREDENTIALS (credentials));

    g_assert_cmpuint (g_credentials_get_unix_user (credentials, NULL), ==,
                      getuid ());
    g_assert_cmpuint (g_credentials_get_unix_pid (credentials, NULL), ==,
                      getpid ());
#else
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
    g_assert (credentials == NULL);
#endif
  }


  /* bring up a connection - don't accept it - this should fail
   */
  data.accept_connection = FALSE;
  error = NULL;
  c2 = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                               G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                               NULL, /* GDBusAuthObserver */
                                               NULL, /* cancellable */
                                               &error);
  _g_assert_error_domain (error, G_IO_ERROR);
  g_error_free (error);
  g_assert (c2 == NULL);

#if 0
  /* TODO: THIS TEST DOESN'T WORK YET */

  /* bring up a connection - accept it.. then disconnect from the client side - check
   * that the server side gets the disconnect signal.
   */
  error = NULL;
  data.accept_connection = TRUE;
  c2 = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                               G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                               NULL, /* GDBusAuthObserver */
                                               NULL, /* cancellable */
                                               &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_get_is_disconnected (c2));
  while (data.num_connection_attempts < 3)
    g_main_loop_run (loop);
  g_assert_cmpint (data.current_connections->len, ==, 2);
  g_assert_cmpint (data.num_connection_attempts, ==, 3);
  g_assert (!g_dbus_connection_get_is_disconnected (G_DBUS_CONNECTION (data.current_connections->pdata[1])));
  g_idle_add (on_do_disconnect_in_idle, c2);
  g_debug ("==================================================");
  g_debug ("==================================================");
  g_debug ("==================================================");
  g_debug ("waiting for disconnect on connection %p, stream %p",
           data.current_connections->pdata[1],
           g_dbus_connection_get_stream (data.current_connections->pdata[1]));

  g_timeout_add (2000, check_connection, &data);
  //_g_assert_signal_received (G_DBUS_CONNECTION (data.current_connections->pdata[1]), "closed");
  g_main_loop_run (loop);
  g_assert (g_dbus_connection_get_is_disconnected (G_DBUS_CONNECTION (data.current_connections->pdata[1])));
  g_ptr_array_set_size (data.current_connections, 1); /* remove disconnected connection object */
#endif

  /* unref the server and stop listening for new connections
   *
   * This won't bring down the established connections - check that c is still connected
   * by invoking a method
   */
  //g_socket_service_stop (service);
  //g_object_unref (service);
  g_dbus_server_stop (server);
  g_object_unref (server);
  server = NULL;

  error = NULL;
  result = g_dbus_proxy_call_sync (proxy,
                                   "HelloPeer",
                                   g_variant_new ("(s)", "Hey Again Peer!"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,  /* GCancellable */
                                   &error);
  g_assert_no_error (error);
  g_variant_get (result, "(&s)", &s);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hey Again Peer!'.");
  g_variant_unref (result);
  g_assert_cmpint (data.num_method_calls, ==, 5);

#if 0
  /* TODO: THIS TEST DOESN'T WORK YET */

  /* now disconnect from the server side - check that the client side gets the signal */
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert (G_DBUS_CONNECTION (data.current_connections->pdata[0]) != c);
  g_dbus_connection_disconnect (G_DBUS_CONNECTION (data.current_connections->pdata[0]));
  if (!g_dbus_connection_get_is_disconnected (c))
    _g_assert_signal_received (c, "closed");
  g_assert (g_dbus_connection_get_is_disconnected (c));
#endif

  g_object_unref (c);
  g_ptr_array_unref (data.current_connections);
  g_object_unref (proxy);

  g_main_loop_quit (service_loop);
  g_thread_join (service_thread);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GDBusServer *server;
  GMainContext *context;
  GMainLoop *loop;

  GList *connections;
} DmpData;

static void
dmp_data_free (DmpData *data)
{
  g_main_loop_unref (data->loop);
  g_main_context_unref (data->context);
  g_object_unref (data->server);
  g_list_free_full (data->connections, g_object_unref);
  g_free (data);
}

static void
dmp_on_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  //DmpData *data = user_data;
  gint32 first;
  gint32 second;
  g_variant_get (parameters,
                 "(ii)",
                 &first,
                 &second);
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(i)", first + second));
}

static const GDBusInterfaceVTable dmp_interface_vtable =
{
  dmp_on_method_call,
  NULL,  /* get_property */
  NULL   /* set_property */
};


/* Runs in thread we created GDBusServer in (since we didn't pass G_DBUS_SERVER_FLAGS_RUN_IN_THREAD) */
static gboolean
dmp_on_new_connection (GDBusServer     *server,
                       GDBusConnection *connection,
                       gpointer         user_data)
{
  DmpData *data = user_data;
  GDBusNodeInfo *node;
  GError *error;

  /* accept the connection */
  data->connections = g_list_prepend (data->connections, g_object_ref (connection));

  error = NULL;
  node = g_dbus_node_info_new_for_xml ("<node>"
                                       "  <interface name='org.gtk.GDBus.DmpInterface'>"
                                       "    <method name='AddPair'>"
                                       "      <arg type='i' name='first' direction='in'/>"
                                       "      <arg type='i' name='second' direction='in'/>"
                                       "      <arg type='i' name='sum' direction='out'/>"
                                       "    </method>"
                                       "  </interface>"
                                       "</node>",
                                       &error);
  g_assert_no_error (error);

  /* sleep 100ms before exporting an object - this is to test that
   * G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING really works
   * (GDBusServer uses this feature).
   */
  usleep (100 * 1000);

  /* export an object */
  error = NULL;
  g_dbus_connection_register_object (connection,
                                     "/dmp/test",
                                     node->interfaces[0],
                                     &dmp_interface_vtable,
                                     data,
                                     NULL,
                                     &error);
  g_dbus_node_info_unref (node);

  return TRUE;
}

static gpointer
dmp_thread_func (gpointer user_data)
{
  DmpData *data = user_data;
  GError *error;
  gchar *guid;

  data->context = g_main_context_new ();
  g_main_context_push_thread_default (data->context);

  error = NULL;
  guid = g_dbus_generate_guid ();
  data->server = g_dbus_server_new_sync (tmp_address,
                                         G_DBUS_SERVER_FLAGS_NONE,
                                         guid,
                                         NULL, /* GDBusAuthObserver */
                                         NULL, /* GCancellable */
                                         &error);
  g_assert_no_error (error);
  g_signal_connect (data->server,
                    "new-connection",
                    G_CALLBACK (dmp_on_new_connection),
                    data);

  g_dbus_server_start (data->server);

  data->loop = g_main_loop_new (data->context, FALSE);
  g_main_loop_run (data->loop);

  g_main_context_pop_thread_default (data->context);

  g_free (guid);
  return NULL;
}

static void
delayed_message_processing (void)
{
  GError *error;
  DmpData *data;
  GThread *service_thread;
  guint n;

  data = g_new0 (DmpData, 1);

  service_thread = g_thread_new ("dmp",
                                 dmp_thread_func,
                                 data);
  while (data->server == NULL || !g_dbus_server_is_active (data->server))
    g_thread_yield ();

  for (n = 0; n < 5; n++)
    {
      GDBusConnection *c;
      GVariant *res;
      gint32 val;

      error = NULL;
      c = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (data->server),
                                                  G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                  NULL, /* GDBusAuthObserver */
                                                  NULL, /* GCancellable */
                                                  &error);
      g_assert_no_error (error);

      error = NULL;
      res = g_dbus_connection_call_sync (c,
                                         NULL,    /* bus name */
                                         "/dmp/test",
                                         "org.gtk.GDBus.DmpInterface",
                                         "AddPair",
                                         g_variant_new ("(ii)", 2, n),
                                         G_VARIANT_TYPE ("(i)"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1, /* timeout_msec */
                                         NULL, /* GCancellable */
                                         &error);
      g_assert_no_error (error);
      g_variant_get (res, "(i)", &val);
      g_assert_cmpint (val, ==, 2 + n);
      g_variant_unref (res);
      g_object_unref (c);
  }

  g_main_loop_quit (data->loop);
  g_thread_join (service_thread);
  dmp_data_free (data);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
nonce_tcp_on_authorize_authenticated_peer (GDBusAuthObserver *observer,
                                           GIOStream         *stream,
                                           GCredentials      *credentials,
                                           gpointer           user_data)
{
  PeerData *data = user_data;
  gboolean authorized;

  data->num_connection_attempts++;

  authorized = TRUE;
  if (!data->accept_connection)
    {
      authorized = FALSE;
      g_main_loop_quit (loop);
    }

  return authorized;
}

/* Runs in thread we created GDBusServer in (since we didn't pass G_DBUS_SERVER_FLAGS_RUN_IN_THREAD) */
static gboolean
nonce_tcp_on_new_connection (GDBusServer *server,
                             GDBusConnection *connection,
                             gpointer user_data)
{
  PeerData *data = user_data;

  g_ptr_array_add (data->current_connections, g_object_ref (connection));

  g_main_loop_quit (loop);

  return TRUE;
}

static gpointer
nonce_tcp_service_thread_func (gpointer user_data)
{
  PeerData *data = user_data;
  GMainContext *service_context;
  GDBusAuthObserver *observer;
  GError *error;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  error = NULL;
  observer = g_dbus_auth_observer_new ();
  server = g_dbus_server_new_sync ("nonce-tcp:",
                                   G_DBUS_SERVER_FLAGS_NONE,
                                   test_guid,
                                   observer,
                                   NULL, /* cancellable */
                                   &error);
  g_assert_no_error (error);

  g_signal_connect (server,
                    "new-connection",
                    G_CALLBACK (nonce_tcp_on_new_connection),
                    data);
  g_signal_connect (observer,
                    "authorize-authenticated-peer",
                    G_CALLBACK (nonce_tcp_on_authorize_authenticated_peer),
                    data);
  g_object_unref (observer);

  g_dbus_server_start (server);

  run_service_loop (service_context);

  g_main_context_pop_thread_default (service_context);

  teardown_service_loop ();
  g_main_context_unref (service_context);

  /* test code specifically unrefs the server - see below */
  g_assert (server == NULL);

  return NULL;
}

static void
test_nonce_tcp (void)
{
  PeerData data;
  GError *error;
  GThread *service_thread;
  GDBusConnection *c;
  gchar *s;
  gchar *nonce_file;
  gboolean res;
  const gchar *address;

  memset (&data, '\0', sizeof (PeerData));
  data.current_connections = g_ptr_array_new_with_free_func (g_object_unref);

  error = NULL;
  server = NULL;
  service_thread = g_thread_new ("nonce-tcp-service",
                                 nonce_tcp_service_thread_func,
                                 &data);
  await_service_loop ();
  g_assert (server != NULL);

  /* bring up a connection and accept it */
  data.accept_connection = TRUE;
  error = NULL;
  c = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* GDBusAuthObserver */
                                              NULL, /* cancellable */
                                              &error);
  g_assert_no_error (error);
  g_assert (c != NULL);
  while (data.current_connections->len < 1)
    g_thread_yield ();
  g_assert_cmpint (data.current_connections->len, ==, 1);
  g_assert_cmpint (data.num_connection_attempts, ==, 1);
  g_assert (g_dbus_connection_get_unique_name (c) == NULL);
  g_assert_cmpstr (g_dbus_connection_get_guid (c), ==, test_guid);
  g_object_unref (c);

  /* now, try to subvert the nonce file (this assumes noncefile is the last key/value pair)
   */

  address = g_dbus_server_get_client_address (server);

  s = strstr (address, "noncefile=");
  g_assert (s != NULL);
  s += sizeof "noncefile=" - 1;
  nonce_file = g_strdup (s);

  /* First try invalid data in the nonce file - this will actually
   * make the client send this and the server will reject it. The way
   * it works is that if the nonce doesn't match, the server will
   * simply close the connection. So, from the client point of view,
   * we can see a variety of errors.
   */
  error = NULL;
  res = g_file_set_contents (nonce_file,
                             "0123456789012345",
                             -1,
                             &error);
  g_assert_no_error (error);
  g_assert (res);
  c = g_dbus_connection_new_for_address_sync (address,
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* GDBusAuthObserver */
                                              NULL, /* cancellable */
                                              &error);
  _g_assert_error_domain (error, G_IO_ERROR);
  g_error_free (error);
  g_assert (c == NULL);

  /* Then try with a nonce-file of incorrect length - this will make
   * the client complain - we won't even try connecting to the server
   * for this
   */
  error = NULL;
  res = g_file_set_contents (nonce_file,
                             "0123456789012345_",
                             -1,
                             &error);
  g_assert_no_error (error);
  g_assert (res);
  c = g_dbus_connection_new_for_address_sync (address,
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* GDBusAuthObserver */
                                              NULL, /* cancellable */
                                              &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_error_free (error);
  g_assert (c == NULL);

  /* Finally try with no nonce-file at all */
  g_assert_cmpint (g_unlink (nonce_file), ==, 0);
  error = NULL;
  c = g_dbus_connection_new_for_address_sync (address,
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                              NULL, /* GDBusAuthObserver */
                                              NULL, /* cancellable */
                                              &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_error_free (error);
  g_assert (c == NULL);

  g_free (nonce_file);

  g_dbus_server_stop (server);
  g_object_unref (server);
  server = NULL;

  g_main_loop_quit (service_loop);
  g_thread_join (service_thread);
}

static void
test_credentials (void)
{
  GCredentials *c1, *c2;
  GError *error;
  gchar *desc;

  c1 = g_credentials_new ();
  c2 = g_credentials_new ();

  error = NULL;
  if (g_credentials_set_unix_user (c2, getuid (), &error))
    g_assert_no_error (error);

  g_clear_error (&error);
  g_assert (g_credentials_is_same_user (c1, c2, &error));
  g_assert_no_error (error);

  desc = g_credentials_to_string (c1);
  g_assert (desc != NULL);
  g_free (desc);

  g_object_unref (c1);
  g_object_unref (c2);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
tcp_anonymous_on_new_connection (GDBusServer     *server,
                                 GDBusConnection *connection,
                                 gpointer         user_data)
{
  gboolean *seen_connection = user_data;
  *seen_connection = TRUE;
  return TRUE;
}

static gpointer
tcp_anonymous_service_thread_func (gpointer user_data)
{
  gboolean *seen_connection = user_data;
  GMainContext *service_context;
  GError *error;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  error = NULL;
  server = g_dbus_server_new_sync ("tcp:",
                                   G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS,
                                   test_guid,
                                   NULL, /* GDBusObserver* */
                                   NULL, /* GCancellable* */
                                   &error);
  g_assert_no_error (error);

  g_signal_connect (server,
                    "new-connection",
                    G_CALLBACK (tcp_anonymous_on_new_connection),
                    seen_connection);

  g_dbus_server_start (server);

  run_service_loop (service_context);

  g_main_context_pop_thread_default (service_context);

  teardown_service_loop ();
  g_main_context_unref (service_context);

  return NULL;
}

static void
test_tcp_anonymous (void)
{
  gboolean seen_connection;
  GThread *service_thread;
  GDBusConnection *connection;
  GError *error;

  seen_connection = FALSE;
  service_thread = g_thread_new ("tcp-anon-service",
                                 tcp_anonymous_service_thread_func,
                                 &seen_connection);
  await_service_loop ();
  g_assert (server != NULL);

  error = NULL;
  connection = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (server),
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                       NULL, /* GDBusAuthObserver* */
                                                       NULL, /* GCancellable */
                                                       &error);
  g_assert_no_error (error);
  g_assert (connection != NULL);

  while (!seen_connection)
    g_thread_yield ();

  g_object_unref (connection);

  g_main_loop_quit (service_loop);
  g_dbus_server_stop (server);
  g_object_unref (server);
  server = NULL;

  g_thread_join (service_thread);
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusServer *codegen_server = NULL;

static gboolean
codegen_on_animal_poke (ExampleAnimal          *animal,
                        GDBusMethodInvocation  *invocation,
                        gboolean                make_sad,
                        gboolean                make_happy,
                        gpointer                user_data)
{
  if ((make_sad && make_happy) || (!make_sad && !make_happy))
    {
      g_main_loop_quit (service_loop);

      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.gtk.GDBus.Examples.ObjectManager.Error.Failed",
                                                  "Exactly one of make_sad or make_happy must be TRUE");
      goto out;
    }

  if (make_sad)
    {
      if (g_strcmp0 (example_animal_get_mood (animal), "Sad") == 0)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gtk.GDBus.Examples.ObjectManager.Error.SadAnimalIsSad",
                                                      "Sad animal is already sad");
          goto out;
        }

      example_animal_set_mood (animal, "Sad");
      example_animal_complete_poke (animal, invocation);
      goto out;
    }

  if (make_happy)
    {
      if (g_strcmp0 (example_animal_get_mood (animal), "Happy") == 0)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "org.gtk.GDBus.Examples.ObjectManager.Error.HappyAnimalIsHappy",
                                                      "Happy animal is already happy");
          goto out;
        }

      example_animal_set_mood (animal, "Happy");
      example_animal_complete_poke (animal, invocation);
      goto out;
    }

  g_assert_not_reached ();

 out:
  return TRUE; /* to indicate that the method was handled */
}

/* Runs in thread we created GDBusServer in (since we didn't pass G_DBUS_SERVER_FLAGS_RUN_IN_THREAD) */
static gboolean
codegen_on_new_connection (GDBusServer *server,
                           GDBusConnection *connection,
                           gpointer user_data)
{
  ExampleAnimal *animal = user_data;
  GError        *error = NULL;

  /* g_printerr ("Client connected.\n" */
  /*          "Negotiated capabilities: unix-fd-passing=%d\n", */
  /*          g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING); */

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (animal), connection,
                                    "/Example/Animals/000", &error);
  g_assert_no_error (error);

  return TRUE;
}

static gpointer
codegen_service_thread_func (gpointer user_data)
{
  GMainContext   *service_context;
  ExampleAnimal  *animal;
  GError         *error = NULL;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  /* Create the animal in the right thread context */
  animal = example_animal_skeleton_new ();

  /* Handle Poke() D-Bus method invocations on the .Animal interface */
  g_signal_connect (animal, "handle-poke",
                    G_CALLBACK (codegen_on_animal_poke),
                    NULL); /* user_data */

  codegen_server = g_dbus_server_new_sync (tmp_address,
                                           G_DBUS_SERVER_FLAGS_NONE,
                                           test_guid,
                                           NULL, /* observer */
                                           NULL, /* cancellable */
                                           &error);
  g_assert_no_error (error);
  g_dbus_server_start (codegen_server);

  g_signal_connect (codegen_server, "new-connection",
                    G_CALLBACK (codegen_on_new_connection),
                    animal);

  run_service_loop (service_context);

  g_object_unref (animal);

  g_main_context_pop_thread_default (service_context);

  teardown_service_loop ();
  g_main_context_unref (service_context);

  g_dbus_server_stop (codegen_server);
  g_object_unref (codegen_server);
  codegen_server = NULL;

  return NULL;
}


static gboolean
codegen_quit_mainloop_timeout (gpointer data)
{
  g_main_loop_quit (loop);
  return FALSE;
}

static void
codegen_test_peer (void)
{
  GDBusConnection     *connection;
  ExampleAnimal       *animal1, *animal2;
  GThread             *service_thread;
  GError              *error = NULL;
  GVariant            *value;
  const gchar         *s;

  /* bring up a server - we run the server in a different thread to avoid deadlocks */
  service_thread = g_thread_new ("codegen_test_peer",
                                 codegen_service_thread_func,
                                 NULL);
  await_service_loop ();
  g_assert (codegen_server != NULL);

  /* Get an animal 1 ...  */
  connection = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (codegen_server),
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                       NULL, /* GDBusAuthObserver */
                                                       NULL, /* cancellable */
                                                       &error);
  g_assert_no_error (error);
  g_assert (connection != NULL);

  animal1 = example_animal_proxy_new_sync (connection, 0, NULL,
                                           "/Example/Animals/000", NULL, &error);
  g_assert_no_error (error);
  g_assert (animal1 != NULL);
  g_object_unref (connection);

  /* Get animal 2 ...  */
  connection = g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (codegen_server),
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                       NULL, /* GDBusAuthObserver */
                                                       NULL, /* cancellable */
                                                       &error);
  g_assert_no_error (error);
  g_assert (connection != NULL);

  animal2 = example_animal_proxy_new_sync (connection, 0, NULL,
                                           "/Example/Animals/000", NULL, &error);
  g_assert_no_error (error);
  g_assert (animal2 != NULL);
  g_object_unref (connection);

  /* Make animal sad via animal1  */
  example_animal_call_poke_sync (animal1, TRUE, FALSE, NULL, &error);
  g_assert_no_error (error);

  /* Poke server and make sure animal is updated */
  value = g_dbus_proxy_call_sync (G_DBUS_PROXY (animal1),
                                  "org.freedesktop.DBus.Peer.Ping",
                                  NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                                  NULL, &error);
  g_assert_no_error (error);
  g_assert (value != NULL);
  g_variant_unref (value);

  /* Give the proxies a chance to refresh in the defaul main loop */
  g_timeout_add (100, codegen_quit_mainloop_timeout, NULL);
  g_main_loop_run (loop);

  /* Assert animals are sad */
  g_assert_cmpstr (example_animal_get_mood (animal1), ==, "Sad");
  g_assert_cmpstr (example_animal_get_mood (animal2), ==, "Sad");

  /* Make animal happy via animal2  */
  example_animal_call_poke_sync (animal2, FALSE, TRUE, NULL, &error);
  g_assert_no_error (error);

  /* Some random unrelated call, just to get some test coverage */
  value = g_dbus_proxy_call_sync (G_DBUS_PROXY (animal2),
                                  "org.freedesktop.DBus.Peer.GetMachineId",
                                  NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                                  NULL, &error);
  g_assert_no_error (error);
  g_variant_get (value, "(&s)", &s);
  g_test_message ("Machine ID: %s", s);
  /* It's valid for machine-id inside containers to be empty, so we
   * need to test for that possibility
   */
  g_assert ((s == NULL || *s == '\0') || g_dbus_is_guid (s));
  g_variant_unref (value);
  
  /* Poke server and make sure animal is updated */
  value = g_dbus_proxy_call_sync (G_DBUS_PROXY (animal2),
                                  "org.freedesktop.DBus.Peer.Ping",
                                  NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                                  NULL, &error);
  g_assert_no_error (error);
  g_assert (value != NULL);
  g_variant_unref (value);

  /* Give the proxies a chance to refresh in the defaul main loop */
  g_timeout_add (1000, codegen_quit_mainloop_timeout, NULL);
  g_main_loop_run (loop);

  /* Assert animals are happy */
  g_assert_cmpstr (example_animal_get_mood (animal1), ==, "Happy");
  g_assert_cmpstr (example_animal_get_mood (animal2), ==, "Happy");

  /* This final call making the animal happy and sad will cause
   * the server to quit, when the server quits we dont get property
   * change notifications anyway because those are done from an idle handler
   */
  example_animal_call_poke_sync (animal2, TRUE, TRUE, NULL, &error);

  g_object_unref (animal1);
  g_object_unref (animal2);
  g_thread_join (service_thread);
}

/* ---------------------------------------------------------------------------------------------------- */


int
main (int   argc,
      char *argv[])
{
  gint ret;
  GDBusNodeInfo *introspection_data = NULL;
  gchar *tmpdir = NULL;

  g_test_init (&argc, &argv, NULL);

  introspection_data = g_dbus_node_info_new_for_xml (test_interface_introspection_xml, NULL);
  g_assert (introspection_data != NULL);
  test_interface_introspection_data = introspection_data->interfaces[0];

  test_guid = g_dbus_generate_guid ();

  if (is_unix)
    {
      if (g_unix_socket_address_abstract_names_supported ())
	tmp_address = g_strdup ("unix:tmpdir=/tmp/gdbus-test-");
      else
	{
	  tmpdir = g_dir_make_tmp ("gdbus-test-XXXXXX", NULL);
	  tmp_address = g_strdup_printf ("unix:tmpdir=%s", tmpdir);
	}
    }
  else
    tmp_address = g_strdup ("nonce-tcp:");

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_add_func ("/gdbus/peer-to-peer", test_peer);
  g_test_add_func ("/gdbus/delayed-message-processing", delayed_message_processing);
  g_test_add_func ("/gdbus/nonce-tcp", test_nonce_tcp);

  g_test_add_func ("/gdbus/tcp-anonymous", test_tcp_anonymous);
  g_test_add_func ("/gdbus/credentials", test_credentials);
  g_test_add_func ("/gdbus/codegen-peer-to-peer", codegen_test_peer);

  ret = g_test_run();

  g_main_loop_unref (loop);
  g_free (test_guid);
  g_dbus_node_info_unref (introspection_data);
  if (is_unix)
    g_free (tmp_address);
  if (tmpdir)
    {
      g_rmdir (tmpdir);
      g_free (tmpdir);
    }

  return ret;
}
