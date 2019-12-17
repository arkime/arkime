#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

typedef struct {
  const char *key;
  const char *str;
} Line;
  

static int 
compare_collate (const void *a, const void *b)
{
  const Line *line_a = a;
  const Line *line_b = b;

  return g_utf8_collate (line_a->str, line_b->str);
}

static int 
compare_key (const void *a, const void *b)
{
  const Line *line_a = a;
  const Line *line_b = b;

  return strcmp (line_a->key, line_b->key);
}

int main (int argc, char **argv)
{
  GIOChannel *in;
  GError *error = NULL;
  GArray *line_array = g_array_new (FALSE, FALSE, sizeof(Line));
  guint i;
  gboolean do_key = FALSE;
  gboolean do_file = FALSE;
  gchar *locale;

  /* FIXME: need to modify environment here,
   * since g_utf8_collate_key calls setlocal (LC_COLLATE, "")
   */
  g_setenv ("LC_ALL", "en_US", TRUE);
  locale = setlocale (LC_ALL, "");
  if (locale == NULL || strcmp (locale, "en_US") != 0)
    {
      fprintf (stderr, "No suitable locale, skipping test\n"); 
      return 2;
    }

  if (argc != 1 && argc != 2 && argc != 3)
    {
      fprintf (stderr, "Usage: unicode-collate [--key|--file] [FILE]\n");
      return 1;
    }

  i = 1;
  if (argc > 1)
    {
      if (strcmp (argv[1], "--key") == 0)
        {
          do_key = TRUE;
	  i = 2;
        }
      else if (strcmp (argv[1], "--file") == 0)
        {
          do_key = TRUE;
          do_file = TRUE;
	  i = 2;
        }
    }

 if (argc > i)
    {
      in = g_io_channel_new_file (argv[i], "r", &error);
      if (!in)
	{
	  fprintf (stderr, "Cannot open %s: %s\n", argv[i], error->message);
	  return 1;
	}
    }
  else
    {
      in = g_io_channel_unix_new (fileno (stdin));
    }

  while (TRUE)
    {
      gsize term_pos;
      gchar *str;
      Line line;

      if (g_io_channel_read_line (in, &str, NULL, &term_pos, &error) != G_IO_STATUS_NORMAL)
	break;

      str[term_pos] = '\0';

      if (do_file)
	line.key = g_utf8_collate_key_for_filename (str, -1);
      else
	line.key = g_utf8_collate_key (str, -1);
      line.str = str;

      g_array_append_val (line_array, line);
    }

  if (error)
    {
      fprintf (stderr, "Error reading test file, %s\n", error->message);
      return 1;
    }

  qsort (line_array->data, line_array->len, sizeof (Line), do_key ? compare_key : compare_collate);
  for (i = 0; i < line_array->len; i++)
    printf ("%s\n", g_array_index (line_array, Line, i).str);

  g_io_channel_unref (in);

  return 0;
}
