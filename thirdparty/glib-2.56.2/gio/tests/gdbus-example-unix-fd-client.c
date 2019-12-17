#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>

#include <time.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

/* see gdbus-example-server.c for the server implementation */
static gint
get_server_stdout (GDBusConnection  *connection,
                   const gchar      *name_owner,
                   GError          **error)
{
  GDBusMessage *method_call_message;
  GDBusMessage *method_reply_message;
  GUnixFDList *fd_list;
  gint fd;

  fd = -1;
  method_call_message = NULL;
  method_reply_message = NULL;

  method_call_message = g_dbus_message_new_method_call (name_owner,
                                                        "/org/gtk/GDBus/TestObject",
                                                        "org.gtk.GDBus.TestInterface",
                                                        "GimmeStdout");
  method_reply_message = g_dbus_connection_send_message_with_reply_sync (connection,
                                                                         method_call_message,
                                                                         G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                                                         -1,
                                                                         NULL, /* out_serial */
                                                                         NULL, /* cancellable */
                                                                         error);
  if (method_reply_message == NULL)
      goto out;

  if (g_dbus_message_get_message_type (method_reply_message) == G_DBUS_MESSAGE_TYPE_ERROR)
    {
      g_dbus_message_to_gerror (method_reply_message, error);
      goto out;
    }

  fd_list = g_dbus_message_get_unix_fd_list (method_reply_message);
  fd = g_unix_fd_list_get (fd_list, 0, error);

 out:
  g_object_unref (method_call_message);
  g_object_unref (method_reply_message);

  return fd;
}

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  gint fd;
  GError *error;

  error = NULL;
  fd = get_server_stdout (connection, name_owner, &error);
  if (fd == -1)
    {
      g_printerr ("Error invoking GimmeStdout(): %s\n",
                  error->message);
      g_error_free (error);
      exit (1);
    }
  else
    {
      gchar now_buf[256];
      time_t now;
      gssize len;
      gchar *str;

      now = time (NULL);
      strftime (now_buf,
                sizeof now_buf,
                "%Y-%m-%d %H:%M:%S",
                localtime (&now));

      str = g_strdup_printf ("On %s, gdbus-example-unix-fd-client with pid %d was here!\n",
                             now_buf,
                             (gint) getpid ());
      len = strlen (str);
      g_warn_if_fail (write (fd, str, len) == len);
      close (fd);

      g_print ("Wrote the following on server's stdout:\n%s", str);

      g_free (str);
      exit (0);
    }
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_printerr ("Failed to get name owner for %s\n"
              "Is ./gdbus-example-server running?\n",
              name);
  exit (1);
}

int
main (int argc, char *argv[])
{
  guint watcher_id;
  GMainLoop *loop;

  watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                 "org.gtk.GDBus.TestServer",
                                 G_BUS_NAME_WATCHER_FLAGS_NONE,
                                 on_name_appeared,
                                 on_name_vanished,
                                 NULL,
                                 NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unwatch_name (watcher_id);
  return 0;
}
