/* Unit tests for GOptionContext
 * Copyright (C) 2007 Openismus GmbH
 * Authors: Mathias Hasselmann
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

#include <glib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

static GOptionEntry main_entries[] = {
  { "main-switch", 0, 0,
    G_OPTION_ARG_NONE, NULL,
    "A switch that is in the main group", NULL },
  { NULL }
};

static GOptionEntry group_entries[] = {
  { "test-switch", 0, 0,
    G_OPTION_ARG_NONE, NULL,
    "A switch that is in the test group", NULL },
  { NULL }
};

static GOptionContext *
make_options (int test_number)
{
  GOptionContext *options;
  GOptionGroup   *group = NULL;
  gboolean have_main_entries = (0 != (test_number & 1));
  gboolean have_test_entries = (0 != (test_number & 2));

  options = g_option_context_new (NULL);

  if (have_main_entries)
    g_option_context_add_main_entries (options, main_entries, NULL);
  if (have_test_entries)
    {
      group = g_option_group_new ("test", "Test Options",
                                  "Show all test options",
                                  NULL, NULL);
      g_option_context_add_group (options, group);
      g_option_group_add_entries (group, group_entries);
    }

  return options;
}

static void
print_help (GOptionContext *options, gchar **argv)
{
  gint    argc = 3;
  GError *error = NULL;

  g_option_context_parse (options, &argc, &argv, &error);
  g_option_context_free (options);
  exit(0);
}

static void
test_group_captions_help (gconstpointer test_number)
{
  GOptionContext *options;
  gchar *argv[] = { __FILE__, "--help", NULL };

  options = make_options (GPOINTER_TO_INT (test_number));
  print_help (options, argv);
}

static void
test_group_captions_help_all (gconstpointer test_number)
{
  GOptionContext *options;
  gchar *argv[] = { __FILE__, "--help-all", NULL };

  options = make_options (GPOINTER_TO_INT (test_number));
  print_help (options, argv);
}

static void
test_group_captions_help_test (gconstpointer test_number)
{
  GOptionContext *options;
  gchar *argv[] = { __FILE__, "--help-test", NULL };

  options = make_options (GPOINTER_TO_INT (test_number));
  print_help (options, argv);
}

static void
test_group_captions (void)
{
  const gchar *test_name_base[] = { "help", "help-all", "help-test" };
  gchar *test_name;
  gint i, j;

  g_test_bug ("504142");

  for (i = 0; i < 4; ++i)
    {
      gboolean have_main_entries = (0 != (i & 1));
      gboolean have_test_entries = (0 != (i & 2));

      for (j = 0; j < G_N_ELEMENTS (test_name_base); ++j)
        {
          GTestSubprocessFlags trap_flags = 0;
          gboolean expect_main_description = FALSE;
          gboolean expect_main_switch      = FALSE;
          gboolean expect_test_description = FALSE;
          gboolean expect_test_switch      = FALSE;
          gboolean expect_test_group       = FALSE;

          if (g_test_verbose ())
            trap_flags |= G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR;

          test_name = g_strdup_printf ("/option/group/captions/subprocess/%s-%d",
                                       test_name_base[j], i);
          g_test_trap_subprocess (test_name, 0, trap_flags);
          g_free (test_name);
          g_test_trap_assert_passed ();
          g_test_trap_assert_stderr ("");

          switch (j)
            {
            case 0:
              g_assert_cmpstr ("help", ==, test_name_base[j]);
              expect_main_switch = have_main_entries;
              expect_test_group  = have_test_entries;
              break;

            case 1:
              g_assert_cmpstr ("help-all", ==, test_name_base[j]);
              expect_main_switch = have_main_entries;
              expect_test_switch = have_test_entries;
              expect_test_group  = have_test_entries;
              break;

            case 2:
              g_assert_cmpstr ("help-test", ==, test_name_base[j]);
              expect_test_switch = have_test_entries;
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          expect_main_description |= expect_main_switch;
          expect_test_description |= expect_test_switch;

          if (expect_main_description)
            g_test_trap_assert_stdout           ("*Application Options*");
          else
            g_test_trap_assert_stdout_unmatched ("*Application Options*");
          if (expect_main_switch)
            g_test_trap_assert_stdout           ("*--main-switch*");
          else
            g_test_trap_assert_stdout_unmatched ("*--main-switch*");

          if (expect_test_description)
            g_test_trap_assert_stdout           ("*Test Options*");
          else
            g_test_trap_assert_stdout_unmatched ("*Test Options*");
          if (expect_test_switch)
            g_test_trap_assert_stdout           ("*--test-switch*");
          else
            g_test_trap_assert_stdout_unmatched ("*--test-switch*");

          if (expect_test_group)
            g_test_trap_assert_stdout           ("*--help-test*");
          else
            g_test_trap_assert_stdout_unmatched ("*--help-test*");
        }
    }
}

int error_test1_int;
char *error_test2_string;
gboolean error_test3_boolean;

int arg_test1_int;
gchar *arg_test2_string;
gchar *arg_test3_filename;
gdouble arg_test4_double;
gdouble arg_test5_double;
gint64 arg_test6_int64;
gint64 arg_test6_int64_2;

gchar *callback_test1_string;
int callback_test2_int;

gchar *callback_test_optional_string;
gboolean callback_test_optional_boolean;

gchar **array_test1_array;

gboolean ignore_test1_boolean;
gboolean ignore_test2_boolean;
gchar *ignore_test3_string;

static gchar **
split_string (const char *str, int *argc)
{
  gchar **argv;
  int len;
  
  argv = g_strsplit (str, " ", 0);

  for (len = 0; argv[len] != NULL; len++);

  if (argc)
    *argc = len;
    
  return argv;
}

static gchar *
join_stringv (int argc, char **argv)
{
  int i;
  GString *str;

  str = g_string_new (NULL);

  for (i = 0; i < argc; i++)
    {
      g_string_append (str, argv[i]);

      if (i < argc - 1)
	g_string_append_c (str, ' ');
    }

  return g_string_free (str, FALSE);
}

/* Performs a shallow copy */
static char **
copy_stringv (char **argv, int argc)
{
  return g_memdup (argv, sizeof (char *) * (argc + 1));
}

