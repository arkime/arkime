/* Unit tests for gstring
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

/* We are testing some deprecated APIs here */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <stdio.h>
#include <string.h>
#include "glib.h"


static void
test_string_chunks (void)
{
  GStringChunk *string_chunk;
  gchar *tmp_string, *tmp_string_2;
  gint i;

  string_chunk = g_string_chunk_new (1024);

  for (i = 0; i < 100000; i ++)
    {
      tmp_string = g_string_chunk_insert (string_chunk, "hi pete");
      g_assert_cmpstr ("hi pete", ==, tmp_string);
    }

  tmp_string_2 = g_string_chunk_insert_const (string_chunk, tmp_string);
  g_assert (tmp_string_2 != tmp_string);
  g_assert_cmpstr (tmp_string_2, ==, tmp_string);

  tmp_string = g_string_chunk_insert_const (string_chunk, tmp_string);
  g_assert_cmpstr (tmp_string_2, ==, tmp_string);

  g_string_chunk_clear (string_chunk);
  g_string_chunk_free (string_chunk);
}

static void
test_string_chunk_insert (void)
{
  const gchar s0[] = "Testing GStringChunk";
  const gchar s1[] = "a\0b\0c\0d\0";
  const gchar s2[] = "Hello, world";
  GStringChunk *chunk;
  gchar *str[3];

  chunk = g_string_chunk_new (512);

  str[0] = g_string_chunk_insert (chunk, s0);
  str[1] = g_string_chunk_insert_len (chunk, s1, 8);
  str[2] = g_string_chunk_insert (chunk, s2);

  g_assert (memcmp (s0, str[0], sizeof s0) == 0);
  g_assert (memcmp (s1, str[1], sizeof s1) == 0);
  g_assert (memcmp (s2, str[2], sizeof s2) == 0);

  g_string_chunk_free (chunk);
}

static void
test_string_new (void)
{
  GString *string1, *string2;

  string1 = g_string_new ("hi pete!");
  string2 = g_string_new (NULL);

  g_assert (string1 != NULL);
  g_assert (string2 != NULL);
  g_assert (strlen (string1->str) == string1->len);
  g_assert (strlen (string2->str) == string2->len);
  g_assert (string2->len == 0);
  g_assert_cmpstr ("hi pete!", ==, string1->str);
  g_assert_cmpstr ("", ==, string2->str);

  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);

  string1 = g_string_new_len ("foo", -1);
  string2 = g_string_new_len ("foobar", 3);

  g_assert_cmpstr (string1->str, ==, "foo");
  g_assert_cmpint (string1->len, ==, 3);
  g_assert_cmpstr (string2->str, ==, "foo");
  g_assert_cmpint (string2->len, ==, 3);

  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);
}

G_GNUC_PRINTF(2, 3)
static void
my_string_printf (GString     *string,
                  const gchar *format,
                  ...)
{
  va_list args;

  va_start (args, format);
  g_string_vprintf (string, format, args);
  va_end (args);
}

static void
test_string_printf (void)
{
  GString *string;

  string = g_string_new (NULL);

#ifndef G_OS_WIN32
  /* MSVC and mingw32 use the same run-time C library, which doesn't like
     the %10000.10000f format... */
  g_string_printf (string, "%s|%0100d|%s|%0*d|%*.*f|%10000.10000f",
		   "this pete guy sure is a wuss, like he's the number ",
		   1,
		   " wuss.  everyone agrees.\n",
		   10, 666, 15, 15, 666.666666666, 666.666666666);
#else
  g_string_printf (string, "%s|%0100d|%s|%0*d|%*.*f|%100.100f",
		   "this pete guy sure is a wuss, like he's the number ",
		   1,
		   " wuss.  everyone agrees.\n",
		   10, 666, 15, 15, 666.666666666, 666.666666666);
#endif

  g_string_free (string, TRUE);

  string = g_string_new (NULL);
  g_string_printf (string, "bla %s %d", "foo", 99);
  g_assert_cmpstr (string->str, ==, "bla foo 99");
  my_string_printf (string, "%d,%s,%d", 1, "two", 3);
  g_assert_cmpstr (string->str, ==, "1,two,3");

  g_string_free (string, TRUE);
}

