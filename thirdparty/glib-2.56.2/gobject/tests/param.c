#define GLIB_DISABLE_DEPRECATION_WARNINGS
#include <glib-object.h>
#include <stdlib.h>

static void
test_param_value (void)
{
  GParamSpec *p, *p2;
  GParamSpec *pp;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_PARAM);
  g_assert (G_VALUE_HOLDS_PARAM (&value));

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);

  g_value_take_param (&value, p);
  p2 = g_value_get_param (&value);
  g_assert (p2 == p);

  pp = g_param_spec_uint ("my-uint", "My UInt", "Blurb", 0, 10, 5, G_PARAM_READWRITE);
  g_value_set_param (&value, pp);

  p2 = g_value_dup_param (&value);
  g_assert (p2 == pp); /* param specs use ref/unref for copy/free */
  g_param_spec_unref (p2);

  g_value_unset (&value);
  g_param_spec_unref (pp);
}

static gint destroy_count;

static void
my_destroy (gpointer data)
{
  destroy_count++;
}

static void
test_param_qdata (void)
{
  GParamSpec *p;
  gchar *bla;
  GQuark q;

  q = g_quark_from_string ("bla");

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);
  g_param_spec_set_qdata (p, q, "bla");
  bla = g_param_spec_get_qdata (p, q);
  g_assert_cmpstr (bla, ==, "bla");

  g_assert_cmpint (destroy_count, ==, 0);
  g_param_spec_set_qdata_full (p, q, "bla", my_destroy);
  g_param_spec_set_qdata_full (p, q, "blabla", my_destroy);
  g_assert_cmpint (destroy_count, ==, 1);
  g_assert_cmpstr (g_param_spec_steal_qdata (p, q), ==, "blabla");
  g_assert_cmpint (destroy_count, ==, 1);
  g_assert (g_param_spec_get_qdata (p, q) == NULL);

  g_param_spec_ref_sink (p);

  g_param_spec_unref (p);
}

static void
test_param_validate (void)
{
  GParamSpec *p;
  GValue value = G_VALUE_INIT;

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, 100);
  g_assert (!g_param_value_defaults (p, &value));
  g_assert (g_param_value_validate (p, &value));
  g_assert_cmpint (g_value_get_int (&value), ==, 20);

  g_param_value_set_default (p, &value);
  g_assert (g_param_value_defaults (p, &value));
  g_assert_cmpint (g_value_get_int (&value), ==, 10);

  g_param_spec_unref (p);
}

static void
test_param_strings (void)
{
  GParamSpec *p;

  /* test canonicalization */
  p = g_param_spec_int ("my_int:bla", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);

  g_assert_cmpstr (g_param_spec_get_name (p), ==, "my-int-bla");
  g_assert_cmpstr (g_param_spec_get_nick (p), ==, "My Int");
  g_assert_cmpstr (g_param_spec_get_blurb (p), ==, "Blurb");

  g_param_spec_unref (p);

  /* test nick defaults to name */
  p = g_param_spec_int ("my-int", NULL, NULL, 0, 20, 10, G_PARAM_READWRITE);

  g_assert_cmpstr (g_param_spec_get_name (p), ==, "my-int");
  g_assert_cmpstr (g_param_spec_get_nick (p), ==, "my-int");
  g_assert (g_param_spec_get_blurb (p) == NULL);

  g_param_spec_unref (p);
}

static void
test_param_convert (void)
{
  GParamSpec *p;
  GValue v1 = G_VALUE_INIT;
  GValue v2 = G_VALUE_INIT;

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);
  g_value_init (&v1, G_TYPE_UINT);
  g_value_set_uint (&v1, 43);

  g_value_init (&v2, G_TYPE_INT);
  g_value_set_int (&v2, -4);

  g_assert (!g_param_value_convert (p, &v1, &v2, TRUE));
  g_assert_cmpint (g_value_get_int (&v2), ==, -4);

  g_assert (g_param_value_convert (p, &v1, &v2, FALSE));
  g_assert_cmpint (g_value_get_int (&v2), ==, 20);

  g_param_spec_unref (p);
}

