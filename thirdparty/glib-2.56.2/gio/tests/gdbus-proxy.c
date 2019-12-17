/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

#include "gdbus-tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that the method aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_methods (GDBusProxy *proxy)
{
  GVariant *result;
  GError *error;
  const gchar *str;
  gchar *dbus_error_name;

  /* check that we can invoke a method */
  error = NULL;
  result = g_dbus_proxy_call_sync (proxy,
                                   "HelloWorld",
                                   g_variant_new ("(s)", "Hey"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "(s)");
  g_variant_get (result, "(&s)", &str);
  g_assert_cmpstr (str, ==, "You greeted me with 'Hey'. Thanks!");
  g_variant_unref (result);

  /* Check that we can completely recover the returned error */
  result = g_dbus_proxy_call_sync (proxy,
                                   "HelloWorld",
                                   g_variant_new ("(s)", "Yo"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR);
  g_assert (g_dbus_error_is_remote_error (error));
  g_assert (g_dbus_error_is_remote_error (error));
  g_assert (result == NULL);
  dbus_error_name = g_dbus_error_get_remote_error (error);
  g_assert_cmpstr (dbus_error_name, ==, "com.example.TestException");
  g_free (dbus_error_name);
  g_assert (g_dbus_error_strip_remote_error (error));
  g_assert_cmpstr (error->message, ==, "Yo is not a proper greeting");
  g_clear_error (&error);

  /* Check that we get a timeout if the method handling is taking longer than
   * timeout. We use such a long sleep because on slow machines, if the
   * sleep isn't much longer than the timeout and we're doing a parallel
   * build, there's no guarantee we'll be scheduled in the window between
   * the timeout being hit and the sleep finishing. */
  error = NULL;
  result = g_dbus_proxy_call_sync (proxy,
                                   "Sleep",
                                   g_variant_new ("(i)", 10000 /* msec */),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   100 /* msec */,
                                   NULL,
                                   &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_assert (result == NULL);
  g_clear_error (&error);

  /* Check that proxy-default timeouts work. */
  g_assert_cmpint (g_dbus_proxy_get_default_timeout (proxy), ==, -1);

  /* the default timeout is 25000 msec so this should work */
  result = g_dbus_proxy_call_sync (proxy,
                                   "Sleep",
                                   g_variant_new ("(i)", 500 /* msec */),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* use proxy default (e.g. -1 -> e.g. 25000 msec) */
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);

  /* Now set the proxy-default timeout to 250 msec and try the 10000 msec
   * call - this should FAIL. Again, we use such a long sleep because on slow
   * machines there's no guarantee we'll be scheduled when we want to be. */
  g_dbus_proxy_set_default_timeout (proxy, 250);
  g_assert_cmpint (g_dbus_proxy_get_default_timeout (proxy), ==, 250);
  result = g_dbus_proxy_call_sync (proxy,
                                   "Sleep",
                                   g_variant_new ("(i)", 10000 /* msec */),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1, /* use proxy default (e.g. 250 msec) */
                                   NULL,
                                   &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_assert (result == NULL);
  g_clear_error (&error);

  /* clean up after ourselves */
  g_dbus_proxy_set_default_timeout (proxy, -1);
}

static gboolean
strv_equal (gchar **strv, ...)
{
  gint count;
  va_list list;
  const gchar *str;
  gboolean res;

  res = TRUE;
  count = 0;
  va_start (list, strv);
  while (1)
    {
      str = va_arg (list, const gchar *);
      if (str == NULL)
        break;
      if (g_strcmp0 (str, strv[count]) != 0)
        {
          res = FALSE;
          break;
        }
      count++;
    }
  va_end (list);

  if (res)
    res = g_strv_length (strv) == count;

  return res;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that the property aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_properties (GDBusProxy *proxy)
{
  GError *error;
  GVariant *variant;
  GVariant *variant2;
  GVariant *result;
  gchar **names;
  gchar *name_owner;
  GDBusProxy *proxy2;

  error = NULL;

  if (g_dbus_proxy_get_flags (proxy) & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES)
    {
       g_assert (g_dbus_proxy_get_cached_property_names (proxy) == NULL);
       return;
    }

  /*
   * Check that we can list all cached properties.
   */
  names = g_dbus_proxy_get_cached_property_names (proxy);

  g_assert (strv_equal (names,
                        "PropertyThatWillBeInvalidated",
                        "ab",
                        "ad",
                        "ai",
                        "an",
                        "ao",
                        "aq",
                        "as",
                        "at",
                        "au",
                        "ax",
                        "ay",
                        "b",
                        "d",
                        "foo",
                        "i",
                        "n",
                        "o",
                        "q",
                        "s",
                        "t",
                        "u",
                        "x",
                        "y",
                        NULL));

  g_strfreev (names);

  /*
   * Check that we can read cached properties.
   *
   * No need to test all properties - GVariant has already been tested
   */
  variant = g_dbus_proxy_get_cached_property (proxy, "y");
  g_assert (variant != NULL);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 1);
  g_variant_unref (variant);
  variant = g_dbus_proxy_get_cached_property (proxy, "o");
  g_assert (variant != NULL);
  g_assert_cmpstr (g_variant_get_string (variant, NULL), ==, "/some/path");
  g_variant_unref (variant);

  /*
   * Now ask the service to change a property and check that #GDBusProxy::g-property-changed
   * is received. Also check that the cache is updated.
   */
  variant2 = g_variant_new_byte (42);
  result = g_dbus_proxy_call_sync (proxy,
                                   "FrobSetProperty",
                                   g_variant_new ("(sv)",
                                                  "y",
                                                  variant2),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);
  _g_assert_signal_received (proxy, "g-properties-changed");
  variant = g_dbus_proxy_get_cached_property (proxy, "y");
  g_assert (variant != NULL);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 42);
  g_variant_unref (variant);

  g_dbus_proxy_set_cached_property (proxy, "y", g_variant_new_byte (142));
  variant = g_dbus_proxy_get_cached_property (proxy, "y");
  g_assert (variant != NULL);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 142);
  g_variant_unref (variant);

  g_dbus_proxy_set_cached_property (proxy, "y", NULL);
  variant = g_dbus_proxy_get_cached_property (proxy, "y");
  g_assert (variant == NULL);

  /* Check that the invalidation feature of the PropertiesChanged()
   * signal works... First, check that we have a cached value of the
   * property (from the initial GetAll() call)
   */
  variant = g_dbus_proxy_get_cached_property (proxy, "PropertyThatWillBeInvalidated");
  g_assert (variant != NULL);
  g_assert_cmpstr (g_variant_get_string (variant, NULL), ==, "InitialValue");
  g_variant_unref (variant);
  /* now ask to invalidate the property - this causes a
   *
   *   PropertiesChanaged("com.example.Frob",
   *                      {},
   *                      ["PropertyThatWillBeInvalidated")
   *
   * signal to be emitted. This is received before the method reply
   * for FrobInvalidateProperty *but* since the proxy was created in
   * the same thread as we're doing this synchronous call, we'll get
   * the method reply before...
   */
  result = g_dbus_proxy_call_sync (proxy,
                                   "FrobInvalidateProperty",
                                   g_variant_new ("(s)", "OMGInvalidated"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);
  /* ... hence we wait for the g-properties-changed signal to be delivered */
  _g_assert_signal_received (proxy, "g-properties-changed");
  /* ... and now we finally, check that the cached value has been invalidated */
  variant = g_dbus_proxy_get_cached_property (proxy, "PropertyThatWillBeInvalidated");
  g_assert (variant == NULL);

  /* Now test that G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES works - we need a new proxy for that */
  error = NULL;
  proxy2 = g_dbus_proxy_new_sync (g_dbus_proxy_get_connection (proxy),
                                  G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                  NULL,                      /* GDBusInterfaceInfo */
                                  "com.example.TestService", /* name */
                                  "/com/example/TestObject", /* object path */
                                  "com.example.Frob",        /* interface */
                                  NULL, /* GCancellable */
                                  &error);
  g_assert_no_error (error);

  name_owner = g_dbus_proxy_get_name_owner (proxy2);
  g_assert (name_owner != NULL);
  g_free (name_owner);

  variant = g_dbus_proxy_get_cached_property (proxy2, "PropertyThatWillBeInvalidated");
  g_assert (variant != NULL);
  g_assert_cmpstr (g_variant_get_string (variant, NULL), ==, "OMGInvalidated"); /* from previous test */
  g_variant_unref (variant);

  result = g_dbus_proxy_call_sync (proxy2,
                                   "FrobInvalidateProperty",
                                   g_variant_new ("(s)", "OMGInvalidated2"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);

  /* this time we should get the ::g-properties-changed _with_ the value */
  _g_assert_signal_received (proxy2, "g-properties-changed");

  variant = g_dbus_proxy_get_cached_property (proxy2, "PropertyThatWillBeInvalidated");
  g_assert (variant != NULL);
  g_assert_cmpstr (g_variant_get_string (variant, NULL), ==, "OMGInvalidated2");
  g_variant_unref (variant);

  g_object_unref (proxy2);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that the signal aspects of GDBusProxy works */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_proxy_signals_on_signal (GDBusProxy  *proxy,
                              const gchar *sender_name,
                              const gchar *signal_name,
                              GVariant    *parameters,
                              gpointer     user_data)
{
  GString *s = user_data;

  g_assert_cmpstr (signal_name, ==, "TestSignal");
  g_assert_cmpstr (g_variant_get_type_string (parameters), ==, "(sov)");

  g_variant_print_string (parameters, s, TRUE);
}

typedef struct
{
  GMainLoop *internal_loop;
  GString *s;
} TestSignalData;

static void
test_proxy_signals_on_emit_signal_cb (GDBusProxy   *proxy,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  TestSignalData *data = user_data;
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_proxy_call_finish (proxy,
                                     res,
                                     &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);

  /* check that the signal was recieved before we got the method result */
  g_assert (strlen (data->s->str) > 0);

  /* break out of the loop */
  g_main_loop_quit (data->internal_loop);
}

static void
test_signals (GDBusProxy *proxy)
{
  GError *error;
  GString *s;
  gulong signal_handler_id;
  TestSignalData data;
  GVariant *result;

  error = NULL;

  /*
   * Ask the service to emit a signal and check that we receive it.
   *
   * Note that blocking calls don't block in the mainloop so wait for the signal (which
   * is dispatched before the method reply)
   */
  s = g_string_new (NULL);
  signal_handler_id = g_signal_connect (proxy,
                                        "g-signal",
                                        G_CALLBACK (test_proxy_signals_on_signal),
                                        s);

  result = g_dbus_proxy_call_sync (proxy,
                                   "EmitSignal",
                                   g_variant_new ("(so)",
                                                  "Accept the next proposition you hear",
                                                  "/some/path"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);
  /* check that we haven't received the signal just yet */
  g_assert (strlen (s->str) == 0);
  /* and now wait for the signal */
  _g_assert_signal_received (proxy, "g-signal");
  g_assert_cmpstr (s->str,
                   ==,
                   "('Accept the next proposition you hear .. in bed!', objectpath '/some/path/in/bed', <'a variant'>)");
  g_signal_handler_disconnect (proxy, signal_handler_id);
  g_string_free (s, TRUE);

  /*
   * Now do this async to check the signal is received before the method returns.
   */
  s = g_string_new (NULL);
  data.internal_loop = g_main_loop_new (NULL, FALSE);
  data.s = s;
  signal_handler_id = g_signal_connect (proxy,
                                        "g-signal",
                                        G_CALLBACK (test_proxy_signals_on_signal),
                                        s);
  g_dbus_proxy_call (proxy,
                     "EmitSignal",
                     g_variant_new ("(so)",
                                    "You will make a great programmer",
                                    "/some/other/path"),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) test_proxy_signals_on_emit_signal_cb,
                     &data);
  g_main_loop_run (data.internal_loop);
  g_main_loop_unref (data.internal_loop);
  g_assert_cmpstr (s->str,
                   ==,
                   "('You will make a great programmer .. in bed!', objectpath '/some/other/path/in/bed', <'a variant'>)");
  g_signal_handler_disconnect (proxy, signal_handler_id);
  g_string_free (s, TRUE);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_bogus_method_return (GDBusProxy *proxy)
{
  GError *error = NULL;
  GVariant *result;

  result = g_dbus_proxy_call_sync (proxy,
                                   "PairReturn",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_error_free (error);
  g_assert (result == NULL);
}

#if 0 /* Disabled: see https://bugzilla.gnome.org/show_bug.cgi?id=658999 */
static void
test_bogus_signal (GDBusProxy *proxy)
{
  GError *error = NULL;
  GVariant *result;
  GDBusInterfaceInfo *old_iface_info;

  result = g_dbus_proxy_call_sync (proxy,
                                   "EmitSignal2",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      /* and now wait for the signal that will never arrive */
      _g_assert_signal_received (proxy, "g-signal");
    }
  g_test_trap_assert_stderr ("*Dropping signal TestSignal2 of type (i) since the type from the expected interface is (u)*");
  g_test_trap_assert_failed();

  /* Our main branch will also do g_warning() when running the mainloop so
   * temporarily remove the expected interface
   */
  old_iface_info = g_dbus_proxy_get_interface_info (proxy);
  g_dbus_proxy_set_interface_info (proxy, NULL);
  _g_assert_signal_received (proxy, "g-signal");
  g_dbus_proxy_set_interface_info (proxy, old_iface_info);
}

static void
test_bogus_property (GDBusProxy *proxy)
{
  GError *error = NULL;
  GVariant *result;
  GDBusInterfaceInfo *old_iface_info;

  /* Make the service emit a PropertiesChanged signal for property 'i' of type 'i' - since
   * our introspection data has this as type 'u' we should get a warning on stderr.
   */
  result = g_dbus_proxy_call_sync (proxy,
                                   "FrobSetProperty",
                                   g_variant_new ("(sv)",
                                                  "i", g_variant_new_int32 (42)),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      /* and now wait for the signal that will never arrive */
      _g_assert_signal_received (proxy, "g-properties-changed");
    }
  g_test_trap_assert_stderr ("*Received property i with type i does not match expected type u in the expected interface*");
  g_test_trap_assert_failed();

  /* Our main branch will also do g_warning() when running the mainloop so
   * temporarily remove the expected interface
   */
  old_iface_info = g_dbus_proxy_get_interface_info (proxy);
  g_dbus_proxy_set_interface_info (proxy, NULL);
  _g_assert_signal_received (proxy, "g-properties-changed");
  g_dbus_proxy_set_interface_info (proxy, old_iface_info);
}
#endif /* Disabled: see https://bugzilla.gnome.org/show_bug.cgi?id=658999 */

/* ---------------------------------------------------------------------------------------------------- */

static const gchar *frob_dbus_interface_xml =
  "<node>"
  "  <interface name='com.example.Frob'>"
  /* PairReturn() is deliberately different from gdbus-testserver's definition */
  "    <method name='PairReturn'>"
  "      <arg type='u' name='somenumber' direction='in'/>"
  "      <arg type='s' name='somestring' direction='out'/>"
  "    </method>"
  "    <method name='HelloWorld'>"
  "      <arg type='s' name='somestring' direction='in'/>"
  "      <arg type='s' name='somestring' direction='out'/>"
  "    </method>"
  "    <method name='Sleep'>"
  "      <arg type='i' name='timeout' direction='in'/>"
  "    </method>"
  /* We deliberately only mention a single property here */
  "    <property name='y' type='y' access='readwrite'/>"
  /* The 'i' property is deliberately different from gdbus-testserver's definition */
  "    <property name='i' type='u' access='readwrite'/>"
  /* ::TestSignal2 is deliberately different from gdbus-testserver's definition */
  "    <signal name='TestSignal2'>"
  "      <arg type='u' name='somenumber'/>"
  "    </signal>"
  "  </interface>"
  "</node>";
static GDBusInterfaceInfo *frob_dbus_interface_info;

static void
test_expected_interface (GDBusProxy *proxy)
{
  GVariant *value;
  GError *error;

  /* This is obviously wrong but expected interface is not set so we don't fail... */
  g_dbus_proxy_set_cached_property (proxy, "y", g_variant_new_string ("error_me_out!"));
  g_dbus_proxy_set_cached_property (proxy, "y", g_variant_new_byte (42));
  g_dbus_proxy_set_cached_property (proxy, "does-not-exist", g_variant_new_string ("something"));
  g_dbus_proxy_set_cached_property (proxy, "does-not-exist", NULL);

  /* Now repeat the method tests, with an expected interface set */
  g_dbus_proxy_set_interface_info (proxy, frob_dbus_interface_info);
  test_methods (proxy);
  test_signals (proxy);

  /* And also where we deliberately set the expected interface definition incorrectly */
  test_bogus_method_return (proxy);
  /* Disabled: see https://bugzilla.gnome.org/show_bug.cgi?id=658999
  test_bogus_signal (proxy);
  test_bogus_property (proxy);
  */

  if (g_test_undefined ())
    {
      /* Also check that we complain if setting a cached property of the wrong type */
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                             "*Trying to set property y of type s but according to the expected interface the type is y*");
      g_dbus_proxy_set_cached_property (proxy, "y", g_variant_new_string ("error_me_out!"));
      g_test_assert_expected_messages ();
    }

  /* this should work, however (since the type is correct) */
  g_dbus_proxy_set_cached_property (proxy, "y", g_variant_new_byte (42));

  if (g_test_undefined ())
    {
      /* Try to get the value of a property where the type we expect is different from
       * what we have in our cache (e.g. what the service returned)
       */
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                             "*Trying to get property i with type i but according to the expected interface the type is u*");
      value = g_dbus_proxy_get_cached_property (proxy, "i");
      g_test_assert_expected_messages ();
    }

  /* Even if a property does not exist in expected_interface, looking it
   * up, or setting it, should never fail. Because it could be that the
   * property has been added to the service but the GDBusInterfaceInfo*
   * passed to g_dbus_proxy_set_interface_info() just haven't been updated.
   *
   * See https://bugzilla.gnome.org/show_bug.cgi?id=660886
   */
  value = g_dbus_proxy_get_cached_property (proxy, "d");
  g_assert (value != NULL);
  g_assert (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE));
  g_assert_cmpfloat (g_variant_get_double (value), ==, 7.5);
  g_variant_unref (value);
  /* update it via the cached property... */
  g_dbus_proxy_set_cached_property (proxy, "d", g_variant_new_double (75.0));
  /* ... and finally check that it has changed */
  value = g_dbus_proxy_get_cached_property (proxy, "d");
  g_assert (value != NULL);
  g_assert (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE));
  g_assert_cmpfloat (g_variant_get_double (value), ==, 75.0);
  g_variant_unref (value);
  /* now update it via the D-Bus interface... */
  error = NULL;
  value = g_dbus_proxy_call_sync (proxy, "FrobSetProperty",
                                  g_variant_new ("(sv)", "d", g_variant_new_double (85.0)),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1, NULL, &error);
  g_assert_no_error (error);
  g_assert (value != NULL);
  g_assert_cmpstr (g_variant_get_type_string (value), ==, "()");
  g_variant_unref (value);
  /* ...ensure we receive the ::PropertiesChanged signal... */
  _g_assert_signal_received (proxy, "g-properties-changed");
  /* ... and finally check that it has changed */
  value = g_dbus_proxy_get_cached_property (proxy, "d");
  g_assert (value != NULL);
  g_assert (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE));
  g_assert_cmpfloat (g_variant_get_double (value), ==, 85.0);
  g_variant_unref (value);
}

static void
test_basic (GDBusProxy *proxy)
{
  GDBusConnection *connection;
  GDBusConnection *conn;
  GDBusProxyFlags flags;
  GDBusInterfaceInfo *info;
  gchar *name;
  gchar *path;
  gchar *interface;
  gint timeout;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  g_assert (g_dbus_proxy_get_connection (proxy) == connection);
  g_assert (g_dbus_proxy_get_flags (proxy) == G_DBUS_PROXY_FLAGS_NONE);
  g_assert (g_dbus_proxy_get_interface_info (proxy) == NULL);
  g_assert_cmpstr (g_dbus_proxy_get_name (proxy), ==, "com.example.TestService");
  g_assert_cmpstr (g_dbus_proxy_get_object_path (proxy), ==, "/com/example/TestObject");
  g_assert_cmpstr (g_dbus_proxy_get_interface_name (proxy), ==, "com.example.Frob");
  g_assert_cmpint (g_dbus_proxy_get_default_timeout (proxy), ==, -1);

  g_object_get (proxy,
                "g-connection", &conn,
                "g-interface-info", &info,
                "g-flags", &flags,
                "g-name", &name,
                "g-object-path", &path,
                "g-interface-name", &interface,
                "g-default-timeout", &timeout,
                NULL);

  g_assert (conn == connection);
  g_assert (info == NULL);
  g_assert_cmpint (flags, ==, G_DBUS_PROXY_FLAGS_NONE);
  g_assert_cmpstr (name, ==, "com.example.TestService");
  g_assert_cmpstr (path, ==, "/com/example/TestObject");
  g_assert_cmpstr (interface, ==, "com.example.Frob");
  g_assert_cmpint (timeout, ==, -1);

  g_object_unref (conn);
  g_free (name);
  g_free (path);
  g_free (interface);

  g_object_unref (connection);
}

static void
kill_test_service (GDBusConnection *connection)
{
#ifdef G_OS_UNIX
  guint pid;
  GVariant *ret;
  GError *error = NULL;
  const gchar *name = "com.example.TestService";

  ret = g_dbus_connection_call_sync (connection,
                                     "org.freedesktop.DBus",
                                     "/org/freedesktop/DBus",
                                     "org.freedesktop.DBus",
                                     "GetConnectionUnixProcessID",
                                     g_variant_new ("(s)", name),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);
  g_variant_get (ret, "(u)", &pid);
  g_variant_unref (ret);
  kill (pid, SIGTERM);
#else
  g_warning ("Can't kill com.example.TestService");
#endif
}

static void
test_proxy (void)
{
  GDBusProxy *proxy;
  GDBusConnection *connection;
  GError *error;

  error = NULL;
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                               NULL,
                               &error);
  g_assert_no_error (error);
  error = NULL;
  proxy = g_dbus_proxy_new_sync (connection,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,                      /* GDBusInterfaceInfo */
                                 "com.example.TestService", /* name */
                                 "/com/example/TestObject", /* object path */
                                 "com.example.Frob",        /* interface */
                                 NULL, /* GCancellable */
                                 &error);
  g_assert_no_error (error);

  /* this is safe; testserver will exit once the bus goes away */
  g_assert (g_spawn_command_line_async (g_test_get_filename (G_TEST_BUILT, "gdbus-testserver", NULL), NULL));

  _g_assert_property_notify (proxy, "g-name-owner");

  test_basic (proxy);
  test_methods (proxy);
  test_properties (proxy);
  test_signals (proxy);
  test_expected_interface (proxy);

  g_object_unref (proxy);
  kill_test_service (connection);
  g_object_unref (connection);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
proxy_ready (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
  GDBusProxy *proxy;
  GError *error;

  error = NULL;
  proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
  g_assert_no_error (error);

  _g_assert_property_notify (proxy, "g-name-owner");

  test_basic (proxy);
  test_methods (proxy);
  test_properties (proxy);
  test_signals (proxy);
  test_expected_interface (proxy);

  kill_test_service (g_dbus_proxy_get_connection (proxy));
  g_object_unref (proxy);
  g_main_loop_quit (loop);
}

static gboolean
fail_test (gpointer user_data)
{
  g_assert_not_reached ();
}

static void
test_async (void)
{
  guint id;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,                      /* GDBusInterfaceInfo */
                            "com.example.TestService", /* name */
                            "/com/example/TestObject", /* object path */
                            "com.example.Frob",        /* interface */
                            NULL, /* GCancellable */
                            proxy_ready,
                            NULL);

  /* this is safe; testserver will exit once the bus goes away */
  g_assert (g_spawn_command_line_async (g_test_get_filename (G_TEST_BUILT, "gdbus-testserver", NULL), NULL));

  id = g_timeout_add (10000, fail_test, NULL);
  g_main_loop_run (loop);

  g_source_remove (id);
}

static void
test_no_properties (void)
{
  GDBusProxy *proxy;
  GError *error;

  error = NULL;
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                         NULL,                      /* GDBusInterfaceInfo */
                                         "com.example.TestService", /* name */
                                         "/com/example/TestObject", /* object path */
                                         "com.example.Frob",        /* interface */
                                         NULL, /* GCancellable */
                                         &error);
  g_assert_no_error (error);

  test_properties (proxy);

  g_object_unref (proxy);
}

