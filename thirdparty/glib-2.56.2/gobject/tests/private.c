#include <glib-object.h>

typedef struct {
  GObject parent_instance;
} TestObject;

typedef struct {
  int dummy_0;
  float dummy_1;
} TestObjectPrivate;

typedef struct {
  GObjectClass parent_class;
} TestObjectClass;

GType test_object_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TestObject, test_object, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (TestObject))

static void
test_object_class_init (TestObjectClass *klass)
{
}

static void
test_object_init (TestObject *self)
{
  TestObjectPrivate *priv = test_object_get_instance_private (self);

  if (g_test_verbose ())
    g_printerr ("Offset of %sPrivate for type '%s': %d\n",
             G_OBJECT_TYPE_NAME (self),
             G_OBJECT_TYPE_NAME (self),
             TestObject_private_offset);

  priv->dummy_0 = 42;
  priv->dummy_1 = 3.14159f;
}

static int
test_object_get_dummy_0 (TestObject *self)
{
  TestObjectPrivate *priv = test_object_get_instance_private (self);

  return priv->dummy_0;
}

static float
test_object_get_dummy_1 (TestObject *self)
{
  TestObjectPrivate *priv = test_object_get_instance_private (self);

  return priv->dummy_1;
}

typedef struct {
  TestObject parent_instance;
} TestDerived;

typedef struct {
  char *dummy_2;
} TestDerivedPrivate;

typedef struct {
  TestObjectClass parent_class;
} TestDerivedClass;

GType test_derived_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TestDerived, test_derived, test_object_get_type (),
                         G_ADD_PRIVATE (TestDerived))

static void
test_derived_finalize (GObject *obj)
{
  TestDerivedPrivate *priv = test_derived_get_instance_private ((TestDerived *) obj);

  g_free (priv->dummy_2);

  G_OBJECT_CLASS (test_derived_parent_class)->finalize (obj);
}

static void
test_derived_class_init (TestDerivedClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = test_derived_finalize;
}

static void
test_derived_init (TestDerived *self)
{
  TestDerivedPrivate *priv = test_derived_get_instance_private (self);

  if (g_test_verbose ())
    g_printerr ("Offset of %sPrivate for type '%s': %d\n",
             G_OBJECT_TYPE_NAME (self),
             G_OBJECT_TYPE_NAME (self),
             TestDerived_private_offset);

  priv->dummy_2 = g_strdup ("Hello");
}

static const char *
test_derived_get_dummy_2 (TestDerived *self)
{
  TestDerivedPrivate *priv = test_derived_get_instance_private (self);

  return priv->dummy_2;
}

typedef struct {
  TestObject parent_instance;
} TestMixed;

typedef struct {
  gint dummy_3;
} TestMixedPrivate;

typedef struct {
  TestObjectClass parent_class;
} TestMixedClass;

GType test_mixed_get_type (void);

G_DEFINE_TYPE (TestMixed, test_mixed, test_object_get_type ())

static void
test_mixed_class_init (TestMixedClass *klass)
{
  g_type_class_add_private (klass, sizeof (TestMixedPrivate));
}

static void
test_mixed_init (TestMixed *self)
{
  TestMixedPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, test_mixed_get_type (), TestMixedPrivate);

  if (g_test_verbose ())
    g_printerr ("Offset of %sPrivate for type '%s': %d\n",
             G_OBJECT_TYPE_NAME (self),
             G_OBJECT_TYPE_NAME (self),
             TestMixed_private_offset);

  priv->dummy_3 = 47;
}

static gint
test_mixed_get_dummy_3 (TestMixed *self)
{
  TestMixedPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, test_mixed_get_type (), TestMixedPrivate);

  return priv->dummy_3;
}

typedef struct {
  TestMixed parent_instance;
} TestMixedDerived;

typedef struct {
  gint64 dummy_4;
} TestMixedDerivedPrivate;

typedef struct {
  TestMixedClass parent_class;
} TestMixedDerivedClass;

GType test_mixed_derived_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TestMixedDerived, test_mixed_derived, test_mixed_get_type (),
                         G_ADD_PRIVATE (TestMixedDerived))

static void
test_mixed_derived_class_init (TestMixedDerivedClass *klass)
{
}

static void
test_mixed_derived_init (TestMixedDerived *self)
{
  TestMixedDerivedPrivate *priv = test_mixed_derived_get_instance_private (self);

  if (g_test_verbose ())
    g_printerr ("Offset of %sPrivate for type '%s': %d\n",
             G_OBJECT_TYPE_NAME (self),
             G_OBJECT_TYPE_NAME (self),
             TestMixedDerived_private_offset);

  priv->dummy_4 = g_get_monotonic_time ();
}

static gint64
test_mixed_derived_get_dummy_4 (TestMixedDerived *self)
{
  TestMixedDerivedPrivate *priv = test_mixed_derived_get_instance_private (self);

  return priv->dummy_4;
}

static void
private_instance (void)
{
  TestObject *obj = g_object_new (test_object_get_type (), NULL);
  gpointer class;
  gint offset;

  g_assert_cmpint (test_object_get_dummy_0 (obj), ==, 42);
  g_assert_cmpfloat (test_object_get_dummy_1 (obj), ==, 3.14159f);

  class = g_type_class_ref (test_object_get_type ());
  offset = g_type_class_get_instance_private_offset (class);
  g_type_class_unref (class);

  g_assert (offset == TestObject_private_offset);

  g_object_unref (obj);
}

static void
private_derived_instance (void)
{
  TestDerived *obj = g_object_new (test_derived_get_type (), NULL);

  g_assert_cmpstr (test_derived_get_dummy_2 (obj), ==, "Hello");
  g_assert_cmpint (test_object_get_dummy_0 ((TestObject *) obj), ==, 42);

  g_object_unref (obj);
}

static void
private_mixed_derived_instance (void)
{
  TestMixedDerived *derived = g_object_new (test_mixed_derived_get_type (), NULL);
  TestMixed *mixed = g_object_new (test_mixed_get_type (), NULL);

  g_assert_cmpint (test_mixed_get_dummy_3 (mixed), ==, 47);
  g_assert (test_mixed_derived_get_dummy_4 (derived) <= g_get_monotonic_time ());

  g_object_unref (derived);
  g_object_unref (mixed);
}

int
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/private/instance", private_instance);
  g_test_add_func ("/private/derived-instance", private_derived_instance);
  g_test_add_func ("/private/mixed-derived-instance", private_mixed_derived_instance);

  return g_test_run ();
}
