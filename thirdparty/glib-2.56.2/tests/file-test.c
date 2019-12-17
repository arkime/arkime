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

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#ifdef GLIB_COMPILATION
#undef GLIB_COMPILATION
#endif

#include <string.h>

#include <glib.h>

#include <gstdio.h>

#include <fcntl.h>		/* For open() */

#ifdef G_OS_UNIX
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>			/* For read(), write() etc */
#endif

static void
test_mkstemp (void)
{
  char template[32];
  int fd;
  int i;
  const char hello[] = "Hello, World";
  const int hellolen = sizeof (hello) - 1;
  char chars[62];

  strcpy (template, "foobar");
  fd = g_mkstemp (template);
  if (fd != -1)
    {
      g_warning ("g_mkstemp works even if template doesn't contain XXXXXX");
      close (fd);
    }

  strcpy (template, "foobarXXX");
  fd = g_mkstemp (template);
  if (fd != -1)
    {
      g_warning ("g_mkstemp works even if template contains less than six X");
      close (fd);
    }

  strcpy (template, "fooXXXXXX");
  fd = g_mkstemp (template);
  g_assert (fd != -1 && "g_mkstemp didn't work for template fooXXXXXX");
  i = write (fd, hello, hellolen);
  g_assert (i != -1 && "write() failed");
  g_assert (i == hellolen && "write() has written too few bytes");

  lseek (fd, 0, 0);
  i = read (fd, chars, sizeof (chars));
  g_assert (i != -1 && "read() failed: %s");
  g_assert (i == hellolen && "read() has got wrong number of bytes");

  chars[i] = 0;
  g_assert (strcmp (chars, hello) == 0 && "read() didn't get same string back");

  close (fd);
  remove (template);

  strcpy (template, "fooXXXXXX.pdf");
  fd = g_mkstemp (template);
  g_assert (fd != -1 && "g_mkstemp didn't work for template fooXXXXXX.pdf");

  close (fd);
  remove (template);
}

static void
test_mkdtemp (void)
{
  char template[32], *retval;
  int fd;
  int i;

  strcpy (template, "foodir");
  retval = g_mkdtemp (template);
  if (retval != NULL)
    {
      g_warning ("g_mkdtemp works even if template doesn't contain XXXXXX");
      g_rmdir (retval);
    }

  strcpy (template, "foodir");
  retval = g_mkdtemp (template);
  if (retval != NULL)
    {
      g_warning ("g_mkdtemp works even if template contains less than six X");
      g_rmdir (retval);
    }

  strcpy (template, "fooXXXXXX");
  retval = g_mkdtemp (template);
  g_assert (retval != NULL && "g_mkdtemp didn't work for template fooXXXXXX");
  g_assert (retval == template && "g_mkdtemp allocated the resulting string?");
  g_assert (!g_file_test (template, G_FILE_TEST_IS_REGULAR));
  g_assert (g_file_test (template, G_FILE_TEST_IS_DIR));

  strcat (template, "/abc");
  fd = g_open (template, O_WRONLY | O_CREAT, 0600);
  g_assert (fd != -1 && "couldn't open file in temporary directory");
  close (fd);
  g_assert (g_file_test (template, G_FILE_TEST_IS_REGULAR));
  i = g_unlink (template);
  g_assert (i != -1 && "couldn't unlink file in temporary directory");

  template[9] = '\0';
  i = g_rmdir (template);
  g_assert (i != -1 && "couldn't remove temporary directory");

  strcpy (template, "fooXXXXXX.dir");
  retval = g_mkdtemp (template);
  g_assert (retval != NULL && "g_mkdtemp didn't work for template fooXXXXXX.dir");
  g_assert (g_file_test (template, G_FILE_TEST_IS_DIR));
  g_rmdir (template);
}

static void
test_readlink (void)
{
#ifdef HAVE_SYMLINK
  FILE *file;
  int result;
  char *filename = "file-test-data";
  char *link1 = "file-test-link1";
  char *link2 = "file-test-link2";
  char *link3 = "file-test-link3";
  char *data;
  GError *error;

  file = fopen (filename, "w");
  g_assert (file != NULL && "fopen() failed");
  fclose (file);

  result = symlink (filename, link1);
  g_assert (result == 0 && "symlink() failed");
  result = symlink (link1, link2);
  g_assert (result == 0 && "symlink() failed");
  
  error = NULL;
  data = g_file_read_link (link1, &error);
  g_assert (data != NULL && "couldn't read link1");
  g_assert (strcmp (data, filename) == 0 && "link1 contains wrong data");
  g_free (data);
  
  error = NULL;
  data = g_file_read_link (link2, &error);
  g_assert (data != NULL && "couldn't read link2");
  g_assert (strcmp (data, link1) == 0 && "link2 contains wrong data");
  g_free (data);
  
  error = NULL;
  data = g_file_read_link (link3, &error);
  g_assert (data == NULL && "could read link3");
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
  g_error_free (error);

  error = NULL;
  data = g_file_read_link (filename, &error);
  g_assert (data == NULL && "could read regular file as link");
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
  g_error_free (error);

  remove (filename);
  remove (link1);
  remove (link2);
#endif
}

static void
test_get_contents (void)
{
  const gchar *text = "abcdefghijklmnopqrstuvwxyz";
  const gchar *filename = "file-test-get-contents";
  gchar *contents;
  gsize len;
  FILE *f;
  GError *error = NULL;

  f = g_fopen (filename, "w");
  fwrite (text, 1, strlen (text), f);
  fclose (f);

  g_assert (g_file_test (filename, G_FILE_TEST_IS_REGULAR));

  if (! g_file_get_contents (filename, &contents, &len, &error))
    g_error ("g_file_get_contents() failed: %s", error->message);

  g_assert (strcmp (text, contents) == 0 && "content mismatch");

  g_free (contents);
}

int 
main (int argc, char *argv[])
{
  test_mkstemp ();
  test_mkdtemp ();
  test_readlink ();
  test_get_contents ();

  return 0;
}
