#include <gio/gio.h>

static void
test_exact (void)
{
  char *exact_matches[] = {
    "*",
    "a::*",
    "a::*,b::*",
    "a::a,a::b",
    "a::a,a::b,b::*"
  };

  GFileAttributeMatcher *matcher;
  char *s;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (exact_matches); i++)
    {
      matcher = g_file_attribute_matcher_new (exact_matches[i]);
      s = g_file_attribute_matcher_to_string (matcher);
      g_assert_cmpstr (exact_matches[i], ==, s);
      g_free (s);
      g_file_attribute_matcher_unref (matcher);
    }
}

static void
test_equality (void)
{
  struct {
    char *expected;
    char *actual;
  } equals[] = {
    /* star makes everything else go away */
    { "*", "*,*" },
    { "*", "*,a::*" },
    { "*", "*,a::b" },
    { "*", "a::*,*" },
    { "*", "a::b,*" },
    { "*", "a::b,*,a::*" },
    /* a::* makes a::<anything> go away */
    { "a::*", "a::*,a::*" },
    { "a::*", "a::*,a::b" },
    { "a::*", "a::b,a::*" },
    { "a::*", "a::b,a::*,a::c" },
    /* a::b does not allow duplicates */
    { "a::b", "a::b,a::b" },
    { "a::b,a::c", "a::b,a::c,a::b" },
    /* stuff gets ordered in registration order */
    { "a::b,a::c", "a::c,a::b" },
    { "a::*,b::*", "b::*,a::*" },
  };

  GFileAttributeMatcher *matcher;
  char *s;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (equals); i++)
    {
      matcher = g_file_attribute_matcher_new (equals[i].actual);
      s = g_file_attribute_matcher_to_string (matcher);
      g_assert_cmpstr (equals[i].expected, ==, s);
      g_free (s);
      g_file_attribute_matcher_unref (matcher);
    }
}

static void
test_subtract (void)
{
  struct {
    char *attributes;
    char *subtract;
    char *result;
  } subtractions[] = {
    /* * subtracts everything */
    { "*", "*", NULL },
    { "a::*", "*", NULL },
    { "a::b", "*", NULL },
    { "a::b,a::c", "*", NULL },
    { "a::*,b::*", "*", NULL },
    { "a::*,b::c", "*", NULL },
    { "a::b,b::*", "*", NULL },
    { "a::b,b::c", "*", NULL },
    { "a::b,a::c,b::*", "*", NULL },
    { "a::b,a::c,b::c", "*", NULL },
    /* a::* subtracts all a's */
    { "*", "a::*", "*" },
    { "a::*", "a::*", NULL },
    { "a::b", "a::*", NULL },
    { "a::b,a::c", "a::*", NULL },
    { "a::*,b::*", "a::*", "b::*" },
    { "a::*,b::c", "a::*", "b::c" },
    { "a::b,b::*", "a::*", "b::*" },
    { "a::b,b::c", "a::*", "b::c" },
    { "a::b,a::c,b::*", "a::*", "b::*" },
    { "a::b,a::c,b::c", "a::*", "b::c" },
    /* a::b subtracts exactly that */
    { "*", "a::b", "*" },
    { "a::*", "a::b", "a::*" },
    { "a::b", "a::b", NULL },
    { "a::b,a::c", "a::b", "a::c" },
    { "a::*,b::*", "a::b", "a::*,b::*" },
    { "a::*,b::c", "a::b", "a::*,b::c" },
    { "a::b,b::*", "a::b", "b::*" },
    { "a::b,b::c", "a::b", "b::c" },
    { "a::b,a::c,b::*", "a::b", "a::c,b::*" },
    { "a::b,a::c,b::c", "a::b", "a::c,b::c" },
    /* a::b,b::* subtracts both of those */
    { "*", "a::b,b::*", "*" },
    { "a::*", "a::b,b::*", "a::*" },
    { "a::b", "a::b,b::*", NULL },
    { "a::b,a::c", "a::b,b::*", "a::c" },
    { "a::*,b::*", "a::b,b::*", "a::*" },
    { "a::*,b::c", "a::b,b::*", "a::*" },
    { "a::b,b::*", "a::b,b::*", NULL },
    { "a::b,b::c", "a::b,b::*", NULL },
    { "a::b,a::c,b::*", "a::b,b::*", "a::c" },
    { "a::b,a::c,b::c", "a::b,b::*", "a::c" },
    /* a::b,b::c should work, too */
    { "*", "a::b,b::c", "*" },
    { "a::*", "a::b,b::c", "a::*" },
    { "a::b", "a::b,b::c", NULL },
    { "a::b,a::c", "a::b,b::c", "a::c" },
    { "a::*,b::*", "a::b,b::c", "a::*,b::*" },
    { "a::*,b::c", "a::b,b::c", "a::*" },
    { "a::b,b::*", "a::b,b::c", "b::*" },
    { "a::b,b::c", "a::b,b::c", NULL },
    { "a::b,a::c,b::*", "a::b,b::c", "a::c,b::*" },
    { "a::b,a::c,b::c", "a::b,b::c", "a::c" },
  };

  GFileAttributeMatcher *matcher, *subtract, *result;
  char *s;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (subtractions); i++)
    {
      matcher = g_file_attribute_matcher_new (subtractions[i].attributes);
      subtract = g_file_attribute_matcher_new (subtractions[i].subtract);
      result = g_file_attribute_matcher_subtract (matcher, subtract);
      s = g_file_attribute_matcher_to_string (result);
      g_assert_cmpstr (subtractions[i].result, ==, s);
      g_free (s);
      g_file_attribute_matcher_unref (matcher);
      g_file_attribute_matcher_unref (subtract);
      g_file_attribute_matcher_unref (result);
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/fileattributematcher/exact", test_exact);
  g_test_add_func ("/fileattributematcher/equality", test_equality);
  g_test_add_func ("/fileattributematcher/subtract", test_subtract);

  return g_test_run ();
}
