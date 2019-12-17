#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gtlsconsoleinteraction.h"

GMainLoop *loop;

gboolean verbose = FALSE;
gboolean non_blocking = FALSE;
gboolean use_udp = FALSE;
int cancel_timeout = 0;
int read_timeout = 0;
gboolean unix_socket = FALSE;
gboolean tls = FALSE;

static GOptionEntry cmd_entries[] = {
  {"cancel", 'c', 0, G_OPTION_ARG_INT, &cancel_timeout,
   "Cancel any op after the specified amount of seconds", NULL},
  {"udp", 'u', 0, G_OPTION_ARG_NONE, &use_udp,
   "Use udp instead of tcp", NULL},
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
   "Be verbose", NULL},
  {"non-blocking", 'n', 0, G_OPTION_ARG_NONE, &non_blocking,
   "Enable non-blocking i/o", NULL},
#ifdef G_OS_UNIX
  {"unix", 'U', 0, G_OPTION_ARG_NONE, &unix_socket,
   "Use a unix socket instead of IP", NULL},
#endif
  {"timeout", 't', 0, G_OPTION_ARG_INT, &read_timeout,
   "Time out reads after the specified number of seconds", NULL},
  {"tls", 'T', 0, G_OPTION_ARG_NONE, &tls,
   "Use TLS (SSL)", NULL},
  {NULL}
};

#include "socket-common.c"

static gboolean
accept_certificate (GTlsClientConnection *conn,
		    GTlsCertificate      *cert,
		    GTlsCertificateFlags  errors,
		    gpointer              user_data)
{
  g_print ("Certificate would have been rejected ( ");
  if (errors & G_TLS_CERTIFICATE_UNKNOWN_CA)
    g_print ("unknown-ca ");
  if (errors & G_TLS_CERTIFICATE_BAD_IDENTITY)
    g_print ("bad-identity ");
  if (errors & G_TLS_CERTIFICATE_NOT_ACTIVATED)
    g_print ("not-activated ");
  if (errors & G_TLS_CERTIFICATE_EXPIRED)
    g_print ("expired ");
  if (errors & G_TLS_CERTIFICATE_REVOKED)
    g_print ("revoked ");
  if (errors & G_TLS_CERTIFICATE_INSECURE)
    g_print ("insecure ");
  g_print (") but accepting anyway.\n");

  return TRUE;
}

static GTlsCertificate *
lookup_client_certificate (GTlsClientConnection  *conn,
			   GError               **error)
{
  GList *l, *accepted;
  GList *c, *certificates;
  GTlsDatabase *database;
  GTlsCertificate *certificate = NULL;
  GTlsConnection *base;

  accepted = g_tls_client_connection_get_accepted_cas (conn);
  for (l = accepted; l != NULL; l = g_list_next (l))
    {
      base = G_TLS_CONNECTION (conn);
      database = g_tls_connection_get_database (base);
      certificates = g_tls_database_lookup_certificates_issued_by (database, l->data,
                                                                   g_tls_connection_get_interaction (base),
                                                                   G_TLS_DATABASE_LOOKUP_KEYPAIR,
                                                                   NULL, error);
      if (error && *error)
        break;

      if (certificates)
          certificate = g_object_ref (certificates->data);

      for (c = certificates; c != NULL; c = g_list_next (c))
        g_object_unref (c->data);
      g_list_free (certificates);
    }

  for (l = accepted; l != NULL; l = g_list_next (l))
    g_byte_array_unref (l->data);
  g_list_free (accepted);

  if (certificate == NULL && error && !*error)
    g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED,
                         "Server requested a certificate, but could not find relevant certificate in database.");
  return certificate;
}

