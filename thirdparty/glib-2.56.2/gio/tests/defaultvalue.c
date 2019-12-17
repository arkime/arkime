/* GIO default value tests
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <gio/gio.h>

static void
check_property (const char *output,
	        GParamSpec *pspec,
		GValue *value)
{
  GValue default_value = G_VALUE_INIT;
  char *v, *dv, *msg;

  if (g_param_value_defaults (pspec, value))
      return;

  g_value_init (&default_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_param_value_set_default (pspec, &default_value);

  v = g_strdup_value_contents (value);
  dv = g_strdup_value_contents (&default_value);

  msg = g_strdup_printf ("%s %s.%s: %s != %s\n",
			 output,
			 g_type_name (pspec->owner_type),
			 pspec->name,
			 dv, v);
  g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, msg);
  g_free (msg);

  g_free (v);
  g_free (dv);
  g_value_unset (&default_value);
}

static void
test_type (gconstpointer data)
{
  GObjectClass *klass;
  GObject *instance;
  GParamSpec **pspecs;
  guint n_pspecs, i;
  GType type;

  type = * (GType *) data;

  if (g_type_is_a (type, G_TYPE_APP_INFO_MONITOR))
    {
      g_test_skip ("singleton");
      return;
    }

  if (g_type_is_a (type, G_TYPE_BINDING) ||
      g_type_is_a (type, G_TYPE_BUFFERED_INPUT_STREAM) ||
      g_type_is_a (type, G_TYPE_BUFFERED_OUTPUT_STREAM) ||
      g_type_is_a (type, G_TYPE_CHARSET_CONVERTER) ||
      g_type_is_a (type, G_TYPE_DBUS_ACTION_GROUP) ||
      g_type_is_a (type, G_TYPE_DBUS_CONNECTION) ||
      g_type_is_a (type, G_TYPE_DBUS_OBJECT_MANAGER_CLIENT) ||
      g_type_is_a (type, G_TYPE_DBUS_OBJECT_MANAGER_SERVER) ||
      g_type_is_a (type, G_TYPE_DBUS_PROXY) ||
      g_type_is_a (type, G_TYPE_DBUS_SERVER) ||
      g_type_is_a (type, G_TYPE_FILTER_OUTPUT_STREAM) ||
      g_type_is_a (type, G_TYPE_FILTER_INPUT_STREAM) ||
      g_type_is_a (type, G_TYPE_INET_ADDRESS) ||
      g_type_is_a (type, G_TYPE_INET_SOCKET_ADDRESS) ||
      g_type_is_a (type, G_TYPE_PROPERTY_ACTION) ||
      g_type_is_a (type, G_TYPE_SETTINGS) ||
      g_type_is_a (type, G_TYPE_SOCKET_CONNECTION) ||
      g_type_is_a (type, G_TYPE_SIMPLE_IO_STREAM) ||
      g_type_is_a (type, G_TYPE_THEMED_ICON) ||
      FALSE)
    {
      g_test_skip ("mandatory construct params");
      return;
    }

  if (g_type_is_a (type, G_TYPE_DBUS_MENU_MODEL) ||
      g_type_is_a (type, G_TYPE_DBUS_METHOD_INVOCATION))
    {
      g_test_skip ("crash in finalize");
      return;
    }

  if (g_type_is_a (type, G_TYPE_FILE_ENUMERATOR) ||
      g_type_is_a (type, G_TYPE_FILE_IO_STREAM))
    {
      g_test_skip ("should be abstract");
      return;
    }

  klass = g_type_class_ref (type);
  instance = g_object_new (type, NULL);

  if (G_IS_INITABLE (instance) &&
      !g_initable_init (G_INITABLE (instance), NULL, NULL))
    {
      g_test_skip ("initialization failed");
      g_object_unref (instance);
      g_type_class_unref (klass);
      return;
    }

  if (g_type_is_a (type, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (instance);

  pspecs = g_object_class_list_properties (klass, &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      GValue value = G_VALUE_INIT;

      if (pspec->owner_type != type)
	continue;

      if ((pspec->flags & G_PARAM_READABLE) == 0)
	continue;

      if (g_type_is_a (type, G_TYPE_APPLICATION) &&
          (strcmp (pspec->name, "is-remote") == 0))
        {
          g_test_message ("skipping GApplication:is-remote");
          continue;
        }

      if (g_type_is_a (type, G_TYPE_PROXY_ADDRESS_ENUMERATOR) &&
          (strcmp (pspec->name, "proxy-resolver") == 0))
        {
          g_test_message ("skipping GProxyAddressEnumerator:proxy-resolver");
          continue;
        }

      if (g_type_is_a (type, G_TYPE_SOCKET_CLIENT) &&
          (strcmp (pspec->name, "proxy-resolver") == 0))
        {
          g_test_message ("skipping GSocketClient:proxy-resolver");
          continue;
        }

      if (g_test_verbose ())
        g_printerr ("Property %s.%s\n", g_type_name (pspec->owner_type), pspec->name);
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (instance, pspec->name, &value);
      check_property ("Property", pspec, &value);
      g_value_unset (&value);
    }
  g_free (pspecs);
  g_object_unref (instance);
  g_type_class_unref (klass);
}

static GType *all_registered_types;

static const GType *
list_all_types (void)
{
  if (!all_registered_types)
    {
      GType *tp;
      all_registered_types = g_new0 (GType, 1000);
      tp = all_registered_types;
#include "giotypefuncs.inc"
      *tp = 0;
    }

  return all_registered_types;
}

int
main (int argc, char **argv)
{
  const GType *otypes;
  guint i;
  GTestDBus *bus;
  gint result;

  g_setenv ("GIO_USE_VFS", "local", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  g_test_init (&argc, &argv, NULL);

  /* Create one test bus for all tests, as we have a lot of very small
   * and quick tests.
   */
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);

  otypes = list_all_types ();
  for (i = 0; otypes[i]; i++)
    {
      gchar *testname;

      if (!G_TYPE_IS_CLASSED (otypes[i]))
        continue;

      if (G_TYPE_IS_ABSTRACT (otypes[i]))
        continue;

      if (!g_type_is_a (otypes[i], G_TYPE_OBJECT))
        continue;

      testname = g_strdup_printf ("/Default Values/%s",
				  g_type_name (otypes[i]));
      g_test_add_data_func (testname, &otypes[i], test_type);
      g_free (testname);
    }

  result = g_test_run ();

  g_test_dbus_down (bus);
  g_object_unref (bus);

  return result;
}
