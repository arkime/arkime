/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <stdio.h>
#include <locale.h>
#include <errno.h>

#include <glib.h>
#include <gio/gio.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#include <io.h>
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#endif

static gchar **locations = NULL;
static char *from_charset = NULL;
static char *to_charset = NULL;
static gboolean decompress = FALSE;
static gboolean compress = FALSE;
static gboolean gzip = FALSE;
static gboolean fallback = FALSE;

static GOptionEntry entries[] = {
  {"decompress", 0, 0, G_OPTION_ARG_NONE, &decompress, "decompress", NULL},
  {"compress", 0, 0, G_OPTION_ARG_NONE, &compress, "compress", NULL},
  {"gzip", 0, 0, G_OPTION_ARG_NONE, &gzip, "use gzip format", NULL},
  {"from-charset", 0, 0, G_OPTION_ARG_STRING, &from_charset, "from charset", NULL},
  {"to-charset", 0, 0, G_OPTION_ARG_STRING, &to_charset, "to charset", NULL},
  {"fallback", 0, 0, G_OPTION_ARG_NONE, &fallback, "use fallback", NULL},
  {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &locations, "locations", NULL},
  {NULL}
};

static void
decompressor_file_info_notify_cb (GZlibDecompressor *decompressor,
                                  GParamSpec *pspec,
                                  gpointer data)
{
  GFileInfo *file_info;
  const gchar *filename;

  file_info = g_zlib_decompressor_get_file_info (decompressor);
  if (file_info == NULL)
    return;

  filename = g_file_info_get_name (file_info);
  if (filename)
    g_printerr ("Decompressor filename: %s\n", filename);
}

static void
cat (GFile * file)
{
  GInputStream *in;
  char buffer[1024 * 8 + 1];
  char *p;
  gssize res;
  gboolean close_res;
  GError *error;
  GConverter *conv;
  GCharsetConverter *cconv = NULL;

  error = NULL;
  in = (GInputStream *) g_file_read (file, NULL, &error);
  if (in == NULL)
    {
      /* Translators: the first %s is the program name, the second one  */
      /* is the URI of the file, the third is the error message.        */
      g_printerr ("%s: %s: error opening file: %s\n",
		  g_get_prgname (), g_file_get_uri (file), error->message);
      g_error_free (error);
      return;
    }

  if (decompress)
    {
      GInputStream *old;
      conv = (GConverter *)g_zlib_decompressor_new (gzip?G_ZLIB_COMPRESSOR_FORMAT_GZIP:G_ZLIB_COMPRESSOR_FORMAT_ZLIB);
      old = in;
      in = (GInputStream *) g_converter_input_stream_new (in, conv);
      g_signal_connect (conv, "notify::file-info", G_CALLBACK (decompressor_file_info_notify_cb), NULL);
      g_object_unref (conv);
      g_object_unref (old);
    }

  if (from_charset && to_charset)
    {
      cconv = g_charset_converter_new (to_charset, from_charset, &error);
      conv = (GConverter *)cconv;
      if (conv)
	{
	  GInputStream *old;

	  g_charset_converter_set_use_fallback (cconv, fallback);

	  old = in;
	  in = (GInputStream *) g_converter_input_stream_new (in, conv);
	  g_object_unref (conv);
	  g_object_unref (old);
	}
      else
	{
	  g_printerr ("%s: Can't convert between charsets: %s\n",
		      g_get_prgname (), error->message);
	}
    }

  if (compress)
    {
      GInputStream *old;
      GFileInfo *in_file_info;

      in_file_info = g_file_query_info (file,
                                        G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                        G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                        G_FILE_QUERY_INFO_NONE,
                                        NULL,
                                        &error);
      if (in_file_info == NULL)
        {
          g_printerr ("%s: %s: error reading file info: %s\n",
                      g_get_prgname (), g_file_get_uri (file), error->message);
          g_error_free (error);
          return;
        }

      conv = (GConverter *)g_zlib_compressor_new(gzip?G_ZLIB_COMPRESSOR_FORMAT_GZIP:G_ZLIB_COMPRESSOR_FORMAT_ZLIB, -1);
      g_zlib_compressor_set_file_info (G_ZLIB_COMPRESSOR (conv), in_file_info);
      old = in;
      in = (GInputStream *) g_converter_input_stream_new (in, conv);
      g_object_unref (conv);
      g_object_unref (old);
      g_object_unref (in_file_info);
    }

  while (1)
    {
      res =
	g_input_stream_read (in, buffer, sizeof (buffer) - 1, NULL, &error);
      if (res > 0)
	{
	  gssize written;

	  p = buffer;
	  while (res > 0)
	    {
	      written = write (STDOUT_FILENO, p, res);

	      if (written == -1 && errno != EINTR)
		{
		  /* Translators: the first %s is the program name, the */
		  /* second one is the URI of the file.                 */
		  g_printerr ("%s: %s, error writing to stdout",
			      g_get_prgname (), g_file_get_uri (file));
		  goto out;
		}
	      res -= written;
	      p += written;
	    }
	}
      else if (res < 0)
	{
	  g_printerr ("%s: %s: error reading: %s\n",
		      g_get_prgname (), g_file_get_uri (file),
		      error->message);
	  g_error_free (error);
	  error = NULL;
	  break;
	}
      else if (res == 0)
	break;
    }

 out:

  close_res = g_input_stream_close (in, NULL, &error);
  if (!close_res)
    {
      g_printerr ("%s: %s:error closing: %s\n",
		  g_get_prgname (), g_file_get_uri (file), error->message);
      g_error_free (error);
    }

  if (cconv != NULL && fallback)
    {
      guint num = g_charset_converter_get_num_fallbacks (cconv);
      if (num > 0)
	g_printerr ("Number of fallback errors: %u\n", num);
    }
}

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context = NULL;
  GFile *file;
  int i;

  context =
    g_option_context_new ("LOCATION... - concatenate LOCATIONS "
			  "to standard output.");

  g_option_context_set_summary (context, "filter files");

  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_parse (context, &argc, &argv, &error);

  g_option_context_free (context);

  if (error != NULL)
    {
      g_printerr ("Error parsing commandline options: %s\n", error->message);
      g_printerr ("\n");
      g_printerr ("Try \"%s --help\" for more information.",
		  g_get_prgname ());
      g_printerr ("\n");
      g_error_free(error);
      return 1;
    }

  if (!locations)
    {
      g_printerr ("%s: missing locations", g_get_prgname ());
      g_printerr ("\n");
      g_printerr ("Try \"%s --help\" for more information.",
		  g_get_prgname ());
      g_printerr ("\n");
      return 1;
    }

  i = 0;

  do
    {
      file = g_file_new_for_commandline_arg (locations[i]);
      cat (file);
      g_object_unref (file);
    }
  while (locations[++i] != NULL);

  return 0;
}
