
#include <glib.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>

static GKeyFile *
load_data (const gchar   *data,
           GKeyFileFlags  flags)
{
  GKeyFile *keyfile;
  GError *error = NULL;

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, flags, &error);
  g_assert_no_error (error);
  return keyfile;
}

static void
check_error (GError **error,
             GQuark   domain,
             gint     code)
{
  g_assert_error (*error, domain, code);
  g_error_free (*error);
  *error = NULL;
}

static void
check_no_error (GError **error)
{
  g_assert_no_error (*error);
}

static void
check_string_value (GKeyFile    *keyfile,
                    const gchar *group,
                    const gchar *key,
                    const gchar *expected)
{
  GError *error = NULL;
  gchar *value;

  value = g_key_file_get_string (keyfile, group, key, &error);
  check_no_error (&error);
  g_assert (value != NULL);
  g_assert_cmpstr (value, ==, expected);
  g_free (value);
}

static void
check_locale_string_value (GKeyFile    *keyfile,
                           const gchar *group,
                           const gchar *key,
                           const gchar *locale,
                           const gchar *expected)
{
  GError *error = NULL;
  gchar *value;

  value = g_key_file_get_locale_string (keyfile, group, key, locale, &error);
  check_no_error (&error);
  g_assert (value != NULL);
  g_assert_cmpstr (value, ==, expected);
  g_free (value);
}

static void
check_string_locale_value (GKeyFile    *keyfile,
                           const gchar *group,
                           const gchar *key,
                           const gchar *locale,
                           const gchar *expected)
{
  gchar *value;

  value = g_key_file_get_locale_for_key (keyfile, group, key, locale);
  g_assert_cmpstr (value, ==, expected);
  g_free (value);
}

static void
check_string_list_value (GKeyFile    *keyfile,
                         const gchar *group,
                         const gchar *key,
                         ...)
{
  gint i;
  gchar *v, **value;
  va_list args;
  gsize len;
  GError *error = NULL;

  value = g_key_file_get_string_list (keyfile, group, key, &len, &error);
  check_no_error (&error);
  g_assert (value != NULL);

  va_start (args, key);
  i = 0;
  v = va_arg (args, gchar*);
  while (v)
    {
      g_assert (value[i] != NULL);
      g_assert_cmpstr (v, ==, value[i]);
      i++;
      v = va_arg (args, gchar*);
    }

  va_end (args);

  g_strfreev (value);
}

static void
check_locale_string_list_value (GKeyFile    *keyfile,
                                const gchar *group,
                                const gchar *key,
                                const gchar *locale,
                                ...)
{
  gint i;
  gchar *v, **value;
  va_list args;
  gsize len;
  GError *error = NULL;

  value = g_key_file_get_locale_string_list (keyfile, group, key, locale, &len, &error);
  check_no_error (&error);
  g_assert (value != NULL);

  va_start (args, locale);
  i = 0;
  v = va_arg (args, gchar*);
  while (v)
    {
      g_assert (value[i] != NULL);
      g_assert_cmpstr (v, ==, value[i]);
      i++;
      v = va_arg (args, gchar*);
    }

  va_end (args);

  g_strfreev (value);
}

static void
check_integer_list_value (GKeyFile    *keyfile,
                          const gchar *group,
                          const gchar *key,
                          ...)
{
  gint i;
  gint v, *value;
  va_list args;
  gsize len;
  GError *error = NULL;

  value = g_key_file_get_integer_list (keyfile, group, key, &len, &error);
  check_no_error (&error);
  g_assert (value != NULL);

  va_start (args, key);
  i = 0;
  v = va_arg (args, gint);
  while (v != -100)
    {
      g_assert_cmpint (i, <, len);
      g_assert_cmpint (value[i], ==, v);
      i++;
      v = va_arg (args, gint);
    }

  va_end (args);

  g_free (value);
}

static void
check_double_list_value (GKeyFile    *keyfile,
                          const gchar *group,
                          const gchar *key,
                          ...)
{
  gint i;
  gdouble v, *value;
  va_list args;
  gsize len;
  GError *error = NULL;

  value = g_key_file_get_double_list (keyfile, group, key, &len, &error);
  check_no_error (&error);
  g_assert (value != NULL);

  va_start (args, key);
  i = 0;
  v = va_arg (args, gdouble);
  while (v != -100)
    {
      g_assert_cmpint (i, <, len);
      g_assert_cmpfloat (value[i], ==, v);
      i++;
      v = va_arg (args, gdouble);
    }

  va_end (args);

  g_free (value);
}

static void
check_boolean_list_value (GKeyFile    *keyfile,
                          const gchar *group,
                          const gchar *key,
                          ...)
{
  gint i;
  gboolean v, *value;
  va_list args;
  gsize len;
  GError *error = NULL;

  value = g_key_file_get_boolean_list (keyfile, group, key, &len, &error);
  check_no_error (&error);
  g_assert (value != NULL);

  va_start (args, key);
  i = 0;
  v = va_arg (args, gboolean);
  while (v != -100)
    {
      g_assert_cmpint (i, <, len);
      g_assert_cmpint (value[i], ==, v);
      i++;
      v = va_arg (args, gboolean);
    }

  va_end (args);

  g_free (value);
}

static void
check_boolean_value (GKeyFile    *keyfile,
                     const gchar *group,
                     const gchar *key,
                     gboolean     expected)
{
  GError *error = NULL;
  gboolean value;

  value = g_key_file_get_boolean (keyfile, group, key, &error);
  check_no_error (&error);
  g_assert_cmpint (value, ==, expected);
}

static void
check_integer_value (GKeyFile    *keyfile,
                     const gchar *group,
                     const gchar *key,
                     gint         expected)
{
  GError *error = NULL;
  gint value;

  value = g_key_file_get_integer (keyfile, group, key, &error);
  check_no_error (&error);
  g_assert_cmpint (value, ==, expected);
}

static void
check_double_value (GKeyFile    *keyfile,
                     const gchar *group,
                     const gchar *key,
                     gdouble      expected)
{
  GError *error = NULL;
  gdouble value;

  value = g_key_file_get_double (keyfile, group, key, &error);
  check_no_error (&error);
  g_assert_cmpfloat (value, ==, expected);
}