static void
check_identical_stringv (gchar **before, gchar **after)
{
  guint i;

  /* Not only is it the same string... */
  for (i = 0; before[i] != NULL; i++)
    g_assert_cmpstr (before[i], ==, after[i]);

  /* ... it is actually the same pointer */
  for (i = 0; before[i] != NULL; i++)
    g_assert (before[i] == after[i]);

  g_assert (after[i] == NULL);
}


static gboolean
error_test1_pre_parse (GOptionContext *context,
		       GOptionGroup   *group,
		       gpointer	       data,
		       GError        **error)
{
  g_assert (error_test1_int == 0x12345678);

  return TRUE;
}

static gboolean
error_test1_post_parse (GOptionContext *context,
			GOptionGroup   *group,
			gpointer	  data,
			GError        **error)
{
  g_assert (error_test1_int == 20);

  /* Set an error in the post hook */
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, " ");

  return FALSE;
}

static void
error_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionGroup *main_group;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_INT, &error_test1_int, NULL, NULL },
      { NULL } };
  
  error_test1_int = 0x12345678;

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Set pre and post parse hooks */
  main_group = g_option_context_get_main_group (context);
  g_option_group_set_parse_hooks (main_group,
				  error_test1_pre_parse, error_test1_post_parse);
  
  /* Now try parsing */
  argv = split_string ("program --test 20", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval == FALSE);
  g_assert (error != NULL);
  /* An error occurred, so argv has not been changed */
  check_identical_stringv (argv_copy, argv);
  g_clear_error (&error);

  /* On failure, values should be reset */
  g_assert (error_test1_int == 0x12345678);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static gboolean
error_test2_pre_parse (GOptionContext *context,
		       GOptionGroup   *group,
		       gpointer	  data,
		       GError        **error)
{
  g_assert (strcmp (error_test2_string, "foo") == 0);

  return TRUE;
}

static gboolean
error_test2_post_parse (GOptionContext *context,
			GOptionGroup   *group,
			gpointer	  data,
			GError        **error)
{
  g_assert (strcmp (error_test2_string, "bar") == 0);

  /* Set an error in the post hook */
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, " ");

  return FALSE;
}

static void
error_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionGroup *main_group;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING, &error_test2_string, NULL, NULL },
      { NULL } };

  error_test2_string = "foo";

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Set pre and post parse hooks */
  main_group = g_option_context_get_main_group (context);
  g_option_group_set_parse_hooks (main_group,
				  error_test2_pre_parse, error_test2_post_parse);
  
  /* Now try parsing */
  argv = split_string ("program --test bar", &argc);
  argv_copy = copy_stringv (argv, argc);
  retval = g_option_context_parse (context, &argc, &argv, &error);

  g_assert (retval == FALSE);
  g_assert (error != NULL);
  check_identical_stringv (argv_copy, argv);
  g_clear_error (&error);

  g_assert (strcmp (error_test2_string, "foo") == 0);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static gboolean
error_test3_pre_parse (GOptionContext *context,
		       GOptionGroup   *group,
		       gpointer	  data,
		       GError        **error)
{
  g_assert (!error_test3_boolean);

  return TRUE;
}

static gboolean
error_test3_post_parse (GOptionContext *context,
			GOptionGroup   *group,
			gpointer	  data,
			GError        **error)
{
  g_assert (error_test3_boolean);

  /* Set an error in the post hook */
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, " ");

  return FALSE;
}

static void
error_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionGroup *main_group;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_NONE, &error_test3_boolean, NULL, NULL },
      { NULL } };

  error_test3_boolean = FALSE;

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Set pre and post parse hooks */
  main_group = g_option_context_get_main_group (context);
  g_option_group_set_parse_hooks (main_group,
				  error_test3_pre_parse, error_test3_post_parse);
  
  /* Now try parsing */
  argv = split_string ("program --test", &argc);
  argv_copy = copy_stringv (argv, argc);
  retval = g_option_context_parse (context, &argc, &argv, &error);

  g_assert (retval == FALSE);
  g_assert (error != NULL);
  check_identical_stringv (argv_copy, argv);
  g_clear_error (&error);

  g_assert (!error_test3_boolean);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
arg_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_INT, &arg_test1_int, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test 20 --test 30", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (arg_test1_int == 30);

  /* We free all of the strings in a copy of argv, because now argv is a
   * subset - some have been removed in-place
   */
  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
arg_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING, &arg_test2_string, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo --test bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (strcmp (arg_test2_string, "bar") == 0);

  g_free (arg_test2_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
arg_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_FILENAME, &arg_test3_filename, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo.txt", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (strcmp (arg_test3_filename, "foo.txt") == 0);

  g_free (arg_test3_filename);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}


