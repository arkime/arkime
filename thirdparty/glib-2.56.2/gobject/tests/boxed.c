#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <glib-object.h>

typedef struct _MyBoxed MyBoxed;

struct _MyBoxed
{
  gint ivalue;
  gchar *bla;
};

static gpointer
my_boxed_copy (gpointer orig)
{
  MyBoxed *a = orig;
  MyBoxed *b;

  b = g_slice_new (MyBoxed);
  b->ivalue = a->ivalue;
  b->bla = g_strdup (a->bla);

  return b;
}

static gint my_boxed_free_count;

static void
my_boxed_free (gpointer orig)
{
  MyBoxed *a = orig;

  g_free (a->bla);
  g_slice_free (MyBoxed, a);

  my_boxed_free_count++;
}

static GType my_boxed_get_type (void);
#define MY_TYPE_BOXED (my_boxed_get_type ())

G_DEFINE_BOXED_TYPE (MyBoxed, my_boxed, my_boxed_copy, my_boxed_free)

static void
test_define_boxed (void)
{
  MyBoxed a;
  MyBoxed *b;

  a.ivalue = 20;
  a.bla = g_strdup ("bla");

  b = g_boxed_copy (MY_TYPE_BOXED, &a);

  g_assert_cmpint (b->ivalue, ==, 20);
  g_assert_cmpstr (b->bla, ==, "bla");

  g_boxed_free (MY_TYPE_BOXED, b);

  g_free (a.bla);
}

static void
test_boxed_ownership (void)
{
  GValue value = G_VALUE_INIT;
  static MyBoxed boxed = { 10, "bla" };

  g_value_init (&value, MY_TYPE_BOXED);

  my_boxed_free_count = 0;

  g_value_set_static_boxed (&value, &boxed);
  g_value_reset (&value);

  g_assert_cmpint (my_boxed_free_count, ==, 0);

  g_value_set_boxed_take_ownership (&value, g_boxed_copy (MY_TYPE_BOXED, &boxed));
  g_value_reset (&value);
  g_assert_cmpint (my_boxed_free_count, ==, 1);

  g_value_take_boxed (&value, g_boxed_copy (MY_TYPE_BOXED, &boxed));
  g_value_reset (&value);
  g_assert_cmpint (my_boxed_free_count, ==, 2);

  g_value_set_boxed (&value, &boxed);
  g_value_reset (&value);
  g_assert_cmpint (my_boxed_free_count, ==, 3);
}

static void
my_callback (gpointer user_data)
{
}

static gint destroy_count;

static void
my_closure_notify (gpointer user_data, GClosure *closure)
{
  destroy_count++;
}

static void
test_boxed_closure (void)
{
  GClosure *closure;
  GClosure *closure2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_CLOSURE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  closure = g_cclosure_new (G_CALLBACK (my_callback), "bla", my_closure_notify);
  g_value_take_boxed (&value, closure);

  closure2 = g_value_get_boxed (&value);
  g_assert (closure2 == closure);

  closure2 = g_value_dup_boxed (&value);
  g_assert (closure2 == closure); /* closures use ref/unref for copy/free */
  g_closure_unref (closure2);

  g_value_unset (&value);
  g_assert_cmpint (destroy_count, ==, 1);
}

static void
test_boxed_date (void)
{
  GDate *date;
  GDate *date2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_DATE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  date = g_date_new_dmy (1, 3, 1970);
  g_value_take_boxed (&value, date);

  date2 = g_value_get_boxed (&value);
  g_assert (date2 == date);

  date2 = g_value_dup_boxed (&value);
  g_assert (date2 != date);
  g_assert (g_date_compare (date, date2) == 0);
  g_date_free (date2);

  g_value_unset (&value);
}

static void
test_boxed_value (void)
{
  GValue value1 = G_VALUE_INIT;
  GValue *value2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_VALUE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  g_value_init (&value1, G_TYPE_INT);
  g_value_set_int (&value1, 26);

  g_value_set_static_boxed (&value, &value1);

  value2 = g_value_get_boxed (&value);
  g_assert (value2 == &value1);

  value2 = g_value_dup_boxed (&value);
  g_assert (value2 != &value1);
  g_assert (G_VALUE_HOLDS_INT (value2));
  g_assert_cmpint (g_value_get_int (value2), ==, 26);
  g_boxed_free (G_TYPE_VALUE, value2);

  g_value_unset (&value);
}

