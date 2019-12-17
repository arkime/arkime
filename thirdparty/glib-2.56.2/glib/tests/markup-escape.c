#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <stdarg.h>
#include <string.h>
#include <glib.h>

typedef struct _EscapeTest EscapeTest;

struct _EscapeTest
{
  const gchar *original;
  const gchar *expected;
};

static EscapeTest escape_tests[] =
{
  { "&", "&amp;" },
  { "<", "&lt;" },
  { ">", "&gt;" },
  { "'", "&apos;" },
  { "\"", "&quot;" },
  { "", "" },
  { "A", "A" },
  { "A&", "A&amp;" },
  { "&A", "&amp;A" },
  { "A&A", "A&amp;A" },
  { "&&A", "&amp;&amp;A" },
  { "A&&", "A&amp;&amp;" },
  { "A&&A", "A&amp;&amp;A" },
  { "A&A&A", "A&amp;A&amp;A" },
  { "A&#23;A", "A&amp;#23;A" },
  { "A&#xa;A", "A&amp;#xa;A" }
};

static void
escape_test (gconstpointer d)
{
  const EscapeTest *test = d;
  gchar *result;

  result = g_markup_escape_text (test->original, -1);

  g_assert_cmpstr (result, ==, test->expected);

  g_free (result);
}

typedef struct _UnicharTest UnicharTest;

struct _UnicharTest
{
  gunichar c;
  gboolean entity;
};

static UnicharTest unichar_tests[] =
{
  { 0x1, TRUE },
  { 0x8, TRUE },
  { 0x9, FALSE },
  { 0xa, FALSE },
  { 0xb, TRUE },
  { 0xc, TRUE },
  { 0xd, FALSE },
  { 0xe, TRUE },
  { 0x1f, TRUE },
  { 0x20, FALSE },
  { 0x7e, FALSE },
  { 0x7f, TRUE },
  { 0x84, TRUE },
  { 0x85, FALSE },
  { 0x86, TRUE },
  { 0x9f, TRUE },
  { 0xa0, FALSE }
};

static void
unichar_test (gconstpointer d)
{
  const UnicharTest *test = d;
  EscapeTest t;
  gint len;
  gchar outbuf[7], expected[12];

  len = g_unichar_to_utf8 (test->c, outbuf);
  outbuf[len] = 0;

  if (test->entity)
    g_snprintf (expected, 12, "&#x%x;", test->c);
  else
    strcpy (expected, outbuf);

  t.original = outbuf;
  t.expected = expected;
  escape_test (&t);
}

G_GNUC_PRINTF(1, 3)
static void
test_format (const gchar *format,
	     const gchar *expected,
	     ...)
{
  gchar *result;
  va_list args;

  va_start (args, expected);
  result = g_markup_vprintf_escaped (format, args);
  va_end (args);

  g_assert_cmpstr (result, ==, expected);

  g_free (result);
}

static void
format_test (void)
{
  test_format ("A", "A");
  test_format ("A%s", "A&amp;", "&");
  test_format ("%sA", "&amp;A", "&");
  test_format ("A%sA", "A&amp;A", "&");
  test_format ("%s%sA", "&amp;&amp;A", "&", "&");
  test_format ("A%s%s", "A&amp;&amp;", "&", "&");
  test_format ("A%s%sA", "A&amp;&amp;A", "&", "&");
  test_format ("A%sA%sA", "A&amp;A&amp;A", "&", "&");
  test_format ("%s", "&lt;B&gt;&amp;", "<B>&");
  test_format ("%c%c", "&lt;&amp;", '<', '&');
  test_format (".%c.%c.", ".&lt;.&amp;.", '<', '&');
  test_format ("%s", "", "");
  test_format ("%-5s", "A    ", "A");
  test_format ("%2$s%1$s", "B.A.", "A.", "B.");
}

int main (int argc, char **argv)
{
  gint i;
  gchar *path;

  g_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (escape_tests); i++)
    {
      path = g_strdup_printf ("/markup/escape-text/%d", i);
      g_test_add_data_func (path, &escape_tests[i], escape_test);
      g_free (path);
    }

  for (i = 0; i < G_N_ELEMENTS (unichar_tests); i++)
    {
      path = g_strdup_printf ("/markup/escape-unichar/%d", i);
      g_test_add_data_func (path, &unichar_tests[i], unichar_test);
      g_free (path);
    }

  g_test_add_func ("/markup/format", format_test);

  return g_test_run ();
}
