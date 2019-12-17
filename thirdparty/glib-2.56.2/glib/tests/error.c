#include <glib.h>

static void
test_overwrite (void)
{
  GError *error, *dest, *src;

  if (!g_test_undefined ())
    return;

  error = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "*set over the top*");
  g_set_error_literal (&error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "bla");
  g_test_assert_expected_messages ();

  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_error_free (error);


  dest = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");
  src = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "bla");

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "*set over the top*");
  g_propagate_error (&dest, src);
  g_test_assert_expected_messages ();

  g_assert_error (dest, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_error_free (dest);
}

static void
test_prefix (void)
{
  GError *error;
  GError *dest, *src;

  error = NULL;
  g_prefix_error (&error, "foo %d %s: ", 1, "two");
  g_assert (error == NULL);

  error = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");
  g_prefix_error (&error, "foo %d %s: ", 1, "two");
  g_assert_cmpstr (error->message, ==, "foo 1 two: bla");
  g_error_free (error);

  dest = NULL;
  src = g_error_new_literal (G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "bla");
  g_propagate_prefixed_error (&dest, src, "foo %d %s: ", 1, "two");
  g_assert_cmpstr (dest->message, ==, "foo 1 two: bla");
  g_error_free (dest);
}

static void
test_literal (void)
{
  GError *error;

  error = NULL;
  g_set_error_literal (&error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "%s %d %x");
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_assert_cmpstr (error->message, ==, "%s %d %x");
  g_error_free (error);
}

static void
test_copy (void)
{
  GError *error;
  GError *copy;

  error = NULL;
  g_set_error_literal (&error, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY, "%s %d %x");
  copy = g_error_copy (error);

  g_assert_error (copy, G_MARKUP_ERROR, G_MARKUP_ERROR_EMPTY);
  g_assert_cmpstr (copy->message, ==, "%s %d %x");

  g_error_free (error);
  g_error_free (copy);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/error/overwrite", test_overwrite);
  g_test_add_func ("/error/prefix", test_prefix);
  g_test_add_func ("/error/literal", test_literal);
  g_test_add_func ("/error/copy", test_copy);

  return g_test_run ();
}

