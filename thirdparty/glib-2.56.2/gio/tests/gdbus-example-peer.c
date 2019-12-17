/*

Usage examples (modulo addresses / credentials).

UNIX domain socket transport:

 Server:
   $ ./gdbus-example-peer --server --address unix:abstract=myaddr
   Server is listening at: unix:abstract=myaddr
   Client connected.
   Peer credentials: GCredentials:unix-user=500,unix-group=500,unix-process=13378
   Negotiated capabilities: unix-fd-passing=1
   Client said: Hey, it's 1273093080 already!

 Client:
   $ ./gdbus-example-peer --address unix:abstract=myaddr
   Connected.
   Negotiated capabilities: unix-fd-passing=1
   Server said: You said 'Hey, it's 1273093080 already!'. KTHXBYE!

Nonce-secured TCP transport on the same host:

 Server:
   $ ./gdbus-example-peer --server --address nonce-tcp:
   Server is listening at: nonce-tcp:host=localhost,port=43077,noncefile=/tmp/gdbus-nonce-file-X1ZNCV
   Client connected.
   Peer credentials: (no credentials received)
   Negotiated capabilities: unix-fd-passing=0
   Client said: Hey, it's 1273093206 already!

 Client:
   $ ./gdbus-example-peer -address nonce-tcp:host=localhost,port=43077,noncefile=/tmp/gdbus-nonce-file-X1ZNCV
   Connected.
   Negotiated capabilities: unix-fd-passing=0
   Server said: You said 'Hey, it's 1273093206 already!'. KTHXBYE!

TCP transport on two different hosts with a shared home directory:

 Server:
   host1 $ ./gdbus-example-peer --server --address tcp:host=0.0.0.0
   Server is listening at: tcp:host=0.0.0.0,port=46314
   Client connected.
   Peer credentials: (no credentials received)
   Negotiated capabilities: unix-fd-passing=0
   Client said: Hey, it's 1273093337 already!

 Client:
   host2 $ ./gdbus-example-peer -a tcp:host=host1,port=46314
   Connected.
   Negotiated capabilities: unix-fd-passing=0
   Server said: You said 'Hey, it's 1273093337 already!'. KTHXBYE!

TCP transport on two different hosts without authentication:

 Server:
   host1 $ ./gdbus-example-peer --server --address tcp:host=0.0.0.0 --allow-anonymous
   Server is listening at: tcp:host=0.0.0.0,port=59556
   Client connected.
   Peer credentials: (no credentials received)
   Negotiated capabilities: unix-fd-passing=0
   Client said: Hey, it's 1273093652 already!

 Client:
   host2 $ ./gdbus-example-peer -a tcp:host=host1,port=59556
   Connected.
   Negotiated capabilities: unix-fd-passing=0
   Server said: You said 'Hey, it's 1273093652 already!'. KTHXBYE!

 */

#include <gio/gio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gtk.GDBus.TestPeerInterface'>"
  "    <method name='HelloWorld'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