static void
test_boxed_string (void)
{
  GString *v;
  GString *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_GSTRING);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_string_new ("bla");
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 != v);
  g_assert (g_string_equal (v, v2));
  g_string_free (v2, TRUE);

  g_value_unset (&value);
}

static void
test_boxed_hashtable (void)
{
  GHashTable *v;
  GHashTable *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_HASH_TABLE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_hash_table_new (g_str_hash, g_str_equal);
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* hash tables use ref/unref for copy/free */
  g_hash_table_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_array (void)
{
  GArray *v;
  GArray *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_ARRAY);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_array_new (TRUE, FALSE, 1);
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* arrays use ref/unref for copy/free */
  g_array_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_ptrarray (void)
{
  GPtrArray *v;
  GPtrArray *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_PTR_ARRAY);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_ptr_array_new ();
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* ptr arrays use ref/unref for copy/free */
  g_ptr_array_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_regex (void)
{
  GRegex *v;
  GRegex *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_REGEX);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_regex_new ("a+b+", 0, 0, NULL);
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* regexes use ref/unref for copy/free */
  g_regex_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_matchinfo (void)
{
  GRegex *r;
  GMatchInfo *info, *info2;
  gboolean ret;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_MATCH_INFO);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  r = g_regex_new ("ab", 0, 0, NULL);
  ret = g_regex_match (r, "blabla abab bla", 0, &info);
  g_assert (ret);
  g_value_take_boxed (&value, info);

  info2 = g_value_get_boxed (&value);
  g_assert (info == info2);

  info2 = g_value_dup_boxed (&value);
  g_assert (info == info2);  /* matchinfo uses ref/unref for copy/free */
  g_match_info_unref (info2);

  g_value_unset (&value);
  g_regex_unref (r);
}

static void
test_boxed_varianttype (void)
{
  GVariantType *v;
  GVariantType *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_VARIANT_TYPE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_variant_type_new ("mas");
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 != v);
  g_assert_cmpstr (g_variant_type_peek_string (v), ==, g_variant_type_peek_string (v2));
  g_variant_type_free (v2);

  g_value_unset (&value);
}

static void
test_boxed_datetime (void)
{
  GDateTime *v;
  GDateTime *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_DATE_TIME);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_date_time_new_now_local ();
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v); /* datetime uses ref/unref for copy/free */
  g_date_time_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_error (void)
{
  GError *v;
  GError *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_ERROR);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_error_new_literal (G_VARIANT_PARSE_ERROR,
                           G_VARIANT_PARSE_ERROR_NUMBER_TOO_BIG,
                           "Too damn big");
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 != v);
  g_assert_cmpint (v->domain, ==, v2->domain);
  g_assert_cmpint (v->code, ==, v2->code);
  g_assert_cmpstr (v->message, ==, v2->message);
  g_error_free (v2);

  g_value_unset (&value);
}

static void
test_boxed_keyfile (void)
{
  GKeyFile *k, *k2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_KEY_FILE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  k = g_key_file_new ();
  g_value_take_boxed (&value, k);

  k2 = g_value_get_boxed (&value);
  g_assert (k == k2);

  k2 = g_value_dup_boxed (&value);
  g_assert (k == k2);  /* keyfile uses ref/unref for copy/free */
  g_key_file_unref (k2);

  g_value_unset (&value);
}

static void
test_boxed_mainloop (void)
{
  GMainLoop *l, *l2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_MAIN_LOOP);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  l = g_main_loop_new (NULL, FALSE);
  g_value_take_boxed (&value, l);

  l2 = g_value_get_boxed (&value);
  g_assert (l == l2);

  l2 = g_value_dup_boxed (&value);
  g_assert (l == l2);  /* mainloop uses ref/unref for copy/free */
  g_main_loop_unref (l2);

  g_value_unset (&value);
}

static void
test_boxed_maincontext (void)
{
  GMainContext *c, *c2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_MAIN_CONTEXT);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  c = g_main_context_new ();
  g_value_take_boxed (&value, c);

  c2 = g_value_get_boxed (&value);
  g_assert (c == c2);

  c2 = g_value_dup_boxed (&value);
  g_assert (c == c2);  /* maincontext uses ref/unref for copy/free */
  g_main_context_unref (c2);

  g_value_unset (&value);
}

static void
test_boxed_source (void)
{
  GSource *s, *s2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_SOURCE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  s = g_idle_source_new ();
  g_value_take_boxed (&value, s);

  s2 = g_value_get_boxed (&value);
  g_assert (s == s2);

  s2 = g_value_dup_boxed (&value);
  g_assert (s == s2);  /* source uses ref/unref for copy/free */
  g_source_unref (s2);

  g_value_unset (&value);
}