static void
arg_test4 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv_copy;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_DOUBLE, &arg_test4_double, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test 20.0 --test 30.03", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (arg_test4_double == 30.03);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
arg_test5 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  char *old_locale, *current_locale;
  const char *locale = "de_DE.UTF-8";
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_DOUBLE, &arg_test5_double, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test 20,0 --test 30,03", &argc);
  argv_copy = copy_stringv (argv, argc);

  /* set it to some locale that uses commas instead of decimal points */
  
  old_locale = g_strdup (setlocale (LC_NUMERIC, locale));
  current_locale = setlocale (LC_NUMERIC, NULL);
  if (strcmp (current_locale, locale) != 0)
    {
      fprintf (stderr, "Cannot set locale to %s, skipping\n", locale);
      goto cleanup; 
    }

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (arg_test5_double == 30.03);

 cleanup:
  setlocale (LC_NUMERIC, old_locale);
  g_free (old_locale);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
arg_test6 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_INT64, &arg_test6_int64, NULL, NULL },
      { "test2", 0, 0, G_OPTION_ARG_INT64, &arg_test6_int64_2, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test 4294967297 --test 4294967296 --test2 0xfffffffff", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (arg_test6_int64 == G_GINT64_CONSTANT(4294967296));
  g_assert (arg_test6_int64_2 == G_GINT64_CONSTANT(0xfffffffff));

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static gboolean
callback_parse1 (const gchar *option_name, const gchar *value,
		 gpointer data, GError **error)
{
	callback_test1_string = g_strdup (value);
	return TRUE;
}

static void
callback_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_CALLBACK, callback_parse1, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo.txt", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (strcmp (callback_test1_string, "foo.txt") == 0);

  g_free (callback_test1_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static gboolean
callback_parse2 (const gchar *option_name, const gchar *value,
		 gpointer data, GError **error)
{
	callback_test2_int++;
	return TRUE;
}

static void
callback_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, callback_parse2, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test --test", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_test2_int == 2);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static gboolean
callback_parse_optional (const gchar *option_name, const gchar *value,
		 gpointer data, GError **error)
{
	callback_test_optional_boolean = TRUE;
	if (value)
		callback_test_optional_string = g_strdup (value);
	else
		callback_test_optional_string = NULL;
	return TRUE;
}

static void
callback_test_optional_1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo.txt", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (strcmp (callback_test_optional_string, "foo.txt") == 0);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
callback_test_optional_2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
callback_test_optional_3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv_copy;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -t foo.txt", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (strcmp (callback_test_optional_string, "foo.txt") == 0);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}


static void
callback_test_optional_4 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -t", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
callback_test_optional_5 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test --dummy", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
callback_test_optional_6 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -t -d", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
callback_test_optional_7 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -td", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
callback_test_optional_8 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -dt foo.txt", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_test_optional_string);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static GPtrArray *callback_remaining_args;
static gboolean
callback_remaining_test1_callback (const gchar *option_name, const gchar *value,
		         gpointer data, GError **error)
{
	g_ptr_array_add (callback_remaining_args, g_strdup (value));
	return TRUE;
}

static void
callback_remaining_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_CALLBACK, callback_remaining_test1_callback, NULL, NULL },
      { NULL } };
  
  callback_remaining_args = g_ptr_array_new ();
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo.txt blah.txt", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (callback_remaining_args->len == 2);
  g_assert (strcmp (callback_remaining_args->pdata[0], "foo.txt") == 0);
  g_assert (strcmp (callback_remaining_args->pdata[1], "blah.txt") == 0);

  g_ptr_array_foreach (callback_remaining_args, (GFunc) g_free, NULL);
  g_ptr_array_free (callback_remaining_args, TRUE);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static gboolean
callback_error (const gchar *option_name, const gchar *value,
                gpointer data, GError **error)
{
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "42");
  return FALSE;
}

static void
callback_returns_false (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "error", 0, 0, G_OPTION_ARG_CALLBACK, callback_error, NULL, NULL },
      { "error-no-arg", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, callback_error, NULL, NULL },
      { "error-optional-arg", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, callback_error, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --error value", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE);
  g_assert (retval == FALSE);
  check_identical_stringv (argv_copy, argv);

  g_option_context_free (context);
  g_clear_error (&error);
  g_strfreev (argv_copy);
  g_free (argv);

  /* And again, this time with a no-arg variant */
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  argv = split_string ("program --error-no-arg", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE);
  g_assert (retval == FALSE);
  check_identical_stringv (argv_copy, argv);

  g_option_context_free (context);
  g_clear_error (&error);
  g_strfreev (argv_copy);
  g_free (argv);

  /* And again, this time with a optional arg variant, with argument */
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  argv = split_string ("program --error-optional-arg value", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE);
  g_assert (retval == FALSE);
  check_identical_stringv (argv_copy, argv);

  g_option_context_free (context);
  g_clear_error (&error);
  g_strfreev (argv_copy);
  g_free (argv);

  /* And again, this time with a optional arg variant, without argument */
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  argv = split_string ("program --error-optional-arg", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE);
  g_assert (retval == FALSE);
  check_identical_stringv (argv_copy, argv);

  g_option_context_free (context);
  g_clear_error (&error);
  g_strfreev (argv_copy);
  g_free (argv);
}