static void
test_value_transform (void)
{
  GValue src = G_VALUE_INIT;
  GValue dest = G_VALUE_INIT;

#define CHECK_INT_CONVERSION(type, getter, value)                       \
  g_assert (g_value_type_transformable (G_TYPE_INT, type));             \
  g_value_init (&src, G_TYPE_INT);                                      \
  g_value_init (&dest, type);                                           \
  g_value_set_int (&src, value);                                        \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_##getter (&dest), ==, value);            \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  /* Keep a check for an integer in the range of 0-127 so we're
   * still testing g_value_get_char().  See
   * https://bugzilla.gnome.org/show_bug.cgi?id=659870
   * for why it is broken.
   */
  CHECK_INT_CONVERSION(G_TYPE_CHAR, char, 124)

  CHECK_INT_CONVERSION(G_TYPE_CHAR, schar, -124)
  CHECK_INT_CONVERSION(G_TYPE_CHAR, schar, 124)
  CHECK_INT_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_INT_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_INT_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_INT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_INT_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_INT_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_INT_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_INT_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_INT_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_INT_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_INT_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_INT_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_UINT_CONVERSION(type, getter, value)                      \
  g_assert (g_value_type_transformable (G_TYPE_UINT, type));            \
  g_value_init (&src, G_TYPE_UINT);                                     \
  g_value_init (&dest, type);                                           \
  g_value_set_uint (&src, value);                                       \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpuint (g_value_get_##getter (&dest), ==, value);           \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_UINT_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_UINT_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_UINT_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_UINT_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_UINT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_UINT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_UINT_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_UINT_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_UINT_CONVERSION(G_TYPE_LONG, long, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_INT64, int64, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_LONG_CONVERSION(type, getter, value)                      \
  g_assert (g_value_type_transformable (G_TYPE_LONG, type));            \
  g_value_init (&src, G_TYPE_LONG);                                     \
  g_value_init (&dest, type);                                           \
  g_value_set_long (&src, value);                                       \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_##getter (&dest), ==, value);            \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_LONG_CONVERSION(G_TYPE_CHAR, schar, -124)
  CHECK_LONG_CONVERSION(G_TYPE_CHAR, schar, 124)
  CHECK_LONG_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_LONG_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_LONG_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_LONG_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_LONG_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_LONG_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_LONG_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_LONG_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_LONG_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_LONG_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_LONG_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_LONG_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_ULONG_CONVERSION(type, getter, value)                     \
  g_assert (g_value_type_transformable (G_TYPE_ULONG, type));           \
  g_value_init (&src, G_TYPE_ULONG);                                    \
  g_value_init (&dest, type);                                           \
  g_value_set_ulong (&src, value);                                      \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpuint (g_value_get_##getter (&dest), ==, value);           \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_ULONG_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_ULONG_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_ULONG_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_ULONG_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_ULONG_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_ULONG_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_ULONG_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_ULONG_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_ULONG_CONVERSION(G_TYPE_LONG, long, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_INT64, int64, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_INT64_CONVERSION(type, getter, value)                     \
  g_assert (g_value_type_transformable (G_TYPE_INT64, type));           \
  g_value_init (&src, G_TYPE_INT64);                                    \
  g_value_init (&dest, type);                                           \
  g_value_set_int64 (&src, value);                                      \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_##getter (&dest), ==, value);            \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_INT64_CONVERSION(G_TYPE_CHAR, schar, -124)
  CHECK_INT64_CONVERSION(G_TYPE_CHAR, schar, 124)
  CHECK_INT64_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_INT64_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_INT64_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_INT64_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_INT64_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_INT64_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_INT64_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_INT64_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_INT64_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_INT64_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_INT64_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_INT64_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_UINT64_CONVERSION(type, getter, value)                    \
  g_assert (g_value_type_transformable (G_TYPE_UINT64, type));          \
  g_value_init (&src, G_TYPE_UINT64);                                   \
  g_value_init (&dest, type);                                           \
  g_value_set_uint64 (&src, value);                                     \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpuint (g_value_get_##getter (&dest), ==, value);           \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_UINT64_CONVERSION(G_TYPE_CHAR, schar, -124)
  CHECK_UINT64_CONVERSION(G_TYPE_CHAR, schar, 124)
  CHECK_UINT64_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_UINT64_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_UINT64_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_UINT64_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_UINT64_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_UINT64_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_UINT64_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_FLOAT_CONVERSION(type, getter, value)                    \
  g_assert (g_value_type_transformable (G_TYPE_FLOAT, type));          \
  g_value_init (&src, G_TYPE_FLOAT);                                   \
  g_value_init (&dest, type);                                          \
  g_value_set_float (&src, value);                                     \
  g_assert (g_value_transform (&src, &dest));                          \
  g_assert_cmpfloat (g_value_get_##getter (&dest), ==, value);         \
  g_value_unset (&src);                                                \
  g_value_unset (&dest);

  CHECK_FLOAT_CONVERSION(G_TYPE_CHAR, schar, -124)
  CHECK_FLOAT_CONVERSION(G_TYPE_CHAR, schar, 124)
  CHECK_FLOAT_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_FLOAT_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_FLOAT_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_FLOAT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_FLOAT_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_FLOAT_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_FLOAT_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_DOUBLE_CONVERSION(type, getter, value)                    \
  g_assert (g_value_type_transformable (G_TYPE_DOUBLE, type));          \
  g_value_init (&src, G_TYPE_DOUBLE);                                   \
  g_value_init (&dest, type);                                           \
  g_value_set_double (&src, value);                                     \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpfloat (g_value_get_##getter (&dest), ==, value);          \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_DOUBLE_CONVERSION(G_TYPE_CHAR, schar, -124)
  CHECK_DOUBLE_CONVERSION(G_TYPE_CHAR, schar, 124)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_DOUBLE_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_DOUBLE_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_DOUBLE_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_BOOLEAN_CONVERSION(type, setter, value)                   \
  g_assert (g_value_type_transformable (type, G_TYPE_BOOLEAN));         \
  g_value_init (&src, type);                                            \
  g_value_init (&dest, G_TYPE_BOOLEAN);                                 \
  g_value_set_##setter (&src, value);                                   \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_boolean (&dest), ==, TRUE);              \
  g_value_set_##setter (&src, 0);                                       \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_boolean (&dest), ==, FALSE);             \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_BOOLEAN_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_UINT64, uint64, 12345678)

#define CHECK_STRING_CONVERSION(int_type, setter, int_value)            \
  g_assert (g_value_type_transformable (int_type, G_TYPE_STRING));      \
  g_value_init (&src, int_type);                                        \
  g_value_init (&dest, G_TYPE_STRING);                                  \
  g_value_set_##setter (&src, int_value);                               \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpstr (g_value_get_string (&dest), ==, #int_value);         \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_STRING_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_STRING_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_STRING_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_STRING_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_STRING_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_STRING_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_STRING_CONVERSION(G_TYPE_FLOAT, float, 0.500000)
  CHECK_STRING_CONVERSION(G_TYPE_DOUBLE, double, -1.234567)

  g_assert (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_CHAR));
  g_value_init (&src, G_TYPE_STRING);
  g_value_init (&dest, G_TYPE_CHAR);
  g_value_set_static_string (&src, "bla");
  g_value_set_schar (&dest, 'c');
  g_assert (!g_value_transform (&src, &dest));
  g_assert_cmpint (g_value_get_schar (&dest), ==, 'c');
  g_value_unset (&src);
  g_value_unset (&dest);
}


/* We create some dummy objects with a simple relationship:
 *
 *           GObject
 *          /       \
 * TestObjectA     TestObjectC
 *      |
 * TestObjectB
 *
 * ie: TestObjectB is a subclass of TestObjectA and TestObjectC is
 * related to neither.
 */

static GType test_object_a_get_type (void);
typedef GObject TestObjectA; typedef GObjectClass TestObjectAClass;
G_DEFINE_TYPE (TestObjectA, test_object_a, G_TYPE_OBJECT)
static void test_object_a_class_init (TestObjectAClass *class) { }
static void test_object_a_init (TestObjectA *a) { }

static GType test_object_b_get_type (void);
typedef GObject TestObjectB; typedef GObjectClass TestObjectBClass;
G_DEFINE_TYPE (TestObjectB, test_object_b, test_object_a_get_type ())
static void test_object_b_class_init (TestObjectBClass *class) { }
static void test_object_b_init (TestObjectB *b) { }

static GType test_object_c_get_type (void);
typedef GObject TestObjectC; typedef GObjectClass TestObjectCClass;
G_DEFINE_TYPE (TestObjectC, test_object_c, G_TYPE_OBJECT)
static void test_object_c_class_init (TestObjectCClass *class) { }
static void test_object_c_init (TestObjectC *c) { }

/* We create an interface and programmatically populate it with
 * properties of each of the above type, with various flag combinations.
 *
 * Properties are named like "type-perm" where type is 'a', 'b' or 'c'
 * and perm is a series of characters, indicating the permissions:
 *
 *   - 'r': readable
 *   - 'w': writable
 *   - 'c': construct
 *   - 'C': construct-only
 *
 * It doesn't make sense to have a property that is neither readable nor
 * writable.  It is also not valid to have construct or construct-only
 * on read-only params.  Finally, it is invalid to have both construct
 * and construct-only specified, so we do not consider those cases.
 * That gives us 7 possible permissions:
 *
 *     'r', 'w', 'rw', 'wc', 'rwc', 'wC', 'rwC'
 *
 * And 9 impossible ones:
 *
 *     '', 'c', 'rc', 'C', 'rC', 'cC', 'rcC', 'wcC', rwcC'
 *
 * For a total of 16 combinations.
 *
 * That gives a total of 48 (16 * 3) possible flag/type combinations, of
 * which 27 (9 * 3) are impossible to install.
 *
 * That gives 21 (7 * 3) properties that will be installed.
 */
typedef GTypeInterface TestInterfaceInterface;
static GType test_interface_get_type (void);
//typedef struct _TestInterface TestInterface;
G_DEFINE_INTERFACE (TestInterface, test_interface, G_TYPE_OBJECT)
static void
test_interface_default_init (TestInterfaceInterface *iface)
{
  const gchar *names[] = { "a", "b", "c" };
  const gchar *perms[] = { NULL, "r",  "w",  "rw",
                           NULL, NULL, "wc", "rwc",
                           NULL, NULL, "wC", "rwC",
                           NULL, NULL, NULL, NULL };
  const GType types[] = { test_object_a_get_type (), test_object_b_get_type (), test_object_c_get_type () };
  guint i, j;

  for (i = 0; i < G_N_ELEMENTS (types); i++)
    for (j = 0; j < G_N_ELEMENTS (perms); j++)
      {
        gchar prop_name[10];
        GParamSpec *pspec;

        if (perms[j] == NULL)
          {
            if (!g_test_undefined ())
              continue;

            /* we think that this is impossible.  make sure. */
            pspec = g_param_spec_object ("xyz", "xyz", "xyz", types[i], j);

            g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                                   "*assertion*pspec->flags*failed*");
            g_object_interface_install_property (iface, pspec);
            g_test_assert_expected_messages ();

            g_param_spec_unref (pspec);
            continue;
          }

        /* install the property */
        g_snprintf (prop_name, sizeof prop_name, "%s-%s", names[i], perms[j]);
        pspec = g_param_spec_object (prop_name, prop_name, prop_name, types[i], j);
        g_object_interface_install_property (iface, pspec);
      }
}

/* We now have 21 properties.  Each property may be correctly
 * implemented with the following types:
 *
 *   Properties         Valid Types       Reason
 *
 *   a-r                a, b              Read only can provide subclasses
 *   a-w, wc, wC        a, GObject        Write only can accept superclasses
 *   a-rw, rwc, rwC     a                 Read-write must be exactly equal
 *
 *   b-r                b                 (as above)
 *   b-w, wc, wC        b, a, GObject
 *   b-rw, rwc, rwC     b
 *
 *   c-r                c                 (as above)
 *   c-wo, wc, wC       c, GObject
 *   c-rw, rwc, rwC     c
 *
 * We can express this in a 48-by-4 table where each row represents an
 * installed property and each column represents a type.  The value in
 * the table represents if it is valid to subclass the row's property
 * with the type of the column:
 *
 *   - 0:   invalid because the interface property doesn't exist (invalid flags)
 *   - 'v': valid
 *   - '=': invalid because of the type not being exactly equal
 *   - '<': invalid because of the type not being a subclass
 *   - '>': invalid because of the type not being a superclass
 *
 * We organise the table by interface property type ('a', 'b', 'c') then
 * by interface property flags.
 */

static gint valid_impl_types[48][4] = {
                    /* a    b    c    GObject */
    /* 'a-' */       { 0, },
    /* 'a-r' */      { 'v', 'v', '<', '<' },
    /* 'a-w' */      { 'v', '>', '>', 'v' },
    /* 'a-rw' */     { 'v', '=', '=', '=' },
    /* 'a-c */       { 0, },
    /* 'a-rc' */     { 0, },
    /* 'a-wc' */     { 'v', '>', '>', 'v' },
    /* 'a-rwc' */    { 'v', '=', '=', '=' },
    /* 'a-C */       { 0, },
    /* 'a-rC' */     { 0, },
    /* 'a-wC' */     { 'v', '>', '>', 'v' },
    /* 'a-rwC' */    { 'v', '=', '=', '=' },
    /* 'a-cC */      { 0, },
    /* 'a-rcC' */    { 0, },
    /* 'a-wcC' */    { 0, },
    /* 'a-rwcC' */   { 0, },

    /* 'b-' */       { 0, },
    /* 'b-r' */      { '<', 'v', '<', '<' },
    /* 'b-w' */      { 'v', 'v', '>', 'v' },
    /* 'b-rw' */     { '=', 'v', '=', '=' },
    /* 'b-c */       { 0, },
    /* 'b-rc' */     { 0, },
    /* 'b-wc' */     { 'v', 'v', '>', 'v' },
    /* 'b-rwc' */    { '=', 'v', '=', '=' },
    /* 'b-C */       { 0, },
    /* 'b-rC' */     { 0, },
    /* 'b-wC' */     { 'v', 'v', '>', 'v' },
    /* 'b-rwC' */    { '=', 'v', '=', '=' },
    /* 'b-cC */      { 0, },
    /* 'b-rcC' */    { 0, },
    /* 'b-wcC' */    { 0, },
    /* 'b-rwcC' */   { 0, },

    /* 'c-' */       { 0, },
    /* 'c-r' */      { '<', '<', 'v', '<' },
    /* 'c-w' */      { '>', '>', 'v', 'v' },
    /* 'c-rw' */     { '=', '=', 'v', '=' },
    /* 'c-c */       { 0, },
    /* 'c-rc' */     { 0, },
    /* 'c-wc' */     { '>', '>', 'v', 'v' },
    /* 'c-rwc' */    { '=', '=', 'v', '=' },
    /* 'c-C */       { 0, },
    /* 'c-rC' */     { 0, },
    /* 'c-wC' */     { '>', '>', 'v', 'v' },
    /* 'c-rwC' */    { '=', '=', 'v', '=' },
    /* 'c-cC */      { 0, },
    /* 'c-rcC' */    { 0, },
    /* 'c-wcC' */    { 0, },
    /* 'c-rwcC' */   { 0, }
};

/* We also try to change the flags.  We must ensure that all
 * implementations provide all functionality promised by the interface.
 * We must therefore never remove readability or writability (but we can
 * add them).  Construct-only is a restrictions that applies to
 * writability, so we can never add it unless writability was never
 * present in the first place, in which case "writable at construct
 * only" is still better than "not writable".
 *
 * The 'construct' flag is of interest only to the implementation.  It
 * may be changed at any time.
 *
 *   Properties         Valid Access      Reason
 *
 *   *-r                r, rw, rwc, rwC   Must keep readable, but can restrict newly-added writable
 *   *-w                w, rw, rwc        Must keep writable unrestricted
 *   *-rw               rw, rwc           Must not add any restrictions
 *   *-rwc              rw, rwc           Must not add any restrictions
 *   *-rwC              rw, rwc, rwC      Can remove 'construct-only' restriction
 *   *-wc               rwc, rw, w, wc    Can add readability
 *   *-wC               rwC, rw, w, wC    Can add readability or remove 'construct only' restriction
 *                        rwc, wc
 *
 * We can represent this with a 16-by-16 table.  The rows represent the
 * flags of the property on the interface.  The columns is the flags to
 * try to use when overriding the property.  The cell contents are:
 *
 *   - 0:   invalid because the interface property doesn't exist (invalid flags)
 *   - 'v': valid
 *   - 'i': invalid because the implementation flags are invalid
 *   - 'f': invalid because of the removal of functionality
 *   - 'r': invalid because of the addition of restrictions (ie: construct-only)
 *
 * We also ensure that removal of functionality is reported before
 * addition of restrictions, since this is a more basic problem.
 */
static gint valid_impl_flags[16][16] = {
                 /* ''   r    w    rw   c    rc   wc   rwc  C    rC   wC   rwC  cC   rcC  wcC  rwcC */
    /* '*-' */    { 0, },
    /* '*-r' */   { 'i', 'v', 'f', 'v', 'i', 'i', 'f', 'v', 'i', 'i', 'f', 'v', 'i', 'i', 'i', 'i' },
    /* '*-w' */   { 'i', 'f', 'v', 'v', 'i', 'i', 'v', 'v', 'i', 'i', 'r', 'r', 'i', 'i', 'i', 'i' },
    /* '*-rw' */  { 'i', 'f', 'f', 'v', 'i', 'i', 'f', 'v', 'i', 'i', 'f', 'r', 'i', 'i', 'i', 'i' },
    /* '*-c */    { 0, },
    /* '*-rc' */  { 0, },
    /* '*-wc' */  { 'i', 'f', 'v', 'v', 'i', 'i', 'v', 'v', 'i', 'i', 'r', 'r', 'i', 'i', 'i', 'i' },
    /* '*-rwc' */ { 'i', 'f', 'f', 'v', 'i', 'i', 'f', 'v', 'i', 'i', 'f', 'r', 'i', 'i', 'i', 'i' },
    /* '*-C */    { 0, },
    /* '*-rC' */  { 0, },
    /* '*-wC' */  { 'i', 'f', 'v', 'v', 'i', 'i', 'v', 'v', 'i', 'i', 'v', 'v', 'i', 'i', 'i', 'i' },
    /* '*-rwC' */ { 'i', 'f', 'f', 'v', 'i', 'i', 'f', 'v', 'i', 'i', 'f', 'v', 'i', 'i', 'i', 'i' },
};

static guint change_this_flag;
static guint change_this_type;
static guint use_this_flag;
static guint use_this_type;

typedef GObjectClass TestImplementationClass;
typedef GObject TestImplementation;

static void test_implementation_init (TestImplementation *impl) { }
static void test_implementation_iface_init (TestInterfaceInterface *iface) { }

static GType test_implementation_get_type (void);
G_DEFINE_TYPE_WITH_CODE (TestImplementation, test_implementation, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (test_interface_get_type (), test_implementation_iface_init))

static void test_implementation_class_init (TestImplementationClass *class)
{
  const gchar *names[] = { "a", "b", "c" };
  const gchar *perms[] = { NULL, "r",  "w",  "rw",
                           NULL, NULL, "wc", "rwc",
                           NULL, NULL, "wC", "rwC",
                           NULL, NULL, NULL, NULL };
  const GType types[] = { test_object_a_get_type (), test_object_b_get_type (),
                          test_object_c_get_type (), G_TYPE_OBJECT };
  gchar prop_name[10];
  GParamSpec *pspec;
  guint i, j;

  class->get_property = GINT_TO_POINTER (1);
  class->set_property = GINT_TO_POINTER (1);

  /* Install all of the non-modified properties or else GObject will
   * complain about non-implemented properties.
   */
  for (i = 0; i < 3; i++)
    for (j = 0; j < G_N_ELEMENTS (perms); j++)
      {
        if (i == change_this_type && j == change_this_flag)
          continue;

        if (perms[j] != NULL)
          {
            /* override the property without making changes */
            g_snprintf (prop_name, sizeof prop_name, "%s-%s", names[i], perms[j]);
            g_object_class_override_property (class, 1, prop_name);
          }
      }

  /* Now try installing our modified property */
  if (perms[change_this_flag] == NULL)
    g_error ("Interface property does not exist");

  g_snprintf (prop_name, sizeof prop_name, "%s-%s", names[change_this_type], perms[change_this_flag]);
  pspec = g_param_spec_object (prop_name, prop_name, prop_name, types[use_this_type], use_this_flag);
  g_object_class_install_property (class, 1, pspec);
}

typedef struct {
  gint change_this_flag;
  gint change_this_type;
  gint use_this_flag;
  gint use_this_type;
} TestParamImplementData;

static void
test_param_implement_child (gconstpointer user_data)
{
  TestParamImplementData *data = (gpointer) user_data;

  /* GObject oddity: GObjectClass must be initialised before we can
   * initialise a GTypeInterface.
   */
  g_type_class_ref (G_TYPE_OBJECT);

  /* Bring up the interface first. */
  g_type_default_interface_ref (test_interface_get_type ());

  /* Copy the flags into the global vars so
   * test_implementation_class_init() will see them.
   */
  change_this_flag = data->change_this_flag;
  change_this_type = data->change_this_type;
  use_this_flag = data->use_this_flag;
  use_this_type = data->use_this_type;

  g_type_class_ref (test_implementation_get_type ());
}

static void
test_param_implement (void)
{
  gchar *test_path;

  for (change_this_flag = 0; change_this_flag < 16; change_this_flag++)
    for (change_this_type = 0; change_this_type < 3; change_this_type++)
      for (use_this_flag = 0; use_this_flag < 16; use_this_flag++)
        for (use_this_type = 0; use_this_type < 4; use_this_type++)
          {
            if (!g_test_undefined ())
              {
                /* only test the valid (defined) cases, e.g. under valgrind */
                if (valid_impl_flags[change_this_flag][use_this_flag] != 'v')
                  continue;

                if (valid_impl_types[change_this_type * 16 + change_this_flag][use_this_type] != 'v')
                  continue;
              }

            test_path = g_strdup_printf ("/param/implement/subprocess/%d-%d-%d-%d",
                                         change_this_flag, change_this_type,
                                         use_this_flag, use_this_type);
            g_test_trap_subprocess (test_path, G_TIME_SPAN_SECOND, 0);
            g_free (test_path);

            /* We want to ensure that any flags mismatch problems are reported first. */
            switch (valid_impl_flags[change_this_flag][use_this_flag])
              {
              case 0:
                /* make sure the other table agrees */
                g_assert (valid_impl_types[change_this_type * 16 + change_this_flag][use_this_type] == 0);
                g_test_trap_assert_failed ();
                g_test_trap_assert_stderr ("*Interface property does not exist*");
                continue;

              case 'i':
                g_test_trap_assert_failed ();
                g_test_trap_assert_stderr ("*g_object_class_install_property*");
                continue;

              case 'f':
                g_test_trap_assert_failed ();
                g_test_trap_assert_stderr ("*remove functionality*");
                continue;

              case 'r':
                g_test_trap_assert_failed ();
                g_test_trap_assert_stderr ("*introduce additional restrictions*");
                continue;

              case 'v':
                break;
              }

            /* Next, we check if there should have been a type error. */
            switch (valid_impl_types[change_this_type * 16 + change_this_flag][use_this_type])
              {
              case 0:
                /* this should have been caught above */
                g_assert_not_reached ();

              case '=':
                g_test_trap_assert_failed ();
                g_test_trap_assert_stderr ("*exactly equal*");
                continue;

              case '<':
                g_test_trap_assert_failed ();
                g_test_trap_assert_stderr ("*equal to or more restrictive*");
                continue;

              case '>':
                g_test_trap_assert_failed ();
                g_test_trap_assert_stderr ("*equal to or less restrictive*");
                continue;

              case 'v':
                break;
              }

            g_test_trap_assert_passed ();
          }
}

static void
test_param_default (void)
{
  GParamSpec *param;
  const GValue *def;

  param = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);
  def = g_param_spec_get_default_value (param);

  g_assert (G_VALUE_HOLDS (def, G_TYPE_INT));
  g_assert_cmpint (g_value_get_int (def), ==, 10);

  g_param_spec_unref (param);
}

int
main (int argc, char *argv[])
{
  TestParamImplementData data, *test_data;
  gchar *test_path;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/param/value", test_param_value);
  g_test_add_func ("/param/strings", test_param_strings);
  g_test_add_func ("/param/qdata", test_param_qdata);
  g_test_add_func ("/param/validate", test_param_validate);
  g_test_add_func ("/param/convert", test_param_convert);

  if (g_test_slow ())
    g_test_add_func ("/param/implement", test_param_implement);

  for (data.change_this_flag = 0; data.change_this_flag < 16; data.change_this_flag++)
    for (data.change_this_type = 0; data.change_this_type < 3; data.change_this_type++)
      for (data.use_this_flag = 0; data.use_this_flag < 16; data.use_this_flag++)
        for (data.use_this_type = 0; data.use_this_type < 4; data.use_this_type++)
          {
            test_path = g_strdup_printf ("/param/implement/subprocess/%d-%d-%d-%d",
                                         data.change_this_flag, data.change_this_type,
                                         data.use_this_flag, data.use_this_type);
            test_data = g_memdup (&data, sizeof (TestParamImplementData));
            g_test_add_data_func_full (test_path, test_data, test_param_implement_child, g_free);
            g_free (test_path);
          }

  g_test_add_func ("/value/transform", test_value_transform);
  g_test_add_func ("/param/default", test_param_default);

  return g_test_run ();
}