/* ---------------------------------------------------------------------------------------------------- */

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  if (g_strcmp0 (method_name, "HelloWorld") == 0)
    {
      const gchar *greeting;
      gchar *response;

      g_variant_get (parameters, "(&s)", &greeting);
      response = g_strdup_printf ("You said '%s'. KTHXBYE!", greeting);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", response));
      g_free (response);
      g_print ("Client said: %s\n", greeting);
    }
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL,
};

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
on_new_connection (GDBusServer *server,
                   GDBusConnection *connection,
                   gpointer user_data)
{
  guint registration_id;
  GCredentials *credentials;
  gchar *s;

  credentials = g_dbus_connection_get_peer_credentials (connection);
  if (credentials == NULL)
    s = g_strdup ("(no credentials received)");
  else
    s = g_credentials_to_string (credentials);


  g_print ("Client connected.\n"
           "Peer credentials: %s\n"
           "Negotiated capabilities: unix-fd-passing=%d\n",
           s,
           g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING);

  g_object_ref (connection);
  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/gtk/GDBus/TestObject",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       NULL,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);

  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int argc, char *argv[])
{
  gint ret;
  gboolean opt_server;
  gchar *opt_address;
  GOptionContext *opt_context;
  gboolean opt_allow_anonymous;
  GError *error;
  GOptionEntry opt_entries[] =
    {
      { "server", 's', 0, G_OPTION_ARG_NONE, &opt_server, "Start a server instead of a client", NULL },
      { "address", 'a', 0, G_OPTION_ARG_STRING, &opt_address, "D-Bus address to use", NULL },
      { "allow-anonymous", 'n', 0, G_OPTION_ARG_NONE, &opt_allow_anonymous, "Allow anonymous authentication", NULL },
      { NULL}
    };

  ret = 1;

  opt_address = NULL;
  opt_server = FALSE;
  opt_allow_anonymous = FALSE;

  opt_context = g_option_context_new ("peer-to-peer example");
  error = NULL;
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s\n", error->message);
      g_error_free (error);
      goto out;
    }
  if (opt_address == NULL)
    {
      g_printerr ("Incorrect usage, try --help.\n");
      goto out;
    }
  if (!opt_server && opt_allow_anonymous)
    {
      g_printerr ("The --allow-anonymous option only makes sense when used with --server.\n");
      goto out;
    }

  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  if (opt_server)
    {
      GDBusServer *server;
      gchar *guid;
      GMainLoop *loop;
      GDBusServerFlags server_flags;

      guid = g_dbus_generate_guid ();

      server_flags = G_DBUS_SERVER_FLAGS_NONE;
      if (opt_allow_anonymous)
        server_flags |= G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS;

      error = NULL;
      server = g_dbus_server_new_sync (opt_address,
                                       server_flags,
                                       guid,
                                       NULL, /* GDBusAuthObserver */
                                       NULL, /* GCancellable */
                                       &error);
      g_dbus_server_start (server);
      g_free (guid);

      if (server == NULL)
        {
          g_printerr ("Error creating server at address %s: %s\n", opt_address, error->message);
          g_error_free (error);
          goto out;
        }
      g_print ("Server is listening at: %s\n", g_dbus_server_get_client_address (server));
      g_signal_connect (server,
                        "new-connection",
                        G_CALLBACK (on_new_connection),
                        NULL);

      loop = g_main_loop_new (NULL, FALSE);
      g_main_loop_run (loop);

      g_object_unref (server);
      g_main_loop_unref (loop);
    }
  else
    {
      GDBusConnection *connection;
      const gchar *greeting_response;
      GVariant *value;
      gchar *greeting;

      error = NULL;
      connection = g_dbus_connection_new_for_address_sync (opt_address,
                                                           G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                           NULL, /* GDBusAuthObserver */
                                                           NULL, /* GCancellable */
                                                           &error);
      if (connection == NULL)
        {
          g_printerr ("Error connecting to D-Bus address %s: %s\n", opt_address, error->message);
          g_error_free (error);
          goto out;
        }

      g_print ("Connected.\n"
               "Negotiated capabilities: unix-fd-passing=%d\n",
               g_dbus_connection_get_capabilities (connection) & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING);

      greeting = g_strdup_printf ("Hey, it's %" G_GUINT64_FORMAT " already!", (guint64) time (NULL));
      value = g_dbus_connection_call_sync (connection,
                                           NULL, /* bus_name */
                                           "/org/gtk/GDBus/TestObject",
                                           "org.gtk.GDBus.TestPeerInterface",
                                           "HelloWorld",
                                           g_variant_new ("(s)", greeting),
                                           G_VARIANT_TYPE ("(s)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           &error);
      if (value == NULL)
        {
          g_printerr ("Error invoking HelloWorld(): %s\n", error->message);
          g_error_free (error);
          goto out;
        }
      g_variant_get (value, "(&s)", &greeting_response);
      g_print ("Server said: %s\n", greeting_response);
      g_variant_unref (value);

      g_object_unref (connection);
    }
  g_dbus_node_info_unref (introspection_data);

  ret = 0;

 out:
  return ret;
}