static void
ignore_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv, **argv_copy;
  int argc;
  gchar *arg;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test --hello", &argc);
  argv_copy = copy_stringv (argv, argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  arg = join_stringv (argc, argv);
  g_assert (strcmp (arg, "program --hello") == 0);

  g_free (arg);
  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
ignore_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  gchar *arg;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_NONE, &ignore_test2_boolean, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -test", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  arg = join_stringv (argc, argv);
  g_assert (strcmp (arg, "program -es") == 0);

  g_free (arg);
  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
ignore_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv, **argv_copy;
  int argc;
  gchar *arg;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING, &ignore_test3_string, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo --hello", &argc);
  argv_copy = copy_stringv (argv, argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  arg = join_stringv (argc, argv);
  g_assert (strcmp (arg, "program --hello") == 0);

  g_assert (strcmp (ignore_test3_string, "foo") == 0);
  g_free (ignore_test3_string);

  g_free (arg);
  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

void
static array_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo --test bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);
  
  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
add_test1 (void)
{
  GOptionContext *context;

  GOptionEntry entries1 [] =
    { { "test1", 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, NULL, NULL },
      { NULL } };
  GOptionEntry entries2 [] =
    { { "test2", 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries1, NULL);
  g_option_context_add_main_entries (context, entries2, NULL);

  g_option_context_free (context);
}

static void
empty_test2 (void)
{
  GOptionContext *context;

  context = g_option_context_new (NULL);
  g_option_context_parse (context, NULL, NULL, NULL);
  
  g_option_context_free (context);
}

static void
empty_test3 (void)
{
  GOptionContext *context;
  gint argc;
  gchar **argv;

  argc = 0;
  argv = NULL;

  context = g_option_context_new (NULL);
  g_option_context_parse (context, &argc, &argv, NULL);
  
  g_option_context_free (context);
}

/* check that non-option arguments are left in argv by default */
static void
rest_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

/* check that -- works */
static void
rest_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -- -bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "--") == 0);
  g_assert (strcmp (argv[3], "-bar") == 0);
  g_assert (argv[4] == NULL);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

/* check that -- stripping works */
static void
rest_test2a (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -- bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
rest_test2b (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -bar --", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "-bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
rest_test2c (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo -- bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
rest_test2d (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test -- -bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "--") == 0);
  g_assert (strcmp (argv[2], "-bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}


/* check that G_OPTION_REMAINING collects non-option arguments */
static void
rest_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}


/* check that G_OPTION_REMAINING and -- work together */
static void
rest_test4 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -- -bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "-bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

/* test that G_OPTION_REMAINING works with G_OPTION_ARG_FILENAME_ARRAY */
static void
rest_test5 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test bar", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
unknown_short_test (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  GOptionEntry entries [] = { { NULL } };

  g_test_bug ("166609");

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -0", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (!retval);
  g_assert (error != NULL);
  g_clear_error (&error);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

/* test that lone dashes are treated as non-options */
static void
lonely_dash_test (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;

  g_test_bug ("168008");

  context = g_option_context_new (NULL);

  /* Now try parsing */
  argv = split_string ("program -", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);

  g_assert (argv[1] && strcmp (argv[1], "-") == 0);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
missing_arg_test (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  gchar *arg = NULL;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };

  g_test_bug ("305576");

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval == FALSE);
  g_assert (error != NULL);
  /* An error occurred, so argv has not been changed */
  check_identical_stringv (argv_copy, argv);
  g_clear_error (&error);

  g_strfreev (argv_copy);
  g_free (argv);

  /* Try parsing again */
  argv = split_string ("program -t", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval == FALSE);
  g_assert (error != NULL);
  /* An error occurred, so argv has not been changed */
  check_identical_stringv (argv_copy, argv);
  g_clear_error (&error);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static gchar *test_arg;

static gboolean cb (const gchar  *option_name,
                    const gchar  *value,
                    gpointer      data,
                    GError      **error)
{
  test_arg = g_strdup (value);
  return TRUE;
}

static void
dash_arg_test (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  gchar **argv_copy;
  int argc;
  gboolean argb = FALSE;
  GOptionEntry entries [] =
    { { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, cb, NULL, NULL },
      { "three", '3', 0, G_OPTION_ARG_NONE, &argb, NULL, NULL },
      { NULL } };

  g_test_bug ("577638");

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test=-3", &argc);
  argv_copy = copy_stringv (argv, argc);

  test_arg = NULL;
  error = NULL;
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);
  g_assert_no_error (error);
  g_assert_cmpstr (test_arg, ==, "-3");

  g_strfreev (argv_copy);
  g_free (argv);
  g_free (test_arg);
  test_arg = NULL;

  /* Try parsing again */
  argv = split_string ("program --test -3", &argc);
  argv_copy = copy_stringv (argv, argc);

  error = NULL;
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);
  g_assert (retval);
  g_assert_cmpstr (test_arg, ==, NULL);

  g_option_context_free (context);
  g_strfreev (argv_copy);
  g_free (argv);
}

static void
test_basic (void)
{
  GOptionContext *context;
  gchar *arg = NULL;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  g_assert (g_option_context_get_help_enabled (context));
  g_assert (!g_option_context_get_ignore_unknown_options (context));
  g_assert_cmpstr (g_option_context_get_summary (context), ==, NULL);
  g_assert_cmpstr (g_option_context_get_description (context), ==, NULL);

  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_summary (context, "summary");
  g_option_context_set_description(context, "description");

  g_assert (!g_option_context_get_help_enabled (context));
  g_assert (g_option_context_get_ignore_unknown_options (context));
  g_assert_cmpstr (g_option_context_get_summary (context), ==, "summary");
  g_assert_cmpstr (g_option_context_get_description (context), ==, "description");

  g_option_context_free (context);
}

typedef struct {
  gboolean parameter_seen;
  gboolean summary_seen;
  gboolean description_seen;
  gboolean destroyed;
} TranslateData;

static const gchar *
translate_func (const gchar *str,
                gpointer     data)
{
  TranslateData *d = data;

  if (strcmp (str, "parameter") == 0)
    d->parameter_seen = TRUE;
  else if (strcmp (str, "summary") == 0)
    d->summary_seen = TRUE;
  else if (strcmp (str, "description") == 0)
    d->description_seen = TRUE;
  
  return str;
}

static void
destroy_notify (gpointer data)
{
  TranslateData *d = data;

  d->destroyed = TRUE;
}

static void
test_translate (void)
{
  GOptionContext *context;
  gchar *arg = NULL;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };
  TranslateData data = { 0, };
  gchar *str;

  context = g_option_context_new ("parameter");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, "summary");
  g_option_context_set_description (context, "description");

  g_option_context_set_translate_func (context, translate_func, &data, destroy_notify);
  
  str = g_option_context_get_help (context, FALSE, NULL);
  g_free (str);
  g_option_context_free (context);

  g_assert (data.parameter_seen);
  g_assert (data.summary_seen);
  g_assert (data.description_seen);
  g_assert (data.destroyed);
}