static void
test_string_assign (void)
{
  GString *string;

  string = g_string_new (NULL);
  g_string_assign (string, "boring text");
  g_assert_cmpstr (string->str, ==, "boring text");
  g_string_free (string, TRUE);

  /* assign with string overlap */
  string = g_string_new ("textbeforetextafter");
  g_string_assign (string, string->str + 10);
  g_assert_cmpstr (string->str, ==, "textafter");
  g_string_free (string, TRUE);

  string = g_string_new ("boring text");
  g_string_assign (string, string->str);
  g_assert_cmpstr (string->str, ==, "boring text");
  g_string_free (string, TRUE);
}

static void
test_string_append_c (void)
{
  GString *string;
  gint i;

  string = g_string_new ("hi pete!");

  for (i = 0; i < 10000; i++)
    if (i % 2)
      g_string_append_c (string, 'a'+(i%26));
    else
      (g_string_append_c) (string, 'a'+(i%26));

  g_assert((strlen("hi pete!") + 10000) == string->len);
  g_assert((strlen("hi pete!") + 10000) == strlen(string->str));

  g_string_free (string, TRUE);
}

static void
test_string_append (void)
{
  GString *string;

  /* append */
  string = g_string_new ("firsthalf");
  g_string_append (string, "lasthalf");
  g_assert_cmpstr (string->str, ==, "firsthalflasthalf");
  g_string_free (string, TRUE);

  /* append_len */
  string = g_string_new ("firsthalf");
  g_string_append_len (string, "lasthalfjunkjunk", strlen ("lasthalf"));
  g_assert_cmpstr (string->str, ==, "firsthalflasthalf");
  g_string_free (string, TRUE);
}

static void
test_string_prepend_c (void)
{
  GString *string;
  gint i;

  string = g_string_new ("hi pete!");

  for (i = 0; i < 10000; i++)
    g_string_prepend_c (string, 'a'+(i%26));

  g_assert((strlen("hi pete!") + 10000) == string->len);
  g_assert((strlen("hi pete!") + 10000) == strlen(string->str));

  g_string_free (string, TRUE);
}

static void
test_string_prepend (void)
{
  GString *string;

  /* prepend */
  string = g_string_new ("lasthalf");
  g_string_prepend (string, "firsthalf");
  g_assert_cmpstr (string->str, ==, "firsthalflasthalf");
  g_string_free (string, TRUE);

  /* prepend_len */
  string = g_string_new ("lasthalf");
  g_string_prepend_len (string, "firsthalfjunkjunk", strlen ("firsthalf"));
  g_assert_cmpstr (string->str, ==, "firsthalflasthalf");
  g_string_free (string, TRUE);
}

static void
test_string_insert (void)
{
  GString *string;

  /* insert */
  string = g_string_new ("firstlast");
  g_string_insert (string, 5, "middle");
  g_assert_cmpstr (string->str, ==, "firstmiddlelast");
  g_string_free (string, TRUE);

  /* insert with pos == end of the string */
  string = g_string_new ("firstmiddle");
  g_string_insert (string, strlen ("firstmiddle"), "last");
  g_assert_cmpstr (string->str, ==, "firstmiddlelast");
  g_string_free (string, TRUE);
  
  /* insert_len */
  string = g_string_new ("firstlast");
  g_string_insert_len (string, 5, "middlejunkjunk", strlen ("middle"));
  g_assert_cmpstr (string->str, ==, "firstmiddlelast");
  g_string_free (string, TRUE);

  /* insert_len with magic -1 pos for append */
  string = g_string_new ("first");
  g_string_insert_len (string, -1, "lastjunkjunk", strlen ("last"));
  g_assert_cmpstr (string->str, ==, "firstlast");
  g_string_free (string, TRUE);
  
  /* insert_len with magic -1 len for strlen-the-string */
  string = g_string_new ("first");
  g_string_insert_len (string, 5, "last", -1);
  g_assert_cmpstr (string->str, ==, "firstlast");
  g_string_free (string, TRUE);

  /* insert_len with string overlap */
  string = g_string_new ("textbeforetextafter");
  g_string_insert_len (string, 10, string->str + 8, 5);
  g_assert_cmpstr (string->str, ==, "textbeforeretextextafter");
  g_string_free (string, TRUE);
}

