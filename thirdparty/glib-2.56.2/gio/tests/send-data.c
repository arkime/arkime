#include <gio/gio.h>
#include <string.h>
#include <stdio.h>

GMainLoop *loop;

int cancel_timeout = 0;
int io_timeout = 0;
gboolean async = FALSE;
gboolean graceful = FALSE;
gboolean verbose = FALSE;
static GOptionEntry cmd_entries[] = {
  {"cancel", 'c', 0, G_OPTION_ARG_INT, &cancel_timeout,
   "Cancel any op after the specified amount of seconds", NULL},
  {"async", 'a', 0, G_OPTION_ARG_NONE, &async,
   "Use async ops", NULL},
  {"graceful-disconnect", 'g', 0, G_OPTION_ARG_NONE, &graceful,
   "Use graceful disconnect", NULL},
  {"timeout", 't', 0, G_OPTION_ARG_INT, &io_timeout,
   "Time out socket I/O after the specified number of seconds", NULL},
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
   "Verbose debugging output", NULL},
  {NULL}
};

static gpointer
cancel_thread (gpointer data)
{
  GCancellable *cancellable = data;

  g_usleep (1000*1000*cancel_timeout);
  g_print ("Cancelling\n");
  g_cancellable_cancel (cancellable);
  return NULL;
}

static char *
socket_address_to_string (GSocketAddress *address)
{
  GInetAddress *inet_address;
  char *str, *res;
  int port;

  inet_address = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));
  str = g_inet_address_to_string (inet_address);
  port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (address));
  res = g_strdup_printf ("%s:%d", str, port);
  g_free (str);
  return res;
}

static void
async_cb (GObject *source_object,
	  GAsyncResult *res,
	  gpointer user_data)
{
  GAsyncResult **resp = user_data;
  *resp = g_object_ref (res);
  g_main_loop_quit (loop);
}

static void
socket_client_event (GSocketClient *client,
		     GSocketClientEvent event,
		     GSocketConnectable *connectable,
		     GSocketConnection *connection)
{
  static GEnumClass *event_class;
  GTimeVal tv;

  if (!event_class)
    event_class = g_type_class_ref (G_TYPE_SOCKET_CLIENT_EVENT);

  g_get_current_time (&tv);
  printf ("% 12ld.%06ld GSocketClient => %s [%s]\n",
	  tv.tv_sec, tv.tv_usec,
	  g_enum_get_value (event_class, event)->value_nick,
	  connection ? G_OBJECT_TYPE_NAME (connection) : "");
}

int
main (int argc, char *argv[])
{
  GOptionContext *context;
  GSocketClient *client;
  GSocketConnection *connection;
  GSocketAddress *address;
  GCancellable *cancellable;
  GOutputStream *out;
  GError *error = NULL;
  char buffer[1000];

  context = g_option_context_new (" <hostname>[:port] - send data to tcp host");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (argc != 2)
    {
      g_printerr ("%s: %s\n", argv[0], "Need to specify hostname");
      return 1;
    }

  if (async)
    loop = g_main_loop_new (NULL, FALSE);

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

  client = g_socket_client_new ();
  if (io_timeout)
    g_socket_client_set_timeout (client, io_timeout);
  if (verbose)
    g_signal_connect (client, "event", G_CALLBACK (socket_client_event), NULL);

  if (async)
    {
      GAsyncResult *res;
      g_socket_client_connect_to_host_async (client, argv[1], 7777,
					     cancellable, async_cb, &res);
      g_main_loop_run (loop);
      connection = g_socket_client_connect_to_host_finish (client, res, &error);
      g_object_unref (res);
    }
  else
    {
      connection = g_socket_client_connect_to_host (client,
						    argv[1],
						    7777,
						    cancellable, &error);
    }
  if (connection == NULL)
    {
      g_printerr ("%s can't connect: %s\n", argv[0], error->message);
      return 1;
    }
  g_object_unref (client);

  address = g_socket_connection_get_remote_address (connection, &error);
  if (!address)
    {
      g_printerr ("Error getting remote address: %s\n",
		  error->message);
      return 1;
    }
  g_print ("Connected to address: %s\n",
	   socket_address_to_string (address));
  g_object_unref (address);

  if (graceful)
    g_tcp_connection_set_graceful_disconnect (G_TCP_CONNECTION (connection), TRUE);

  out = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  while (fgets(buffer, sizeof (buffer), stdin) != NULL)
    {
      /* FIXME if (async) */
      if (!g_output_stream_write_all (out, buffer, strlen (buffer),
				      NULL, cancellable, &error))
	{
	  g_warning ("send error: %s\n",  error->message);
	  g_error_free (error);
	  error = NULL;
	}
    }

  g_print ("closing stream\n");
  if (async)
    {
      GAsyncResult *res;
      g_io_stream_close_async (G_IO_STREAM (connection),
			       0, cancellable, async_cb, &res);
      g_main_loop_run (loop);
      if (!g_io_stream_close_finish (G_IO_STREAM (connection),
				     res, &error))
	{
	  g_object_unref (res);
	  g_warning ("close error: %s\n",  error->message);
	  return 1;
	}
      g_object_unref (res);
    }
  else
    {
      if (!g_io_stream_close (G_IO_STREAM (connection), cancellable, &error))
	{
	  g_warning ("close error: %s\n",  error->message);
	  return 1;
	}
    }

  g_object_unref (connection);

  return 0;
}