static void
test_help (void)
{
  GOptionContext *context;
  GOptionGroup *group;
  gchar *str;
  gchar *arg = NULL;
  gchar **sarr = NULL;
  GOptionEntry entries[] = {
    { "test", 't', 0, G_OPTION_ARG_STRING, &arg, "Test tests", "Argument to use in test" },
    { "test2", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL, "Tests also", NULL },
    { "frob", 0, 0, G_OPTION_ARG_NONE, NULL, "Main frob", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &sarr, "Rest goes here", "REST" },
    { NULL }
  };
  GOptionEntry group_entries[] = {
    { "test", 't', 0, G_OPTION_ARG_STRING, &arg, "Group test", "Group test arg" },
    { "frob", 0, G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_NONE, NULL, "Group frob", NULL },
    { NULL }
  };

  context = g_option_context_new ("blabla");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, "Summary");
  g_option_context_set_description (context, "Description");

  group = g_option_group_new ("group1", "Group1-description", "Group1-help", NULL, NULL);
  g_option_group_add_entries (group, group_entries);

  g_option_context_add_group (context, group);

  str = g_option_context_get_help (context, FALSE, NULL);
  g_assert (strstr (str, "blabla") != NULL);
  g_assert (strstr (str, "Test tests") != NULL);
  g_assert (strstr (str, "Argument to use in test") != NULL);
  g_assert (strstr (str, "Tests also") == NULL);
  g_assert (strstr (str, "REST") != NULL);
  g_assert (strstr (str, "Summary") != NULL);
  g_assert (strstr (str, "Description") != NULL);
  g_assert (strstr (str, "--help") != NULL);
  g_assert (strstr (str, "--help-all") != NULL);
  g_assert (strstr (str, "--help-group1") != NULL);
  g_assert (strstr (str, "Group1-description") != NULL);
  g_assert (strstr (str, "Group1-help") != NULL);
  g_assert (strstr (str, "Group test arg") != NULL);
  g_assert (strstr (str, "Group frob") != NULL);
  g_assert (strstr (str, "Main frob") != NULL);
  g_assert (strstr (str, "--frob") != NULL);
  g_assert (strstr (str, "--group1-test") != NULL);
  g_assert (strstr (str, "--group1-frob") == NULL);
  g_free (str);

  g_option_context_free (context);
}

static void
test_help_no_options (void)
{
  GOptionContext *context;
  gchar **sarr = NULL;
  GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &sarr, "Rest goes here", "REST" },
    { NULL }
  };
  gchar *str;

  context = g_option_context_new ("blabla");
  g_option_context_add_main_entries (context, entries, NULL);

  str = g_option_context_get_help (context, FALSE, NULL);
  g_assert (strstr (str, "blabla") != NULL);
  g_assert (strstr (str, "REST") != NULL);
  g_assert (strstr (str, "Help Options") != NULL);
  g_assert (strstr (str, "Application Options") == NULL);

  g_free (str);
  g_option_context_free (context);
}

static void
test_help_no_help_options (void)
{
  GOptionContext *context;
  GOptionGroup *group;
  gchar *str;
  gchar *arg = NULL;
  gchar **sarr = NULL;
  GOptionEntry entries[] = {
    { "test", 't', 0, G_OPTION_ARG_STRING, &arg, "Test tests", "Argument to use in test" },
    { "test2", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL, "Tests also", NULL },
    { "frob", 0, 0, G_OPTION_ARG_NONE, NULL, "Main frob", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &sarr, "Rest goes here", "REST" },
    { NULL }
  };
  GOptionEntry group_entries[] = {
    { "test", 't', 0, G_OPTION_ARG_STRING, &arg, "Group test", "Group test arg" },
    { "frob", 0, G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_NONE, NULL, "Group frob", NULL },
    { NULL }
  };

  g_test_bug ("697652");

  context = g_option_context_new ("blabla");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, "Summary");
  g_option_context_set_description (context, "Description");
  g_option_context_set_help_enabled (context, FALSE);

  group = g_option_group_new ("group1", "Group1-description", "Group1-help", NULL, NULL);
  g_option_group_add_entries (group, group_entries);

  g_option_context_add_group (context, group);

  str = g_option_context_get_help (context, FALSE, NULL);
  g_assert (strstr (str, "blabla") != NULL);
  g_assert (strstr (str, "Test tests") != NULL);
  g_assert (strstr (str, "Argument to use in test") != NULL);
  g_assert (strstr (str, "Tests also") == NULL);
  g_assert (strstr (str, "REST") != NULL);
  g_assert (strstr (str, "Summary") != NULL);
  g_assert (strstr (str, "Description") != NULL);
  g_assert (strstr (str, "Help Options") == NULL);
  g_assert (strstr (str, "--help") == NULL);
  g_assert (strstr (str, "--help-all") == NULL);
  g_assert (strstr (str, "--help-group1") == NULL);
  g_assert (strstr (str, "Group1-description") != NULL);
  g_assert (strstr (str, "Group1-help") == NULL);
  g_assert (strstr (str, "Group test arg") != NULL);
  g_assert (strstr (str, "Group frob") != NULL);
  g_assert (strstr (str, "Main frob") != NULL);
  g_assert (strstr (str, "--frob") != NULL);
  g_assert (strstr (str, "--group1-test") != NULL);
  g_assert (strstr (str, "--group1-frob") == NULL);
  g_free (str);

  g_option_context_free (context);
}

