#include <gio/gio.h>

static gchar *opt_name         = NULL;
static gchar *opt_object_path  = NULL;
static gchar *opt_interface    = NULL;
static gboolean opt_system_bus = FALSE;
static gboolean opt_no_auto_start = FALSE;
static gboolean opt_no_properties = FALSE;

static GOptionEntry opt_entries[] =
{
  { "name", 'n', 0, G_OPTION_ARG_STRING, &opt_name, "Name of the remote object to watch", NULL },
  { "object-path", 'o', 0, G_OPTION_ARG_STRING, &opt_object_path, "Object path of the remote object", NULL },
  { "interface", 'i', 0, G_OPTION_ARG_STRING, &opt_interface, "D-Bus interface of remote object", NULL },
  { "system-bus", 's', 0, G_OPTION_ARG_NONE, &opt_system_bus, "Use the system-bus instead of the session-bus", NULL },
  { "no-auto-start", 'a', 0, G_OPTION_ARG_NONE, &opt_no_auto_start, "Don't instruct the bus to launch an owner for the name", NULL},
  { "no-properties", 'p', 0, G_OPTION_ARG_NONE, &opt_no_properties, "Do not load properties", NULL},
  { NULL}
};

static GMainLoop *loop = NULL;

static void
print_properties (GDBusProxy *proxy)
{
  gchar **property_names;
  guint n;

  g_print ("    properties:\n");

  property_names = g_dbus_proxy_get_cached_property_names (proxy);
  for (n = 0; property_names != NULL && property_names[n] != NULL; n++)
    {
      const gchar *key = property_names[n];
      GVariant *value;
      gchar *value_str;
      value = g_dbus_proxy_get_cached_property (proxy, key);
      value_str = g_variant_print (value, TRUE);
      g_print ("      %s -> %s\n", key, value_str);
      g_variant_unref (value);
      g_free (value_str);
    }
  g_strfreev (property_names);
}

static void
on_properties_changed (GDBusProxy          *proxy,
                       GVariant            *changed_properties,
                       const gchar* const  *invalidated_properties,
                       gpointer             user_data)
{
  /* Note that we are guaranteed that changed_properties and
   * invalidated_properties are never NULL
   */

  if (g_variant_n_children (changed_properties) > 0)
    {
      GVariantIter *iter;
      const gchar *key;
      GVariant *value;

      g_print (" *** Properties Changed:\n");
      g_variant_get (changed_properties,
                     "a{sv}",
                     &iter);
      while (g_variant_iter_loop (iter, "{&sv}", &key, &value))
        {
          gchar *value_str;
          value_str = g_variant_print (value, TRUE);
          g_print ("      %s -> %s\n", key, value_str);
          g_free (value_str);
        }
      g_variant_iter_free (iter);
    }

  if (g_strv_length ((GStrv) invalidated_properties) > 0)
    {
      guint n;
      g_print (" *** Properties Invalidated:\n");
      for (n = 0; invalidated_properties[n] != NULL; n++)
        {
          const gchar *key = invalidated_properties[n];
          g_print ("      %s\n", key);
        }
    }
}

static void
on_signal (GDBusProxy *proxy,
           gchar      *sender_name,
           gchar      *signal_name,
           GVariant   *parameters,
           gpointer    user_data)
{
  gchar *parameters_str;

  parameters_str = g_variant_print (parameters, TRUE);
  g_print (" *** Received Signal: %s: %s\n",
           signal_name,
           parameters_str);
  g_free (parameters_str);
}

static void
print_proxy (GDBusProxy *proxy)
{
  gchar *name_owner;

  name_owner = g_dbus_proxy_get_name_owner (proxy);
  if (name_owner != NULL)
    {
      g_print ("+++ Proxy object points to remote object owned by %s\n"
               "    bus:          %s\n"
               "    name:         %s\n"
               "    object path:  %s\n"
               "    interface:    %s\n",
               name_owner,
               opt_system_bus ? "System Bus" : "Session Bus",
               opt_name,
               opt_object_path,
               opt_interface);
      print_properties (proxy);
    }
  else
    {
      g_print ("--- Proxy object is inert - there is no name owner for the name\n"
               "    bus:          %s\n"
               "    name:         %s\n"
               "    object path:  %s\n"
               "    interface:    %s\n",
               opt_system_bus ? "System Bus" : "Session Bus",
               opt_name,
               opt_object_path,
               opt_interface);
    }
  g_free (name_owner);
}

static void
on_name_owner_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  print_proxy (proxy);
}

int
main (int argc, char *argv[])
{
  GOptionContext *opt_context;
  GError *error;
  GDBusProxyFlags flags;
  GDBusProxy *proxy;

  loop = NULL;
  proxy = NULL;

  opt_context = g_option_context_new ("g_bus_watch_proxy() example");
  g_option_context_set_summary (opt_context,
                                "Example: to watch the object of gdbus-example-server, use:\n"
                                "\n"
                                "  ./gdbus-example-watch-proxy -n org.gtk.GDBus.TestServer  \\\n"
                                "                              -o /org/gtk/GDBus/TestObject \\\n"
                                "                              -i org.gtk.GDBus.TestInterface");
  g_option_context_add_main_entries (opt_context, opt_entries, NULL);
  error = NULL;
  if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
      g_printerr ("Error parsing options: %s\n", error->message);
      goto out;
    }
  if (opt_name == NULL || opt_object_path == NULL || opt_interface == NULL)
    {
      g_printerr ("Incorrect usage, try --help.\n");
      goto out;
    }

  flags = G_DBUS_PROXY_FLAGS_NONE;
  if (opt_no_properties)
    flags |= G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES;
  if (opt_no_auto_start)
    flags |= G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;

  loop = g_main_loop_new (NULL, FALSE);

  error = NULL;
  proxy = g_dbus_proxy_new_for_bus_sync (opt_system_bus ? G_BUS_TYPE_SYSTEM : G_BUS_TYPE_SESSION,
                                         flags,
                                         NULL, /* GDBusInterfaceInfo */
                                         opt_name,
                                         opt_object_path,
                                         opt_interface,
                                         NULL, /* GCancellable */
                                         &error);
  if (proxy == NULL)
    {
      g_printerr ("Error creating proxy: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  g_signal_connect (proxy,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    NULL);
  g_signal_connect (proxy,
                    "g-signal",
                    G_CALLBACK (on_signal),
                    NULL);
  g_signal_connect (proxy,
                    "notify::g-name-owner",
                    G_CALLBACK (on_name_owner_notify),
                    NULL);
  print_proxy (proxy);

  g_main_loop_run (loop);

 out:
  if (proxy != NULL)
    g_object_unref (proxy);
  if (loop != NULL)
    g_main_loop_unref (loop);
  g_option_context_free (opt_context);
  g_free (opt_name);
  g_free (opt_object_path);
  g_free (opt_interface);

  return 0;
}
