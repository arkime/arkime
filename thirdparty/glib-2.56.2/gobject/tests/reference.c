#include <glib-object.h>

static void
test_fundamentals (void)
{
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_NONE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INTERFACE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_CHAR));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UCHAR));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_BOOLEAN));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UINT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_LONG));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_ULONG));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INT64));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UINT64));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_ENUM));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_FLAGS));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_FLOAT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_DOUBLE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_STRING));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_POINTER));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_BOXED));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_PARAM));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_OBJECT));
  g_assert (G_TYPE_OBJECT == g_object_get_type ());
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_VARIANT));
  g_assert (G_TYPE_IS_DERIVED (G_TYPE_INITIALLY_UNOWNED));

  g_assert (g_type_fundamental_next () == G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST));
}

static void
test_type_qdata (void)
{
  gchar *data;

  g_type_set_qdata (G_TYPE_ENUM, g_quark_from_string ("bla"), "bla");
  data = g_type_get_qdata (G_TYPE_ENUM, g_quark_from_string ("bla"));
  g_assert_cmpstr (data, ==, "bla");
}

static void
test_type_query (void)
{
  GTypeQuery query;

  g_type_query (G_TYPE_ENUM, &query);
  g_assert_cmpint (query.type, ==, G_TYPE_ENUM);
  g_assert_cmpstr (query.type_name, ==, "GEnum");
  g_assert_cmpint (query.class_size, ==, sizeof (GEnumClass));
  g_assert_cmpint (query.instance_size, ==, 0);
}

typedef struct _MyObject MyObject;
typedef struct _MyObjectClass MyObjectClass;
typedef struct _MyObjectClassPrivate MyObjectClassPrivate;

struct _MyObject
{
  GObject parent_instance;

  gint count;
};

struct _MyObjectClass
{
  GObjectClass parent_class;
};

struct _MyObjectClassPrivate
{
  gint secret_class_count;
};

static GType my_object_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyObject, my_object, G_TYPE_OBJECT,
                         g_type_add_class_private (g_define_type_id, sizeof (MyObjectClassPrivate)) );

static void
my_object_init (MyObject *obj)
{
  obj->count = 42;
}

static void
my_object_class_init (MyObjectClass *klass)
{
}

static void
test_class_private (void)
{
  GObject *obj;
  MyObjectClass *class;
  MyObjectClassPrivate *priv;

  obj = g_object_new (my_object_get_type (), NULL);

  class = g_type_class_ref (my_object_get_type ());
  priv = G_TYPE_CLASS_GET_PRIVATE (class, my_object_get_type (), MyObjectClassPrivate);
  priv->secret_class_count = 13;
  g_type_class_unref (class);

  g_object_unref (obj);

  g_assert_cmpint (g_type_qname (my_object_get_type ()), ==, g_quark_from_string ("MyObject"));
}

static void
test_clear (void)
{
  GObject *o = NULL;
  GObject *tmp;

  g_clear_object (&o);
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  o = g_object_ref (tmp);
  g_assert (o != NULL);

  g_assert_cmpint (tmp->ref_count, ==, 2);
  g_clear_object (&o);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert (o == NULL);

  g_object_unref (tmp);
}

static void
test_clear_function (void)
{
  volatile GObject *o = NULL;
  GObject *tmp;

  (g_clear_object) (&o);
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  o = g_object_ref (tmp);
  g_assert (o != NULL);

  g_assert_cmpint (tmp->ref_count, ==, 2);
  (g_clear_object) (&o);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert (o == NULL);

  g_object_unref (tmp);
}

