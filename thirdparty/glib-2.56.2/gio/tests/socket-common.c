/* #included into both socket-client.c and socket-server.c */

#ifdef G_OS_UNIX
static const char *unix_socket_address_types[] = {
  "invalid",
  "anonymous",
  "path",
  "abstract",
  "padded"
};
#endif

static char *
socket_address_to_string (GSocketAddress *address)
{
  char *res = NULL;

  if (G_IS_INET_SOCKET_ADDRESS (address))
    {
      GInetAddress *inet_address;
      char *str;
      int port;

      inet_address = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));
      str = g_inet_address_to_string (inet_address);
      port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (address));
      res = g_strdup_printf ("%s:%d", str, port);
      g_free (str);
    }
#ifdef G_OS_UNIX
  else if (G_IS_UNIX_SOCKET_ADDRESS (address))
    {
      GUnixSocketAddress *uaddr = G_UNIX_SOCKET_ADDRESS (address);

      res = g_strdup_printf ("%s:%s",
			     unix_socket_address_types[g_unix_socket_address_get_address_type (uaddr)],
			     g_unix_socket_address_get_path (uaddr));
    }
#endif

  return res;
}

static GSocketAddress *
socket_address_from_string (const char *name)
{
#ifdef G_OS_UNIX
  int i, len;

  for (i = 0; i < G_N_ELEMENTS (unix_socket_address_types); i++)
    {
      len = strlen (unix_socket_address_types[i]);
      if (!strncmp (name, unix_socket_address_types[i], len) &&
	  name[len] == ':')
	{
	  return g_unix_socket_address_new_with_type (name + len + 1, -1,
						      (GUnixSocketAddressType)i);
	}
    }
#endif
  return NULL;
}

static gboolean
source_ready (GPollableInputStream *stream,
	      gpointer              data)
{
  g_main_loop_quit (loop);
  return FALSE;
}

static void
ensure_socket_condition (GSocket      *socket,
			 GIOCondition  condition,
			 GCancellable *cancellable)
{
  GSource *source;

  if (!non_blocking)
    return;

  source = g_socket_create_source (socket, condition, cancellable);
  g_source_set_callback (source,
			 (GSourceFunc) source_ready,
			 NULL, NULL);
  g_source_attach (source, NULL);
  g_source_unref (source);
  g_main_loop_run (loop);
}

static void
ensure_connection_condition (GIOStream    *stream,
			     GIOCondition  condition,
			     GCancellable *cancellable)
{
  GSource *source;

  if (!non_blocking)
    return;

  if (condition & G_IO_IN)
    source = g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (g_io_stream_get_input_stream (stream)), cancellable);
  else
    source = g_pollable_output_stream_create_source (G_POLLABLE_OUTPUT_STREAM (g_io_stream_get_output_stream (stream)), cancellable);

  g_source_set_callback (source,
			 (GSourceFunc) source_ready,
			 NULL, NULL);
  g_source_attach (source, NULL);
  g_source_unref (source);
  g_main_loop_run (loop);
}

static gpointer
cancel_thread (gpointer data)
{
  GCancellable *cancellable = data;

  g_usleep (1000*1000*cancel_timeout);
  g_print ("Cancelling\n");
  g_cancellable_cancel (cancellable);
  return NULL;
}
