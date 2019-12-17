#include <gio/gio.h>
#include <stdlib.h>

static GDBusNodeInfo *introspection_data = NULL;
static GMainLoop *loop = NULL;
static GHashTable *properties = NULL;

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='com.example.Frob'>"
  "    <method name='Quit'>"
  "    </method>"
  "    <method name='TestArrayOfStringTypes'>"
  "      <arg direction='in'  type='as' name='val_string' />"
  "      <arg direction='in'  type='ao' name='val_objpath' />"
  "      <arg direction='in'  type='ag' name='val_signature' />"
  "      <arg direction='out' type='as' />"
  "      <arg direction='out' type='ao' />"
  "      <arg direction='out' type='ag' />"
  "    </method>"
  "    <method name='TestPrimitiveTypes'>"
  "      <arg direction='in'  type='y' name='val_byte' />"
  "      <arg direction='in'  type='b' name='val_boolean' />"
  "      <arg direction='in'  type='n' name='val_int16' />"
  "      <arg direction='in'  type='q' name='val_uint16' />"
  "      <arg direction='in'  type='i' name='val_int32' />"
  "      <arg direction='in'  type='u' name='val_uint32' />"
  "      <arg direction='in'  type='x' name='val_int64' />"
  "      <arg direction='in'  type='t' name='val_uint64' />"
  "      <arg direction='in'  type='d' name='val_double' />"
  "      <arg direction='in'  type='s' name='val_string' />"
  "      <arg direction='in'  type='o' name='val_objpath' />"
  "      <arg direction='in'  type='g' name='val_signature' />"
  "      <arg direction='out' type='y' />"
  "      <arg direction='out' type='b' />"
  "      <arg direction='out' type='n' />"
  "      <arg direction='out' type='q' />"
  "      <arg direction='out' type='i' />"
  "      <arg direction='out' type='u' />"
  "      <arg direction='out' type='x' />"
  "      <arg direction='out' type='t' />"
  "      <arg direction='out' type='d' />"
  "      <arg direction='out' type='s' />"
  "      <arg direction='out' type='o' />"
  "      <arg direction='out' type='g' />"
  "    </method>"
  "    <method name='EmitSignal'>"
  "      <arg direction='in'  type='s' name='str1' />"
  "      <arg direction='in'  type='o' name='objpath1' />"
  "    </method>"
  "    <method name='TestArrayOfPrimitiveTypes'>"
  "      <arg direction='in'  type='ay' name='val_byte' />"
  "      <arg direction='in'  type='ab' name='val_boolean' />"
  "      <arg direction='in'  type='an' name='val_int16' />"
  "      <arg direction='in'  type='aq' name='val_uint16' />"
  "      <arg direction='in'  type='ai' name='val_int32' />"
  "      <arg direction='in'  type='au' name='val_uint32' />"
  "      <arg direction='in'  type='ax' name='val_int64' />"
  "      <arg direction='in'  type='at' name='val_uint64' />"
  "      <arg direction='in'  type='ad' name='val_double' />"
  "      <arg direction='out' type='ay' />"
  "      <arg direction='out' type='ab' />"
  "      <arg direction='out' type='an' />"
  "      <arg direction='out' type='aq' />"
  "      <arg direction='out' type='ai' />"
  "      <arg direction='out' type='au' />"
  "      <arg direction='out' type='ax' />"
  "      <arg direction='out' type='at' />"
  "      <arg direction='out' type='ad' />"
  "    </method>"
  "    <method name='FrobSetProperty'>"
  "      <arg direction='in'  type='s' name='prop_name' />"
  "      <arg direction='in'  type='v' name='prop_value' />"
  "    </method>"
  "    <signal name='TestSignal'>"
  "      <arg type='s' name='str1' />"
  "      <arg type='o' name='objpath1' />"
  "      <arg type='v' name='variant1' />"
  "    </signal>"
  "    <method name='TestComplexArrays'>"
  "      <arg direction='in'  type='a(ii)' name='aii' />"
  "      <arg direction='in'  type='aa(ii)' name='aaii' />"
  "      <arg direction='in'  type='aas' name='aas' />"
  "      <arg direction='in'  type='aa{ss}' name='ahashes' />"
  "      <arg direction='in'  type='aay' name='aay' />"
  "      <arg direction='in'  type='av' name='av' />"
  "      <arg direction='in'  type='aav' name='aav' />"
  "      <arg direction='out' type='a(ii)' />"
  "      <arg direction='out' type='aa(ii)' />"
  "      <arg direction='out' type='aas' />"
  "      <arg direction='out' type='aa{ss}' />"
  "      <arg direction='out' type='aay' />"
  "      <arg direction='out' type='av' />"
  "      <arg direction='out' type='aav' />"
  "    </method>"
  "    <method name='TestVariant'>"
  "      <arg direction='in'  type='v' name='v' />"
  "      <arg direction='in'  type='b' name='modify' />"
  "      <arg direction='out' type='v' />"
  "    </method>"
  "    <method name='FrobInvalidateProperty'>"
  "      <arg direction='in'  type='s' name='new_value' />"
  "    </method>"
  "    <method name='HelloWorld'>"
  "      <arg direction='in'  type='s' name='hello_message' />"
  "      <arg direction='out' type='s' />"
  "    </method>"
  "    <method name='PairReturn'>"
  "      <arg direction='out' type='s' />"
  "      <arg direction='out' type='u' />"
  "    </method>"
  "    <method name='TestStructureTypes'>"
  "      <arg direction='in'  type='(ii)' name='s1' />"
  "      <arg direction='in'  type='(s(ii)aya{ss})' name='s2' />"
  "      <arg direction='out' type='(ii)' />"
  "      <arg direction='out' type='(s(ii)aya{ss})' />"
  "    </method>"
  "    <method name='EmitSignal2'>"
  "    </method>"
  "    <method name='DoubleHelloWorld'>"
  "      <arg direction='in'  type='s' name='hello1' />"
  "      <arg direction='in'  type='s' name='hello2' />"
  "      <arg direction='out' type='s' />"
  "      <arg direction='out' type='s' />"
  "    </method>"
  "    <method name='Sleep'>"
  "      <arg direction='in'  type='i' name='msec' />"
  "    </method>"
  "    <method name='TestHashTables'>"
  "      <arg direction='in'  type='a{yy}' name='hyy' />"
  "      <arg direction='in'  type='a{bb}' name='hbb' />"
  "      <arg direction='in'  type='a{nn}' name='hnn' />"
  "      <arg direction='in'  type='a{qq}' name='hqq' />"
  "      <arg direction='in'  type='a{ii}' name='hii' />"
  "      <arg direction='in'  type='a{uu}' name='huu' />"
  "      <arg direction='in'  type='a{xx}' name='hxx' />"
  "      <arg direction='in'  type='a{tt}' name='htt' />"
  "      <arg direction='in'  type='a{dd}' name='hdd' />"
  "      <arg direction='in'  type='a{ss}' name='hss' />"
  "      <arg direction='in'  type='a{oo}' name='hoo' />"
  "      <arg direction='in'  type='a{gg}' name='hgg' />"
  "      <arg direction='out' type='a{yy}' />"
  "      <arg direction='out' type='a{bb}' />"
  "      <arg direction='out' type='a{nn}' />"
  "      <arg direction='out' type='a{qq}' />"
  "      <arg direction='out' type='a{ii}' />"
  "      <arg direction='out' type='a{uu}' />"
  "      <arg direction='out' type='a{xx}' />"
  "      <arg direction='out' type='a{tt}' />"
  "      <arg direction='out' type='a{dd}' />"
  "      <arg direction='out' type='a{ss}' />"
  "      <arg direction='out' type='a{oo}' />"
  "      <arg direction='out' type='a{gg}' />"
  "    </method>"
  "    <signal name='TestSignal2'>"
  "      <arg type='i' name='int1' />"
  "    </signal>"
  "    <method name='TestComplexHashTables'>"
  "      <arg direction='in'  type='a{s(ii)}' name='h_str_to_pair' />"
  "      <arg direction='in'  type='a{sv}' name='h_str_to_variant' />"
  "      <arg direction='in'  type='a{sav}' name='h_str_to_av' />"
  "      <arg direction='in'  type='a{saav}' name='h_str_to_aav' />"
  "      <arg direction='in'  type='a{sa(ii)}' name='h_str_to_array_of_pairs' />"
  "      <arg direction='in'  type='a{sa{ss}}' name='hash_of_hashes' />"
  "      <arg direction='out' type='a{s(ii)}' />"
  "      <arg direction='out' type='a{sv}' />"
  "      <arg direction='out' type='a{sav}' />"
  "      <arg direction='out' type='a{saav}' />"
  "      <arg direction='out' type='a{sa(ii)}' />"
  "      <arg direction='out' type='a{sa{ss}}' />"
  "    </method>"
  "    <property type='y' name='y' access='readwrite' />"
  "    <property type='b' name='b' access='readwrite' />"
  "    <property type='n' name='n' access='readwrite' />"
  "    <property type='q' name='q' access='readwrite' />"
  "    <property type='i' name='i' access='readwrite' />"
  "    <property type='u' name='u' access='readwrite' />"
  "    <property type='x' name='x' access='readwrite' />"
  "    <property type='t' name='t' access='readwrite' />"
  "    <property type='d' name='d' access='readwrite' />"
  "    <property type='s' name='s' access='readwrite' />"
  "    <property type='o' name='o' access='readwrite' />"
  "    <property type='ay' name='ay' access='readwrite' />"
  "    <property type='ab' name='ab' access='readwrite' />"
  "    <property type='an' name='an' access='readwrite' />"
  "    <property type='aq' name='aq' access='readwrite' />"
  "    <property type='ai' name='ai' access='readwrite' />"
  "    <property type='au' name='au' access='readwrite' />"
  "    <property type='ax' name='ax' access='readwrite' />"
  "    <property type='at' name='at' access='readwrite' />"
  "    <property type='ad' name='ad' access='readwrite' />"
  "    <property type='as' name='as' access='readwrite' />"
  "    <property type='ao' name='ao' access='readwrite' />"
  "    <property type='s' name='foo' access='readwrite' />"
  "    <property type='s' name='PropertyThatWillBeInvalidated' access='readwrite' />"
  "  </interface>"
  "</node>";