static void
check_name (const gchar *what,
            const gchar *value,
            const gchar *expected,
            gint         position)
{
  g_assert_cmpstr (value, ==, expected);
}

static void
check_length (const gchar *what,
              gint         n_items,
              gint         length,
              gint         expected)
{
  g_assert_cmpint (n_items, ==, length);
  g_assert_cmpint (n_items, ==, expected);
}


/* check that both \n and \r\n are accepted as line ends,
 * and that stray \r are passed through
 */
static void
test_line_ends (void)
{
  GKeyFile *keyfile;

  const gchar *data =
    "[group1]\n"
    "key1=value1\n"
    "key2=value2\r\n"
    "[group2]\r\n"
    "key3=value3\r\r\n"
    "key4=value4\n";

  keyfile = load_data (data, 0);

  check_string_value (keyfile, "group1", "key1", "value1");
  check_string_value (keyfile, "group1", "key2", "value2");
  check_string_value (keyfile, "group2", "key3", "value3\r");
  check_string_value (keyfile, "group2", "key4", "value4");

  g_key_file_free (keyfile);
}

/* check handling of whitespace
 */
static void
test_whitespace (void)
{
  GKeyFile *keyfile;

  const gchar *data =
    "[group1]\n"
    "key1 = value1\n"
    "key2\t=\tvalue2\n"
    " [ group2 ] \n"
    "key3  =  value3  \n"
    "key4  =  value \t4\n"
    "  key5  =  value5\n";

  keyfile = load_data (data, 0);

  check_string_value (keyfile, "group1", "key1", "value1");
  check_string_value (keyfile, "group1", "key2", "value2");
  check_string_value (keyfile, " group2 ", "key3", "value3  ");
  check_string_value (keyfile, " group2 ", "key4", "value \t4");
  check_string_value (keyfile, " group2 ", "key5", "value5");

  g_key_file_free (keyfile);
}

/* check handling of comments
 */
