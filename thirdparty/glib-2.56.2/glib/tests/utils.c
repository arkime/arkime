/* Unit tests for utilities
 * Copyright (C) 2010 Red Hat, Inc.
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
 *
 * Author: Matthias Clasen
 */

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "glib.h"
#include "glib-private.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static gboolean
strv_check (const gchar * const *strv, ...)
{
  va_list args;
  gchar *s;
  gint i;

  va_start (args, strv);
  for (i = 0; strv[i]; i++)
    {
      s = va_arg (args, gchar*);
      if (g_strcmp0 (strv[i], s) != 0)
        {
          va_end (args);
          return FALSE;
        }
    }

  va_end (args);

  return TRUE;
}

static void
test_language_names (void)
{
  const gchar * const *names;

  g_setenv ("LANGUAGE", "de:en_US", TRUE);
  names = g_get_language_names ();
  g_assert (strv_check (names, "de", "en_US", "en", "C", NULL));

  g_setenv ("LANGUAGE", "tt_RU.UTF-8@iqtelif", TRUE);
  names = g_get_language_names ();
  g_assert (strv_check (names,
                        "tt_RU.UTF-8@iqtelif",
                        "tt_RU@iqtelif",
                        "tt.UTF-8@iqtelif",
                        "tt@iqtelif",
                        "tt_RU.UTF-8",
                        "tt_RU",
                        "tt.UTF-8",
                        "tt",
                        "C",
                        NULL));
}

static void
test_locale_variants (void)
{
  char **v;

  v = g_get_locale_variants ("fr_BE");
  g_assert (strv_check ((const gchar * const *) v, "fr_BE", "fr", NULL));
  g_strfreev (v);

  v = g_get_locale_variants ("sr_SR@latin");
  g_assert (strv_check ((const gchar * const *) v, "sr_SR@latin", "sr@latin", "sr_SR", "sr", NULL));
  g_strfreev (v);
}

static void
test_version (void)
{
  if (g_test_verbose ())
    g_printerr ("(header %d.%d.%d library %d.%d.%d) ",
              GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
              glib_major_version, glib_minor_version, glib_micro_version);

  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION,
                                GLIB_MICRO_VERSION) == NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION,
                                0) == NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION - 1,
                                0,
                                0) != NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION + 1,
                                0,
                                0) != NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION + 1,
                                0) != NULL);
  /* don't use + 1 here, since a +/-1 difference can
   * happen due to post-release version bumps in git
   */
  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION,
                                GLIB_MICRO_VERSION + 3) != NULL);
}

static const gchar *argv0;

static void
test_appname (void)
{
  const gchar *prgname;
  const gchar *appname;

  prgname = g_get_prgname ();
  appname = g_get_application_name ();
  g_assert_cmpstr (prgname, ==, argv0);
  g_assert_cmpstr (appname, ==, prgname);

  g_set_prgname ("prgname");

  prgname = g_get_prgname ();
  appname = g_get_application_name ();
  g_assert_cmpstr (prgname, ==, "prgname");
  g_assert_cmpstr (appname, ==, "prgname");

  g_set_application_name ("appname");

  prgname = g_get_prgname ();
  appname = g_get_application_name ();
  g_assert_cmpstr (prgname, ==, "prgname");
  g_assert_cmpstr (appname, ==, "appname");
}

static void
test_tmpdir (void)
{
  g_test_bug ("627969");
  g_assert_cmpstr (g_get_tmp_dir (), !=, "");
}

