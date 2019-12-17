#include <glib-object.h>

static const GEnumValue my_enum_values[] =
{
  { 1, "the first value", "one" },
  { 2, "the second value", "two" },
  { 3, "the third value", "three" },
  { 0, NULL, NULL }
};

static void
test_enum_basic (void)
{
  GType type;
  GEnumClass *class;
  GEnumValue *val;
  GValue value = G_VALUE_INIT;
  gchar *to_string;

  type = g_enum_register_static ("MyEnum", my_enum_values);

  g_value_init (&value, type);
  g_assert (G_VALUE_HOLDS_ENUM (&value));

  g_value_set_enum (&value, 2);
  g_assert_cmpint (g_value_get_enum (&value), ==, 2);
  g_value_unset (&value);

  class = g_type_class_ref (type);

  g_assert_cmpint (class->minimum, ==, 1);
  g_assert_cmpint (class->maximum, ==, 3);
  g_assert_cmpint (class->n_values, ==, 3);

  val = g_enum_get_value (class, 2);
  g_assert (val != NULL);
  g_assert_cmpstr (val->value_name, ==, "the second value");
  val = g_enum_get_value (class, 15);
  g_assert (val == NULL);

  val = g_enum_get_value_by_name (class, "the third value");
  g_assert (val != NULL);
  g_assert_cmpint (val->value, ==, 3);
  val = g_enum_get_value_by_name (class, "the color purple");
  g_assert (val == NULL);

  val = g_enum_get_value_by_nick (class, "one");
  g_assert (val != NULL);
  g_assert_cmpint (val->value, ==, 1);
  val = g_enum_get_value_by_nick (class, "purple");
  g_assert (val == NULL);

  to_string = g_enum_to_string (type, 2);
  g_assert_cmpstr (to_string, ==, "the second value");
  g_free (to_string);

  to_string = g_enum_to_string (type, 15);
  g_assert_cmpstr (to_string, ==, "15");
  g_free (to_string);

  g_type_class_unref (class);
}

static const GFlagsValue my_flag_values[] =
{
  { 0, "no flags", "none" },
  { 1, "the first flag", "one" },
  { 2, "the second flag", "two" },
  { 8, "the third flag", "three" },
  { 0, NULL, NULL }
};

static const GFlagsValue no_default_flag_values[] =
{
  { 1, "the first flag", "one" },
  { 0, NULL, NULL }
};

static void
test_flags_transform_to_string (const GValue *value)
{
  GValue tmp = G_VALUE_INIT;

  g_value_init (&tmp, G_TYPE_STRING);
  g_value_transform (value, &tmp);
  g_value_unset (&tmp);
}

static void
test_flags_basic (void)
{
  GType type, no_default_type;
  GFlagsClass *class;
  GFlagsValue *val;
  GValue value = G_VALUE_INIT;
  gchar *to_string;

  type = g_flags_register_static ("MyFlags", my_flag_values);
  no_default_type = g_flags_register_static ("NoDefaultFlags",
                                             no_default_flag_values);

  g_value_init (&value, type);
  g_assert (G_VALUE_HOLDS_FLAGS (&value));

  g_value_set_flags (&value, 2|8);
  g_assert_cmpint (g_value_get_flags (&value), ==, 2|8);

  class = g_type_class_ref (type);

  g_assert_cmpint (class->mask, ==, 1|2|8);
  g_assert_cmpint (class->n_values, ==, 4);

  val = g_flags_get_first_value (class, 2|8);
  g_assert (val != NULL);
  g_assert_cmpstr (val->value_name, ==, "the second flag");
  val = g_flags_get_first_value (class, 16);
  g_assert (val == NULL);

  val = g_flags_get_value_by_name (class, "the third flag");
  g_assert (val != NULL);
  g_assert_cmpint (val->value, ==, 8);
  val = g_flags_get_value_by_name (class, "the color purple");
  g_assert (val == NULL);

  val = g_flags_get_value_by_nick (class, "one");
  g_assert (val != NULL);
  g_assert_cmpint (val->value, ==, 1);
  val = g_flags_get_value_by_nick (class, "purple");
  g_assert (val == NULL);

  test_flags_transform_to_string (&value);
  g_value_unset (&value);

  to_string = g_flags_to_string (type, 1|8);
  g_assert_cmpstr (to_string, ==, "the first flag | the third flag");
  g_free (to_string);

  to_string = g_flags_to_string (type, 0);
  g_assert_cmpstr (to_string, ==, "no flags");
  g_free (to_string);

  to_string = g_flags_to_string (type, 16);
  g_assert_cmpstr (to_string, ==, "0x10");
  g_free (to_string);

  to_string = g_flags_to_string (type, 1|16);
  g_assert_cmpstr (to_string, ==, "the first flag | 0x10");
  g_free (to_string);

  to_string = g_flags_to_string (no_default_type, 0);
  g_assert_cmpstr (to_string, ==, "0x0");
  g_free (to_string);

  to_string = g_flags_to_string (no_default_type, 16);
  g_assert_cmpstr (to_string, ==, "0x10");
  g_free (to_string);

  g_type_class_unref (class);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/enum/basic", test_enum_basic);
  g_test_add_func ("/flags/basic", test_flags_basic);

  return g_test_run ();
}
