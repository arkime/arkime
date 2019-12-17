#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;
static GDBusInterfaceInfo *manager_interface_info = NULL;
static GDBusInterfaceInfo *block_interface_info = NULL;
static GDBusInterfaceInfo *partition_interface_info = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gtk.GDBus.Example.Manager'>"
  "    <method name='Hello'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "  </interface>"
  "  <interface name='org.gtk.GDBus.Example.Block'>"
  "    <method name='Hello'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <property type='i' name='Major' access='read'/>"
  "    <property type='i' name='Minor' access='read'/>"
  "    <property type='s' name='Notes' access='readwrite'/>"
  "  </interface>"
  "  <interface name='org.gtk.GDBus.Example.Partition'>"
  "    <method name='Hello'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <property type='i' name='PartitionNumber' access='read'/>"
  "    <property type='s' name='Notes' access='readwrite'/>"
  "  </interface>"
  "</node>";

/* ---------------------------------------------------------------------------------------------------- */

static void
manager_method_call (GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
  const gchar *greeting;
  gchar *response;

  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.Example.Manager");
  g_assert_cmpstr (method_name, ==, "Hello");

  g_variant_get (parameters, "(&s)", &greeting);

  response = g_strdup_printf ("Method %s.%s with user_data '%s' on object path %s called with arg '%s'",
                              interface_name,
                              method_name,
                              (const gchar *) user_data,
                              object_path,
                              greeting);
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", response));
  g_free (response);
}

const GDBusInterfaceVTable manager_vtable =
{
  manager_method_call,
  NULL,                 /* get_property */
  NULL                  /* set_property */
};

/* ---------------------------------------------------------------------------------------------------- */

static void
block_method_call (GDBusConnection       *connection,
                   const gchar           *sender,
                   const gchar           *object_path,
                   const gchar           *interface_name,
                   const gchar           *method_name,
                   GVariant              *parameters,
                   GDBusMethodInvocation *invocation,
                   gpointer               user_data)
{
  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.Example.Block");

  if (g_strcmp0 (method_name, "Hello") == 0)
    {
      const gchar *greeting;
      gchar *response;

      g_variant_get (parameters, "(&s)", &greeting);

      response = g_strdup_printf ("Method %s.%s with user_data '%s' on object path %s called with arg '%s'",
                                  interface_name,
                                  method_name,
                                  (const gchar *) user_data,
                                  object_path,
                                  greeting);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", response));
      g_free (response);
    }
  else if (g_strcmp0 (method_name, "DoStuff") == 0)
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.gtk.GDBus.TestSubtree.Error.Failed",
                                                  "This method intentionally always fails");
    }
  else
    {
      g_assert_not_reached ();
    }
}

static GVariant *
block_get_property (GDBusConnection  *connection,
                    const gchar      *sender,
                    const gchar      *object_path,
                    const gchar      *interface_name,
                    const gchar      *property_name,
                    GError          **error,
                    gpointer          user_data)
{
  GVariant *ret;
  const gchar *node;
  gint major;
  gint minor;

  node = strrchr (object_path, '/') + 1;
  if (g_str_has_prefix (node, "sda"))
    major = 8;
  else
    major = 9;
  if (strlen (node) == 4)
    minor = node[3] - '0';
  else
    minor = 0;

  ret = NULL;
  if (g_strcmp0 (property_name, "Major") == 0)
    {
      ret = g_variant_new_int32 (major);
    }
  else if (g_strcmp0 (property_name, "Minor") == 0)
    {
      ret = g_variant_new_int32 (minor);
    }
  else if (g_strcmp0 (property_name, "Notes") == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Hello %s. I thought I said reading this property "
                   "always results in an error. kthxbye",
                   sender);
    }
  else
    {
      g_assert_not_reached ();
    }

  return ret;
}

static gboolean
block_set_property (GDBusConnection  *connection,
                    const gchar      *sender,
                    const gchar      *object_path,
                    const gchar      *interface_name,
                    const gchar      *property_name,
                    GVariant         *value,
                    GError          **error,
                    gpointer          user_data)
{
  /* TODO */
  g_assert_not_reached ();
}

const GDBusInterfaceVTable block_vtable =
{
  block_method_call,
  block_get_property,
  block_set_property,
};

/* ---------------------------------------------------------------------------------------------------- */

