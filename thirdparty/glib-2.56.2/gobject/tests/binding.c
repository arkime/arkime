#include <stdlib.h>
#include <gstdio.h>
#include <glib-object.h>

typedef struct _BindingSource
{
  GObject parent_instance;

  gint foo;
  gint bar;
  gdouble value;
  gboolean toggle;
} BindingSource;

typedef struct _BindingSourceClass
{
  GObjectClass parent_class;
} BindingSourceClass;

enum
{
  PROP_SOURCE_0,

  PROP_SOURCE_FOO,
  PROP_SOURCE_BAR,
  PROP_SOURCE_VALUE,
  PROP_SOURCE_TOGGLE
};

static GType binding_source_get_type (void);
G_DEFINE_TYPE (BindingSource, binding_source, G_TYPE_OBJECT)

static void
binding_source_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BindingSource *source = (BindingSource *) gobject;

  switch (prop_id)
    {
    case PROP_SOURCE_FOO:
      source->foo = g_value_get_int (value);
      break;

    case PROP_SOURCE_BAR:
      source->bar = g_value_get_int (value);
      break;

    case PROP_SOURCE_VALUE:
      source->value = g_value_get_double (value);
      break;

    case PROP_SOURCE_TOGGLE:
      source->toggle = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_source_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BindingSource *source = (BindingSource *) gobject;

  switch (prop_id)
    {
    case PROP_SOURCE_FOO:
      g_value_set_int (value, source->foo);
      break;

    case PROP_SOURCE_BAR:
      g_value_set_int (value, source->bar);
      break;

    case PROP_SOURCE_VALUE:
      g_value_set_double (value, source->value);
      break;

    case PROP_SOURCE_TOGGLE:
      g_value_set_boolean (value, source->toggle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_source_class_init (BindingSourceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = binding_source_set_property;
  gobject_class->get_property = binding_source_get_property;

  g_object_class_install_property (gobject_class, PROP_SOURCE_FOO,
                                   g_param_spec_int ("foo", "Foo", "Foo",
                                                     -1, 100,
                                                     0,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SOURCE_BAR,
                                   g_param_spec_int ("bar", "Bar", "Bar",
                                                     -1, 100,
                                                     0,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SOURCE_VALUE,
                                   g_param_spec_double ("value", "Value", "Value",
                                                        -100.0, 200.0,
                                                        0.0,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SOURCE_TOGGLE,
                                   g_param_spec_boolean ("toggle", "Toggle", "Toggle",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
binding_source_init (BindingSource *self)
{
}

typedef struct _BindingTarget
{
  GObject parent_instance;

  gint bar;
  gdouble value;
  gboolean toggle;
} BindingTarget;

typedef struct _BindingTargetClass
{
  GObjectClass parent_class;
} BindingTargetClass;

enum
{
  PROP_TARGET_0,

  PROP_TARGET_BAR,
  PROP_TARGET_VALUE,
  PROP_TARGET_TOGGLE
};

static GType binding_target_get_type (void);
G_DEFINE_TYPE (BindingTarget, binding_target, G_TYPE_OBJECT)

static void
binding_target_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BindingTarget *target = (BindingTarget *) gobject;

  switch (prop_id)
    {
    case PROP_TARGET_BAR:
      target->bar = g_value_get_int (value);
      break;

    case PROP_TARGET_VALUE:
      target->value = g_value_get_double (value);
      break;

    case PROP_TARGET_TOGGLE:
      target->toggle = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_target_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BindingTarget *target = (BindingTarget *) gobject;

  switch (prop_id)
    {
    case PROP_TARGET_BAR:
      g_value_set_int (value, target->bar);
      break;

    case PROP_TARGET_VALUE:
      g_value_set_double (value, target->value);
      break;

    case PROP_TARGET_TOGGLE:
      g_value_set_boolean (value, target->toggle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
binding_target_class_init (BindingTargetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = binding_target_set_property;
  gobject_class->get_property = binding_target_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET_BAR,
                                   g_param_spec_int ("bar", "Bar", "Bar",
                                                     -1, 100,
                                                     0,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_TARGET_VALUE,
                                   g_param_spec_double ("value", "Value", "Value",
                                                        -100.0, 200.0,
                                                        0.0,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_TARGET_TOGGLE,
                                   g_param_spec_boolean ("toggle", "Toggle", "Toggle",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
binding_target_init (BindingTarget *self)
{
}

static gboolean
celsius_to_fahrenheit (GBinding     *binding,
                       const GValue *from_value,
                       GValue       *to_value,
                       gpointer      user_data G_GNUC_UNUSED)
{
  gdouble celsius, fahrenheit;

  g_assert (G_VALUE_HOLDS (from_value, G_TYPE_DOUBLE));
  g_assert (G_VALUE_HOLDS (to_value, G_TYPE_DOUBLE));

  celsius = g_value_get_double (from_value);
  fahrenheit = (9 * celsius / 5) + 32.0;

  if (g_test_verbose ())
    g_printerr ("Converting %.2fC to %.2fF\n", celsius, fahrenheit);

  g_value_set_double (to_value, fahrenheit);

  return TRUE;
}

static gboolean
fahrenheit_to_celsius (GBinding     *binding,
                       const GValue *from_value,
                       GValue       *to_value,
                       gpointer      user_data G_GNUC_UNUSED)
{
  gdouble celsius, fahrenheit;

  g_assert (G_VALUE_HOLDS (from_value, G_TYPE_DOUBLE));
  g_assert (G_VALUE_HOLDS (to_value, G_TYPE_DOUBLE));

  fahrenheit = g_value_get_double (from_value);
  celsius = 5 * (fahrenheit - 32.0) / 9;

  if (g_test_verbose ())
    g_printerr ("Converting %.2fF to %.2fC\n", fahrenheit, celsius);

  g_value_set_double (to_value, celsius);

  return TRUE;
}

static void
binding_default (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (), NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (), NULL);
  GBinding *binding;

  binding = g_object_bind_property (source, "foo",
                                    target, "bar",
                                    G_BINDING_DEFAULT);

  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);
  g_assert ((BindingSource *) g_binding_get_source (binding) == source);
  g_assert ((BindingTarget *) g_binding_get_target (binding) == target);
  g_assert_cmpstr (g_binding_get_source_property (binding), ==, "foo");
  g_assert_cmpstr (g_binding_get_target_property (binding), ==, "bar");
  g_assert_cmpint (g_binding_get_flags (binding), ==, G_BINDING_DEFAULT);

  g_object_set (source, "foo", 42, NULL);
  g_assert_cmpint (source->foo, ==, target->bar);

  g_object_set (target, "bar", 47, NULL);
  g_assert_cmpint (source->foo, !=, target->bar);

  g_object_unref (binding);

  g_object_set (source, "foo", 0, NULL);
  g_assert_cmpint (source->foo, !=, target->bar);

  g_object_unref (source);
  g_object_unref (target);
  g_assert (binding == NULL);
}

static void
binding_bidirectional (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (), NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (), NULL);
  GBinding *binding;

  binding = g_object_bind_property (source, "foo",
                                    target, "bar",
                                    G_BINDING_BIDIRECTIONAL);
  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);

  g_object_set (source, "foo", 42, NULL);
  g_assert_cmpint (source->foo, ==, target->bar);

  g_object_set (target, "bar", 47, NULL);
  g_assert_cmpint (source->foo, ==, target->bar);

  g_object_unref (binding);

  g_object_set (source, "foo", 0, NULL);
  g_assert_cmpint (source->foo, !=, target->bar);

  g_object_unref (source);
  g_object_unref (target);
  g_assert (binding == NULL);
}

static void
data_free (gpointer data)
{
  gboolean *b = data;

  *b = TRUE;
}

static void
binding_transform_default (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (), NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (), NULL);
  GBinding *binding;
  gpointer src, trg;
  gchar *src_prop, *trg_prop;
  GBindingFlags flags;

  binding = g_object_bind_property (source, "foo",
                                    target, "value",
                                    G_BINDING_BIDIRECTIONAL);

  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);

  g_object_get (binding,
                "source", &src,
                "source-property", &src_prop,
                "target", &trg,
                "target-property", &trg_prop,
                "flags", &flags,
                NULL);
  g_assert (src == source);
  g_assert (trg == target);
  g_assert_cmpstr (src_prop, ==, "foo");
  g_assert_cmpstr (trg_prop, ==, "value");
  g_assert_cmpint (flags, ==, G_BINDING_BIDIRECTIONAL);
  g_object_unref (src);
  g_object_unref (trg);
  g_free (src_prop);
  g_free (trg_prop);

  g_object_set (source, "foo", 24, NULL);
  g_assert_cmpfloat (target->value, ==, 24.0);

  g_object_set (target, "value", 69.0, NULL);
  g_assert_cmpint (source->foo, ==, 69);

  g_object_unref (target);
  g_object_unref (source);
  g_assert (binding == NULL);
}

static void
binding_transform (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (), NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (), NULL);
  GBinding *binding G_GNUC_UNUSED;
  gboolean unused_data = FALSE;

  binding = g_object_bind_property_full (source, "value",
                                         target, "value",
                                         G_BINDING_BIDIRECTIONAL,
                                         celsius_to_fahrenheit,
                                         fahrenheit_to_celsius,
                                         &unused_data, data_free);

  g_object_set (source, "value", 24.0, NULL);
  g_assert_cmpfloat (target->value, ==, ((9 * 24.0 / 5) + 32.0));

  g_object_set (target, "value", 69.0, NULL);
  g_assert_cmpfloat (source->value, ==, (5 * (69.0 - 32.0) / 9));

  g_object_unref (source);
  g_object_unref (target);

  g_assert (unused_data);
}

static void
binding_transform_closure (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (), NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (), NULL);
  GBinding *binding G_GNUC_UNUSED;
  gboolean unused_data_1 = FALSE, unused_data_2 = FALSE;
  GClosure *c2f_clos, *f2c_clos;

  c2f_clos = g_cclosure_new (G_CALLBACK (celsius_to_fahrenheit), &unused_data_1, (GClosureNotify) data_free);

  f2c_clos = g_cclosure_new (G_CALLBACK (fahrenheit_to_celsius), &unused_data_2, (GClosureNotify) data_free);

  binding = g_object_bind_property_with_closures (source, "value",
                                                  target, "value",
                                                  G_BINDING_BIDIRECTIONAL,
                                                  c2f_clos,
                                                  f2c_clos);

  g_object_set (source, "value", 24.0, NULL);
  g_assert_cmpfloat (target->value, ==, ((9 * 24.0 / 5) + 32.0));

  g_object_set (target, "value", 69.0, NULL);
  g_assert_cmpfloat (source->value, ==, (5 * (69.0 - 32.0) / 9));

  g_object_unref (source);
  g_object_unref (target);

  g_assert (unused_data_1);
  g_assert (unused_data_2);
}

static void
binding_chain (void)
{
  BindingSource *a = g_object_new (binding_source_get_type (), NULL);
  BindingSource *b = g_object_new (binding_source_get_type (), NULL);
  BindingSource *c = g_object_new (binding_source_get_type (), NULL);
  GBinding *binding_1, *binding_2;

  g_test_bug ("621782");

  /* A -> B, B -> C */
  binding_1 = g_object_bind_property (a, "foo", b, "foo", G_BINDING_BIDIRECTIONAL);
  g_object_add_weak_pointer (G_OBJECT (binding_1), (gpointer *) &binding_1);

  binding_2 = g_object_bind_property (b, "foo", c, "foo", G_BINDING_BIDIRECTIONAL);
  g_object_add_weak_pointer (G_OBJECT (binding_2), (gpointer *) &binding_2);

  /* verify the chain */
  g_object_set (a, "foo", 42, NULL);
  g_assert_cmpint (a->foo, ==, b->foo);
  g_assert_cmpint (b->foo, ==, c->foo);

  /* unbind A -> B and B -> C */
  g_object_unref (binding_1);
  g_assert (binding_1 == NULL);
  g_object_unref (binding_2);
  g_assert (binding_2 == NULL);

  /* bind A -> C directly */
  binding_2 = g_object_bind_property (a, "foo", c, "foo", G_BINDING_BIDIRECTIONAL);

  /* verify the chain is broken */
  g_object_set (a, "foo", 47, NULL);
  g_assert_cmpint (a->foo, !=, b->foo);
  g_assert_cmpint (a->foo, ==, c->foo);

  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
}

static void
binding_sync_create (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (),
                                        "foo", 42,
                                        NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (),
                                        "bar", 47,
                                        NULL);
  GBinding *binding;

  binding = g_object_bind_property (source, "foo",
                                    target, "bar",
                                    G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_assert_cmpint (source->foo, ==, 42);
  g_assert_cmpint (target->bar, ==, 42);

  g_object_set (source, "foo", 47, NULL);
  g_assert_cmpint (source->foo, ==, target->bar);

  g_object_unref (binding);

  g_object_set (target, "bar", 49, NULL);

  binding = g_object_bind_property (source, "foo",
                                    target, "bar",
                                    G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_assert_cmpint (source->foo, ==, 47);
  g_assert_cmpint (target->bar, ==, 47);

  g_object_unref (source);
  g_object_unref (target);
}

static void
binding_invert_boolean (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (),
                                        "toggle", TRUE,
                                        NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (),
                                        "toggle", FALSE,
                                        NULL);
  GBinding *binding;

  binding = g_object_bind_property (source, "toggle",
                                    target, "toggle",
                                    G_BINDING_BIDIRECTIONAL | G_BINDING_INVERT_BOOLEAN);

  g_assert (source->toggle);
  g_assert (!target->toggle);

  g_object_set (source, "toggle", FALSE, NULL);
  g_assert (!source->toggle);
  g_assert (target->toggle);

  g_object_set (target, "toggle", FALSE, NULL);
  g_assert (source->toggle);
  g_assert (!target->toggle);

  g_object_unref (binding);
  g_object_unref (source);
  g_object_unref (target);
}

static void
binding_same_object (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (),
                                        "foo", 100,
                                        "bar", 50,
                                        NULL);
  GBinding *binding;

  binding = g_object_bind_property (source, "foo",
                                    source, "bar",
                                    G_BINDING_BIDIRECTIONAL);
  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);

  g_object_set (source, "foo", 10, NULL);
  g_assert_cmpint (source->foo, ==, 10);
  g_assert_cmpint (source->bar, ==, 10);
  g_object_set (source, "bar", 30, NULL);
  g_assert_cmpint (source->foo, ==, 30);
  g_assert_cmpint (source->bar, ==, 30);

  g_object_unref (source);
  g_assert (binding == NULL);
}

static void
binding_unbind (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (), NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (), NULL);
  GBinding *binding;

  binding = g_object_bind_property (source, "foo",
                                    target, "bar",
                                    G_BINDING_DEFAULT);
  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);

  g_object_set (source, "foo", 42, NULL);
  g_assert_cmpint (source->foo, ==, target->bar);

  g_object_set (target, "bar", 47, NULL);
  g_assert_cmpint (source->foo, !=, target->bar);

  g_binding_unbind (binding);
  g_assert (binding == NULL);

  g_object_set (source, "foo", 0, NULL);
  g_assert_cmpint (source->foo, !=, target->bar);

  g_object_unref (source);
  g_object_unref (target);


  /* g_binding_unbind() has a special case for this */
  source = g_object_new (binding_source_get_type (), NULL);
  binding = g_object_bind_property (source, "foo",
                                    source, "bar",
                                    G_BINDING_DEFAULT);
  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);

  g_binding_unbind (binding);
  g_assert (binding == NULL);

  g_object_unref (source);
}

static void
binding_fail (void)
{
  BindingSource *source = g_object_new (binding_source_get_type (), NULL);
  BindingTarget *target = g_object_new (binding_target_get_type (), NULL);
  GBinding *binding;

  /* double -> boolean is not supported */
  binding = g_object_bind_property (source, "value",
                                    target, "toggle",
                                    G_BINDING_DEFAULT);
  g_object_add_weak_pointer (G_OBJECT (binding), (gpointer *) &binding);

  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_WARNING,
                         "*Unable to convert*double*boolean*");
  g_object_set (source, "value", 1.0, NULL);
  g_test_assert_expected_messages ();

  g_object_unref (source);
  g_object_unref (target);
  g_assert (binding == NULL);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/binding/default", binding_default);
  g_test_add_func ("/binding/bidirectional", binding_bidirectional);
  g_test_add_func ("/binding/transform", binding_transform);
  g_test_add_func ("/binding/transform-default", binding_transform_default);
  g_test_add_func ("/binding/transform-closure", binding_transform_closure);
  g_test_add_func ("/binding/chain", binding_chain);
  g_test_add_func ("/binding/sync-create", binding_sync_create);
  g_test_add_func ("/binding/invert-boolean", binding_invert_boolean);
  g_test_add_func ("/binding/same-object", binding_same_object);
  g_test_add_func ("/binding/unbind", binding_unbind);
  g_test_add_func ("/binding/fail", binding_fail);

  return g_test_run ();
}