static void
set_bool (gpointer data)
{
  gboolean *b = data;

  *b = TRUE;
}

static void
test_main_group (void)
{
  GOptionContext *context;
  GOptionGroup *group;
  gboolean b = FALSE;

  context = g_option_context_new (NULL);
  g_assert (g_option_context_get_main_group (context) == NULL);
  group = g_option_group_new ("name", "description", "hlep", &b, set_bool);
  g_option_context_add_group (context, group);
  group = g_option_group_new ("name2", "description", "hlep", NULL, NULL);
  g_option_context_add_group (context, group);
  g_assert (g_option_context_get_main_group (context) == NULL);
  group = g_option_group_new ("name", "description", "hlep", NULL, NULL);
  g_option_context_set_main_group (context, group);
  g_assert (g_option_context_get_main_group (context) == group);

  g_option_context_free (context);

  g_assert (b);
}

static gboolean error_func_called = FALSE;

static void
error_func (GOptionContext  *context,
            GOptionGroup    *group,
            gpointer         data,
            GError         **error)
{
  g_assert_cmpint (GPOINTER_TO_INT(data), ==, 1234);
  error_func_called = TRUE;
}

static void
test_error_hook (void)
{
  GOptionContext *context;
  gchar *arg = NULL;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };
  GOptionGroup *group;
  gchar **argv;
  gchar **argv_copy;
  gint argc;
  gboolean retval;
  GError *error = NULL;

  context = g_option_context_new (NULL);
  group = g_option_group_new ("name", "description", "hlep", GINT_TO_POINTER(1234), NULL);
  g_option_group_add_entries (group, entries);
  g_option_context_set_main_group (context, group);
  g_option_group_set_error_hook (g_option_context_get_main_group (context),
                                 error_func);

  argv = split_string ("program --test", &argc);
  argv_copy = copy_stringv (argv, argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval == FALSE);
  g_assert (error != NULL);
  /* An error occurred, so argv has not been changed */
  check_identical_stringv (argv_copy, argv);
  g_clear_error (&error);

  g_assert (error_func_called);

  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
test_group_parse (void)
{
  GOptionContext *context;
  GOptionGroup *group;
  gchar *arg1 = NULL;
  gchar *arg2 = NULL;
  gchar *arg3 = NULL;
  gchar *arg4 = NULL;
  gchar *arg5 = NULL;
  GOptionEntry entries[] = {
    { "test", 't', 0, G_OPTION_ARG_STRING, &arg1, NULL, NULL },
    { "faz", 'f', 0, G_OPTION_ARG_STRING, &arg2, NULL, NULL },
    { NULL }
  };
  GOptionEntry group_entries[] = {
    { "test", 0, 0, G_OPTION_ARG_STRING, &arg3, NULL, NULL },
    { "frob", 'f', 0, G_OPTION_ARG_STRING, &arg4, NULL, NULL },
    { "faz", 'z', 0, G_OPTION_ARG_STRING, &arg5, NULL, NULL },
    { NULL }
  };
  gchar **argv, **orig_argv;
  gint argc;
  GError *error = NULL;
  gboolean retval;

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);
  group = g_option_group_new ("group", "A group", "help for group", NULL, NULL);
  g_option_group_add_entries (group, group_entries);
  g_option_context_add_group (context, group);

  argv = split_string ("program --test arg1 -f arg2 --group-test arg3 --frob arg4 -z arg5", &argc);
  orig_argv = g_memdup (argv, (argc + 1) * sizeof (char *));

  retval = g_option_context_parse (context, &argc, &argv, &error);

  g_assert_no_error (error);
  g_assert (retval);
  g_assert_cmpstr (arg1, ==, "arg1");
  g_assert_cmpstr (arg2, ==, "arg2");
  g_assert_cmpstr (arg3, ==, "arg3");
  g_assert_cmpstr (arg4, ==, "arg4");
  g_assert_cmpstr (arg5, ==, "arg5");

  g_free (arg1);
  g_free (arg2);
  g_free (arg3);
  g_free (arg4);
  g_free (arg5);

  g_free (argv);
  g_strfreev (orig_argv);
  g_option_context_free (context);
}

static gint
option_context_parse_command_line (GOptionContext *context,
                                   const gchar    *command_line)
{
  gchar **argv;
  guint argv_len, argv_new_len;
  gboolean success;

  argv = split_string (command_line, NULL);
  argv_len = g_strv_length (argv);

  success = g_option_context_parse_strv (context, &argv, NULL);
  argv_new_len = g_strv_length (argv);

  g_strfreev (argv);
  return success ? argv_len - argv_new_len : -1;
}

