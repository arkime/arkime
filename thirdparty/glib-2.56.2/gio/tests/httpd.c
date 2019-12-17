#include <gio/gio.h>
#include <string.h>

static int port = 8080;
static char *root = NULL;
static GOptionEntry cmd_entries[] = {
  {"port", 'p', 0, G_OPTION_ARG_INT, &port,
   "Local port to bind to", NULL},
  {NULL}
};

static void
send_error (GOutputStream *out,
	    int error_code,
	    const char *reason)
{
  char *res;

  res = g_strdup_printf ("HTTP/1.0 %d %s\r\n\r\n"
			 "<html><head><title>%d %s</title></head>"
			 "<body>%s</body></html>",
			 error_code, reason,
			 error_code, reason,
			 reason);
  g_output_stream_write_all (out, res, strlen (res), NULL, NULL, NULL);
  g_free (res);
}

static gboolean
handler (GThreadedSocketService *service,
	 GSocketConnection      *connection,
	 GSocketListener        *listener,
	 gpointer                user_data)
{
  GOutputStream *out;
  GInputStream *in;
  GFileInputStream *file_in;
  GDataInputStream *data;
  char *line, *escaped, *tmp, *query, *unescaped, *path, *version;
  GFile *f;
  GError *error;
  GFileInfo *info;
  GString *s;

  in = g_io_stream_get_input_stream (G_IO_STREAM (connection));
  out = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  data = g_data_input_stream_new (in);
  /* Be tolerant of input */
  g_data_input_stream_set_newline_type (data, G_DATA_STREAM_NEWLINE_TYPE_ANY);

  line = g_data_input_stream_read_line (data, NULL, NULL, NULL);

  if (line == NULL)
    {
      send_error (out, 400, "Invalid request");
      goto out;
    }

  if (!g_str_has_prefix (line, "GET "))
    {
      send_error (out, 501, "Only GET implemented");
      goto out;
    }

  escaped = line + 4; /* Skip "GET " */

  version = NULL;
  tmp = strchr (escaped, ' ');
  if (tmp == NULL)
    {
      send_error (out, 400, "Bad Request");
      goto out;
    }
  *tmp = 0;

  version = tmp + 1;
  if (!g_str_has_prefix (version, "HTTP/1."))
    {
      send_error(out, 505, "HTTP Version Not Supported");
      goto out;
    }

  query = strchr (escaped, '?');
  if (query != NULL)
    *query++ = 0;

  unescaped = g_uri_unescape_string (escaped, NULL);
  path = g_build_filename (root, unescaped, NULL);
  g_free (unescaped);
  f = g_file_new_for_path (path);
  g_free (path);

  error = NULL;
  file_in = g_file_read (f, NULL, &error);
  if (file_in == NULL)
    {
      send_error (out, 404, error->message);
      g_error_free (error);
      goto out;
    }

  s = g_string_new ("HTTP/1.0 200 OK\r\n");
  info = g_file_input_stream_query_info (file_in,
					 G_FILE_ATTRIBUTE_STANDARD_SIZE ","
					 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
					 NULL, NULL);
  if (info)
    {
      const char *content_type;
      char *mime_type;

      if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
	g_string_append_printf (s, "Content-Length: %"G_GINT64_FORMAT"\r\n",
				g_file_info_get_size (info));
      content_type = g_file_info_get_content_type (info);
      if (content_type)
	{
	  mime_type = g_content_type_get_mime_type (content_type);
	  if (mime_type)
	    {
	      g_string_append_printf (s, "Content-Type: %s\r\n",
				      mime_type);
	      g_free (mime_type);
	    }
	}
    }
  g_string_append (s, "\r\n");

  if (g_output_stream_write_all (out,
				 s->str, s->len,
				 NULL, NULL, NULL))
    {
      g_output_stream_splice (out,
			      G_INPUT_STREAM (file_in),
			      0, NULL, NULL);
    }
  g_string_free (s, TRUE);

  g_input_stream_close (G_INPUT_STREAM (file_in), NULL, NULL);
  g_object_unref (file_in);

 out:
  g_object_unref (data);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GSocketService *service;
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new ("<http root dir> - Simple HTTP server");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (argc != 2)
    {
      g_printerr ("Root directory not specified\n");
      return 1;
    }

  root = g_strdup (argv[1]);

  service = g_threaded_socket_service_new (10);
  if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (service),
					port,
					NULL,
					&error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  g_print ("Http server listening on port %d\n", port);

  g_signal_connect (service, "run", G_CALLBACK (handler), NULL);

  g_main_loop_run (g_main_loop_new (NULL, FALSE));
  g_assert_not_reached ();
}