static void
test_string_insert_unichar (void)
{
  GString *string;

  /* insert_unichar with insertion in middle */
  string = g_string_new ("firsthalf");
  g_string_insert_unichar (string, 5, 0x0041);
  g_assert_cmpstr (string->str, ==, "first\x41half");
  g_string_free (string, TRUE);

  string = g_string_new ("firsthalf");
  g_string_insert_unichar (string, 5, 0x0298);
  g_assert_cmpstr (string->str, ==, "first\xCA\x98half");
  g_string_free (string, TRUE);

  string = g_string_new ("firsthalf");
  g_string_insert_unichar (string, 5, 0xFFFD);
  g_assert_cmpstr (string->str, ==, "first\xEF\xBF\xBDhalf");
  g_string_free (string, TRUE);

  string = g_string_new ("firsthalf");
  g_string_insert_unichar (string, 5, 0x1D100);
  g_assert_cmpstr (string->str, ==, "first\xF0\x9D\x84\x80half");
  g_string_free (string, TRUE);

  /* insert_unichar with insertion at end */
  string = g_string_new ("start");
  g_string_insert_unichar (string, -1, 0x0041);
  g_assert_cmpstr (string->str, ==, "start\x41");
  g_string_free (string, TRUE);

  string = g_string_new ("start");
  g_string_insert_unichar (string, -1, 0x0298);
  g_assert_cmpstr (string->str, ==, "start\xCA\x98");
  g_string_free (string, TRUE);

  string = g_string_new ("start");
  g_string_insert_unichar (string, -1, 0xFFFD);
  g_assert_cmpstr (string->str, ==, "start\xEF\xBF\xBD");
  g_string_free (string, TRUE);

  string = g_string_new ("start");
  g_string_insert_unichar (string, -1, 0x1D100);
  g_assert_cmpstr (string->str, ==, "start\xF0\x9D\x84\x80");
  g_string_free (string, TRUE);
}

static void
test_string_equal (void)
{
  GString *string1, *string2;

  string1 = g_string_new ("test");
  string2 = g_string_new ("te");
  g_assert (!g_string_equal(string1, string2));
  g_string_append (string2, "st");
  g_assert (g_string_equal(string1, string2));
  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);
}

static void
test_string_truncate (void)
{
  GString *string;

  string = g_string_new ("testing");

  g_string_truncate (string, 1000);
  g_assert (string->len == strlen("testing"));
  g_assert_cmpstr (string->str, ==, "testing");

  g_string_truncate (string, 4);
  g_assert (string->len == 4);
  g_assert_cmpstr (string->str, ==, "test");

  g_string_truncate (string, 0);
  g_assert (string->len == 0);
  g_assert_cmpstr (string->str, ==, "");

  g_string_free (string, TRUE);
}

static void
test_string_overwrite (void)
{
  GString *string;

  /* overwriting functions */
  string = g_string_new ("testing");

  g_string_overwrite (string, 4, " and expand");
  g_assert (15 == string->len);
  g_assert ('\0' == string->str[15]);
  g_assert (g_str_equal ("test and expand", string->str));

  g_string_overwrite (string, 5, "NOT-");
  g_assert (15 == string->len);
  g_assert ('\0' == string->str[15]);
  g_assert (g_str_equal ("test NOT-expand", string->str));

  g_string_overwrite_len (string, 9, "blablabla", 6);
  g_assert (15 == string->len);
  g_assert ('\0' == string->str[15]);
  g_assert (g_str_equal ("test NOT-blabla", string->str));

  g_string_overwrite_len (string, 4, "BLABL", 0);
  g_assert (g_str_equal ("test NOT-blabla", string->str));
  g_string_overwrite_len (string, 4, "BLABL", -1);
  g_assert (g_str_equal ("testBLABLblabla", string->str));

  g_string_free (string, TRUE);
}

