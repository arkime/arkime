#include <gio/gio.h>

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  /* This is where we'd export some objects on the bus */
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_print ("Acquired the name %s on the session bus\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_print ("Lost the name %s on the session bus\n", name);
}

int
main (int argc, char *argv[])
{
  guint owner_id;
  GMainLoop *loop;
  GBusNameOwnerFlags flags;
  gboolean opt_replace;
  gboolean opt_allow_replacement;
  gchar *opt_name;
  GOptionContext *opt_context;
  GError *error;
  GOptionEntry opt_entries[] =
    {
      { "replace", 'r', 0, G_OPTION_ARG_NONE, &opt_replace, "Replace existing name if possible", NULL },
      { "allow-replacement", 'a', 0, G_OPTION_ARG_NONE, &opt_allow_replacement, "Allow replacement", NULL },
      { "name", 'n', 0, G_OPTION_ARG_STRING, &opt_name, "Name to acquire", NULL },
      { NULL}
    };

  error = NULL;
  opt_name = NULL;
  opt_replace = FALSE;
  opt_allow_replacement = FALSE;
  opt_context = g_option_context_new ("g_bus_own_name() example");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s", error->message);
      return 1;
    }
  if (opt_name == NULL)
    {
      g_printerr ("Incorrect usage, try --help.\n");
      return 1;
    }

  flags = G_BUS_NAME_OWNER_FLAGS_NONE;
  if (opt_replace)
    flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;
  if (opt_allow_replacement)
    flags |= G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             opt_name,
                             flags,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  return 0;
}
