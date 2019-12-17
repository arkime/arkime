#include <gio/gio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------------------------------- */

/* The object we want to export */
typedef struct _MyObjectClass MyObjectClass;
typedef struct _MyObject MyObject;

struct _MyObjectClass
{
  GObjectClass parent_class;
};

struct _MyObject
{
  GObject parent_instance;

  gint count;
  gchar *name;
};

enum
{
  PROP_0,
  PROP_COUNT,
  PROP_NAME
};

static GType my_object_get_type (void);
G_DEFINE_TYPE (MyObject, my_object, G_TYPE_OBJECT)

static void
my_object_finalize (GObject *object)
{
  MyObject *myobj = (MyObject*)object;

  g_free (myobj->name);

  G_OBJECT_CLASS (my_object_parent_class)->finalize (object);
}

static void
my_object_init (MyObject *object)
{
  object->count = 0;
  object->name = NULL;
}

static void
my_object_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MyObject *myobj = (MyObject*)object;

  switch (prop_id)
    {
    case PROP_COUNT:
      g_value_set_int (value, myobj->count);
      break;

    case PROP_NAME:
      g_value_set_string (value, myobj->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
my_object_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  MyObject *myobj = (MyObject*)object;

  switch (prop_id)
    {
    case PROP_COUNT:
      myobj->count = g_value_get_int (value);
      break;

    case PROP_NAME:
      g_free (myobj->name);
      myobj->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
my_object_class_init (MyObjectClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = my_object_finalize;
  gobject_class->set_property = my_object_set_property;
  gobject_class->get_property = my_object_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_COUNT,
                                   g_param_spec_int ("count",
                                                     "Count",
                                                     "Count",
                                                     0, 99999, 0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}

/* A method that we want to export */
static void
my_object_change_count (MyObject *myobj,
                        gint      change)
{
  myobj->count = 2 * myobj->count + change;

  g_object_notify (G_OBJECT (myobj), "count");
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.myorg.MyObject'>"
  "    <method name='ChangeCount'>"
  "      <arg type='i' name='change' direction='in'/>"
  "    </method>"
  "    <property type='i' name='Count' access='read'/>"
  "    <property type='s' name='Name' access='readwrite'/>"
  "  </interface>"
  "</node>";


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
  MyObject *myobj = user_data;

  if (g_strcmp0 (method_name, "ChangeCount") == 0)
    {
      gint change;
      g_variant_get (parameters, "(i)", &change);

      my_object_change_count (myobj, change);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
  GVariant *ret;
  MyObject *myobj = user_data;

  ret = NULL;
  if (g_strcmp0 (property_name, "Count") == 0)
    {
      ret = g_variant_new_int32 (myobj->count);
    }
  else if (g_strcmp0 (property_name, "Name") == 0)
    {
      ret = g_variant_new_string (myobj->name ? myobj->name : "");
    }

  return ret;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
  MyObject *myobj = user_data;

  if (g_strcmp0 (property_name, "Count") == 0)
    {
      g_object_set (myobj, "count", g_variant_get_int32 (value), NULL);
    }
  else if (g_strcmp0 (property_name, "Name") == 0)
    {
      g_object_set (myobj, "name", g_variant_get_string (value, NULL), NULL);
    }

  return TRUE;
}


/* for now */
static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  handle_get_property,
  handle_set_property
};

static void
send_property_change (GObject         *obj,
                      GParamSpec      *pspec,
                      GDBusConnection *connection)
{
  GVariantBuilder *builder;
  GVariantBuilder *invalidated_builder;
  MyObject *myobj = (MyObject *)obj;

  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  invalidated_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));

  if (g_strcmp0 (pspec->name, "count") == 0)
    g_variant_builder_add (builder,
                           "{sv}",
                           "Count", g_variant_new_int32 (myobj->count));
  else if (g_strcmp0 (pspec->name, "name") == 0)
    g_variant_builder_add (builder,
                           "{sv}",
                           "Name", g_variant_new_string (myobj->name ? myobj->name : ""));

  g_dbus_connection_emit_signal (connection,
                                 NULL,
                                 "/org/myorg/MyObject",
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv}as)",
                                                "org.myorg.MyObject",
                                                builder,
                                                invalidated_builder),
                                 NULL);
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  MyObject *myobj = user_data;
  guint registration_id;

  g_signal_connect (myobj, "notify",
                    G_CALLBACK (send_property_change), connection);
  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/myorg/MyObject",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       myobj,
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  exit (1);
}

int
main (int argc, char *argv[])
{
  guint owner_id;
  GMainLoop *loop;
  MyObject *myobj;

  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  myobj = g_object_new (my_object_get_type (), NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.myorg.MyObject",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             myobj,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  g_dbus_node_info_unref (introspection_data);

  g_object_unref (myobj);

  return 0;
}