static gboolean
end_sleep (gpointer data)
{
  GDBusMethodInvocation *invocation = data;

  g_dbus_method_invocation_return_value (invocation, NULL);
  g_object_unref (invocation);

  return G_SOURCE_REMOVE;
}

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
  if (g_strcmp0 (method_name, "HelloWorld") == 0)
    {
      const gchar *greeting;

      g_variant_get (parameters, "(&s)", &greeting);
      if (g_strcmp0 (greeting, "Yo") == 0)
        {
          g_dbus_method_invocation_return_dbus_error (invocation,
                                                      "com.example.TestException",
                                                      "Yo is not a proper greeting");
        }
      else
        {
          gchar *response;
          response = g_strdup_printf ("You greeted me with '%s'. Thanks!", greeting);
          g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("(s)", response));
          g_free ( response);
        }
    }
  else if (g_strcmp0 (method_name, "DoubleHelloWorld") == 0)
    {
      const gchar *hello1, *hello2;
      gchar *reply1, *reply2;

      g_variant_get (parameters, "(&s&s)", &hello1, &hello2);
      reply1 = g_strdup_printf ("You greeted me with '%s'. Thanks!", hello1);
      reply2 = g_strdup_printf ("Yo dawg, you uttered '%s'. Thanks!", hello2);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(ss)", reply1, reply2));
      g_free (reply1);
      g_free (reply2);
    }
  else if (g_strcmp0 (method_name, "PairReturn") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(su)", "foo", 42));
    }
  else if (g_strcmp0 (method_name, "TestPrimitiveTypes") == 0)
    {
      guchar val_byte;
      gboolean val_boolean;
      gint16 val_int16;
      guint16 val_uint16;
      gint32 val_int32;
      guint32 val_uint32;
      gint64 val_int64;
      guint64 val_uint64;
      gdouble val_double;
      const gchar *val_string;
      const gchar *val_objpath;
      const gchar *val_signature;
      gchar *ret_string;
      gchar *ret_objpath;
      gchar *ret_signature;

      g_variant_get (parameters, "(ybnqiuxtd&s&o&g)",
                     &val_byte,
                     &val_boolean,
                     &val_int16,
                     &val_uint16,
                     &val_int32,
                     &val_uint32,
                     &val_int64,
                     &val_uint64,
                     &val_double,
                     &val_string,
                     &val_objpath,
                     &val_signature);

      ret_string = g_strconcat (val_string, val_string, NULL);
      ret_objpath = g_strconcat (val_objpath, "/modified", NULL);
      ret_signature = g_strconcat (val_signature, val_signature, NULL);

      g_dbus_method_invocation_return_value (invocation,
          g_variant_new ("(ybnqiuxtdsog)",
                         val_byte + 1,
                         !val_boolean,
                         val_int16 + 1,
                         val_uint16 + 1,
                         val_int32 + 1,
                         val_uint32 + 1,
                         val_int64 + 1,
                         val_uint64 + 1,
                         - val_double + 0.123,
                         ret_string,
                         ret_objpath,
                         ret_signature));

      g_free (ret_string);
      g_free (ret_objpath);
      g_free (ret_signature);
    }
  else if (g_strcmp0 (method_name, "TestArrayOfPrimitiveTypes") == 0)
    {
      GVariant *v;
      const guchar *bytes;
      const gint16 *int16s;
      const guint16 *uint16s;
      const gint32 *int32s;
      const guint32 *uint32s;
      const gint64 *int64s;
      const guint64 *uint64s;
      const gdouble *doubles;
      gsize n_elts;
      gint i, j;
      GVariantBuilder ret;

      g_variant_builder_init (&ret, G_VARIANT_TYPE ("(ayabanaqaiauaxatad)"));

      v = g_variant_get_child_value (parameters, 0);
      bytes = g_variant_get_fixed_array (v, &n_elts, 1);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("ay"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "y", bytes[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 1);
      bytes = g_variant_get_fixed_array (v, &n_elts, 1);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("ab"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "b", (gboolean)bytes[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 2);
      int16s = g_variant_get_fixed_array (v, &n_elts, 2);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("an"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "n", int16s[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 3);
      uint16s = g_variant_get_fixed_array (v, &n_elts, 2);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("aq"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "q", uint16s[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 4);
      int32s = g_variant_get_fixed_array (v, &n_elts, 4);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("ai"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "i", int32s[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 5);
      uint32s = g_variant_get_fixed_array (v, &n_elts, 4);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("au"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "u", uint32s[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 6);
      int64s = g_variant_get_fixed_array (v, &n_elts, 8);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("ax"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "x", int64s[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 7);
      uint64s = g_variant_get_fixed_array (v, &n_elts, 8);
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("at"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "t", uint64s[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      v = g_variant_get_child_value (parameters, 8);
      doubles = g_variant_get_fixed_array (v, &n_elts, sizeof (gdouble));
      g_variant_builder_open (&ret, G_VARIANT_TYPE ("ad"));
      for (j = 0; j < 2; j++)
        for (i = 0; i < n_elts; i++)
          g_variant_builder_add (&ret, "d", doubles[i]);
      g_variant_builder_close (&ret);
      g_variant_unref (v);

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_builder_end (&ret));
    }
  else if (g_strcmp0 (method_name, "TestArrayOfStringTypes") == 0)
    {
      GVariantIter *iter1;
      GVariantIter *iter2;
      GVariantIter *iter3;
      GVariantIter *iter;
      GVariantBuilder ret;
      const gchar *s;
      gint i;

      g_variant_builder_init (&ret, G_VARIANT_TYPE ("(asaoag)"));
      g_variant_get (parameters, "(asaoag)", &iter1, &iter2, &iter3);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("as"));
      for (i = 0; i < 2; i++)
        {
          iter = g_variant_iter_copy (iter1);
          while (g_variant_iter_loop (iter, "s", &s))
            g_variant_builder_add (&ret, "s", s);
          g_variant_iter_free (iter);
        }
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("ao"));
      for (i = 0; i < 2; i++)
        {
          iter = g_variant_iter_copy (iter1);
          while (g_variant_iter_loop (iter, "o", &s))
            g_variant_builder_add (&ret, "o", s);
          g_variant_iter_free (iter);
        }
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("ag"));
      for (i = 0; i < 2; i++)
        {
          iter = g_variant_iter_copy (iter1);
          while (g_variant_iter_loop (iter, "g", &s))
            g_variant_builder_add (&ret, "g", s);
          g_variant_iter_free (iter);
        }
      g_variant_builder_close (&ret);

      g_variant_iter_free (iter1);
      g_variant_iter_free (iter2);
      g_variant_iter_free (iter3);

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_builder_end (&ret));
    }
  else if (g_strcmp0 (method_name, "TestHashTables") == 0)
    {
      GVariant *v;
      GVariantIter iter;
      GVariantBuilder ret;
      guint8 y1, y2;
      gboolean b1, b2;
      gint16 n1, n2;
      guint16 q1, q2;
      gint i1, i2;
      guint u1, u2;
      gint64 x1, x2;
      guint64 t1, t2;
      gdouble d1, d2;
      gchar *s1, *s2;

      g_variant_builder_init (&ret, G_VARIANT_TYPE ("(a{yy}a{bb}a{nn}a{qq}a{ii}a{uu}a{xx}a{tt}a{dd}a{ss}a{oo}a{gg})"));

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{yy}"));
      v = g_variant_get_child_value (parameters, 0);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "yy", &y1, &y2))
        g_variant_builder_add (&ret, "{yy}", y1 * 2, (y2 * 3) & 255);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{bb}"));
      v = g_variant_get_child_value (parameters, 1);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "bb", &b1, &b2))
        g_variant_builder_add (&ret, "{bb}", b1, TRUE);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{nn}"));
      v = g_variant_get_child_value (parameters, 2);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "nn", &n1, &n2))
        g_variant_builder_add (&ret, "{nn}", n1 * 2, n2 * 3);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{qq}"));
      v = g_variant_get_child_value (parameters, 3);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "qq", &q1, &q2))
        g_variant_builder_add (&ret, "{qq}", q1 * 2, q2 * 3);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{ii}"));
      v = g_variant_get_child_value (parameters, 4);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "ii", &i1, &i2))
        g_variant_builder_add (&ret, "{ii}", i1 * 2, i2 * 3);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{uu}"));
      v = g_variant_get_child_value (parameters, 5);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "uu", &u1, &u2))
        g_variant_builder_add (&ret, "{uu}", u1 * 2, u2 * 3);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{xx}"));
      v = g_variant_get_child_value (parameters, 6);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "xx", &x1, &x2))
        g_variant_builder_add (&ret, "{xx}", x1 + 2, x2  + 1);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{tt}"));
      v = g_variant_get_child_value (parameters, 7);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "tt", &t1, &t2))
        g_variant_builder_add (&ret, "{tt}", t1 + 2, t2  + 1);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{dd}"));
      v = g_variant_get_child_value (parameters, 8);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "dd", &d1, &d2))
        g_variant_builder_add (&ret, "{dd}", d1 + 2.5, d2  + 5.0);
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{ss}"));
      v = g_variant_get_child_value (parameters, 9);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "ss", &s1, &s2))
        {
          gchar *tmp1, *tmp2;
          tmp1 = g_strconcat (s1, "mod", NULL);
          tmp2 = g_strconcat (s2, s2, NULL);
          g_variant_builder_add (&ret, "{ss}", tmp1, tmp2);
          g_free (tmp1);
          g_free (tmp2);
        }
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{oo}"));
      v = g_variant_get_child_value (parameters, 10);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "oo", &s1, &s2))
        {
          gchar *tmp1, *tmp2;
          tmp1 = g_strconcat (s1, "/mod", NULL);
          tmp2 = g_strconcat (s2, "/mod2", NULL);
          g_variant_builder_add (&ret, "{oo}", tmp1, tmp2);
          g_free (tmp1);
          g_free (tmp2);
        }
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_variant_builder_open (&ret, G_VARIANT_TYPE ("a{gg}"));
      v = g_variant_get_child_value (parameters, 11);
      g_variant_iter_init (&iter, v);
      while (g_variant_iter_loop (&iter, "gg", &s1, &s2))
        {
          gchar *tmp1, *tmp2;
          tmp1 = g_strconcat (s1, "assgit", NULL);
          tmp2 = g_strconcat (s2, s2, NULL);
          g_variant_builder_add (&ret, "{gg}", tmp1, tmp2);
          g_free (tmp1);
          g_free (tmp2);
        }
      g_variant_unref (v);
      g_variant_builder_close (&ret);

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_builder_end (&ret));
    }
  else if (g_strcmp0 (method_name, "TestStructureTypes") == 0)
    {
      gint x, y, x1, y1;
      const gchar *desc;
      GVariantIter *iter1, *iter2;
      gchar *desc_ret;
      GVariantBuilder ret1, ret2;
      GVariantIter *iter;
      GVariant *v;
      gchar *s1, *s2;

      g_variant_get (parameters, "((ii)(&s(ii)aya{ss}))",
                     &x, &y, &desc, &x1, &y1, &iter1, &iter2);

      desc_ret = g_strconcat (desc, "... in bed!", NULL);

      g_variant_builder_init (&ret1, G_VARIANT_TYPE ("ay"));
      iter = g_variant_iter_copy (iter1);
      while (g_variant_iter_loop (iter1, "y", &v))
        g_variant_builder_add (&ret1, "y", v);
      while (g_variant_iter_loop (iter, "y", &v))
        g_variant_builder_add (&ret1, "y", v);
      g_variant_iter_free (iter);
      g_variant_iter_free (iter1);

      g_variant_builder_init (&ret2, G_VARIANT_TYPE ("a{ss}"));
      while (g_variant_iter_loop (iter1, "ss", &s1, &s2))
        {
          gchar *tmp;
          tmp = g_strconcat (s2, " ... in bed!", NULL);
          g_variant_builder_add (&ret1, "{ss}", s1, tmp);
          g_free (tmp);
        }
      g_variant_iter_free (iter2);

      g_dbus_method_invocation_return_value (invocation,
           g_variant_new ("((ii)(&s(ii)aya{ss}))",
                          x + 1, y + 1, desc_ret, x1 + 2, y1 + 2,
                          &ret1, &ret2));

      g_free (desc_ret);
    }
  else if (g_strcmp0 (method_name, "TestVariant") == 0)
    {
      GVariant *v;
      gboolean modify;
      GVariant *ret;

      g_variant_get (parameters, "(vb)", &v, &modify);

      /* FIXME handle more cases */
      if (modify)
        {
          if (g_variant_is_of_type (v, G_VARIANT_TYPE_BOOLEAN))
            {
              ret = g_variant_new_boolean (FALSE);
            }
          else if (g_variant_is_of_type (v, G_VARIANT_TYPE_TUPLE))
            {
              ret = g_variant_new ("(si)", "other struct", 100);
            }
          else
            g_assert_not_reached ();
        }
      else
        ret = v;

      g_dbus_method_invocation_return_value (invocation, ret);
      g_variant_unref (v);
    }
  else if (g_strcmp0 (method_name, "TestComplexArrays") == 0)
    {
      /* FIXME */
      g_dbus_method_invocation_return_value (invocation, parameters);
    }
  else if (g_strcmp0 (method_name, "TestComplexHashTables") == 0)
    {
      /* FIXME */
      g_dbus_method_invocation_return_value (invocation, parameters);
    }
  else if (g_strcmp0 (method_name, "FrobSetProperty") == 0)
    {
      gchar *name;
      GVariant *value;
      g_variant_get (parameters, "(sv)", &name, &value);
      g_hash_table_replace (properties, name, value);
      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     "/com/example/TestObject",
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged",
                                     g_variant_new_parsed ("('com.example.Frob', [{%s, %v}], @as [])", name, value),
                                     NULL);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "FrobInvalidateProperty") == 0)
    {
      const gchar *value;
      g_variant_get (parameters, "(&s)", &value);
      g_hash_table_replace (properties, g_strdup ("PropertyThatWillBeInvalidated"), g_variant_ref_sink (g_variant_new_string (value)));

      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     "/com/example/TestObject",
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged",
                                     g_variant_new_parsed ("('com.example.Frob', @a{sv} [], ['PropertyThatWillBeInvalidated'])"),
                                     NULL);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "EmitSignal") == 0)
    {
      const gchar *str;
      const gchar *path;
      gchar *str_ret;
      gchar *path_ret;
      g_variant_get (parameters, "(&s&o)", &str, &path);
      str_ret = g_strconcat (str, " .. in bed!", NULL);
      path_ret = g_strconcat (path, "/in/bed", NULL);
      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     "/com/example/TestObject",
                                     "com.example.Frob",
                                     "TestSignal",
                                     g_variant_new_parsed ("(%s, %o, <'a variant'>)", str_ret, path_ret),
                                     NULL);
      g_free (str_ret);
      g_free (path_ret);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "EmitSignal2") == 0)
    {
      g_dbus_connection_emit_signal (connection,
                                     NULL,
                                     "/com/example/TestObject",
                                     "com.example.Frob",
                                     "TestSignal2",
                                     g_variant_new_parsed ("(42, )"),
                                     NULL);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "Sleep") == 0)
    {
      gint msec;

      g_variant_get (parameters, "(i)", &msec);

      g_timeout_add ((guint)msec, end_sleep, g_object_ref (invocation));
    }
  else if (g_strcmp0 (method_name, "Quit") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, NULL);
      g_main_loop_quit (loop);
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

  ret = g_hash_table_lookup (properties, property_name);
  if (ret)
    {
      g_assert (!g_variant_is_floating (ret));
      g_variant_ref (ret);
    }
  else
    {
      g_set_error (error,
                   G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                   "no such property: %s", property_name);
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
  g_set_error (error,
               G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
               "SetProperty not implemented");
  return FALSE;
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  handle_get_property,
  handle_set_property
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  guint id;

  id = g_dbus_connection_register_object (connection,
                                          "/com/example/TestObject",
                                          introspection_data->interfaces[0],
                                          &interface_vtable,
                                          NULL,
                                          NULL,
                                          NULL);
  g_assert (id > 0);
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

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  properties = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);
  g_hash_table_insert (properties, g_strdup ("y"), g_variant_ref_sink (g_variant_new_byte (1)));
  g_hash_table_insert (properties, g_strdup ("b"), g_variant_ref_sink (g_variant_new_boolean (TRUE)));
  g_hash_table_insert (properties, g_strdup ("n"), g_variant_ref_sink (g_variant_new_int16 (2)));
  g_hash_table_insert (properties, g_strdup ("q"), g_variant_ref_sink (g_variant_new_uint16 (3)));
  g_hash_table_insert (properties, g_strdup ("i"), g_variant_ref_sink (g_variant_new_int32 (4)));
  g_hash_table_insert (properties, g_strdup ("u"), g_variant_ref_sink (g_variant_new_uint32 (5)));
  g_hash_table_insert (properties, g_strdup ("x"), g_variant_ref_sink (g_variant_new_int64 (6)));
  g_hash_table_insert (properties, g_strdup ("t"), g_variant_ref_sink (g_variant_new_uint64 (7)));
  g_hash_table_insert (properties, g_strdup ("d"), g_variant_ref_sink (g_variant_new_double (7.5)));
  g_hash_table_insert (properties, g_strdup ("s"), g_variant_ref_sink (g_variant_new_string ("a string")));
  g_hash_table_insert (properties, g_strdup ("o"), g_variant_ref_sink (g_variant_new_object_path ("/some/path")));
  g_hash_table_insert (properties, g_strdup ("ay"), g_variant_ref_sink (g_variant_new_parsed ("[@y 1, @y 11]")));
  g_hash_table_insert (properties, g_strdup ("ab"), g_variant_ref_sink (g_variant_new_parsed ("[true, false]")));
  g_hash_table_insert (properties, g_strdup ("an"), g_variant_ref_sink (g_variant_new_parsed ("[@n 2, @n 12]")));
  g_hash_table_insert (properties, g_strdup ("aq"), g_variant_ref_sink (g_variant_new_parsed ("[@q 3, @q 13]")));
  g_hash_table_insert (properties, g_strdup ("ai"), g_variant_ref_sink (g_variant_new_parsed ("[@i 4, @i 14]")));
  g_hash_table_insert (properties, g_strdup ("au"), g_variant_ref_sink (g_variant_new_parsed ("[@u 5, @u 15]")));
  g_hash_table_insert (properties, g_strdup ("ax"), g_variant_ref_sink (g_variant_new_parsed ("[@x 6, @x 16]")));
  g_hash_table_insert (properties, g_strdup ("at"), g_variant_ref_sink (g_variant_new_parsed ("[@t 7, @t 17]")));
  g_hash_table_insert (properties, g_strdup ("ad"), g_variant_ref_sink (g_variant_new_parsed ("[7.5, 17.5]")));
  g_hash_table_insert (properties, g_strdup ("as"), g_variant_ref_sink (g_variant_new_parsed ("['a string', 'another string']")));
  g_hash_table_insert (properties, g_strdup ("ao"), g_variant_ref_sink (g_variant_new_parsed ("[@o '/some/path', @o '/another/path']")));
  g_hash_table_insert (properties, g_strdup ("foo"), g_variant_ref_sink (g_variant_new_string ("a frobbed string")));
  g_hash_table_insert (properties, g_strdup ("PropertyThatWillBeInvalidated"), g_variant_ref_sink (g_variant_new_string ("InitialValue")));

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "com.example.TestService",
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