static void
test_strict_posix (void)
{
  GOptionContext *context;
  gboolean foo;
  gboolean bar;
  GOptionEntry entries[] = {
    { "foo", 'f', 0, G_OPTION_ARG_NONE, &foo, NULL, NULL },
    { "bar", 'b', 0, G_OPTION_ARG_NONE, &bar, NULL, NULL },
    { NULL }
  };
  gint n_parsed;

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  foo = bar = FALSE;
  g_option_context_set_strict_posix (context, FALSE);
  n_parsed = option_context_parse_command_line (context, "program --foo command --bar");
  g_assert_cmpint (n_parsed, ==, 2);
  g_assert (foo == TRUE);
  g_assert (bar == TRUE);

  foo = bar = FALSE;
  g_option_context_set_strict_posix (context, TRUE);
  n_parsed = option_context_parse_command_line (context, "program --foo command --bar");
  g_assert_cmpint (n_parsed, ==, 1);
  g_assert (foo == TRUE);
  g_assert (bar == FALSE);

  foo = bar = FALSE;
  g_option_context_set_strict_posix (context, TRUE);
  n_parsed = option_context_parse_command_line (context, "program --foo --bar command");
  g_assert_cmpint (n_parsed, ==, 2);
  g_assert (foo == TRUE);
  g_assert (bar == TRUE);

  foo = bar = FALSE;
  g_option_context_set_strict_posix (context, TRUE);
  n_parsed = option_context_parse_command_line (context, "program command --foo --bar");
  g_assert_cmpint (n_parsed, ==, 0);
  g_assert (foo == FALSE);
  g_assert (bar == FALSE);

  g_option_context_free (context);
}

static void
flag_reverse_string (void)
{
  GOptionContext *context;
  gchar *arg = NULL;
  GOptionEntry entries [] =
    { { "test", 't', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };
  gchar **argv;
  gint argc;
  gboolean retval;
  GError *error = NULL;

  if (!g_test_undefined ())
    return;

  context = g_option_context_new (NULL);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "*ignoring reverse flag*");
  g_option_context_add_main_entries (context, entries, NULL);
  g_test_assert_expected_messages ();

  argv = split_string ("program --test bla", &argc);

  retval = g_option_context_parse_strv (context, &argv, &error);
  g_assert (retval == TRUE);
  g_assert_no_error (error);
  g_strfreev (argv);
  g_option_context_free (context);
  g_free (arg);
}

static void
flag_optional_int (void)
{
  GOptionContext *context;
  gint arg = 0;
  GOptionEntry entries [] =
    { { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_INT, &arg, NULL, NULL },
      { NULL } };
  gchar **argv;
  gint argc;
  gboolean retval;
  GError *error = NULL;

  if (!g_test_undefined ())
    return;

  context = g_option_context_new (NULL);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "*ignoring no-arg, optional-arg or filename flags*");
  g_option_context_add_main_entries (context, entries, NULL);
  g_test_assert_expected_messages ();

  argv = split_string ("program --test 5", &argc);

  retval = g_option_context_parse_strv (context, &argv, &error);
  g_assert (retval == TRUE);
  g_assert_no_error (error);
  g_strfreev (argv);
  g_option_context_free (context);
}