static void
partition_method_call (GDBusConnection       *connection,
                       const gchar           *sender,
                       const gchar           *object_path,
                       const gchar           *interface_name,
                       const gchar           *method_name,
                       GVariant              *parameters,
                       GDBusMethodInvocation *invocation,
                       gpointer               user_data)
{
  const gchar *greeting;
  gchar *response;

  g_assert_cmpstr (interface_name, ==, "org.gtk.GDBus.Example.Partition");
  g_assert_cmpstr (method_name, ==, "Hello");

  g_variant_get (parameters, "(&s)", &greeting);

  response = g_strdup_printf ("Method %s.%s with user_data '%s' on object path %s called with arg '%s'",
                              interface_name,
                              method_name,
                              (const gchar *) user_data,
                              object_path,
                              greeting);
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", response));
  g_free (response);
}

const GDBusInterfaceVTable partition_vtable =
{
  partition_method_call,
  //partition_get_property,
  //partition_set_property
};

/* ---------------------------------------------------------------------------------------------------- */

static gchar **
subtree_enumerate (GDBusConnection       *connection,
                   const gchar           *sender,
                   const gchar           *object_path,
                   gpointer               user_data)
{
  gchar **nodes;
  GPtrArray *p;

  p = g_ptr_array_new ();
  g_ptr_array_add (p, g_strdup ("sda"));
  g_ptr_array_add (p, g_strdup ("sda1"));
  g_ptr_array_add (p, g_strdup ("sda2"));
  g_ptr_array_add (p, g_strdup ("sda3"));
  g_ptr_array_add (p, g_strdup ("sdb"));
  g_ptr_array_add (p, g_strdup ("sdb1"));
  g_ptr_array_add (p, g_strdup ("sdc"));
  g_ptr_array_add (p, g_strdup ("sdc1"));
  g_ptr_array_add (p, NULL);
  nodes = (gchar **) g_ptr_array_free (p, FALSE);

  return nodes;
}

static GDBusInterfaceInfo **
subtree_introspect (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *node,
                    gpointer               user_data)
{
  GPtrArray *p;

  p = g_ptr_array_new ();
  if (node == NULL)
    {
      g_ptr_array_add (p, g_dbus_interface_info_ref (manager_interface_info));
    }
  else
    {
      g_ptr_array_add (p, g_dbus_interface_info_ref (block_interface_info));
      if (strlen (node) == 4)
        g_ptr_array_add (p,
                         g_dbus_interface_info_ref (partition_interface_info));
    }

  g_ptr_array_add (p, NULL);

  return (GDBusInterfaceInfo **) g_ptr_array_free (p, FALSE);
}

static const GDBusInterfaceVTable *
subtree_dispatch (GDBusConnection             *connection,
                  const gchar                 *sender,
                  const gchar                 *object_path,
                  const gchar                 *interface_name,
                  const gchar                 *node,
                  gpointer                    *out_user_data,
                  gpointer                     user_data)
{
  const GDBusInterfaceVTable *vtable_to_return;
  gpointer user_data_to_return;

  if (g_strcmp0 (interface_name, "org.gtk.GDBus.Example.Manager") == 0)
    {
      user_data_to_return = "The Root";
      vtable_to_return = &manager_vtable;
    }
  else
    {
      if (strlen (node) == 4)
        user_data_to_return = "A partition";
      else
        user_data_to_return = "A block device";

      if (g_strcmp0 (interface_name, "org.gtk.GDBus.Example.Block") == 0)
        vtable_to_return = &block_vtable;
      else if (g_strcmp0 (interface_name, "org.gtk.GDBus.Example.Partition") == 0)
        vtable_to_return = &partition_vtable;
      else
        g_assert_not_reached ();
    }

  *out_user_data = user_data_to_return;

  return vtable_to_return;
}

const GDBusSubtreeVTable subtree_vtable =
{
  subtree_enumerate,
  subtree_introspect,
  subtree_dispatch
};

/* ---------------------------------------------------------------------------------------------------- */

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  guint registration_id;

  registration_id = g_dbus_connection_register_subtree (connection,
                                                        "/org/gtk/GDBus/TestSubtree/Devices",
                                                        &subtree_vtable,
                                                        G_DBUS_SUBTREE_FLAGS_NONE,
                                                        NULL,  /* user_data */
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

  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  manager_interface_info = g_dbus_node_info_lookup_interface (introspection_data, "org.gtk.GDBus.Example.Manager");
  block_interface_info = g_dbus_node_info_lookup_interface (introspection_data, "org.gtk.GDBus.Example.Block");
  partition_interface_info = g_dbus_node_info_lookup_interface (introspection_data, "org.gtk.GDBus.Example.Partition");
  g_assert (manager_interface_info != NULL);
  g_assert (block_interface_info != NULL);
  g_assert (partition_interface_info != NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.TestSubtree",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  g_dbus_node_info_unref (introspection_data);

  return 0;
}