static void
test_set (void)
{
  GObject *o = NULL;
  GObject *tmp;

  g_assert (!g_set_object (&o, NULL));
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  g_assert (g_set_object (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 2);

  g_object_unref (tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  /* Setting it again shouldn’t cause finalisation. */
  g_assert (!g_set_object (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  g_assert (g_set_object (&o, NULL));
  g_assert (o == NULL);
  g_assert (!G_IS_OBJECT (tmp));  /* finalised */
}

static void
test_set_function (void)
{
  GObject *o = NULL;
  GObject *tmp;

  g_assert (!(g_set_object) (&o, NULL));
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  g_assert ((g_set_object) (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 2);

  g_object_unref (tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  /* Setting it again shouldn’t cause finalisation. */
  g_assert (!(g_set_object) (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  g_assert ((g_set_object) (&o, NULL));
  g_assert (o == NULL);
  g_assert (!G_IS_OBJECT (tmp));  /* finalised */
}

static void
toggle_cb (gpointer data, GObject *obj, gboolean is_last)
{
  gboolean *b = data;

  *b = TRUE;
}

static void
test_object_value (void)
{
  GObject *v;
  GObject *v2;
  GValue value = G_VALUE_INIT;
  gboolean toggled = FALSE;

  g_value_init (&value, G_TYPE_OBJECT);

  v = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_add_toggle_ref (v, toggle_cb, &toggled);

  g_value_take_object (&value, v);

  v2 = g_value_get_object (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_object (&value);
  g_assert (v2 == v);  /* objects use ref/unref for copy/free */
  g_object_unref (v2);

  g_assert (!toggled);
  g_value_unset (&value);
  g_assert (toggled);

  /* test the deprecated variant too */
  g_value_init (&value, G_TYPE_OBJECT);
  /* get a new reference */
  g_object_ref (v);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_value_set_object_take_ownership (&value, v);
G_GNUC_END_IGNORE_DEPRECATIONS

  toggled = FALSE;
  g_value_unset (&value);
  g_assert (toggled);

  g_object_remove_toggle_ref (v, toggle_cb, &toggled);
}

static void
test_initially_unowned (void)
{
  GObject *obj;

  obj = g_object_new (G_TYPE_INITIALLY_UNOWNED, NULL);
  g_assert (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_assert (!g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_assert (!g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 2);

  g_object_unref (obj);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_force_floating (obj);
  g_assert (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_object_unref (obj);
}

static void
test_weak_pointer (void)
{
  GObject *obj;
  gpointer weak;
  gpointer weak2;

  weak = weak2 = obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_add_weak_pointer (obj, &weak);
  g_object_add_weak_pointer (obj, &weak2);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert (weak == obj);
  g_assert (weak2 == obj);

  g_object_remove_weak_pointer (obj, &weak2);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert (weak == obj);
  g_assert (weak2 == obj);

  g_object_unref (obj);
  g_assert (weak == NULL);
  g_assert (weak2 == obj);
}

static void
test_weak_pointer_clear (void)
{
  GObject *obj;
  gpointer weak = NULL;

  g_clear_weak_pointer (&weak);
  g_assert_null (weak);

  weak = obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_add_weak_pointer (obj, &weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_clear_weak_pointer (&weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_object_unref (obj);
}

static void
test_weak_pointer_clear_function (void)
{
  GObject *obj;
  gpointer weak = NULL;

  (g_clear_weak_pointer) (&weak);
  g_assert_null (weak);

  weak = obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_add_weak_pointer (obj, &weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  (g_clear_weak_pointer) (&weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_object_unref (obj);
}

static void
test_weak_pointer_set (void)
{
  GObject *obj;
  gpointer weak = NULL;

  g_assert_false (g_set_weak_pointer (&weak, NULL));
  g_assert_null (weak);

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_assert_true (g_set_weak_pointer (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_assert_true (g_set_weak_pointer (&weak, NULL));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_assert_true (g_set_weak_pointer (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_object_unref (obj);
  g_assert_null (weak);
}

static void
test_weak_pointer_set_function (void)
{
  GObject *obj;
  gpointer weak = NULL;

  g_assert_false ((g_set_weak_pointer) (&weak, NULL));
  g_assert_null (weak);

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_assert_true ((g_set_weak_pointer) (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_assert_true ((g_set_weak_pointer) (&weak, NULL));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_assert_true ((g_set_weak_pointer) (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_object_unref (obj);
  g_assert_null (weak);
}

/* See gobject/tests/threadtests.c for the threaded version */
static void
test_weak_ref (void)
{
  GObject *obj;
  GObject *obj2;
  GObject *tmp;
  GWeakRef weak = { { GUINT_TO_POINTER (0xDEADBEEFU) } };
  GWeakRef weak2 = { { GUINT_TO_POINTER (0xDEADBEEFU) } };
  GWeakRef weak3 = { { GUINT_TO_POINTER (0xDEADBEEFU) } };
  GWeakRef *dynamic_weak = g_new (GWeakRef, 1);

  /* you can initialize to empty like this... */
  g_weak_ref_init (&weak2, NULL);
  g_assert (g_weak_ref_get (&weak2) == NULL);

  /* ... or via an initializer */
  g_weak_ref_init (&weak3, NULL);
  g_assert (g_weak_ref_get (&weak3) == NULL);

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  obj2 = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj2->ref_count, ==, 1);

  /* you can init with an object (even if uninitialized) */
  g_weak_ref_init (&weak, obj);
  g_weak_ref_init (dynamic_weak, obj);
  /* or set to point at an object, if initialized (maybe to 0) */
  g_weak_ref_set (&weak2, obj);
  g_weak_ref_set (&weak3, obj);
  /* none of this affects its refcount */
  g_assert_cmpint (obj->ref_count, ==, 1);

  /* getting the value takes a ref */
  tmp = g_weak_ref_get (&weak);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  tmp = g_weak_ref_get (&weak2);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  tmp = g_weak_ref_get (&weak3);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  tmp = g_weak_ref_get (dynamic_weak);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  /* clearing a weak ref stops tracking */
  g_weak_ref_clear (&weak);

  /* setting a weak ref to NULL stops tracking too */
  g_weak_ref_set (&weak2, NULL);
  g_assert (g_weak_ref_get (&weak2) == NULL);
  g_weak_ref_clear (&weak2);

  /* setting a weak ref to a new object stops tracking the old one */
  g_weak_ref_set (dynamic_weak, obj2);
  tmp = g_weak_ref_get (dynamic_weak);
  g_assert (tmp == obj2);
  g_assert_cmpint (obj2->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj2->ref_count, ==, 1);

  g_assert_cmpint (obj->ref_count, ==, 1);

  /* free the object: weak3 is the only one left pointing there */
  g_object_unref (obj);
  g_assert (g_weak_ref_get (&weak3) == NULL);

  /* setting a weak ref to a new object stops tracking the old one */
  g_weak_ref_set (dynamic_weak, obj2);
  tmp = g_weak_ref_get (dynamic_weak);
  g_assert (tmp == obj2);
  g_assert_cmpint (obj2->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj2->ref_count, ==, 1);

  g_weak_ref_clear (&weak3);

  /* clear and free dynamic_weak... */
  g_weak_ref_clear (dynamic_weak);

  /* ... to prove that doing so stops this from being a use-after-free */
  g_object_unref (obj2);
  g_free (dynamic_weak);
}

typedef struct
{
  gboolean should_be_last;
  gint count;
} Count;

static void
toggle_notify (gpointer  data,
               GObject  *obj,
               gboolean  is_last)
{
  Count *c = data;

  g_assert (is_last == c->should_be_last);

  c->count++;
}

static void
test_toggle_ref (void)
{
  GObject *obj;
  Count c, c2;

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  g_object_add_toggle_ref (obj, toggle_notify, &c);
  g_object_add_toggle_ref (obj, toggle_notify, &c2);

  c.should_be_last = c2.should_be_last = TRUE;
  c.count = c2.count = 0;

  g_object_unref (obj);

  g_assert_cmpint (c.count, ==, 0);
  g_assert_cmpint (c2.count, ==, 0);

  g_object_ref (obj);

  g_assert_cmpint (c.count, ==, 0);
  g_assert_cmpint (c2.count, ==, 0);

  g_object_remove_toggle_ref (obj, toggle_notify, &c2);

  g_object_unref (obj);

  g_assert_cmpint (c.count, ==, 1);

  c.should_be_last = FALSE;

  g_object_ref (obj);

  g_assert_cmpint (c.count, ==, 2);

  c.should_be_last = TRUE;

  g_object_unref (obj);

  g_assert_cmpint (c.count, ==, 3);

  g_object_remove_toggle_ref (obj, toggle_notify, &c);
}

static gboolean destroyed;
static gint value;

static void
data_destroy (gpointer data)
{
  g_assert_cmpint (GPOINTER_TO_INT (data), ==, value);

  destroyed = TRUE;
}

static void
test_object_qdata (void)
{
  GObject *obj;
  gpointer v;
  GQuark quark;

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  value = 1;
  destroyed = FALSE;
  g_object_set_data_full (obj, "test", GINT_TO_POINTER (1), data_destroy);
  v = g_object_get_data (obj, "test");
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 1);
  g_object_set_data_full (obj, "test", GINT_TO_POINTER (2), data_destroy);
  g_assert (destroyed);
  value = 2;
  destroyed = FALSE;
  v = g_object_steal_data (obj, "test");
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 2);
  g_assert (!destroyed);

  value = 1;
  destroyed = FALSE;
  quark = g_quark_from_string ("test");
  g_object_set_qdata_full (obj, quark, GINT_TO_POINTER (1), data_destroy);
  v = g_object_get_qdata (obj, quark);
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 1);
  g_object_set_qdata_full (obj, quark, GINT_TO_POINTER (2), data_destroy);
  g_assert (destroyed);
  value = 2;
  destroyed = FALSE;
  v = g_object_steal_qdata (obj, quark);
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 2);
  g_assert (!destroyed);

  g_object_set_qdata_full (obj, quark, GINT_TO_POINTER (3), data_destroy);
  value = 3;
  destroyed = FALSE;
  g_object_unref (obj);

  g_assert (destroyed);
}

typedef struct {
  const gchar *value;
  gint refcount;
} Value;

static gpointer
ref_value (gpointer value, gpointer user_data)
{
  Value *v = value;
  Value **old_value_p = user_data;

  if (old_value_p)
    *old_value_p = v;

  if (v)
    v->refcount += 1;

  return value;
}

static void
unref_value (gpointer value)
{
  Value *v = value;

  v->refcount -= 1;
  if (v->refcount == 0)
    g_free (value);
}

static
Value *
new_value (const gchar *s)
{
  Value *v;

  v = g_new (Value, 1);
  v->value = s;
  v->refcount = 1;

  return v;
}

static void
test_object_qdata2 (void)
{
  GObject *obj;
  Value *v, *v1, *v2, *v3, *old_val;
  GDestroyNotify old_destroy;
  gboolean res;

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  v1 = new_value ("bla");

  g_object_set_data_full (obj, "test", v1, unref_value);

  v = g_object_get_data (obj, "test");
  g_assert_cmpstr (v->value, ==, "bla");
  g_assert_cmpint (v->refcount, ==, 1);

  v = g_object_dup_data (obj, "test", ref_value, &old_val);
  g_assert (old_val == v1);
  g_assert_cmpstr (v->value, ==, "bla");
  g_assert_cmpint (v->refcount, ==, 2);
  unref_value (v);

  v = g_object_dup_data (obj, "nono", ref_value, &old_val);
  g_assert (old_val == NULL);
  g_assert (v == NULL);

  v2 = new_value ("not");

  res = g_object_replace_data (obj, "test", v1, v2, unref_value, &old_destroy);
  g_assert (res == TRUE);
  g_assert (old_destroy == unref_value);
  g_assert_cmpstr (v1->value, ==, "bla");
  g_assert_cmpint (v1->refcount, ==, 1);

  v = g_object_get_data (obj, "test");
  g_assert_cmpstr (v->value, ==, "not");
  g_assert_cmpint (v->refcount, ==, 1);

  v3 = new_value ("xyz");
  res = g_object_replace_data (obj, "test", v1, v3, unref_value, &old_destroy);
  g_assert (res == FALSE);
  g_assert_cmpstr (v2->value, ==, "not");
  g_assert_cmpint (v2->refcount, ==, 1);

  unref_value (v1);

  res = g_object_replace_data (obj, "test", NULL, v3, unref_value, &old_destroy);
  g_assert (res == FALSE);
  g_assert_cmpstr (v2->value, ==, "not");
  g_assert_cmpint (v2->refcount, ==, 1);

  res = g_object_replace_data (obj, "test", v2, NULL, unref_value, &old_destroy);
  g_assert (res == TRUE);
  g_assert (old_destroy == unref_value);
  g_assert_cmpstr (v2->value, ==, "not");
  g_assert_cmpint (v2->refcount, ==, 1);

  unref_value (v2);

  v = g_object_get_data (obj, "test");
  g_assert (v == NULL);

  res = g_object_replace_data (obj, "test", NULL, v3, unref_value, &old_destroy);
  g_assert (res == TRUE);

  v = g_object_get_data (obj, "test");
  g_assert (v == v3);

  ref_value (v3, NULL);
  g_assert_cmpint (v3->refcount, ==, 2);
  g_object_unref (obj);
  g_assert_cmpint (v3->refcount, ==, 1);
  unref_value (v3);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/type/fundamentals", test_fundamentals);
  g_test_add_func ("/type/qdata", test_type_qdata);
  g_test_add_func ("/type/query", test_type_query);
  g_test_add_func ("/type/class-private", test_class_private);
  g_test_add_func ("/object/clear", test_clear);
  g_test_add_func ("/object/clear-function", test_clear_function);
  g_test_add_func ("/object/set", test_set);
  g_test_add_func ("/object/set-function", test_set_function);
  g_test_add_func ("/object/value", test_object_value);
  g_test_add_func ("/object/initially-unowned", test_initially_unowned);
  g_test_add_func ("/object/weak-pointer", test_weak_pointer);
  g_test_add_func ("/object/weak-pointer/clear", test_weak_pointer_clear);
  g_test_add_func ("/object/weak-pointer/clear-function", test_weak_pointer_clear_function);
  g_test_add_func ("/object/weak-pointer/set", test_weak_pointer_set);
  g_test_add_func ("/object/weak-pointer/set-function", test_weak_pointer_set_function);
  g_test_add_func ("/object/weak-ref", test_weak_ref);
  g_test_add_func ("/object/toggle-ref", test_toggle_ref);
  g_test_add_func ("/object/qdata", test_object_qdata);
  g_test_add_func ("/object/qdata2", test_object_qdata2);

  return g_test_run ();
}