static void
test_comments (void)
{
  GKeyFile *keyfile;
  gchar **names;
  gsize len;
  GError *error = NULL;
  gchar *comment;

  const gchar *data =
    "# top comment\n"
    "# top comment, continued\n"
    "[group1]\n"
    "key1 = value1\n"
    "# key comment\n"
    "# key comment, continued\n"
    "key2 = value2\n"
    "# line end check\r\n"
    "key3 = value3\n"
    "key4 = value4\n"
    "# group comment\n"
    "# group comment, continued\n"
    "[group2]\n";

  const gchar *top_comment= " top comment\n top comment, continued\n";
  const gchar *group_comment= " group comment\n group comment, continued\n";
  const gchar *key_comment= " key comment\n key comment, continued\n";

  keyfile = load_data (data, 0);

  check_string_value (keyfile, "group1", "key1", "value1");
  check_string_value (keyfile, "group1", "key2", "value2");
  check_string_value (keyfile, "group1", "key3", "value3");
  check_string_value (keyfile, "group1", "key4", "value4");

  names = g_key_file_get_keys (keyfile, "group1", &len, &error);
  check_no_error (&error);

  check_length ("keys", g_strv_length (names), len, 4);
  check_name ("key", names[0], "key1", 0);
  check_name ("key", names[1], "key2", 1);
  check_name ("key", names[2], "key3", 2);
  check_name ("key", names[3], "key4", 3);

  g_strfreev (names);

  g_key_file_free (keyfile);

  keyfile = load_data (data, G_KEY_FILE_KEEP_COMMENTS);

  names = g_key_file_get_keys (keyfile, "group1", &len, &error);
  check_no_error (&error);

  check_length ("keys", g_strv_length (names), len, 4);
  check_name ("key", names[0], "key1", 0);
  check_name ("key", names[1], "key2", 1);
  check_name ("key", names[2], "key3", 2);
  check_name ("key", names[3], "key4", 3);

  g_strfreev (names);

  comment = g_key_file_get_comment (keyfile, NULL, NULL, &error);
  check_no_error (&error);
  check_name ("top comment", comment, top_comment, 0);
  g_free (comment);

  comment = g_key_file_get_comment (keyfile, "group1", "key2", &error);
  check_no_error (&error);
  check_name ("key comment", comment, key_comment, 0);
  g_free (comment);

  g_key_file_remove_comment (keyfile, "group1", "key2", &error);
  check_no_error (&error);
  comment = g_key_file_get_comment (keyfile, "group1", "key2", &error);
  check_no_error (&error);
  g_assert (comment == NULL);

  comment = g_key_file_get_comment (keyfile, "group2", NULL, &error);
  check_no_error (&error);
  check_name ("group comment", comment, group_comment, 0);
  g_free (comment);

  comment = g_key_file_get_comment (keyfile, "group3", NULL, &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_assert (comment == NULL);

  g_key_file_free (keyfile);
}


/* check key and group listing */
static void
test_listing (void)
{
  GKeyFile *keyfile;
  gchar **names;
  gsize len;
  gchar *start;
  GError *error = NULL;

  const gchar *data =
    "[group1]\n"
    "key1=value1\n"
    "key2=value2\n"
    "[group2]\n"
    "key3=value3\n"
    "key4=value4\n";

  keyfile = load_data (data, 0);

  names = g_key_file_get_groups (keyfile, &len);
  g_assert (names != NULL);

  check_length ("groups", g_strv_length (names), len, 2);
  check_name ("group name", names[0], "group1", 0);
  check_name ("group name", names[1], "group2", 1);

  g_strfreev (names);

  names = g_key_file_get_keys (keyfile, "group1", &len, &error);
  check_no_error (&error);

  check_length ("keys", g_strv_length (names), len, 2);
  check_name ("key", names[0], "key1", 0);
  check_name ("key", names[1], "key2", 1);

  g_strfreev (names);

  names = g_key_file_get_keys (keyfile, "no-such-group", &len, &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);

  g_strfreev (names);

  g_assert (g_key_file_has_group (keyfile, "group1"));
  g_assert (g_key_file_has_group (keyfile, "group2"));
  g_assert (!g_key_file_has_group (keyfile, "group10"));
  g_assert (!g_key_file_has_group (keyfile, "group20"));

  start = g_key_file_get_start_group (keyfile);
  g_assert_cmpstr (start, ==, "group1");
  g_free (start);

  g_assert (g_key_file_has_key (keyfile, "group1", "key1", &error));
  check_no_error (&error);
  g_assert (g_key_file_has_key (keyfile, "group2", "key3", &error));
  check_no_error (&error);
  g_assert (!g_key_file_has_key (keyfile, "group2", "no-such-key", NULL));

  g_key_file_has_key (keyfile, "no-such-group", "key", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);

  g_key_file_free (keyfile);
}

/* check parsing of string values */
static void
test_string (void)
{
  GKeyFile *keyfile;
  GError *error = NULL;
  gchar *value;
  const gchar * const list[3] = {
    "one",
    "two;andahalf",
    "3",
  };
  const gchar *data =
    "[valid]\n"
    "key1=\\s\\n\\t\\r\\\\\n"
    "key2=\"quoted\"\n"
    "key3='quoted'\n"
    "key4=\xe2\x89\xa0\xe2\x89\xa0\n"
    "key5=  leading space\n"
    "key6=trailing space  \n"
    "[invalid]\n"
    "key1=\\a\\b\\0800xff\n"
    "key2=blabla\\\n";

  keyfile = load_data (data, 0);

  check_string_value (keyfile, "valid", "key1", " \n\t\r\\");
  check_string_value (keyfile, "valid", "key2", "\"quoted\"");
  check_string_value (keyfile, "valid", "key3", "'quoted'");
  check_string_value (keyfile, "valid", "key4", "\xe2\x89\xa0\xe2\x89\xa0");
  check_string_value (keyfile, "valid", "key5", "leading space");
  check_string_value (keyfile, "valid", "key6", "trailing space  ");

  value = g_key_file_get_string (keyfile, "invalid", "key1", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
  g_free (value);

  value = g_key_file_get_string (keyfile, "invalid", "key2", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
  g_free (value);

  g_key_file_set_string (keyfile, "inserted", "key1", "simple");
  g_key_file_set_string (keyfile, "inserted", "key2", " leading space");
  g_key_file_set_string (keyfile, "inserted", "key3", "\tleading tab");
  g_key_file_set_string (keyfile, "inserted", "key4", "new\nline");
  g_key_file_set_string (keyfile, "inserted", "key5", "carriage\rreturn");
  g_key_file_set_string (keyfile, "inserted", "key6", "slash\\yay!");
  g_key_file_set_string_list (keyfile, "inserted", "key7", list, 3);

  check_string_value (keyfile, "inserted", "key1", "simple");
  check_string_value (keyfile, "inserted", "key2", " leading space");
  check_string_value (keyfile, "inserted", "key3", "\tleading tab");
  check_string_value (keyfile, "inserted", "key4", "new\nline");
  check_string_value (keyfile, "inserted", "key5", "carriage\rreturn");
  check_string_value (keyfile, "inserted", "key6", "slash\\yay!");
  check_string_list_value (keyfile, "inserted", "key7", "one", "two;andahalf", "3", NULL);

  g_key_file_free (keyfile);
}

/* check parsing of boolean values */
static void
test_boolean (void)
{
  GKeyFile *keyfile;
  GError *error = NULL;

  const gchar *data =
    "[valid]\n"
    "key1=true\n"
    "key2=false\n"
    "key3=1\n"
    "key4=0\n"
    "key5= true\n"
    "key6=true \n"
    "[invalid]\n"
    "key1=t\n"
    "key2=f\n"
    "key3=yes\n"
    "key4=no\n";

  keyfile = load_data (data, 0);

  check_boolean_value (keyfile, "valid", "key1", TRUE);
  check_boolean_value (keyfile, "valid", "key2", FALSE);
  check_boolean_value (keyfile, "valid", "key3", TRUE);
  check_boolean_value (keyfile, "valid", "key4", FALSE);
  check_boolean_value (keyfile, "valid", "key5", TRUE);
  check_boolean_value (keyfile, "valid", "key6", TRUE);

  g_key_file_get_boolean (keyfile, "invalid", "key1", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  g_key_file_get_boolean (keyfile, "invalid", "key2", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  g_key_file_get_boolean (keyfile, "invalid", "key3", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  g_key_file_get_boolean (keyfile, "invalid", "key4", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  g_key_file_set_boolean (keyfile, "valid", "key1", FALSE);
  check_boolean_value (keyfile, "valid", "key1", FALSE);

  g_key_file_free (keyfile);
}

/* check parsing of integer and double values */
static void
test_number (void)
{
  GKeyFile *keyfile;
  GError *error = NULL;
  gdouble dval = 0.0;

  const gchar *data =
    "[valid]\n"
    "key1=0\n"
    "key2=1\n"
    "key3=-1\n"
    "key4=2324431\n"
    "key5=-2324431\n"
    "key6=000111\n"
    "key7= 1\n"
    "key8=1 \n"
    "dkey1=000111\n"
    "dkey2=145.45\n"
    "dkey3=-3453.7\n"
    "[invalid]\n"
    "key1=0xffff\n"
    "key2=0.5\n"
    "key3=1e37\n"
    "key4=ten\n"
    "key5=\n"
    "key6=1.0.0\n"
    "key7=2x2\n"
    "key8=abc\n";

  keyfile = load_data (data, 0);

  check_integer_value (keyfile, "valid", "key1", 0);
  check_integer_value (keyfile, "valid", "key2", 1);
  check_integer_value (keyfile, "valid", "key3", -1);
  check_integer_value (keyfile, "valid", "key4", 2324431);
  check_integer_value (keyfile, "valid", "key5", -2324431);
  check_integer_value (keyfile, "valid", "key6", 111);
  check_integer_value (keyfile, "valid", "key7", 1);
  check_integer_value (keyfile, "valid", "key8", 1);
  check_double_value (keyfile, "valid", "dkey1", 111.0);
  check_double_value (keyfile, "valid", "dkey2", 145.45);
  check_double_value (keyfile, "valid", "dkey3", -3453.7);

  g_key_file_get_integer (keyfile, "invalid", "key1", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  g_key_file_get_integer (keyfile, "invalid", "key2", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  g_key_file_get_integer (keyfile, "invalid", "key3", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  g_key_file_get_integer (keyfile, "invalid", "key4", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

  dval = g_key_file_get_double (keyfile, "invalid", "key5", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
  g_assert_cmpfloat (dval, ==, 0.0);

  dval = g_key_file_get_double (keyfile, "invalid", "key6", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
  g_assert_cmpfloat (dval, ==, 0.0);

  dval = g_key_file_get_double (keyfile, "invalid", "key7", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
  g_assert_cmpfloat (dval, ==, 0.0);

  dval = g_key_file_get_double (keyfile, "invalid", "key8", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
  g_assert_cmpfloat (dval, ==, 0.0);

  g_key_file_free (keyfile);
}

/* check handling of translated strings */
static void
test_locale_string (void)
{
  GKeyFile *keyfile;
  gchar *old_locale;

  const gchar *data =
    "[valid]\n"
    "key1=v1\n"
    "key1[de]=v1-de\n"
    "key1[de_DE]=v1-de_DE\n"
    "key1[de_DE.UTF8]=v1-de_DE.UTF8\n"
    "key1[fr]=v1-fr\n"
    "key1[en] =v1-en\n"
    "key1[sr@Latn]=v1-sr\n";

  keyfile = load_data (data, G_KEY_FILE_KEEP_TRANSLATIONS);

  check_locale_string_value (keyfile, "valid", "key1", "it", "v1");
  check_locale_string_value (keyfile, "valid", "key1", "de", "v1-de");
  check_locale_string_value (keyfile, "valid", "key1", "de_DE", "v1-de_DE");
  check_locale_string_value (keyfile, "valid", "key1", "de_DE.UTF8", "v1-de_DE.UTF8");
  check_locale_string_value (keyfile, "valid", "key1", "fr", "v1-fr");
  check_locale_string_value (keyfile, "valid", "key1", "fr_FR", "v1-fr");
  check_locale_string_value (keyfile, "valid", "key1", "en", "v1-en");
  check_locale_string_value (keyfile, "valid", "key1", "sr@Latn", "v1-sr");

  g_key_file_free (keyfile);

  /* now test that translations are thrown away */

  old_locale = g_strdup (setlocale (LC_ALL, NULL));
  g_setenv ("LANGUAGE", "de", TRUE);
  setlocale (LC_ALL, "");

  keyfile = load_data (data, 0);

  check_locale_string_value (keyfile, "valid", "key1", "it", "v1");
  check_locale_string_value (keyfile, "valid", "key1", "de", "v1-de");
  check_locale_string_value (keyfile, "valid", "key1", "de_DE", "v1-de");
  check_locale_string_value (keyfile, "valid", "key1", "de_DE.UTF8", "v1-de");
  check_locale_string_value (keyfile, "valid", "key1", "fr", "v1");
  check_locale_string_value (keyfile, "valid", "key1", "fr_FR", "v1");
  check_locale_string_value (keyfile, "valid", "key1", "en", "v1");

  g_key_file_free (keyfile);

  setlocale (LC_ALL, old_locale);
  g_free (old_locale);
}

static void
test_lists (void)
{
  GKeyFile *keyfile;

  const gchar *data =
    "[valid]\n"
    "key1=v1;v2\n"
    "key2=v1;v2;\n"
    "key3=v1,v2\n"
    "key4=v1\\;v2\n"
    "key5=true;false\n"
    "key6=1;0;-1\n"
    "key7= 1 ; 0 ; -1 \n"
    "key8=v1\\,v2\n"
    "key9=0;1.3456;-76532.456\n";

  keyfile = load_data (data, 0);

  check_string_list_value (keyfile, "valid", "key1", "v1", "v2", NULL);
  check_string_list_value (keyfile, "valid", "key2", "v1", "v2", NULL);
  check_string_list_value (keyfile, "valid", "key3", "v1,v2", NULL);
  check_string_list_value (keyfile, "valid", "key4", "v1;v2", NULL);
  check_boolean_list_value (keyfile, "valid", "key5", TRUE, FALSE, -100);
  check_integer_list_value (keyfile, "valid", "key6", 1, 0, -1, -100);
  check_double_list_value (keyfile, "valid", "key9", 0.0, 1.3456, -76532.456, -100.0);
  /* maybe these should be valid */
  /* check_integer_list_value (keyfile, "valid", "key7", 1, 0, -1, -100);*/
  /* check_string_list_value (keyfile, "valid", "key8", "v1\\,v2", NULL);*/

  g_key_file_free (keyfile);

  /* Now check an alternate separator */

  keyfile = load_data (data, 0);
  g_key_file_set_list_separator (keyfile, ',');

  check_string_list_value (keyfile, "valid", "key1", "v1;v2", NULL);
  check_string_list_value (keyfile, "valid", "key2", "v1;v2;", NULL);
  check_string_list_value (keyfile, "valid", "key3", "v1", "v2", NULL);

  g_key_file_free (keyfile);
}

static void
test_lists_set_get (void)
{
  GKeyFile *keyfile;
  static const char * const strings[] = { "v1", "v2" };
  static const char * const locale_strings[] = { "v1-l", "v2-l" };
  static int integers[] = { 1, -1, 2 };
  static gdouble doubles[] = { 3.14, 2.71 };

  keyfile = g_key_file_new ();
  g_key_file_set_string_list (keyfile, "group0", "key1", strings, G_N_ELEMENTS (strings));
  g_key_file_set_locale_string_list (keyfile, "group0", "key1", "de", locale_strings, G_N_ELEMENTS (locale_strings));
  g_key_file_set_integer_list (keyfile, "group0", "key2", integers, G_N_ELEMENTS (integers));
  g_key_file_set_double_list (keyfile, "group0", "key3", doubles, G_N_ELEMENTS (doubles));

  check_string_list_value (keyfile, "group0", "key1", strings[0], strings[1], NULL);
  check_locale_string_list_value (keyfile, "group0", "key1", "de", locale_strings[0], locale_strings[1], NULL);
  check_integer_list_value (keyfile, "group0", "key2", integers[0], integers[1], -100);
  check_double_list_value (keyfile, "group0", "key3", doubles[0], doubles[1], -100.0);
  g_key_file_free (keyfile);

  /* and again with a different list separator */
  keyfile = g_key_file_new ();
  g_key_file_set_list_separator (keyfile, ',');
  g_key_file_set_string_list (keyfile, "group0", "key1", strings, G_N_ELEMENTS (strings));
  g_key_file_set_locale_string_list (keyfile, "group0", "key1", "de", locale_strings, G_N_ELEMENTS (locale_strings));
  g_key_file_set_integer_list (keyfile, "group0", "key2", integers, G_N_ELEMENTS (integers));
  g_key_file_set_double_list (keyfile, "group0", "key3", doubles, G_N_ELEMENTS (doubles));

  check_string_list_value (keyfile, "group0", "key1", strings[0], strings[1], NULL);
  check_locale_string_list_value (keyfile, "group0", "key1", "de", locale_strings[0], locale_strings[1], NULL);
  check_integer_list_value (keyfile, "group0", "key2", integers[0], integers[1], -100);
  check_double_list_value (keyfile, "group0", "key3", doubles[0], doubles[1], -100.0);
  g_key_file_free (keyfile);
}

static void
test_group_remove (void)
{
  GKeyFile *keyfile;
  gchar **names;
  gsize len;
  GError *error = NULL;

  const gchar *data =
    "[group1]\n"
    "[group2]\n"
    "key1=bla\n"
    "key2=bla\n"
    "[group3]\n"
    "key1=bla\n"
    "key2=bla\n";

  g_test_bug ("165887");

  keyfile = load_data (data, 0);

  names = g_key_file_get_groups (keyfile, &len);
  g_assert (names != NULL);

  check_length ("groups", g_strv_length (names), len, 3);
  check_name ("group name", names[0], "group1", 0);
  check_name ("group name", names[1], "group2", 1);
  check_name ("group name", names[2], "group3", 2);

  g_key_file_remove_group (keyfile, "group1", &error);
  check_no_error (&error);

  g_strfreev (names);

  names = g_key_file_get_groups (keyfile, &len);
  g_assert (names != NULL);

  check_length ("groups", g_strv_length (names), len, 2);
  check_name ("group name", names[0], "group2", 0);
  check_name ("group name", names[1], "group3", 1);

  g_key_file_remove_group (keyfile, "group2", &error);
  check_no_error (&error);

  g_strfreev (names);

  names = g_key_file_get_groups (keyfile, &len);
  g_assert (names != NULL);

  check_length ("groups", g_strv_length (names), len, 1);
  check_name ("group name", names[0], "group3", 0);

  g_key_file_remove_group (keyfile, "no such group", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);

  g_strfreev (names);

  g_key_file_free (keyfile);
}

static void
test_key_remove (void)
{
  GKeyFile *keyfile;
  gchar *value;
  GError *error = NULL;

  const gchar *data =
    "[group1]\n"
    "key1=bla\n"
    "key2=bla\n";

  g_test_bug ("165980");

  keyfile = load_data (data, 0);

  check_string_value (keyfile, "group1", "key1", "bla");

  g_key_file_remove_key (keyfile, "group1", "key1", &error);
  check_no_error (&error);

  value = g_key_file_get_string (keyfile, "group1", "key1", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND);
  g_free (value);

  g_key_file_remove_key (keyfile, "group1", "key1", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND);

  g_key_file_remove_key (keyfile, "no such group", "key1", &error);
  check_error (&error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);

  g_key_file_free (keyfile);
}


static void
test_groups (void)
{
  GKeyFile *keyfile;

  const gchar *data =
    "[1]\n"
    "key1=123\n"
    "[2]\n"
    "key2=123\n";

  g_test_bug ("316309");

  keyfile = load_data (data, 0);

  check_string_value (keyfile, "1", "key1", "123");
  check_string_value (keyfile, "2", "key2", "123");

  g_key_file_free (keyfile);
}

static void
test_group_names (void)
{
  GKeyFile *keyfile;
  GError *error = NULL;
  const gchar *data;
  gchar *value;

  /* [ in group name */
  data = "[a[b]\n"
         "key1=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* ] in group name */
  data = "[a]b]\n"
         "key1=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* control char in group name */
  data = "[a\tb]\n"
         "key1=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* empty group name */
  data = "[]\n"
         "key1=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* Unicode in group name */
  data = "[\xc2\xbd]\n"
         "key1=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_no_error (&error);

  keyfile = g_key_file_new ();
  /*g_key_file_set_string (keyfile, "a[b", "key1", "123");*/
  value = g_key_file_get_string (keyfile, "a[b", "key1", &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_assert (value == NULL);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  /*g_key_file_set_string (keyfile, "a]b", "key1", "123");*/
  value = g_key_file_get_string (keyfile, "a]b", "key1", &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_assert (value == NULL);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  /*g_key_file_set_string (keyfile, "a\tb", "key1", "123");*/
  value = g_key_file_get_string (keyfile, "a\tb", "key1", &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_assert (value == NULL);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_set_string (keyfile, "\xc2\xbd", "key1", "123");
  check_string_value (keyfile, "\xc2\xbd", "key1", "123");
  g_key_file_free (keyfile);
}

static void
test_key_names (void)
{
  GKeyFile *keyfile;
  GError *error = NULL;
  const gchar *data;
  gchar *value;

  /* [ in key name */
  data = "[a]\n"
         "key[=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* empty key name */
  data = "[a]\n"
         " =123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* empty key name */
  data = "[a]\n"
         " [de] =123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* bad locale suffix */
  data = "[a]\n"
         "foo[@#!&%]=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);

  /* initial space */
  data = "[a]\n"
         " foo=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  check_no_error (&error);
  check_string_value (keyfile, "a", "foo", "123");
  g_key_file_free (keyfile);

  /* final space */
  data = "[a]\n"
         "foo =123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  check_no_error (&error);
  check_string_value (keyfile, "a", "foo", "123");
  g_key_file_free (keyfile);

  /* inner space */
  data = "[a]\n"
         "foo bar=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  check_no_error (&error);
  check_string_value (keyfile, "a", "foo bar", "123");
  g_key_file_free (keyfile);

  /* inner space */
  data = "[a]\n"
         "foo [de] =123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_PARSE);
  g_key_file_free (keyfile);

  /* control char in key name */
  data = "[a]\n"
         "key\tfoo=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_no_error (&error);

  /* Unicode in key name */
  data = "[a]\n"
         "\xc2\xbd=123\n";
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  g_key_file_free (keyfile);
  check_no_error (&error);

  keyfile = g_key_file_new ();
  g_key_file_set_string (keyfile, "a", "x", "123");
  /*g_key_file_set_string (keyfile, "a", "key=", "123");*/
  value = g_key_file_get_string (keyfile, "a", "key=", &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_KEY_NOT_FOUND);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_set_string (keyfile, "a", "x", "123");
  /*g_key_file_set_string (keyfile, "a", "key[", "123");*/
  value = g_key_file_get_string (keyfile, "a", "key[", &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_KEY_NOT_FOUND);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_set_string (keyfile, "a", "x", "123");
  g_key_file_set_string (keyfile, "a", "key\tfoo", "123");
  value = g_key_file_get_string (keyfile, "a", "key\tfoo", &error);
  check_no_error (&error);
  g_free (value);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_set_string (keyfile, "a", "x", "123");
  /*g_key_file_set_string (keyfile, "a", " key", "123");*/
  value = g_key_file_get_string (keyfile, "a", " key", &error);
  check_error (&error,
               G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_KEY_NOT_FOUND);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_set_string (keyfile, "a", "x", "123");

  /* Unicode key */
  g_key_file_set_string (keyfile, "a", "\xc2\xbd", "123");
  check_string_value (keyfile, "a", "\xc2\xbd", "123");

  /* Keys with / + . (as used by the gnome-vfs mime cache) */
  g_key_file_set_string (keyfile, "a", "foo/bar", "/");
  check_string_value (keyfile, "a", "foo/bar", "/");
  g_key_file_set_string (keyfile, "a", "foo+bar", "+");
  check_string_value (keyfile, "a", "foo+bar", "+");
  g_key_file_set_string (keyfile, "a", "foo.bar", ".");
  check_string_value (keyfile, "a", "foo.bar", ".");

  g_key_file_free (keyfile);
}

static void
test_duplicate_keys (void)
{
  GKeyFile *keyfile;
  const gchar *data =
    "[1]\n"
    "key1=123\n"
    "key1=345\n";

  keyfile = load_data (data, 0);
  check_string_value (keyfile, "1", "key1", "345");

  g_key_file_free (keyfile);
}

static void
test_duplicate_groups (void)
{
  GKeyFile *keyfile;
  const gchar *data =
    "[Desktop Entry]\n"
    "key1=123\n"
    "[Desktop Entry]\n"
    "key2=123\n";

  g_test_bug ("157877");

  keyfile = load_data (data, 0);
  check_string_value (keyfile, "Desktop Entry", "key1", "123");
  check_string_value (keyfile, "Desktop Entry", "key2", "123");

  g_key_file_free (keyfile);
}

static void
test_duplicate_groups2 (void)
{
  GKeyFile *keyfile;
  const gchar *data =
    "[A]\n"
    "foo=bar\n"
    "[B]\n"
    "foo=baz\n"
    "[A]\n"
    "foo=bang\n";

  g_test_bug ("385910");

  keyfile = load_data (data, 0);
  check_string_value (keyfile, "A", "foo", "bang");
  check_string_value (keyfile, "B", "foo", "baz");

  g_key_file_free (keyfile);
}

static void
test_reload_idempotency (void)
{
  static const gchar *original_data=""
    "# Top comment\n"
    "\n"
    "# First comment\n"
    "[first]\n"
    "key=value\n"
    "# A random comment in the first group\n"
    "anotherkey=anothervalue\n"
    "# Second comment - one line\n"
    "[second]\n"
    "# Third comment - two lines\n"
    "# Third comment - two lines\n"
    "[third]\n"
    "blank_line=1\n"
    "\n"
    "blank_lines=2\n"
    "\n\n"
    "[fourth]\n"
    "[fifth]\n";
  GKeyFile *keyfile;
  GError *error = NULL;
  gchar *data1, *data2;
  gsize len1, len2;

  g_test_bug ("420686");

  /* check that we only insert a single new line between groups */
  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile,
                             original_data, strlen(original_data),
                             G_KEY_FILE_KEEP_COMMENTS,
                             &error);
  check_no_error (&error);

  data1 = g_key_file_to_data (keyfile, &len1, &error);
  g_assert (data1 != NULL);
  g_key_file_free (keyfile);

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile,
                             data1, len1,
                             G_KEY_FILE_KEEP_COMMENTS,
                             &error);
  check_no_error (&error);

  data2 = g_key_file_to_data (keyfile, &len2, &error);
  g_assert (data2 != NULL);
  g_key_file_free (keyfile);

  g_assert_cmpstr (data1, ==, data2);

  g_free (data2);
  g_free (data1);
}

static const char int64_data[] =
"[bees]\n"
"a=1\n"
"b=2\n"
"c=123456789123456789\n"
"d=-123456789123456789\n";

static void
test_int64 (void)
{
  GKeyFile *file;
  gboolean ok;
  guint64 c;
  gint64 d;
  gchar *value;

  g_test_bug ("614864");

  file = g_key_file_new ();

  ok = g_key_file_load_from_data (file, int64_data, strlen (int64_data),
      0, NULL);
  g_assert (ok);

  c = g_key_file_get_uint64 (file, "bees", "c", NULL);
  g_assert (c == G_GUINT64_CONSTANT (123456789123456789));

  d = g_key_file_get_int64 (file, "bees", "d", NULL);
  g_assert (d == G_GINT64_CONSTANT (-123456789123456789));

  g_key_file_set_uint64 (file, "bees", "c",
      G_GUINT64_CONSTANT (987654321987654321));
  value = g_key_file_get_value (file, "bees", "c", NULL);
  g_assert_cmpstr (value, ==, "987654321987654321");
  g_free (value);

  g_key_file_set_int64 (file, "bees", "d",
      G_GINT64_CONSTANT (-987654321987654321));
  value = g_key_file_get_value (file, "bees", "d", NULL);
  g_assert_cmpstr (value, ==, "-987654321987654321");
  g_free (value);

  g_key_file_free (file);
}

static void
test_load (void)
{
  GKeyFile *file;
  GError *error;
  gboolean bools[2] = { TRUE, FALSE };
  gboolean loaded;

  file = g_key_file_new ();
  error = NULL;
#ifdef G_OS_UNIX
  /* Uses the value of $XDG_DATA_HOME we set in main() */
  loaded = g_key_file_load_from_data_dirs (file, "keyfiletest.ini", NULL, 0, &error);
#else
  loaded = g_key_file_load_from_file (file, g_test_get_filename (G_TEST_DIST, "keyfiletest.ini", NULL), 0, &error);
#endif
  g_assert_no_error (error);
  g_assert (loaded);

  g_key_file_set_locale_string (file, "test", "key4", "de", "Vierter Schlüssel");
  g_key_file_set_boolean_list (file, "test", "key5", bools, 2);
  g_key_file_set_integer (file, "test", "key6", 22);
  g_key_file_set_double (file, "test", "key7", 2.5);
  g_key_file_set_comment (file, "test", "key7", "some float", NULL);
  g_key_file_set_comment (file, "test", NULL, "the test group", NULL);
  g_key_file_set_comment (file, NULL, NULL, "top comment", NULL);

  g_key_file_free (file);

  file = g_key_file_new ();
  error = NULL;
  g_assert (!g_key_file_load_from_data_dirs (file, "keyfile-test.ini", NULL, 0, &error));
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_NOT_FOUND);
  g_error_free (error);
  g_key_file_free (file);
}

static void
test_save (void)
{
  GKeyFile *kf;
  GKeyFile *kf2;
  static const char data[] =
    "[bees]\n"
    "a=1\n"
    "b=2\n"
    "c=123456789123456789\n"
    "d=-123456789123456789\n";
  gboolean ok;
  gchar *file;
  guint64 c;
  GError *error = NULL;
  int fd;

  kf = g_key_file_new ();
  ok = g_key_file_load_from_data (kf, data, strlen (data), 0, NULL);
  g_assert (ok);

  file = g_strdup ("key_file_XXXXXX");
  fd = g_mkstemp (file);
  g_assert (fd != -1);
  ok = g_close (fd, &error);
  g_assert (ok);
  g_assert_no_error (error);
  ok = g_key_file_save_to_file (kf, file, &error);
  g_assert (ok);
  g_assert_no_error (error);

  kf2 = g_key_file_new ();
  ok = g_key_file_load_from_file (kf2, file, 0, &error);
  g_assert (ok);
  g_assert_no_error (error);

  c = g_key_file_get_uint64 (kf2, "bees", "c", NULL);
  g_assert (c == G_GUINT64_CONSTANT (123456789123456789));

  remove (file);
  g_free (file);
  g_key_file_free (kf);
  g_key_file_free (kf2);
}

static void
test_load_fail (void)
{
  GKeyFile *file;
  GError *error;

  file = g_key_file_new ();
  error = NULL;
  g_assert (!g_key_file_load_from_file (file, g_test_get_filename (G_TEST_DIST, "keyfile.c", NULL), 0, &error));
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE);
  g_clear_error (&error);
  g_assert (!g_key_file_load_from_file (file, "/nosuchfile", 0, &error));
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
  g_clear_error (&error);

  g_key_file_free (file);
}

static void
test_non_utf8 (void)
{
  GKeyFile *file;
  static const char data[] =
"[group]\n"
"a=\230\230\230\n"
"b=a;b;\230\230\230;\n"
"c=a\\\n";
  gboolean ok;
  GError *error;
  gchar *s;
  gchar **l;

  file = g_key_file_new ();

  ok = g_key_file_load_from_data (file, data, strlen (data), 0, NULL);
  g_assert (ok);

  error = NULL;
  s = g_key_file_get_string (file, "group", "a", &error);
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_UNKNOWN_ENCODING);
  g_assert (s == NULL);

  g_clear_error (&error);
  l = g_key_file_get_string_list (file, "group", "b", NULL, &error);
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_UNKNOWN_ENCODING);
  g_assert (l == NULL);

  g_clear_error (&error);
  l = g_key_file_get_string_list (file, "group", "c", NULL, &error);
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
  g_assert (l == NULL);

  g_clear_error (&error);

  g_key_file_free (file);
}

static void
test_page_boundary (void)
{
  GKeyFile *file;
  GError *error;
  gint i;

#define GROUP "main_section"
#define KEY_PREFIX "fill_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvw_"
#define FIRST_KEY 10
#define LAST_KEY 99
#define VALUE 92

  g_test_bug ("640695");

  file = g_key_file_new ();

  error = NULL;
  g_key_file_load_from_file (file, g_test_get_filename (G_TEST_DIST, "pages.ini", NULL), G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  for (i = FIRST_KEY; i <= LAST_KEY; i++)
    {
      gchar *key;
      gint val;

      key = g_strdup_printf (KEY_PREFIX "%d", i);
      val = g_key_file_get_integer (file, GROUP, key, &error);
      g_free (key);
      g_assert_no_error (error);
      g_assert_cmpint (val, ==, VALUE);
    }

  g_key_file_free (file);
}

static void
test_ref (void)
{
  GKeyFile *file;
  static const char data[] =
"[group]\n"
"a=1\n";
  gboolean ok;

  file = g_key_file_new ();

  ok = g_key_file_load_from_data (file, data, strlen (data), 0, NULL);
  g_assert (ok);
  g_assert (g_key_file_has_key (file, "group", "a", NULL));
  g_key_file_ref (file);
  g_key_file_free (file);
  g_key_file_unref (file);
}

/* https://bugzilla.gnome.org/show_bug.cgi?id=634232 */
static void
test_replace_value (void)
{
  GKeyFile *keyfile;

  keyfile = g_key_file_new();
  g_key_file_set_value(keyfile, "grupo1", "chave1", "1234567890");
  g_key_file_set_value(keyfile, "grupo1", "chave1", "123123423423423432432423423");
  g_key_file_remove_group(keyfile, "grupo1", NULL);
  g_free (g_key_file_to_data (keyfile, NULL, NULL));
  g_key_file_unref (keyfile);
}

static void
test_list_separator (void)
{
  GKeyFile *keyfile;
  GError *error = NULL;

  const gchar *data =
    "[test]\n"
    "key1=v1,v2\n";

  keyfile = g_key_file_new ();
  g_key_file_set_list_separator (keyfile, ',');
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);

  check_string_list_value (keyfile, "test", "key1", "v1", "v2", NULL);
  g_key_file_unref (keyfile);
}

static void
test_empty_string (void)
{
  GError *error = NULL;
  GKeyFile *kf;

  kf = g_key_file_new ();

  g_key_file_load_from_data (kf, "", 0, 0, &error);
  g_assert_no_error (error);

  g_key_file_load_from_data (kf, "", -1, 0, &error);
  g_assert_no_error (error);

  /* NULL is a fine pointer to use if length is zero */
  g_key_file_load_from_data (kf, NULL, 0, 0, &error);
  g_assert_no_error (error);

  /* should not attempt to access non-NULL pointer if length is zero */
  g_key_file_load_from_data (kf, GINT_TO_POINTER (1), 0, 0, &error);
  g_assert_no_error (error);

  g_key_file_unref (kf);
}

static void
test_limbo (void)
{
  GKeyFile *file;
  static const char data[] =
"a=b\n"
"[group]\n"
"b=c\n";
  gboolean ok;
  GError *error;

  file = g_key_file_new ();

  error = NULL;
  ok = g_key_file_load_from_data (file, data, strlen (data), 0, &error);
  g_assert (!ok);
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_clear_error (&error);
  g_key_file_free (file);
}

static void
test_utf8 (void)
{
  GKeyFile *file;
  static const char data[] =
"[group]\n"
"Encoding=non-UTF-8\n";
  gboolean ok;
  GError *error;

  file = g_key_file_new ();

  error = NULL;
  ok = g_key_file_load_from_data (file, data, strlen (data), 0, &error);
  g_assert (!ok);
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_UNKNOWN_ENCODING);
  g_clear_error (&error);
  g_key_file_free (file);
}

static void
test_roundtrip (void)
{
  GKeyFile *kf;
  const gchar orig[] =
    "[Group1]\n"
    "key1=value1\n"
    "\n"
    "[Group2]\n"
    "key1=value1\n";
  gsize len;
  gchar *data;

  kf = load_data (orig, G_KEY_FILE_KEEP_COMMENTS);
  g_key_file_set_integer (kf, "Group1", "key2", 0);
  g_key_file_remove_key (kf, "Group1", "key2", NULL);

  data = g_key_file_to_data (kf, &len, NULL);
  g_assert_cmpstr (data, ==, orig);

  g_free (data);
  g_key_file_free (kf);
}

static void
test_bytes (void)
{
  const gchar data[] =
    "[Group1]\n"
    "key1=value1\n"
    "\n"
    "[Group2]\n"
    "key2=value2\n";

  GKeyFile *kf = g_key_file_new ();
  GBytes *bytes = g_bytes_new (data, strlen (data));
  GError *error = NULL;

  gchar **names;
  gsize len;

  g_key_file_load_from_bytes (kf, bytes, 0, &error);

  g_assert_no_error (error);

  names = g_key_file_get_groups (kf, &len);
  g_assert_nonnull (names);

  check_length ("groups", g_strv_length (names), len, 2);
  check_name ("group name", names[0], "Group1", 0);
  check_name ("group name", names[1], "Group2", 1);

  check_string_value (kf, "Group1", "key1", "value1");
  check_string_value (kf, "Group2", "key2", "value2");

  g_strfreev (names);
  g_bytes_unref (bytes);
  g_key_file_free (kf);
}

static void
test_get_locale (void)
{
  GKeyFile *kf;

  kf = g_key_file_new ();
  g_key_file_load_from_data (kf,
                             "[Group]\n"
                             "x[fr_CA]=a\n"
                             "x[fr]=b\n"
                             "x=c\n",
                             -1, G_KEY_FILE_KEEP_TRANSLATIONS,
                             NULL);

  check_locale_string_value (kf, "Group", "x", "fr_CA", "a");
  check_string_locale_value (kf, "Group", "x", "fr_CA", "fr_CA");

  check_locale_string_value (kf, "Group", "x", "fr_CH", "b");
  check_string_locale_value (kf, "Group", "x", "fr_CH", "fr");

  check_locale_string_value (kf, "Group", "x", "eo", "c");
  check_string_locale_value (kf, "Group", "x", "eo", NULL);

  g_key_file_free (kf);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#ifdef G_OS_UNIX
  g_setenv ("XDG_DATA_HOME", g_test_get_dir (G_TEST_DIST), TRUE);
#endif

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/keyfile/line-ends", test_line_ends);
  g_test_add_func ("/keyfile/whitespace", test_whitespace);
  g_test_add_func ("/keyfile/comments", test_comments);
  g_test_add_func ("/keyfile/listing", test_listing);
  g_test_add_func ("/keyfile/string", test_string);
  g_test_add_func ("/keyfile/boolean", test_boolean);
  g_test_add_func ("/keyfile/number", test_number);
  g_test_add_func ("/keyfile/locale-string", test_locale_string);
  g_test_add_func ("/keyfile/lists", test_lists);
  g_test_add_func ("/keyfile/lists-set-get", test_lists_set_get);
  g_test_add_func ("/keyfile/group-remove", test_group_remove);
  g_test_add_func ("/keyfile/key-remove", test_key_remove);
  g_test_add_func ("/keyfile/groups", test_groups);
  g_test_add_func ("/keyfile/duplicate-keys", test_duplicate_keys);
  g_test_add_func ("/keyfile/duplicate-groups", test_duplicate_groups);
  g_test_add_func ("/keyfile/duplicate-groups2", test_duplicate_groups2);
  g_test_add_func ("/keyfile/group-names", test_group_names);
  g_test_add_func ("/keyfile/key-names", test_key_names);
  g_test_add_func ("/keyfile/reload", test_reload_idempotency);
  g_test_add_func ("/keyfile/int64", test_int64);
  g_test_add_func ("/keyfile/load", test_load);
  g_test_add_func ("/keyfile/save", test_save);
  g_test_add_func ("/keyfile/load-fail", test_load_fail);
  g_test_add_func ("/keyfile/non-utf8", test_non_utf8);
  g_test_add_func ("/keyfile/page-boundary", test_page_boundary);
  g_test_add_func ("/keyfile/ref", test_ref);
  g_test_add_func ("/keyfile/replace-value", test_replace_value);
  g_test_add_func ("/keyfile/list-separator", test_list_separator);
  g_test_add_func ("/keyfile/empty-string", test_empty_string);
  g_test_add_func ("/keyfile/limbo", test_limbo);
  g_test_add_func ("/keyfile/utf8", test_utf8);
  g_test_add_func ("/keyfile/roundtrip", test_roundtrip);
  g_test_add_func ("/keyfile/bytes", test_bytes);
  g_test_add_func ("/keyfile/get-locale", test_get_locale);

  return g_test_run ();
}