static void
short_remaining (void)
{
  gboolean ignore = FALSE;
  gboolean remaining = FALSE;
  gint number = 0;
  gchar* text = NULL;
  gchar** files = NULL;
  GError* error = NULL;
  GOptionEntry entries[] =
  {
    { "ignore", 'i', 0, G_OPTION_ARG_NONE, &ignore, NULL, NULL },
    { "remaining", 'r', 0, G_OPTION_ARG_NONE, &remaining, NULL, NULL },
    { "number", 'n', 0, G_OPTION_ARG_INT, &number, NULL, NULL },
    { "text", 't', 0, G_OPTION_ARG_STRING, &text, NULL, NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, NULL },
    { NULL }
  };
  GOptionContext* context;
  gchar **argv, **argv_copy;
  gint argc;

  g_test_bug ("729563");

  argv = split_string ("program -ri -n 4 -t hello file1 file2", &argc);
  argv_copy = copy_stringv (argv, argc);

  context = g_option_context_new (NULL);

  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  g_option_context_parse (context, &argc, &argv, &error);
  g_assert_no_error (error);

  g_assert (ignore);
  g_assert (remaining);
  g_assert_cmpint (number, ==, 4);
  g_assert_cmpstr (text, ==, "hello");
  g_assert_cmpstr (files[0], ==, "file1"); 
  g_assert_cmpstr (files[1], ==, "file2"); 
  g_assert (files[2] == NULL); 

  g_free (text);
  g_strfreev (files);
  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

static void
double_free (void)
{
  gchar* text = NULL;
  GOptionEntry entries[] =
  {
    { "known", 0, 0, G_OPTION_ARG_STRING, &text, NULL, NULL },
    { NULL }
  };
  GOptionContext* context;
  gchar **argv;
  gint argc;
  GError *error = NULL;

  g_test_bug ("646926");

  argv = split_string ("program --known=foo --known=bar --unknown=baz", &argc);

  context = g_option_context_new (NULL);

  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_ignore_unknown_options (context, FALSE);
  g_option_context_parse (context, &argc, &argv, &error);

  g_assert_error (error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION);
  g_assert_null (text);

  g_option_context_free (context);
  g_clear_error (&error);

}

int
main (int   argc,
      char *argv[])
{
  int i;
  gchar *test_name;

  g_setenv ("LC_ALL", "C", TRUE);
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/option/help/options", test_help);
  g_test_add_func ("/option/help/no-options", test_help_no_options);
  g_test_add_func ("/option/help/no-help-options", test_help_no_help_options);

  g_test_add_func ("/option/basic", test_basic);
  g_test_add_func ("/option/translate", test_translate);

  g_test_add_func ("/option/group/captions", test_group_captions);
  for (i = 0; i < 4; i++)
    {
      test_name = g_strdup_printf ("/option/group/captions/subprocess/help-%d", i);
      g_test_add_data_func (test_name, GINT_TO_POINTER (i),
                            test_group_captions_help);
      g_free (test_name);
      test_name = g_strdup_printf ("/option/group/captions/subprocess/help-all-%d", i);
      g_test_add_data_func (test_name, GINT_TO_POINTER (i),
                            test_group_captions_help_all);
      g_free (test_name);
      test_name = g_strdup_printf ("/option/group/captions/subprocess/help-test-%d", i);
      g_test_add_data_func (test_name, GINT_TO_POINTER (i),
                            test_group_captions_help_test);
                            
      g_free (test_name);
    }

  g_test_add_func ("/option/group/main", test_main_group);
  g_test_add_func ("/option/group/error-hook", test_error_hook);
  g_test_add_func ("/option/group/parse", test_group_parse);
  g_test_add_func ("/option/strict-posix", test_strict_posix);

  /* Test that restoration on failure works */
  g_test_add_func ("/option/restoration/int", error_test1);
  g_test_add_func ("/option/restoration/string", error_test2);
  g_test_add_func ("/option/restoration/boolean", error_test3);

  /* Test that special argument parsing works */
  g_test_add_func ("/option/arg/repetition/int", arg_test1);
  g_test_add_func ("/option/arg/repetition/string", arg_test2);
  g_test_add_func ("/option/arg/repetition/filename", arg_test3);
  g_test_add_func ("/option/arg/repetition/double", arg_test4);
  g_test_add_func ("/option/arg/repetition/locale", arg_test5);
  g_test_add_func ("/option/arg/repetition/int64", arg_test6);

  /* Test string arrays */
  g_test_add_func ("/option/arg/array/string", array_test1);

  /* Test callback args */
  g_test_add_func ("/option/arg/callback/string", callback_test1);
  g_test_add_func ("/option/arg/callback/count", callback_test2);

  /* Test optional arg flag for callback */
  g_test_add_func ("/option/arg/callback/optional1", callback_test_optional_1);
  g_test_add_func ("/option/arg/callback/optional2", callback_test_optional_2);
  g_test_add_func ("/option/arg/callback/optional3", callback_test_optional_3);
  g_test_add_func ("/option/arg/callback/optional4", callback_test_optional_4);
  g_test_add_func ("/option/arg/callback/optional5", callback_test_optional_5);
  g_test_add_func ("/option/arg/callback/optional6", callback_test_optional_6);
  g_test_add_func ("/option/arg/callback/optional7", callback_test_optional_7);
  g_test_add_func ("/option/arg/callback/optional8", callback_test_optional_8);

  /* Test callback with G_OPTION_REMAINING */
  g_test_add_func ("/option/arg/remaining/callback", callback_remaining_test1);

  /* Test callbacks which return FALSE */
  g_test_add_func ("/option/arg/remaining/callback-false", callback_returns_false);

  /* Test ignoring options */
  g_test_add_func ("/option/arg/ignore/long", ignore_test1);
  g_test_add_func ("/option/arg/ignore/short", ignore_test2);
  g_test_add_func ("/option/arg/ignore/arg", ignore_test3);
  g_test_add_func ("/option/context/add", add_test1);

  /* Test parsing empty args */
  /* Note there used to be an empty1 here, but it effectively moved
   * to option-argv0.c.
   */
  g_test_add_func ("/option/context/empty2", empty_test2);
  g_test_add_func ("/option/context/empty3", empty_test3);

  /* Test handling of rest args */
  g_test_add_func ("/option/arg/rest/non-option", rest_test1);
  g_test_add_func ("/option/arg/rest/separator1", rest_test2);
  g_test_add_func ("/option/arg/rest/separator2", rest_test2a);
  g_test_add_func ("/option/arg/rest/separator3", rest_test2b);
  g_test_add_func ("/option/arg/rest/separator4", rest_test2c);
  g_test_add_func ("/option/arg/rest/separator5", rest_test2d);
  g_test_add_func ("/option/arg/remaining/non-option", rest_test3);
  g_test_add_func ("/option/arg/remaining/separator", rest_test4);
  g_test_add_func ("/option/arg/remaining/array", rest_test5);

  /* Test some invalid flag combinations */
  g_test_add_func ("/option/arg/reverse-string", flag_reverse_string);
  g_test_add_func ("/option/arg/optional-int", flag_optional_int);

  /* regression tests for individual bugs */
  g_test_add_func ("/option/bug/unknown-short", unknown_short_test);
  g_test_add_func ("/option/bug/lonely-dash", lonely_dash_test);
  g_test_add_func ("/option/bug/missing-arg", missing_arg_test);
  g_test_add_func ("/option/bug/dash-arg", dash_arg_test);
  g_test_add_func ("/option/bug/short-remaining", short_remaining);
  g_test_add_func ("/option/bug/double-free", double_free);

  return g_test_run();
}
