/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct
{
  char *filename;
  char *hostname;
  char *expected_result;
  GConvertError expected_error; /* If failed */
}  ToUriTest;

ToUriTest
to_uri_tests[] = {
  { "/etc", NULL, "file:///etc"},
  { "/etc", "", "file:///etc"},
  { "/etc", "otherhost", "file://otherhost/etc"},
#ifdef G_OS_WIN32
  { "/etc", "localhost", "file:///etc"},
  { "c:\\windows", NULL, "file:///c:/windows"},
  { "c:\\windows", "localhost", "file:///c:/windows"},
  { "c:\\windows", "otherhost", "file://otherhost/c:/windows"},
  { "\\\\server\\share\\dir", NULL, "file:////server/share/dir"},
  { "\\\\server\\share\\dir", "localhost", "file:////server/share/dir"},
#else
  { "/etc", "localhost", "file://localhost/etc"},
  { "c:\\windows", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH}, /* it's important to get this error on Unix */
  { "c:\\windows", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "c:\\windows", "otherhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
#endif
  { "etc", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
#ifndef G_PLATFORM_WIN32
  { "/etc/\xE5\xE4\xF6", NULL, "file:///etc/%E5%E4%F6" },
  { "/etc/\xC3\xB6\xC3\xA4\xC3\xA5", NULL, "file:///etc/%C3%B6%C3%A4%C3%A5"},
#endif
  { "/etc", "\xC3\xB6\xC3\xA4\xC3\xA5", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/etc", "\xE5\xE4\xF6", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/etc/file with #%", NULL, "file:///etc/file%20with%20%23%25"},
  { "", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "otherhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "/0123456789", NULL, "file:///0123456789"},
  { "/ABCDEFGHIJKLMNOPQRSTUVWXYZ", NULL, "file:///ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
  { "/abcdefghijklmnopqrstuvwxyz", NULL, "file:///abcdefghijklmnopqrstuvwxyz"},
  { "/-_.!~*'()", NULL, "file:///-_.!~*'()"},
#ifdef G_OS_WIN32
  /* As '\\' is a path separator on Win32, it gets turned into '/' in the URI */
  { "/\"#%<>[\\]^`{|}\x7F", NULL, "file:///%22%23%25%3C%3E%5B/%5D%5E%60%7B%7C%7D%7F"},
#else
  /* On Unix, '\\' is a normal character in the file name */
  { "/\"#%<>[\\]^`{|}\x7F", NULL, "file:///%22%23%25%3C%3E%5B%5C%5D%5E%60%7B%7C%7D%7F"},
#endif
  { "/;@+$,", NULL, "file:///%3B@+$,"},
  /* This and some of the following are of course as such illegal file names on Windows,
   * and would not occur in real life.
   */
  { "/:", NULL, "file:///:"},
  { "/?&=", NULL, "file:///%3F&="}, 
  { "/", "0123456789-", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "file://ABCDEFGHIJKLMNOPQRSTUVWXYZ/"},
  { "/", "abcdefghijklmnopqrstuvwxyz", "file://abcdefghijklmnopqrstuvwxyz/"},
  { "/", "_.!~*'()", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "\"#%<>[\\]^`{|}\x7F", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", ";?&=+$,", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "/", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "@:", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "\x80\xFF", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "\xC3\x80\xC3\xBF", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
};


typedef struct
{
  char *uri;
  char *expected_filename;
  char *expected_hostname;
  GConvertError expected_error; /* If failed */
}  FromUriTest;

FromUriTest
from_uri_tests[] = {
  { "file:///etc", "/etc"},
  { "file:/etc", "/etc"},
#ifdef G_OS_WIN32
  /* On Win32 we don't return "localhost" hostames, just in case
   * it isn't recognized anyway.
   */
  { "file://localhost/etc", "/etc", NULL},
  { "file://localhost/etc/%23%25%20file", "/etc/#% file", NULL},
  { "file://localhost/\xE5\xE4\xF6", "/\xe5\xe4\xf6", NULL},
  { "file://localhost/%E5%E4%F6", "/\xe5\xe4\xf6", NULL},
#else
  { "file://localhost/etc", "/etc", "localhost"},
  { "file://localhost/etc/%23%25%20file", "/etc/#% file", "localhost"},
  { "file://localhost/\xE5\xE4\xF6", "/\xe5\xe4\xf6", "localhost"},
  { "file://localhost/%E5%E4%F6", "/\xe5\xe4\xf6", "localhost"},
#endif
  { "file://otherhost/etc", "/etc", "otherhost"},
  { "file://otherhost/etc/%23%25%20file", "/etc/#% file", "otherhost"},
  { "file://%C3%B6%C3%A4%C3%A5/etc", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:////etc/%C3%B6%C3%C3%C3%A5", "//etc/\xc3\xb6\xc3\xc3\xc3\xa5", NULL},
  { "file://\xE5\xE4\xF6/etc", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%E5%E4%F6/etc", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:///some/file#bad", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://some", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:test", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "http://www.yahoo.com/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:////etc", "//etc"},
  { "file://///etc", "///etc"},
#ifdef G_OS_WIN32
  /* URIs with backslashes come from some nonstandard application, but accept them anyhow */
  { "file:///c:\\foo", "c:\\foo"},
  { "file:///c:/foo\\bar", "c:\\foo\\bar"},
  /* Accept also the old Netscape drive-letter-and-vertical bar convention */
  { "file:///c|/foo", "c:\\foo"},
  { "file:////server/share/dir", "\\\\server\\share\\dir"},
  { "file://localhost//server/share/foo", "\\\\server\\share\\foo"},
  { "file://otherhost//server/share/foo", "\\\\server\\share\\foo", "otherhost"},
#else
  { "file:///c:\\foo", "/c:\\foo"},
  { "file:///c:/foo", "/c:/foo"},
  { "file:////c:/foo", "//c:/foo"},
#endif
  { "file://0123456789/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://ABCDEFGHIJKLMNOPQRSTUVWXYZ/", "/", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
  { "file://abcdefghijklmnopqrstuvwxyz/", "/", "abcdefghijklmnopqrstuvwxyz"},
  { "file://-_.!~*'()/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://\"<>[\\]^`{|}\x7F/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://;?&=+$,/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%C3%80%C3%BF/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://@/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://:/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://#/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%23/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%2F/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
};

static void
run_to_uri_tests (void)
{
  int i;
  gchar *res;
  GError *error;

  for (i = 0; i < G_N_ELEMENTS (to_uri_tests); i++)
    {
      error = NULL;
      res = g_filename_to_uri (to_uri_tests[i].filename,
                               to_uri_tests[i].hostname,
                               &error);

      if (res)
        g_assert_cmpstr (res, ==, to_uri_tests[i].expected_result);
      else
        g_assert_error (error, G_CONVERT_ERROR, to_uri_tests[i].expected_error);

      g_free (res);
      g_clear_error (&error);
    }
}

static void
run_from_uri_tests (void)
{
  int i;
  gchar *res;
  gchar *hostname;
  GError *error;

  for (i = 0; i < G_N_ELEMENTS (from_uri_tests); i++)
    {
      error = NULL;
      res = g_filename_from_uri (from_uri_tests[i].uri,
                                 &hostname,
                                 &error);

#ifdef G_OS_WIN32
      if (from_uri_tests[i].expected_filename)
        {
          gchar *p, *slash;
          p = from_uri_tests[i].expected_filename = g_strdup (from_uri_tests[i].expected_filename);
          while ((slash = strchr (p, '/')) != NULL)
            {
              *slash = '\\';
              p = slash + 1;
            }
        }
#endif
      if (res)
        g_assert_cmpstr (res, ==, from_uri_tests[i].expected_filename);
      else
        g_assert_error (error, G_CONVERT_ERROR, from_uri_tests[i].expected_error);
      g_assert_cmpstr (hostname, ==, from_uri_tests[i].expected_hostname);

      g_free (res);
      g_free (hostname);
      g_clear_error (&error);
    }
}

static gint
safe_strcmp_filename (const gchar *a, const gchar *b)
{
#ifndef G_OS_WIN32
  return g_strcmp0 (a, b);
#else
  if (!a || !b)
    return g_strcmp0 (a, b);
  else
    {
      while (*a && *b)
        {
          if ((G_IS_DIR_SEPARATOR (*a) && G_IS_DIR_SEPARATOR (*b)) ||
              *a == *b)
            a++, b++;
          else
            return (*a - *b);
        }
      return (*a - *b);
    }
#endif
}

static gint
safe_strcmp_hostname (const gchar *a, const gchar *b)
{
  if (a == NULL)
    a = "";
  if (b == NULL)
    b = "";
#ifndef G_OS_WIN32
  return strcmp (a, b);
#else
  if (strcmp (a, "localhost") == 0 && !*b)
    return 0;
  else
    return strcmp (a, b);
#endif
}

static void
run_roundtrip_tests (void)
{
  int i;
  gchar *uri, *hostname, *res;
  GError *error;

  for (i = 0; i < G_N_ELEMENTS (to_uri_tests); i++)
    {
      if (to_uri_tests[i].expected_error != 0)
        continue;

      error = NULL;
      uri = g_filename_to_uri (to_uri_tests[i].filename,
                               to_uri_tests[i].hostname,
                               &error);
      g_assert_no_error (error);

      hostname = NULL;
      res = g_filename_from_uri (uri, &hostname, &error);
      g_assert_no_error (error);

      g_assert (safe_strcmp_filename (to_uri_tests[i].filename, res) == 0);
      g_assert (safe_strcmp_hostname (to_uri_tests[i].hostname, hostname) == 0);
      g_free (res);
      g_free (uri);
      g_free (hostname);
    }
}

static void
run_uri_list_tests (void)
{
  /* straight from the RFC */
  gchar *list =
    "# urn:isbn:0-201-08372-8\r\n"
    "http://www.huh.org/books/foo.html\r\n"
    "http://www.huh.org/books/foo.pdf   \r\n"
    "   ftp://ftp.foo.org/books/foo.txt\r\n";
  gchar *expected_uris[] = {
    "http://www.huh.org/books/foo.html",
    "http://www.huh.org/books/foo.pdf",
    "ftp://ftp.foo.org/books/foo.txt"
  };

  gchar **uris;
  gint j;

  uris = g_uri_list_extract_uris (list);
  g_assert_cmpint (g_strv_length (uris), ==, 3);

  for (j = 0; j < 3; j++)
    g_assert_cmpstr (uris[j], ==, expected_uris[j]);

  g_strfreev (uris);

  uris = g_uri_list_extract_uris ("# just hot air\r\n# more hot air");
  g_assert_cmpint (g_strv_length (uris), ==, 0);
  g_strfreev (uris);
}

static void
test_uri_unescape (void)
{
  gchar *s;

  s = g_uri_unescape_string ("%2Babc %4F",  NULL);
  g_assert_cmpstr (s, ==, "+abc O");
  g_free (s);
  g_assert_cmpstr (g_uri_unescape_string ("%2Babc %4F",  "+"), ==, NULL);
  g_assert_cmpstr (g_uri_unescape_string ("%00abc %4F",  "+/"), ==, NULL);
  g_assert_cmpstr (g_uri_unescape_string ("%0",  NULL), ==, NULL);
  g_assert_cmpstr (g_uri_unescape_string ("%ra",  NULL), ==, NULL);
  g_assert_cmpstr (g_uri_unescape_string ("%2r",  NULL), ==, NULL);
  g_assert_cmpstr (g_uri_unescape_string (NULL,  NULL), ==, NULL);
}

static void
test_uri_escape (void)
{
  gchar *s;

  s = g_uri_escape_string ("abcdefgABCDEFG._~", NULL, FALSE);
  g_assert_cmpstr (s, ==, "abcdefgABCDEFG._~");
  g_free (s);
  s = g_uri_escape_string (":+ \\?#", NULL, FALSE);
  g_assert_cmpstr (s, ==, "%3A%2B%20%5C%3F%23");
  g_free (s);
  s = g_uri_escape_string ("a+b:c", "+", FALSE);
  g_assert_cmpstr (s, ==, "a+b%3Ac");
  g_free (s);
  s = g_uri_escape_string ("a+b:c\303\234", "+", TRUE);
  g_assert_cmpstr (s, ==, "a+b%3Ac\303\234");
  g_free (s);
}

static void
test_uri_scheme (void)
{
  gchar *s;

  s = g_uri_parse_scheme ("ftp://ftp.gtk.org");
  g_assert_cmpstr (s, ==, "ftp");
  g_free (s);
  s = g_uri_parse_scheme ("1bad:");
  g_assert (s == NULL);
  s = g_uri_parse_scheme ("bad");
  g_assert (s == NULL);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/uri/to-uri", run_to_uri_tests);
  g_test_add_func ("/uri/from-uri", run_from_uri_tests);
  g_test_add_func ("/uri/roundtrip", run_roundtrip_tests);
  g_test_add_func ("/uri/list", run_uri_list_tests);
  g_test_add_func ("/uri/unescape", test_uri_unescape);
  g_test_add_func ("/uri/escape", test_uri_escape);
  g_test_add_func ("/uri/scheme", test_uri_scheme);

  return g_test_run ();
}