static gboolean
make_connection (const char       *argument,
		 GTlsCertificate  *certificate,
		 GCancellable     *cancellable,
		 GSocket         **socket,
		 GSocketAddress  **address,
		 GIOStream       **connection,
		 GInputStream    **istream,
		 GOutputStream   **ostream,
		 GError          **error)
{
  GSocketType socket_type;
  GSocketFamily socket_family;
  GSocketAddressEnumerator *enumerator;
  GSocketConnectable *connectable;
  GSocketAddress *src_address;
  GTlsInteraction *interaction;
  GError *err = NULL;

  if (use_udp)
    socket_type = G_SOCKET_TYPE_DATAGRAM;
  else
    socket_type = G_SOCKET_TYPE_STREAM;

  if (unix_socket)
    socket_family = G_SOCKET_FAMILY_UNIX;
  else
    socket_family = G_SOCKET_FAMILY_IPV4;

  *socket = g_socket_new (socket_family, socket_type, 0, error);
  if (*socket == NULL)
    return FALSE;

  if (read_timeout)
    g_socket_set_timeout (*socket, read_timeout);

  if (unix_socket)
    {
      GSocketAddress *addr;

      addr = socket_address_from_string (argument);
      if (addr == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Could not parse '%s' as unix socket name", argument);
          return FALSE;
        }
      connectable = G_SOCKET_CONNECTABLE (addr);
    }
  else
    {
      connectable = g_network_address_parse (argument, 7777, error);
      if (connectable == NULL)
        return FALSE;
    }

  enumerator = g_socket_connectable_enumerate (connectable);
  while (TRUE)
    {
      *address = g_socket_address_enumerator_next (enumerator, cancellable, error);
      if (*address == NULL)
        {
          if (error != NULL && *error == NULL)
            g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                 "No more addresses to try");
          return FALSE;
        }

      if (g_socket_connect (*socket, *address, cancellable, &err))
        break;
      g_message ("Connection to %s failed: %s, trying next\n", socket_address_to_string (*address), err->message);
      g_clear_error (&err);

      g_object_unref (*address);
    }
  g_object_unref (enumerator);

  g_print ("Connected to %s\n",
           socket_address_to_string (*address));

  src_address = g_socket_get_local_address (*socket, error);
  if (!src_address)
    {
      g_prefix_error (error, "Error getting local address: ");
      return FALSE;
    }

  g_print ("local address: %s\n",
           socket_address_to_string (src_address));
  g_object_unref (src_address);

  if (use_udp)
    {
      *connection = NULL;
      *istream = NULL;
      *ostream = NULL;
    }
  else
    *connection = G_IO_STREAM (g_socket_connection_factory_create_connection (*socket));

  if (tls)
    {
      GIOStream *tls_conn;

      tls_conn = g_tls_client_connection_new (*connection, connectable, error);
      if (!tls_conn)
        {
          g_prefix_error (error, "Could not create TLS connection: ");
          return FALSE;
        }

      g_signal_connect (tls_conn, "accept-certificate",
                        G_CALLBACK (accept_certificate), NULL);

      interaction = g_tls_console_interaction_new ();
      g_tls_connection_set_interaction (G_TLS_CONNECTION (tls_conn), interaction);
      g_object_unref (interaction);

      if (certificate)
        g_tls_connection_set_certificate (G_TLS_CONNECTION (tls_conn), certificate);

      g_object_unref (*connection);
      *connection = G_IO_STREAM (tls_conn);

      if (!g_tls_connection_handshake (G_TLS_CONNECTION (tls_conn),
                                       cancellable, error))
        {
          g_prefix_error (error, "Error during TLS handshake: ");
          return FALSE;
        }
    }
  g_object_unref (connectable);

  if (*connection)
    {
      *istream = g_io_stream_get_input_stream (*connection);
      *ostream = g_io_stream_get_output_stream (*connection);
    }

  return TRUE;
}