static void
test_string_nul_handling (void)
{
  GString *string1, *string2;

  /* Check handling of embedded ASCII 0 (NUL) characters in GString. */
  string1 = g_string_new ("fiddle");
  string2 = g_string_new ("fiddle");
  g_assert (g_string_equal (string1, string2));
  g_string_append_c (string1, '\0');
  g_assert (!g_string_equal (string1, string2));
  g_string_append_c (string2, '\0');
  g_assert (g_string_equal (string1, string2));
  g_string_append_c (string1, 'x');
  g_string_append_c (string2, 'y');
  g_assert (!g_string_equal (string1, string2));
  g_assert (string1->len == 8);
  g_string_append (string1, "yzzy");
  g_assert_cmpmem (string1->str, string1->len + 1, "fiddle\0xyzzy", 13);
  g_string_insert (string1, 1, "QED");
  g_assert_cmpmem (string1->str, string1->len + 1, "fQEDiddle\0xyzzy", 16);
  g_string_printf (string1, "fiddle%cxyzzy", '\0');
  g_assert_cmpmem (string1->str, string1->len + 1, "fiddle\0xyzzy", 13);

  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);
}

static void
test_string_up_down (void)
{
  GString *s;

  s = g_string_new ("Mixed Case String !?");
  g_string_ascii_down (s);
  g_assert_cmpstr (s->str, ==, "mixed case string !?");

  g_string_assign (s, "Mixed Case String !?");
  g_string_down (s);
  g_assert_cmpstr (s->str, ==, "mixed case string !?");

  g_string_assign (s, "Mixed Case String !?");
  g_string_ascii_up (s);
  g_assert_cmpstr (s->str, ==, "MIXED CASE STRING !?");

  g_string_assign (s, "Mixed Case String !?");
  g_string_up (s);
  g_assert_cmpstr (s->str, ==, "MIXED CASE STRING !?");

  g_string_free (s, TRUE);
}

static void
test_string_set_size (void)
{
  GString *s;

  s = g_string_new ("foo");
  g_string_set_size (s, 30);

  g_assert_cmpstr (s->str, ==, "foo");
  g_assert_cmpint (s->len, ==, 30);

  g_string_free (s, TRUE);
}

static void
test_string_to_bytes (void)
{
  GString *s;
  GBytes *bytes;
  gconstpointer byte_data;
  gsize byte_len;

  s = g_string_new ("foo");
  g_string_append (s, "-bar");

  bytes = g_string_free_to_bytes (s);

  byte_data = g_bytes_get_data (bytes, &byte_len);

  g_assert_cmpint (byte_len, ==, 7);

  g_assert_cmpmem (byte_data, byte_len, "foo-bar", 7);

  g_bytes_unref (bytes);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/string/test-string-chunks", test_string_chunks);
  g_test_add_func ("/string/test-string-chunk-insert", test_string_chunk_insert);
  g_test_add_func ("/string/test-string-new", test_string_new);
  g_test_add_func ("/string/test-string-printf", test_string_printf);
  g_test_add_func ("/string/test-string-assign", test_string_assign);
  g_test_add_func ("/string/test-string-append-c", test_string_append_c);
  g_test_add_func ("/string/test-string-append", test_string_append);
  g_test_add_func ("/string/test-string-prepend-c", test_string_prepend_c);
  g_test_add_func ("/string/test-string-prepend", test_string_prepend);
  g_test_add_func ("/string/test-string-insert", test_string_insert);
  g_test_add_func ("/string/test-string-insert-unichar", test_string_insert_unichar);
  g_test_add_func ("/string/test-string-equal", test_string_equal);
  g_test_add_func ("/string/test-string-truncate", test_string_truncate);
  g_test_add_func ("/string/test-string-overwrite", test_string_overwrite);
  g_test_add_func ("/string/test-string-nul-handling", test_string_nul_handling);
  g_test_add_func ("/string/test-string-up-down", test_string_up_down);
  g_test_add_func ("/string/test-string-set-size", test_string_set_size);
  g_test_add_func ("/string/test-string-to-bytes", test_string_to_bytes);

  return g_test_run();
}
