/* 
 * Copyright © 2008 Ryan Lortie
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * See the included COPYING file for more information.
 */

#include <string.h>
#include <stdio.h>
#include <glib.h>

/* keep track of GString instances to make sure nothing leaks */
static int strings_allocated;

/* === the GMarkupParser functions === */
static void
subparser_start_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         gpointer              user_data,
                         GError              **error)
{
  g_string_append_printf (user_data, "{%s}", element_name);

  /* we don't like trouble... */
  if (strcmp (element_name, "trouble") == 0)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 "we don't like trouble");
}

static void
subparser_end_element (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       gpointer              user_data,
                       GError              **error)
{
  g_string_append_printf (user_data, "{/%s}", element_name);
}

static void
subparser_error (GMarkupParseContext *context,
                 GError              *error,
                 gpointer             user_data)
{
  g_string_free (user_data, TRUE);
  strings_allocated--;
}

static GMarkupParser subparser_parser =
{
  subparser_start_element,
  subparser_end_element,
  NULL,
  NULL,
  subparser_error
};

/* convenience functions for a parser that does not
 * replay the starting tag into the subparser...
 */
static void
subparser_start (GMarkupParseContext *ctx)
{
  gpointer user_data;

  user_data = g_string_new (NULL);
  strings_allocated++;
  g_markup_parse_context_push (ctx, &subparser_parser, user_data);
}

static char *
subparser_end (GMarkupParseContext  *ctx,
               GError              **error)
{
  GString *string;
  char *result;

  string = g_markup_parse_context_pop (ctx);
  result = string->str;

  g_string_free (string, FALSE);
  strings_allocated--;

  if (result == NULL || result[0] == '\0')
    {
      g_free (result);
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "got no data");

      return NULL;
    }

  return result;
}

/* convenience functions for a parser that -does-
 * replay the starting tag into the subparser...
 */
static gboolean
replay_parser_start (GMarkupParseContext  *ctx,
                     const char           *element_name,
                     const char          **attribute_names,
                     const char          **attribute_values,
                     GError              **error)
{
  GError *tmp_error = NULL;
  gpointer user_data;

  user_data = g_string_new (NULL);
  strings_allocated++;

  subparser_parser.start_element (ctx, element_name,
                                  attribute_names, attribute_values,
                                  user_data, &tmp_error);

  if (tmp_error)
    {
      g_propagate_error (error, tmp_error);
      g_string_free (user_data, TRUE);
      strings_allocated--;

      return FALSE;
    }

  g_markup_parse_context_push (ctx, &subparser_parser, user_data);

  return TRUE;
}

static char *
replay_parser_end (GMarkupParseContext  *ctx,
                   GError              **error)
{
  GError *tmp_error = NULL;
  GString *string;
  char *result;

  string = g_markup_parse_context_pop (ctx);

  subparser_parser.end_element (ctx, g_markup_parse_context_get_element (ctx),
                                string, &tmp_error);

  if (tmp_error)
    {
      g_propagate_error (error, tmp_error);
      g_string_free (string, TRUE);
      strings_allocated--;

      return NULL;
    }

  result = string->str;

  g_string_free (string, FALSE);
  strings_allocated--;

  if (result == NULL || result[0] == '\0')
    {
      g_free (result);
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "got no data");

      return NULL;
    }

  return result;
}


/* === start interface between subparser and calling parser === */
static void      subparser_start      (GMarkupParseContext  *ctx);
static char     *subparser_end        (GMarkupParseContext  *ctx,
                                       GError              **error);
/* === end interface between subparser and calling parser === */

/* === start interface between replay parser and calling parser === */
static gboolean  replay_parser_start  (GMarkupParseContext  *ctx,
                                       const char           *element_name,
                                       const char          **attribute_names,
                                       const char          **attribute_values,
                                       GError              **error);
static char     *replay_parser_end    (GMarkupParseContext  *ctx,
                                       GError              **error);
/* === end interface between replay parser and calling parser === */



/* now comes our parser for the test.
 *
 * we recognise the tags <test> and <sub>.
 * <test> is ignored.
 * <sub> invokes the subparser (no replay).
 *
 * "unknown tags" are passed to the reply subparser
 * (so the unknown tag is fed to the subparser...)
 */