static void
test_bits (void)
{
  gulong mask;
  gint max_bit;
  gint i, pos;

  pos = g_bit_nth_lsf (0, -1);
  g_assert_cmpint (pos, ==, -1);

  max_bit = sizeof (gulong) * 8;
  for (i = 0; i < max_bit; i++)
    {
      mask = 1UL << i;

      pos = g_bit_nth_lsf (mask, -1);
      g_assert_cmpint (pos, ==, i);

      pos = g_bit_nth_lsf (mask, i - 3);
      g_assert_cmpint (pos , ==, i);

      pos = g_bit_nth_lsf (mask, i);
      g_assert_cmpint (pos , ==, -1);

      pos = g_bit_nth_lsf (mask, i + 1);
      g_assert_cmpint (pos , ==, -1);
    }

  pos = g_bit_nth_msf (0, -1);
  g_assert_cmpint (pos, ==, -1);

  for (i = 0; i < max_bit; i++)
    {
      mask = 1UL << i;

      pos = g_bit_nth_msf (mask, -1);
      g_assert_cmpint (pos, ==, i);

      pos = g_bit_nth_msf (mask, i + 3);
      g_assert_cmpint (pos , ==, i);

      pos = g_bit_nth_msf (mask, i);
      g_assert_cmpint (pos , ==, -1);

      if (i > 0)
        {
          pos = g_bit_nth_msf (mask, i - 1);
          g_assert_cmpint (pos , ==, -1);
        }
    }
}

static void
test_swap (void)
{
  guint16 a16, b16;
  guint32 a32, b32;
  guint64 a64, b64;

  a16 = 0xaabb;
  b16 = 0xbbaa;

  g_assert_cmpint (GUINT16_SWAP_LE_BE (a16), ==, b16);

  a32 = 0xaaaabbbb;
  b32 = 0xbbbbaaaa;

  g_assert_cmpint (GUINT32_SWAP_LE_BE (a32), ==, b32);

  a64 = G_GUINT64_CONSTANT(0xaaaaaaaabbbbbbbb);
  b64 = G_GUINT64_CONSTANT(0xbbbbbbbbaaaaaaaa);

  g_assert_cmpint (GUINT64_SWAP_LE_BE (a64), ==, b64);
}

static void
test_find_program (void)
{
  gchar *res;

#ifdef G_OS_UNIX
  res = g_find_program_in_path ("sh");
  g_assert (res != NULL);
  g_free (res);

  res = g_find_program_in_path ("/bin/sh");
  g_assert (res != NULL);
  g_free (res);
#else
  /* There's not a lot we can search for that would reliably work both
   * on real Windows and mingw.
   */
#endif

  res = g_find_program_in_path ("this_program_does_not_exit");
  g_assert (res == NULL);

  res = g_find_program_in_path ("/bin");
  g_assert (res == NULL);

  res = g_find_program_in_path ("/etc/passwd");
  g_assert (res == NULL);
}

