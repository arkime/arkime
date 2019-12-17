#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gboolean success = TRUE;

static char *
decode (const gchar *input)
{
  unsigned ch;
  int offset = 0;
  GString *result = g_string_new (NULL);
  
  do 
    {
      if (sscanf (input + offset, "%x", &ch) != 1)
	{
	  fprintf (stderr, "Error parsing character string %s\n", input);
	  exit (1);
	}

      g_string_append_unichar (result, ch);
      
      while (input[offset] && input[offset] != ' ')
	offset++;
      while (input[offset] && input[offset] == ' ')
	offset++;
    }
  while (input[offset]);

  return g_string_free (result, FALSE);
}

const char *names[4] = {
  "NFD",
  "NFC",
  "NFKD",
  "NFKC"
};

static char *
encode (const gchar *input)
{
  GString *result = g_string_new(NULL);

  const gchar *p = input;
  while (*p)
    {
      gunichar c = g_utf8_get_char (p);
      g_string_append_printf (result, "%04X ", c);
      p = g_utf8_next_char(p);
    }

  return g_string_free (result, FALSE);
}

static void
test_form (int            line,
	   GNormalizeMode mode,
	   gboolean       do_compat,
	   int            expected,
	   char         **c,
	   char         **raw)
{
  int i;
  
  gboolean mode_is_compat = (mode == G_NORMALIZE_NFKC ||
			     mode == G_NORMALIZE_NFKD);

  if (mode_is_compat || !do_compat)
    {
      for (i = 0; i < 3; i++)
	{
	  char *result = g_utf8_normalize (c[i], -1, mode);
	  if (strcmp (result, c[expected]) != 0)
	    {
	      char *result_raw = encode(result);
	      fprintf (stderr, "\nFailure: %d/%d: %s\n", line, i + 1, raw[5]);
	      fprintf (stderr, "  g_utf8_normalize (%s, %s) != %s but %s\n",
		   raw[i], names[mode], raw[expected], result_raw);
	      g_free (result_raw);
	      success = FALSE;
	    }
	  
	  g_free (result);
	}
    }
  if (mode_is_compat || do_compat)
    {
      for (i = 3; i < 5; i++)
	{
	  char *result = g_utf8_normalize (c[i], -1, mode);
	  if (strcmp (result, c[expected]) != 0)
	    {
	      char *result_raw = encode(result);
	      fprintf (stderr, "\nFailure: %d/%d: %s\n", line, i, raw[5]);
	      fprintf (stderr, "  g_utf8_normalize (%s, %s) != %s but %s\n",
		   raw[i], names[mode], raw[expected], result_raw);
	      g_free (result_raw);
	      success = FALSE;
	    }
	  
	  g_free (result);
	}
    }
}

static gboolean
process_one (int line, gchar **columns)
{
  char *c[5];
  int i;
  gboolean skip = FALSE;

  for (i=0; i < 5; i++)
    {
      c[i] = decode(columns[i]);
      if (!c[i])
	skip = TRUE;
    }

  if (!skip)
    {
      test_form (line, G_NORMALIZE_NFD, FALSE, 2, c, columns);
      test_form (line, G_NORMALIZE_NFD, TRUE, 4, c, columns);
      test_form (line, G_NORMALIZE_NFC, FALSE, 1, c, columns);
      test_form (line, G_NORMALIZE_NFC, TRUE, 3, c, columns);
      test_form (line, G_NORMALIZE_NFKD, TRUE, 4, c, columns);
      test_form (line, G_NORMALIZE_NFKC, TRUE, 3, c, columns);
    }

  for (i=0; i < 5; i++)
    g_free (c[i]);
  
  return TRUE;
}

int main (int argc, char **argv)
{
  GIOChannel *in;
  GError *error = NULL;
  GString *buffer = g_string_new (NULL);
  int line_to_do = 0;
  int line = 1;

  if (argc != 2 && argc != 3)
    {
      fprintf (stderr, "Usage: unicode-normalize NormalizationTest.txt LINE\n");
      return 1;
    }

  if (argc == 3)
    line_to_do = atoi(argv[2]);

  in = g_io_channel_new_file (argv[1], "r", &error);
  if (!in)
    {
      fprintf (stderr, "Cannot open %s: %s\n", argv[1], error->message);
      return 1;
    }

  while (TRUE)
    {
      gsize term_pos;
      gchar **columns;

      if (g_io_channel_read_line_string (in, buffer, &term_pos, &error) != G_IO_STATUS_NORMAL)
	break;
	
      if (line_to_do && line != line_to_do)
	goto next;
      
      buffer->str[term_pos] = '\0';
      
      if (buffer->str[0] == '#') /* Comment */
	goto next;
      if (buffer->str[0] == '@') /* Part */
	{
	  fprintf (stderr, "\nProcessing %s\n", buffer->str + 1);
	  goto next;
	}
      
      columns = g_strsplit (buffer->str, ";", -1);
      if (!columns[0])
	goto next;
      
      if (!process_one (line, columns))
	return 1;
      g_strfreev (columns);

    next:
      g_string_truncate (buffer, 0);
      line++;
    }

  if (error)
    {
      fprintf (stderr, "Error reading test file, %s\n", error->message);
      return 1;
    }

  g_io_channel_unref (in);
  g_string_free (buffer, TRUE);

  return !success;
}
