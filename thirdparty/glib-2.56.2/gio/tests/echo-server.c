#include <gio/gio.h>
#include <string.h>

#define MESSAGE "Welcome to the echo service!\n"

int port = 7777;
static GOptionEntry cmd_entries[] = {
  {"port", 'p', 0, G_OPTION_ARG_INT, &port,
   "Local port to bind to", NULL},
  {NULL}
};


static gboolean
handler (GThreadedSocketService *service,
         GSocketConnection      *connection,
         GSocketListener        *listener,
         gpointer                user_data)
{
  GOutputStream *out;
  GInputStream *in;
  char buffer[1024];
  gssize size;

  out = g_io_stream_get_output_stream (G_IO_STREAM (connection));
  in = g_io_stream_get_input_stream (G_IO_STREAM (connection));

  g_output_stream_write_all (out, MESSAGE, strlen (MESSAGE),
                             NULL, NULL, NULL);

  while (0 < (size = g_input_stream_read (in, buffer,
                                          sizeof buffer, NULL, NULL)))
    g_output_stream_write (out, buffer, size, NULL, NULL);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GSocketService *service;
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new (" - Test GSocket server stuff");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  service = g_threaded_socket_service_new (10);

  if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (service),
					port,
					NULL,
					&error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  g_print ("Echo service listening on port %d\n", port);

  g_signal_connect (service, "run", G_CALLBACK (handler), NULL);

  g_main_loop_run (g_main_loop_new (NULL, FALSE));
  g_assert_not_reached ();
}