static void
check_error (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
  GError *error = NULL;
  GVariant *reply;

  reply = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), result, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert (reply == NULL);
  g_error_free (error);

  g_main_loop_quit (loop);
}

static void
test_wellknown_noauto (void)
{
  GError *error = NULL;
  GDBusProxy *proxy;
  guint id;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                         NULL, "some.name.that.does.not.exist",
                                         "/", "some.interface", NULL, &error);
  g_assert_no_error (error);
  g_assert (proxy != NULL);

  g_dbus_proxy_call (proxy, "method", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, check_error, NULL);
  id = g_timeout_add (10000, fail_test, NULL);
  g_main_loop_run (loop);
  g_object_unref (proxy);
  g_source_remove (id);
}

int
main (int   argc,
      char *argv[])
{
  gint ret;
  GDBusNodeInfo *introspection_data = NULL;

  g_test_init (&argc, &argv, NULL);

  introspection_data = g_dbus_node_info_new_for_xml (frob_dbus_interface_xml, NULL);
  g_assert (introspection_data != NULL);
  frob_dbus_interface_info = introspection_data->interfaces[0];

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_add_func ("/gdbus/proxy", test_proxy);
  g_test_add_func ("/gdbus/proxy/no-properties", test_no_properties);
  g_test_add_func ("/gdbus/proxy/wellknown-noauto", test_wellknown_noauto);
  g_test_add_func ("/gdbus/proxy/async", test_async);

  ret = session_bus_run();

  g_dbus_node_info_unref (introspection_data);

  return ret;
}
