#include "config.h"

#include <locale.h>
#include <glib.h>

typedef struct
{
  const gchar *string;
  const gchar *prefix;
  gboolean should_match;
} SearchTest;

static void
test_search (void)
{
  SearchTest tests[] =
    {
      /* Test word separators and case */
      { "Hello World", "he", TRUE },
      { "Hello World", "wo", TRUE },
      { "Hello World", "lo", FALSE },
      { "Hello World", "ld", FALSE },
      { "Hello-World", "wo", TRUE },
      { "HelloWorld", "wo", FALSE },

      /* Test composed chars (accentued letters) */
      { "Jörgen", "jor", TRUE },
      { "Gaëtan", "gaetan", TRUE },
      { "élève", "ele", TRUE },
      { "Azais", "AzaÏs", FALSE },
      { "AzaÏs", "Azais", TRUE },

      /* Test decomposed chars, they looks the same, but are actually
       * composed of multiple unicodes */
      { "Jorgen", "Jör", FALSE },
      { "Jörgen", "jor", TRUE },

      /* Turkish special case */
      { "İstanbul", "ist", TRUE },
      { "Diyarbakır", "diyarbakir", TRUE },

      /* Multi words */
      { "Xavier Claessens", "Xav Cla", TRUE },
      { "Xavier Claessens", "Cla Xav", TRUE },
      { "Foo Bar Baz", "   b  ", TRUE },
      { "Foo Bar Baz", "bar bazz", FALSE },

      { NULL, NULL, FALSE }
    };
  guint i;

  setlocale(LC_ALL, "");

  g_debug ("Started");
  for (i = 0; tests[i].string != NULL; i ++)
    {
      gboolean match;
      gboolean ok;

      match = g_str_match_string (tests[i].prefix, tests[i].string, TRUE);
      ok = (match == tests[i].should_match);

      g_debug ("'%s' - '%s' %s: %s", tests[i].prefix, tests[i].string,
          tests[i].should_match ? "should match" : "should NOT match",
          ok ? "OK" : "FAILED");

      g_assert (ok);
    }
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/search", test_search);

  return g_test_run ();
}