static void
start_element (GMarkupParseContext  *context,
               const gchar          *element_name,
               const gchar         **attribute_names,
               const gchar         **attribute_values,
               gpointer              user_data,
               GError              **error)
{
  g_string_append_printf (user_data, "<%s>", element_name);

  if (strcmp (element_name, "test") == 0)
    {
      /* do nothing */
    }
  else if (strcmp (element_name, "sub") == 0)
    {
      /* invoke subparser */
      subparser_start (context);
    }
  else
    {
      /* unknown tag.  invoke replay subparser */
      if (!replay_parser_start (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;
    }
}

static void
end_element (GMarkupParseContext  *context,
             const gchar          *element_name,
             gpointer              user_data,
             GError              **error)
{
  if (strcmp (element_name, "test") == 0)
    {
      /* do nothing */
    }
  else if (strcmp (element_name, "sub") == 0)
    {
      char *result;

      if ((result = subparser_end (context, error)) == NULL)
        return;

      g_string_append_printf (user_data, "<<%s>>", result);
      g_free (result);
    }
  else
    {
      char *result;

      if ((result = replay_parser_end (context, error)) == NULL)
        return;

      g_string_append_printf (user_data, "[[%s]]", result);
      g_free (result);
    }

  g_string_append_printf (user_data, "</%s>", element_name);
}

static GMarkupParser parser =
{
  start_element,
  end_element
};

typedef struct
{
  const char *markup;
  const char *result;
  const char *error_message;
} TestCase;

static void
test (gconstpointer user_data)
{
  const TestCase *tc = user_data;
  GMarkupParseContext *ctx;
  GString *string;
  gboolean result;
  GError *error;

  error = NULL;
  string = g_string_new (NULL);
  ctx = g_markup_parse_context_new (&parser, 0, string, NULL);
  result = g_markup_parse_context_parse (ctx, tc->markup,
                                         strlen (tc->markup), &error);
  if (result)
    result = g_markup_parse_context_end_parse (ctx, &error);
  g_markup_parse_context_free (ctx);
  g_assert (strings_allocated == 0);

  if (result)
    {
      if (tc->error_message)
        g_error ("expected failure (about '%s') passed!\n"
                 "  in: %s\n  out: %s",
                 tc->error_message, tc->markup, string->str);
    }
  else
    {
      if (!tc->error_message)
        g_error ("unexpected failure: '%s'\n"
                 "  in: %s\n  out: %s",
                 error->message, tc->markup, string->str);

      if (!strstr (error->message, tc->error_message))
        g_error ("failed for the wrong reason.\n"
                 "  expecting message about '%s'\n"
                 "  got message '%s'\n"
                 "  in: %s\n  out: %s",
                 tc->error_message, error->message, tc->markup, string->str);
    }

  if (strcmp (string->str, tc->result) != 0)
    g_error ("got the wrong result.\n"
             "  expected: '%s'\n"
             "  got: '%s'\n"
             "  input: %s",
             tc->result, string->str, tc->markup);

  if (error)
    g_error_free (error);

  g_string_free (string, TRUE);
}

TestCase test_cases[] = /* successful runs */
{
    /* in */                    /* out */
  { "<test/>",                  "<test></test>" },
  { "<sub><foo/></sub>",        "<sub><<{foo}{/foo}>></sub>" },
  { "<sub><foo/><bar/></sub>",  "<sub><<{foo}{/foo}{bar}{/bar}>></sub>" },
  { "<foo><bar/></foo>",        "<foo>[[{foo}{bar}{/bar}{/foo}]]</foo>" },
  { "<foo><x/><y/></foo>",      "<foo>[[{foo}{x}{/x}{y}{/y}{/foo}]]</foo>" },
  { "<foo/>",                   "<foo>[[{foo}{/foo}]]</foo>" },
  { "<sub><foo/></sub><bar/>",  "<sub><<{foo}{/foo}>></sub>"
                                "<bar>[[{bar}{/bar}]]</bar>" }
};

TestCase error_cases[] = /* error cases */
{
    /* in */                    /* out */                       /* error */
  { "<foo><>",                  "<foo>",                        ">"},
  { "",                         "",                             "empty" },
  { "<trouble/>",               "<trouble>",                    "trouble" },
  { "<sub><trouble>",           "<sub>",                        "trouble" },
  { "<foo><trouble>",           "<foo>",                        "trouble" },
  { "<sub></sub>",              "<sub>",                        "no data" },
  { "<sub/>",                   "<sub>",                        "no data" }
};

#define add_tests(func, basename, array) \
  G_STMT_START { \
    int __add_tests_i;                                                  \
                                                                        \
    for (__add_tests_i  = 0;                                            \
         __add_tests_i < G_N_ELEMENTS (array);                          \
         __add_tests_i++)                                               \
      {                                                                 \
        char *testname;                                                 \
                                                                        \
        testname = g_strdup_printf ("%s/%d", basename, __add_tests_i);  \
        g_test_add_data_func (testname, &array[__add_tests_i], func);   \
        g_free (testname);                                              \
      }                                                                 \
  } G_STMT_END

int
main (int argc, char **argv)
{
  g_setenv ("LC_ALL", "C", TRUE);
  g_test_init (&argc, &argv, NULL);
  add_tests (test, "/glib/markup/subparser/success", test_cases);
  add_tests (test, "/glib/markup/subparser/failure", error_cases);
  return g_test_run ();
}
