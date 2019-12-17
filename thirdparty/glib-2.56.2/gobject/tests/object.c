#include <glib-object.h>

/* --------------------------------- */
/* test_object_constructor_singleton */

typedef GObject MySingletonObject;
typedef GObjectClass MySingletonObjectClass;

GType my_singleton_object_get_type (void);

G_DEFINE_TYPE (MySingletonObject, my_singleton_object, G_TYPE_OBJECT)

static MySingletonObject *singleton;

static void
my_singleton_object_init (MySingletonObject *obj)
{
}

static GObject *
my_singleton_object_constructor (GType                  type,
                                 guint                  n_construct_properties,
                                 GObjectConstructParam *construct_params)
{
  GObject *object;

  if (singleton)
    return g_object_ref (singleton);

  object = G_OBJECT_CLASS (my_singleton_object_parent_class)->
    constructor (type, n_construct_properties, construct_params);
  singleton = (MySingletonObject *)object;

  return object;
}

static void
my_singleton_object_finalize (MySingletonObject *obj)
{
  singleton = NULL;

  G_OBJECT_CLASS (my_singleton_object_parent_class)->finalize (obj);
}

static void
my_singleton_object_class_init (MySingletonObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = my_singleton_object_constructor;
  object_class->finalize = my_singleton_object_finalize;
}

static void
test_object_constructor_singleton (void)
{
  MySingletonObject *one, *two, *three;

  one = g_object_new (my_singleton_object_get_type (), NULL);
  g_assert_cmpint (G_OBJECT (one)->ref_count, ==, 1);

  two = g_object_new (my_singleton_object_get_type (), NULL);
  g_assert (one == two);
  g_assert_cmpint (G_OBJECT (two)->ref_count, ==, 2);

  three = g_object_new (my_singleton_object_get_type (), NULL);
  g_assert (one == three);
  g_assert_cmpint (G_OBJECT (three)->ref_count, ==, 3);

  g_object_add_weak_pointer (G_OBJECT (one), (gpointer *)&one);

  g_object_unref (one);
  g_assert (one != NULL);

  g_object_unref (three);
  g_object_unref (two);

  g_assert (one == NULL);
}

/* ----------------------------------- */
/* test_object_constructor_infanticide */

typedef GObject MyInfanticideObject;
typedef GObjectClass MyInfanticideObjectClass;

GType my_infanticide_object_get_type (void);

G_DEFINE_TYPE (MyInfanticideObject, my_infanticide_object, G_TYPE_OBJECT)

static void
my_infanticide_object_init (MyInfanticideObject *obj)
{
}

static GObject *
my_infanticide_object_constructor (GType                  type,
                                   guint                  n_construct_properties,
                                   GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (my_infanticide_object_parent_class)->
    constructor (type, n_construct_properties, construct_params);

  g_object_unref (object);

  return NULL;
}

static void
my_infanticide_object_class_init (MyInfanticideObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = my_infanticide_object_constructor;
}

static void
test_object_constructor_infanticide (void)
{
  GObject *obj;
  int i;

  g_test_bug ("661576");

  for (i = 0; i < 1000; i++)
    {
      g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                             "*finalized while still in-construction*");
      g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                             "*Custom constructor*returned NULL*");
      obj = g_object_new (my_infanticide_object_get_type (), NULL);
      g_assert_null (obj);
      g_test_assert_expected_messages ();
    }
}

/* --------------------------------- */

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/object/constructor/singleton", test_object_constructor_singleton);
  g_test_add_func ("/object/constructor/infanticide", test_object_constructor_infanticide);

  return g_test_run ();
}