int
main (int argc,
      char *argv[])
{
  GSocket *socket;
  GSocketAddress *address;
  GError *error = NULL;
  GOptionContext *context;
  GCancellable *cancellable;
  GIOStream *connection;
  GInputStream *istream;
  GOutputStream *ostream;
  GSocketAddress *src_address;
  GTlsCertificate *certificate = NULL;
  gint i;

  address = NULL;
  connection = NULL;

  context = g_option_context_new (" <hostname>[:port] - Test GSocket client stuff");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (argc != 2)
    {
      g_printerr ("%s: %s\n", argv[0], "Need to specify hostname / unix socket name");
      return 1;
    }

  if (use_udp && tls)
    {
      g_printerr ("DTLS (TLS over UDP) is not supported");
      return 1;
    }

  if (cancel_timeout)
    {
      GThread *thread;
      cancellable = g_cancellable_new ();
      thread = g_thread_new ("cancel", cancel_thread, cancellable);
      g_thread_unref (thread);
    }
  else
    {
      cancellable = NULL;
    }

  loop = g_main_loop_new (NULL, FALSE);

  for (i = 0; i < 2; i++)
    {
      if (make_connection (argv[1], certificate, cancellable, &socket, &address,
                           &connection, &istream, &ostream, &error))
          break;

      if (g_error_matches (error, G_TLS_ERROR, G_TLS_ERROR_CERTIFICATE_REQUIRED))
        {
          g_clear_error (&error);
          certificate = lookup_client_certificate (G_TLS_CLIENT_CONNECTION (connection), &error);
          if (certificate != NULL)
            continue;
        }

      g_printerr ("%s: %s", argv[0], error->message);
      return 1;
    }

  /* TODO: Test non-blocking connect/handshake */
  if (non_blocking)
    g_socket_set_blocking (socket, FALSE);

  while (TRUE)
    {
      gchar buffer[4096];
      gssize size;
      gsize to_send;

      if (fgets (buffer, sizeof buffer, stdin) == NULL)
	break;

      to_send = strlen (buffer);
      while (to_send > 0)
	{
	  if (use_udp)
	    {
	      ensure_socket_condition (socket, G_IO_OUT, cancellable);
	      size = g_socket_send_to (socket, address,
				       buffer, to_send,
				       cancellable, &error);
	    }
	  else
	    {
	      ensure_connection_condition (connection, G_IO_OUT, cancellable);
	      size = g_output_stream_write (ostream,
					    buffer, to_send,
					    cancellable, &error);
	    }

	  if (size < 0)
	    {
	      if (g_error_matches (error,
				   G_IO_ERROR,
				   G_IO_ERROR_WOULD_BLOCK))
		{
		  g_print ("socket send would block, handling\n");
		  g_error_free (error);
		  error = NULL;
		  continue;
		}
	      else
		{
		  g_printerr ("Error sending to socket: %s\n",
			      error->message);
		  return 1;
		}
	    }

	  g_print ("sent %" G_GSSIZE_FORMAT " bytes of data\n", size);

	  if (size == 0)
	    {
	      g_printerr ("Unexpected short write\n");
	      return 1;
	    }

	  to_send -= size;
	}

      if (use_udp)
	{
	  ensure_socket_condition (socket, G_IO_IN, cancellable);
	  size = g_socket_receive_from (socket, &src_address,
					buffer, sizeof buffer,
					cancellable, &error);
	}
      else
	{
	  ensure_connection_condition (connection, G_IO_IN, cancellable);
	  size = g_input_stream_read (istream,
				      buffer, sizeof buffer,
				      cancellable, &error);
	}

      if (size < 0)
	{
	  g_printerr ("Error receiving from socket: %s\n",
		      error->message);
	  return 1;
	}

      if (size == 0)
	break;

      g_print ("received %" G_GSSIZE_FORMAT " bytes of data", size);
      if (use_udp)
	g_print (" from %s", socket_address_to_string (src_address));
      g_print ("\n");

      if (verbose)
	g_print ("-------------------------\n"
		 "%.*s"
		 "-------------------------\n",
		 (int)size, buffer);

    }

  g_print ("closing socket\n");

  if (connection)
    {
      if (!g_io_stream_close (connection, cancellable, &error))
	{
	  g_printerr ("Error closing connection: %s\n",
		      error->message);
	  return 1;
	}
      g_object_unref (connection);
    }
  else
    {
      if (!g_socket_close (socket, &error))
	{
	  g_printerr ("Error closing master socket: %s\n",
		      error->message);
	  return 1;
	}
    }

  g_object_unref (socket);
  g_object_unref (address);

  return 0;
}
