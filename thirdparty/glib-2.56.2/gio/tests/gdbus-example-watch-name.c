#include <gio/gio.h>

static gchar *opt_name         = NULL;
static gboolean opt_system_bus = FALSE;
static gboolean opt_auto_start = FALSE;

static GOptionEntry opt_entries[] =
{
  { "name", 'n', 0, G_OPTION_ARG_STRING, &opt_name, "Name to watch", NULL },
  { "system-bus", 's', 0, G_OPTION_ARG_NONE, &opt_system_bus, "Use the system-bus instead of the session-bus", NULL },
  { "auto-start", 'a', 0, G_OPTION_ARG_NONE, &opt_auto_start, "Instruct the bus to launch an owner for the name", NULL},
  { NULL}
};

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  g_print ("Name %s on %s is owned by %s\n",
           name,
           opt_system_bus ? "the system bus" : "the session bus",
           name_owner);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_print ("Name %s does not exist on %s\n",
           name,
           opt_system_bus ? "the system bus" : "the session bus");
}

int
main (int argc, char *argv[])
{
  guint watcher_id;
  GMainLoop *loop;
  GOptionContext *opt_context;
  GError *error;
  GBusNameWatcherFlags flags;

  error = NULL;
  opt_context = g_option_context_new ("g_bus_watch_name() example");
  g_option_context_set_summary (opt_context,
                                "Example: to watch the power manager on the session bus, use:\n"
                                "\n"
                                "  ./example-watch-name -n org.gnome.PowerManager");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s", error->message);
      goto out;
    }
  if (opt_name == NULL)
    {
      g_printerr ("Incorrect usage, try --help.\n");
      goto out;
    }

  flags = G_BUS_NAME_WATCHER_FLAGS_NONE;
  if (opt_auto_start)
    flags |= G_BUS_NAME_WATCHER_FLAGS_AUTO_START;

  watcher_id = g_bus_watch_name (opt_system_bus ? G_BUS_TYPE_SYSTEM : G_BUS_TYPE_SESSION,
                                 opt_name,
                                 flags,
                                 on_name_appeared,
                                 on_name_vanished,
                                 NULL,
                                 NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unwatch_name (watcher_id);

 out:
  g_option_context_free (opt_context);
  g_free (opt_name);

  return 0;
}
