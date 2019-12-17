/*
 * Copyright © 2009 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include <gio/gio.h>
#include <string.h>

#define MAX_PIECE_SIZE  100
#define MAX_PIECES       60

static gchar *
cook_piece (void)
{
  char buffer[MAX_PIECE_SIZE * 2];
  gint symbols, i = 0;

  symbols = g_test_rand_int_range (1, MAX_PIECE_SIZE + 1);

  while (symbols--)
    {
      gint c = g_test_rand_int_range (0, 30);

      switch (c)
        {
         case 26:
          buffer[i++] = '\n';
         case 27:
          buffer[i++] = '\r';
          break;

         case 28:
          buffer[i++] = '\r';
         case 29:
          buffer[i++] = '\n';
          break;

         default:
          buffer[i++] = c + 'a';
          break;
        }

      g_assert_cmpint (i, <=, sizeof buffer);
    }

  return g_strndup (buffer, i);
}

static gchar **
cook_pieces (void)
{
  gchar **array;
  gint pieces;

  pieces = g_test_rand_int_range (0, MAX_PIECES + 1);
  array = g_new (char *, pieces + 1);
  array[pieces] = NULL;

  while (pieces--)
    array[pieces] = cook_piece ();

  return array;
}

typedef struct
{
  GInputStream parent_instance;

  gboolean built_to_fail;
  gchar **pieces;
  gint index;

  const gchar *current;
} SleepyStream;

typedef GInputStreamClass SleepyStreamClass;

GType sleepy_stream_get_type (void);

G_DEFINE_TYPE (SleepyStream, sleepy_stream, G_TYPE_INPUT_STREAM)

static gssize
sleepy_stream_read (GInputStream  *stream,
                    void          *buffer,
                    gsize          length,
                    GCancellable  *cancellable,
                    GError       **error)
{
  SleepyStream *sleepy = (SleepyStream *) stream;

  if (sleepy->pieces[sleepy->index] == NULL)
    {
      if (sleepy->built_to_fail)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "fail");
          return -1;
        }
      else
        return 0;
    }
  else
    {
      if (!sleepy->current)
        sleepy->current = sleepy->pieces[sleepy->index++];

      length = MIN (strlen (sleepy->current), length);
      memcpy (buffer, sleepy->current, length);

      sleepy->current += length;
      if (*sleepy->current == '\0')
        sleepy->current = NULL;

      return length;
    }
}

static void
sleepy_stream_init (SleepyStream *sleepy)
{
  sleepy->pieces = cook_pieces ();
  sleepy->built_to_fail = FALSE;
  sleepy->index = 0;
}

static void
sleepy_stream_finalize (GObject *object)
{
  SleepyStream *sleepy = (SleepyStream *) object;

  g_strfreev (sleepy->pieces);
  G_OBJECT_CLASS (sleepy_stream_parent_class)
    ->finalize (object);
}

static void
sleepy_stream_class_init (SleepyStreamClass *class)
{
  G_OBJECT_CLASS (class)->finalize = sleepy_stream_finalize;
  class->read_fn = sleepy_stream_read;

  /* no read_async implementation.
   * main thread will sleep while read runs in a worker.
   */
}

static SleepyStream *
sleepy_stream_new (void)
{
  return g_object_new (sleepy_stream_get_type (), NULL);
}

static gboolean
read_line (GDataInputStream  *stream,
           GString           *string,
           const gchar       *eol,
           GError           **error)
{
  gsize length;
  char *str;

  str = g_data_input_stream_read_line (stream, &length, NULL, error);

  if (str == NULL)
    return FALSE;

  g_assert (strstr (str, eol) == NULL);
  g_assert (strlen (str) == length);

  g_string_append (string, str);
  g_string_append (string, eol);
  g_free (str);

  return TRUE;
}

static void
build_comparison (GString      *str,
                  SleepyStream *stream)
{
  /* build this for comparison */
  gint i;

  for (i = 0; stream->pieces[i]; i++)
    g_string_append (str, stream->pieces[i]);

  if (str->len && str->str[str->len - 1] != '\n')
    g_string_append_c (str, '\n');
}


static void
test (void)
{
  SleepyStream *stream = sleepy_stream_new ();
  GDataInputStream *data;
  GError *error = NULL;
  GString *one;
  GString *two;

  one = g_string_new (NULL);
  two = g_string_new (NULL);

  data = g_data_input_stream_new (G_INPUT_STREAM (stream));
  g_data_input_stream_set_newline_type (data, G_DATA_STREAM_NEWLINE_TYPE_LF);
  build_comparison (one, stream);

  while (read_line (data, two, "\n", &error));

  g_assert_cmpstr (one->str, ==, two->str);
  g_string_free (one, TRUE);
  g_string_free (two, TRUE);
  g_object_unref (stream);
  g_object_unref (data);
}

static GDataInputStream *data;
static GString *one, *two;
static GMainLoop *loop;
static const gchar *eol;

static void
asynch_ready (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  GError *error = NULL;
  gsize length;
  gchar *str;

  g_assert (data == G_DATA_INPUT_STREAM (object));

  str = g_data_input_stream_read_line_finish (data, result, &length, &error);

  if (str == NULL)
    {
      g_main_loop_quit (loop);
      if (error)
        g_error_free (error);
    }
  else
    {
      g_assert (length == strlen (str));
      g_string_append (two, str);
      g_string_append (two, eol);
      g_free (str);

      /* MOAR!! */
      g_data_input_stream_read_line_async (data, 0, NULL, asynch_ready, NULL);
    }
}


static void
asynch (void)
{
  SleepyStream *sleepy = sleepy_stream_new ();

  data = g_data_input_stream_new (G_INPUT_STREAM (sleepy));
  one = g_string_new (NULL);
  two = g_string_new (NULL);
  eol = "\n";

  build_comparison (one, sleepy);
  g_data_input_stream_read_line_async (data, 0, NULL, asynch_ready, NULL);
  g_main_loop_run (loop = g_main_loop_new (NULL, FALSE));

  g_assert_cmpstr (one->str, ==, two->str);
  g_string_free (one, TRUE);
  g_string_free (two, TRUE);
  g_object_unref (sleepy);
  g_object_unref (data);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/filter-stream/input", test);
  g_test_add_func ("/filter-stream/async", asynch);

  return g_test_run();
}