static void
test_debug (void)
{
  GDebugKey keys[] = {
    { "key1", 1 },
    { "key2", 2 },
    { "key3", 4 },
  };
  guint res;

  res = g_parse_debug_string (NULL, keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 0);

  res = g_parse_debug_string ("foobabla;#!%!$%112 223", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 0);

  res = g_parse_debug_string ("key1:key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1;key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1,key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1   key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1\tkey2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("all", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 7);

  if (g_test_subprocess ())
    {
      res = g_parse_debug_string ("help", keys, G_N_ELEMENTS (keys));
      g_assert_cmpint (res, ==, 0);
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*Supported debug values: key1 key2 key3 all help*");
}

static void
test_codeset (void)
{
  gchar *c;
  const gchar *c2;

  c = g_get_codeset ();
  g_get_charset (&c2);

  g_assert_cmpstr (c, ==, c2);

  g_free (c);
}

static void
test_codeset2 (void)
{
  if (g_test_subprocess ())
    {
      const gchar *c;
      g_setenv ("CHARSET", "UTF-8", TRUE);
      g_get_charset (&c);
      g_assert_cmpstr (c, ==, "UTF-8");
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
}

static void
test_basename (void)
{
  const gchar *path = "/path/to/a/file/deep/down.sh";
  const gchar *b;

  b = g_basename (path);

  g_assert_cmpstr (b, ==, "down.sh");
}

extern const gchar *glib_pgettext (const gchar *msgidctxt, gsize msgidoffset);

static void
test_gettext (void)
{
  const gchar *am0, *am1, *am2, *am3;

  am0 = glib_pgettext ("GDateTime\004AM", strlen ("GDateTime") + 1);
  am1 = g_dpgettext ("glib20", "GDateTime\004AM", strlen ("GDateTime") + 1);
  am2 = g_dpgettext ("glib20", "GDateTime|AM", 0);
  am3 = g_dpgettext2 ("glib20", "GDateTime", "AM");

  g_assert_cmpstr (am0, ==, am1);
  g_assert_cmpstr (am1, ==, am2);
  g_assert_cmpstr (am2, ==, am3);
}

static void
test_username (void)
{
  const gchar *name;

  name = g_get_user_name ();

  g_assert (name != NULL);
}

static void
test_realname (void)
{
  const gchar *name;

  name = g_get_real_name ();

  g_assert (name != NULL);
}

static void
test_hostname (void)
{
  const gchar *name;

  name = g_get_host_name ();

  g_assert (name != NULL);
  g_assert_true (g_utf8_validate (name, -1, NULL));
}

#ifdef G_OS_UNIX
static void
test_xdg_dirs (void)
{
  gchar *xdg;
  const gchar *dir;
  const gchar * const *dirs;
  gchar *s;

  xdg = g_strdup (g_getenv ("XDG_CONFIG_HOME"));
  if (!xdg)
    xdg = g_build_filename (g_get_home_dir (), ".config", NULL);

  dir = g_get_user_config_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = g_strdup (g_getenv ("XDG_DATA_HOME"));
  if (!xdg)
    xdg = g_build_filename (g_get_home_dir (), ".local", "share", NULL);

  dir = g_get_user_data_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = g_strdup (g_getenv ("XDG_CACHE_HOME"));
  if (!xdg)
    xdg = g_build_filename (g_get_home_dir (), ".cache", NULL);

  dir = g_get_user_cache_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = g_strdup (g_getenv ("XDG_RUNTIME_DIR"));
  if (!xdg)
    xdg = g_strdup (g_get_user_cache_dir ());

  dir = g_get_user_runtime_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = (gchar *)g_getenv ("XDG_CONFIG_DIRS");
  if (!xdg)
    xdg = "/etc/xdg";

  dirs = g_get_system_config_dirs ();

  s = g_strjoinv (":", (gchar **)dirs);

  g_assert_cmpstr (s, ==, xdg);

  g_free (s);
}
#endif

static void
test_special_dir (void)
{
  const gchar *dir, *dir2;

  dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  g_reload_user_special_dirs_cache ();
  dir2 = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

  g_assert_cmpstr (dir, ==, dir2);
}

static void
test_desktop_special_dir (void)
{
  const gchar *dir, *dir2;

  dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  g_assert (dir != NULL);

  g_reload_user_special_dirs_cache ();
  dir2 = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  g_assert (dir2 != NULL);
}

static gboolean
source_test (gpointer data)
{
  g_assert_not_reached ();
  return G_SOURCE_REMOVE;
}

static void
test_clear_source (void)
{
  guint id;

  id = g_idle_add (source_test, NULL);
  g_assert_cmpuint (id, >, 0);

  g_clear_handle_id (&id, g_source_remove);
  g_assert_cmpuint (id, ==, 0);

  id = g_timeout_add (100, source_test, NULL);
  g_assert_cmpuint (id, >, 0);

  g_clear_handle_id (&id, g_source_remove);
  g_assert_cmpuint (id, ==, 0);
}

static void
test_clear_pointer (void)
{
  gpointer a;

  a = g_malloc (5);
  g_clear_pointer (&a, g_free);
  g_assert (a == NULL);

  a = g_malloc (5);
  (g_clear_pointer) (&a, g_free);
  g_assert (a == NULL);
}

static int obj_count;

static void
get_obj (gpointer *obj_out)
{
  gpointer obj = g_malloc (5);
  obj_count++;

  if (obj_out)
    *obj_out = g_steal_pointer (&obj);

  if (obj)
    {
      g_free (obj);
      obj_count--;
    }
}

static void
test_take_pointer (void)
{
  gpointer a;
  gpointer b;

  get_obj (NULL);

  get_obj (&a);
  g_assert (a);

  /* ensure that it works to skip the macro */
  b = (g_steal_pointer) (&a);
  g_assert (!a);
  obj_count--;
  g_free (b);

  g_assert (!obj_count);
}

static void
test_misc_mem (void)
{
  gpointer a;

  a = g_try_malloc (0);
  g_assert (a == NULL);

  a = g_try_malloc0 (0);
  g_assert (a == NULL);

  a = g_malloc (16);
  a = g_try_realloc (a, 20);
  a = g_try_realloc (a, 0);

  g_assert (a == NULL);
}

static void
test_nullify (void)
{
  gpointer p = &test_nullify;

  g_assert (p != NULL);

  g_nullify_pointer (&p);

  g_assert (p == NULL);
}

static void
atexit_func (void)
{
  g_print ("atexit called");
}

static void
test_atexit (void)
{
  if (g_test_subprocess ())
    {
      g_atexit (atexit_func);
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*atexit called*");
}

static void
test_check_setuid (void)
{
  gboolean res;

  res = GLIB_PRIVATE_CALL(g_check_setuid) ();
  g_assert (!res);
}

int
main (int   argc,
      char *argv[])
{
  argv0 = argv[0];

  /* for tmpdir test, need to do this early before g_get_any_init */
  g_setenv ("TMPDIR", "", TRUE);
  g_unsetenv ("TMP");
  g_unsetenv ("TEMP");

  /* g_test_init() only calls g_set_prgname() if g_get_prgname()
   * returns %NULL, but g_get_prgname() on Windows never returns NULL.
   * So we need to do this by hand to make test_appname() work on
   * Windows.
   */
  g_set_prgname (argv[0]);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/utils/language-names", test_language_names);
  g_test_add_func ("/utils/locale-variants", test_locale_variants);
  g_test_add_func ("/utils/version", test_version);
  g_test_add_func ("/utils/appname", test_appname);
  g_test_add_func ("/utils/tmpdir", test_tmpdir);
  g_test_add_func ("/utils/bits", test_bits);
  g_test_add_func ("/utils/swap", test_swap);
  g_test_add_func ("/utils/find-program", test_find_program);
  g_test_add_func ("/utils/debug", test_debug);
  g_test_add_func ("/utils/codeset", test_codeset);
  g_test_add_func ("/utils/codeset2", test_codeset2);
  g_test_add_func ("/utils/basename", test_basename);
  g_test_add_func ("/utils/gettext", test_gettext);
  g_test_add_func ("/utils/username", test_username);
  g_test_add_func ("/utils/realname", test_realname);
  g_test_add_func ("/utils/hostname", test_hostname);
#ifdef G_OS_UNIX
  g_test_add_func ("/utils/xdgdirs", test_xdg_dirs);
#endif
  g_test_add_func ("/utils/specialdir", test_special_dir);
  g_test_add_func ("/utils/specialdir/desktop", test_desktop_special_dir);
  g_test_add_func ("/utils/clear-pointer", test_clear_pointer);
  g_test_add_func ("/utils/take-pointer", test_take_pointer);
  g_test_add_func ("/utils/clear-source", test_clear_source);
  g_test_add_func ("/utils/misc-mem", test_misc_mem);
  g_test_add_func ("/utils/nullify", test_nullify);
  g_test_add_func ("/utils/atexit", test_atexit);
  g_test_add_func ("/utils/check-setuid", test_check_setuid);

  return g_test_run ();
}