static void
test_boxed_variantbuilder (void)
{
  GVariantBuilder *v, *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_VARIANT_BUILDER);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_variant_builder_new (G_VARIANT_TYPE_OBJECT_PATH_ARRAY);
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v == v2);

  v2 = g_value_dup_boxed (&value);
  g_assert (v == v2);  /* variantbuilder uses ref/unref for copy/free */
  g_variant_builder_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_timezone (void)
{
  GTimeZone *z, *z2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_TIME_ZONE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  z = g_time_zone_new_utc ();
  g_value_take_boxed (&value, z);

  z2 = g_value_get_boxed (&value);
  g_assert (z == z2);

  z2 = g_value_dup_boxed (&value);
  g_assert (z == z2);  /* timezone uses ref/unref for copy/free */
  g_time_zone_unref (z2);

  g_value_unset (&value);
}

static void
test_boxed_pollfd (void)
{
  GPollFD *p, *p2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_POLLFD);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  p = g_new (GPollFD, 1);
  g_value_take_boxed (&value, p);

  p2 = g_value_get_boxed (&value);
  g_assert (p == p2);

  p2 = g_value_dup_boxed (&value);
  g_assert (p != p2);
  g_free (p2);

  g_value_unset (&value);
}

static void
test_boxed_markup (void)
{
  GMarkupParseContext *c, *c2;
  const GMarkupParser parser = { 0 };
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_MARKUP_PARSE_CONTEXT);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  c = g_markup_parse_context_new (&parser, 0, NULL, NULL);
  g_value_take_boxed (&value, c);

  c2 = g_value_get_boxed (&value);
  g_assert (c == c2);

  c2 = g_value_dup_boxed (&value);
  g_assert (c == c2);
  g_markup_parse_context_unref (c2);

  g_value_unset (&value);
}

static void
test_boxed_thread (void)
{
  GThread *t, *t2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_THREAD);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  t = g_thread_self ();
  g_value_set_boxed (&value, t);

  t2 = g_value_get_boxed (&value);
  g_assert (t == t2);

  t2 = g_value_dup_boxed (&value);
  g_assert (t == t2);
  g_thread_unref (t2);

  g_value_unset (&value);
}

static void
test_boxed_checksum (void)
{
  GChecksum *c, *c2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_CHECKSUM);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  c = g_checksum_new (G_CHECKSUM_SHA512);
  g_value_take_boxed (&value, c);

  c2 = g_value_get_boxed (&value);
  g_assert (c == c2);

  c2 = g_value_dup_boxed (&value);
  g_assert (c != c2);
  g_checksum_free (c2);

  g_value_unset (&value);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/boxed/define", test_define_boxed);
  g_test_add_func ("/boxed/ownership", test_boxed_ownership);
  g_test_add_func ("/boxed/closure", test_boxed_closure);
  g_test_add_func ("/boxed/date", test_boxed_date);
  g_test_add_func ("/boxed/value", test_boxed_value);
  g_test_add_func ("/boxed/string", test_boxed_string);
  g_test_add_func ("/boxed/hashtable", test_boxed_hashtable);
  g_test_add_func ("/boxed/array", test_boxed_array);
  g_test_add_func ("/boxed/ptrarray", test_boxed_ptrarray);
  g_test_add_func ("/boxed/regex", test_boxed_regex);
  g_test_add_func ("/boxed/varianttype", test_boxed_varianttype);
  g_test_add_func ("/boxed/error", test_boxed_error);
  g_test_add_func ("/boxed/datetime", test_boxed_datetime);
  g_test_add_func ("/boxed/matchinfo", test_boxed_matchinfo);
  g_test_add_func ("/boxed/keyfile", test_boxed_keyfile);
  g_test_add_func ("/boxed/mainloop", test_boxed_mainloop);
  g_test_add_func ("/boxed/maincontext", test_boxed_maincontext);
  g_test_add_func ("/boxed/source", test_boxed_source);
  g_test_add_func ("/boxed/variantbuilder", test_boxed_variantbuilder);
  g_test_add_func ("/boxed/timezone", test_boxed_timezone);
  g_test_add_func ("/boxed/pollfd", test_boxed_pollfd);
  g_test_add_func ("/boxed/markup", test_boxed_markup);
  g_test_add_func ("/boxed/thread", test_boxed_thread);
  g_test_add_func ("/boxed/checksum", test_boxed_checksum);

  return g_test_run ();
}
