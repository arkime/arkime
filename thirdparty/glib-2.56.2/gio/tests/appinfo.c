
#include <locale.h>
#include <string.h>

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

static void
test_launch_for_app_info (GAppInfo *appinfo)
{
  GError *error;
  GFile *file;
  GList *l;
  const gchar *path;
  gchar *uri;

  if (g_getenv ("DISPLAY") == NULL || g_getenv ("DISPLAY")[0] == '\0')
    {
      g_printerr ("No DISPLAY.  Skipping test.  ");
      return;
    }

  error = NULL;
  g_assert (g_app_info_launch (appinfo, NULL, NULL, &error));
  g_assert_no_error (error);

  g_assert (g_app_info_launch_uris (appinfo, NULL, NULL, &error));
  g_assert_no_error (error);

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  file = g_file_new_for_path (path);
  l = NULL;
  l = g_list_append (l, file);

  g_assert (g_app_info_launch (appinfo, l, NULL, &error));
  g_assert_no_error (error);
  g_list_free (l);
  g_object_unref (file);

  l = NULL;
  uri = g_strconcat ("file://", g_test_get_dir (G_TEST_DIST), "/appinfo-test.desktop", NULL);
  l = g_list_append (l, uri);
  l = g_list_append (l, "file:///etc/group#adm");

  g_assert (g_app_info_launch_uris (appinfo, l, NULL, &error));
  g_assert_no_error (error);
  g_list_free (l);
  g_free (uri);
}

static void
test_launch (void)
{
  GAppInfo *appinfo;
  const gchar *path;

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (path);
  g_assert (appinfo != NULL);

  test_launch_for_app_info (appinfo);
  g_object_unref (appinfo);
}

static void
test_launch_no_app_id (void)
{
  const gchar desktop_file_base_contents[] =
    "[Desktop Entry]\n"
    "Type=Application\n"
    "GenericName=generic-appinfo-test\n"
    "Name=appinfo-test\n"
    "Name[de]=appinfo-test-de\n"
    "X-GNOME-FullName=example\n"
    "X-GNOME-FullName[de]=Beispiel\n"
    "Comment=GAppInfo example\n"
    "Comment[de]=GAppInfo Beispiel\n"
    "Icon=testicon.svg\n"
    "Terminal=true\n"
    "StartupNotify=true\n"
    "StartupWMClass=appinfo-class\n"
    "MimeType=image/png;image/jpeg;\n"
    "Keywords=keyword1;test keyword;\n"
    "Categories=GNOME;GTK;\n";

  const char *exec_line_variants[] = {
    "Exec=./appinfo-test --option %U %i --name %c --filename %k %m %%",
    "Exec=./appinfo-test --option %u %i --name %c --filename %k %m %%"
  };

  gsize i;

  g_test_bug ("791337");

  for (i = 0; i < G_N_ELEMENTS (exec_line_variants); i++)
  {
    gchar *desktop_file_contents;
    GKeyFile *fake_desktop_file;
    GAppInfo *appinfo;
    gboolean loaded;

    g_test_message ("Exec line variant #%" G_GSIZE_FORMAT, i);

    desktop_file_contents = g_strdup_printf ("%s\n%s",
                                             desktop_file_base_contents,
                                             exec_line_variants[i]);

    /* We load a desktop file from memory to force the app not
     * to have an app ID, which would check different codepaths.
     */
    fake_desktop_file = g_key_file_new ();
    loaded = g_key_file_load_from_data (fake_desktop_file, desktop_file_contents, -1, G_KEY_FILE_NONE, NULL);
    g_assert_true (loaded);

    appinfo = (GAppInfo*)g_desktop_app_info_new_from_keyfile (fake_desktop_file);
    g_assert (appinfo != NULL);

    test_launch_for_app_info (appinfo);

    g_free (desktop_file_contents);
    g_object_unref (appinfo);
    g_key_file_unref (fake_desktop_file);
  }
}

static void
test_locale (const char *locale)
{
  GAppInfo *appinfo;
  const gchar *orig;
  const gchar *path;

  orig = setlocale (LC_ALL, NULL);
  g_setenv ("LANGUAGE", locale, TRUE);
  setlocale (LC_ALL, "");

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (path);

  if (g_strcmp0 (locale, "C") == 0)
    {
      g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "appinfo-test");
      g_assert_cmpstr (g_app_info_get_description (appinfo), ==, "GAppInfo example");
      g_assert_cmpstr (g_app_info_get_display_name (appinfo), ==, "example");
    }
  else if (g_str_has_prefix (locale, "en"))
    {
      g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "appinfo-test");
      g_assert_cmpstr (g_app_info_get_description (appinfo), ==, "GAppInfo example");
      g_assert_cmpstr (g_app_info_get_display_name (appinfo), ==, "example");
    }
  else if (g_str_has_prefix (locale, "de"))
    {
      g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "appinfo-test-de");
      g_assert_cmpstr (g_app_info_get_description (appinfo), ==, "GAppInfo Beispiel");
      g_assert_cmpstr (g_app_info_get_display_name (appinfo), ==, "Beispiel");
    }

  g_object_unref (appinfo);

  g_setenv ("LANGUAGE", orig, TRUE);
  setlocale (LC_ALL, "");
}

static void
test_text (void)
{
  test_locale ("C");
  test_locale ("en_US");
  test_locale ("de");
  test_locale ("de_DE.UTF-8");
}

static void
test_basic (void)
{
  GAppInfo *appinfo;
  GAppInfo *appinfo2;
  GIcon *icon, *icon2;
  const gchar *path;

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (path);

  g_assert_cmpstr (g_app_info_get_id (appinfo), ==, "appinfo-test.desktop");
  g_assert (strstr (g_app_info_get_executable (appinfo), "appinfo-test") != NULL);

  icon = g_app_info_get_icon (appinfo);
  g_assert (G_IS_THEMED_ICON (icon));
  icon2 = g_themed_icon_new ("testicon");
  g_assert (g_icon_equal (icon, icon2));
  g_object_unref (icon2);

  appinfo2 = g_app_info_dup (appinfo);
  g_assert_cmpstr (g_app_info_get_id (appinfo), ==, g_app_info_get_id (appinfo2));
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, g_app_info_get_commandline (appinfo2));

  g_object_unref (appinfo);
  g_object_unref (appinfo2);
}

static void
test_show_in (void)
{
  GAppInfo *appinfo;
  const gchar *path;

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (path);
  g_assert (g_app_info_should_show (appinfo));
  g_object_unref (appinfo);

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test-gnome.desktop", NULL);
  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (path);
  g_assert (g_app_info_should_show (appinfo));
  g_object_unref (appinfo);

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test-notgnome.desktop", NULL);
  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (path);
  g_assert (!g_app_info_should_show (appinfo));
  g_object_unref (appinfo);
}

static void
test_commandline (void)
{
  GAppInfo *appinfo;
  GError *error;
  gchar *cmdline;
  gchar *cmdline_out;

  cmdline = g_strconcat (g_test_get_dir (G_TEST_BUILT), "/appinfo-test --option", NULL);
  cmdline_out = g_strconcat (cmdline, " %u", NULL);

  error = NULL;
  appinfo = g_app_info_create_from_commandline (cmdline,
                                                "cmdline-app-test",
                                                G_APP_INFO_CREATE_SUPPORTS_URIS,
                                                &error);
  g_assert (appinfo != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "cmdline-app-test");
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, cmdline_out);
  g_assert (g_app_info_supports_uris (appinfo));
  g_assert (!g_app_info_supports_files (appinfo));

  g_object_unref (appinfo);

  g_free (cmdline_out);
  cmdline_out = g_strconcat (cmdline, " %f", NULL);

  error = NULL;
  appinfo = g_app_info_create_from_commandline (cmdline,
                                                "cmdline-app-test",
                                                G_APP_INFO_CREATE_NONE,
                                                &error);
  g_assert (appinfo != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "cmdline-app-test");
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, cmdline_out);
  g_assert (!g_app_info_supports_uris (appinfo));
  g_assert (g_app_info_supports_files (appinfo));

  g_object_unref (appinfo);

  g_free (cmdline);
  g_free (cmdline_out);
}

static void
test_launch_context (void)
{
  GAppLaunchContext *context;
  GAppInfo *appinfo;
  gchar *str;
  gchar *cmdline;

  cmdline = g_strconcat (g_test_get_dir (G_TEST_BUILT), "/appinfo-test --option", NULL);

  context = g_app_launch_context_new ();
  appinfo = g_app_info_create_from_commandline (cmdline,
                                                "cmdline-app-test",
                                                G_APP_INFO_CREATE_SUPPORTS_URIS,
                                                NULL);

  str = g_app_launch_context_get_display (context, appinfo, NULL);
  g_assert (str == NULL);

  str = g_app_launch_context_get_startup_notify_id (context, appinfo, NULL);
  g_assert (str == NULL);

  g_object_unref (appinfo);
  g_object_unref (context);

  g_free (cmdline);
}

static gboolean launched_reached;

static void
launched (GAppLaunchContext *context,
          GAppInfo          *info,
          GVariant          *platform_data,
          gpointer           user_data)
{
  gint pid;

  pid = 0;
  g_assert (g_variant_lookup (platform_data, "pid", "i", &pid));
  g_assert (pid != 0);

  launched_reached = TRUE;
}

static void
launch_failed (GAppLaunchContext *context,
               const gchar       *startup_notify_id)
{
  g_assert_not_reached ();
}

static void
test_launch_context_signals (void)
{
  GAppLaunchContext *context;
  GAppInfo *appinfo;
  GError *error = NULL;
  gchar *cmdline;

  cmdline = g_strconcat (g_test_get_dir (G_TEST_BUILT), "/appinfo-test --option", NULL);

  context = g_app_launch_context_new ();
  g_signal_connect (context, "launched", G_CALLBACK (launched), NULL);
  g_signal_connect (context, "launch_failed", G_CALLBACK (launch_failed), NULL);
  appinfo = g_app_info_create_from_commandline (cmdline,
                                                "cmdline-app-test",
                                                G_APP_INFO_CREATE_SUPPORTS_URIS,
                                                NULL);

  error = NULL;
  g_assert (g_app_info_launch (appinfo, NULL, context, &error));
  g_assert_no_error (error);

  g_assert (launched_reached);

  g_object_unref (appinfo);
  g_object_unref (context);

  g_free (cmdline);
}

static void
test_tryexec (void)
{
  GAppInfo *appinfo;
  const gchar *path;

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test2.desktop", NULL);
  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (path);

  g_assert (appinfo == NULL);
}

/* Test that we can set an appinfo as default for a mime type or
 * file extension, and also add and remove handled mime types.
 */
static void
test_associations (void)
{
  GAppInfo *appinfo;
  GAppInfo *appinfo2;
  GError *error;
  gboolean result;
  GList *list;
  gchar *cmdline;

  cmdline = g_strconcat (g_test_get_dir (G_TEST_BUILT), "/appinfo-test --option", NULL);
  appinfo = g_app_info_create_from_commandline (cmdline,
                                                "cmdline-app-test",
                                                G_APP_INFO_CREATE_SUPPORTS_URIS,
                                                NULL);

  error = NULL;
  result = g_app_info_set_as_default_for_type (appinfo, "application/x-glib-test", &error);

  g_assert (result);
  g_assert_no_error (error);

  appinfo2 = g_app_info_get_default_for_type ("application/x-glib-test", FALSE);

  g_assert (appinfo2);
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, g_app_info_get_commandline (appinfo2));

  g_object_unref (appinfo2);

  result = g_app_info_set_as_default_for_extension (appinfo, "gio-tests", &error);
  g_assert (result);
  g_assert_no_error (error);

  appinfo2 = g_app_info_get_default_for_type ("application/x-extension-gio-tests", FALSE);

  g_assert (appinfo2);
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, g_app_info_get_commandline (appinfo2));

  g_object_unref (appinfo2);

  result = g_app_info_add_supports_type (appinfo, "application/x-gio-test", &error);
  g_assert (result);
  g_assert_no_error (error);

  list = g_app_info_get_all_for_type ("application/x-gio-test");
  g_assert_cmpint (g_list_length (list), ==, 1);
  appinfo2 = list->data;
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, g_app_info_get_commandline (appinfo2));
  g_object_unref (appinfo2);
  g_list_free (list);

  g_assert (g_app_info_can_remove_supports_type (appinfo));
  g_assert (g_app_info_remove_supports_type (appinfo, "application/x-gio-test", &error));
  g_assert_no_error (error);

  g_assert (g_app_info_can_delete (appinfo));
  g_assert (g_app_info_delete (appinfo));
  g_object_unref (appinfo);
}

static void
test_environment (void)
{
  GAppLaunchContext *ctx;
  gchar **env;
  const gchar *path;

  g_unsetenv ("FOO");
  g_unsetenv ("BLA");
  path = g_getenv ("PATH");

  ctx = g_app_launch_context_new ();

  env = g_app_launch_context_get_environment (ctx);

  g_assert (g_environ_getenv (env, "FOO") == NULL);
  g_assert (g_environ_getenv (env, "BLA") == NULL);
  g_assert_cmpstr (g_environ_getenv (env, "PATH"), ==, path);

  g_strfreev (env);

  g_app_launch_context_setenv (ctx, "FOO", "bar");
  g_app_launch_context_setenv (ctx, "BLA", "bla");

  env = g_app_launch_context_get_environment (ctx);

  g_assert_cmpstr (g_environ_getenv (env, "FOO"), ==, "bar");
  g_assert_cmpstr (g_environ_getenv (env, "BLA"), ==, "bla");
  g_assert_cmpstr (g_environ_getenv (env, "PATH"), ==, path);

  g_strfreev (env);

  g_app_launch_context_setenv (ctx, "FOO", "baz");
  g_app_launch_context_unsetenv (ctx, "BLA");

  env = g_app_launch_context_get_environment (ctx);

  g_assert_cmpstr (g_environ_getenv (env, "FOO"), ==, "baz");
  g_assert (g_environ_getenv (env, "BLA") == NULL);

  g_strfreev (env);

  g_object_unref (ctx);
}

static void
test_startup_wm_class (void)
{
  GDesktopAppInfo *appinfo;
  const char *wm_class;
  const gchar *path;

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  appinfo = g_desktop_app_info_new_from_filename (path);
  wm_class = g_desktop_app_info_get_startup_wm_class (appinfo);

  g_assert_cmpstr (wm_class, ==, "appinfo-class");

  g_object_unref (appinfo);
}

static void
test_supported_types (void)
{
  GAppInfo *appinfo;
  const char * const *content_types;
  const gchar *path;

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  appinfo = G_APP_INFO (g_desktop_app_info_new_from_filename (path));
  content_types = g_app_info_get_supported_types (appinfo);

  g_assert_cmpint (g_strv_length ((char**)content_types), ==, 2);
  g_assert_cmpstr (content_types[0], ==, "image/png");

  g_object_unref (appinfo);
}

static void
test_from_keyfile (void)
{
  GDesktopAppInfo *info;
  GKeyFile *kf;
  GError *error = NULL;
  const gchar *categories;
  gchar **keywords;
  const gchar *file;
  const gchar *name;
  const gchar *path;

  path = g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL);
  kf = g_key_file_new ();
  g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);
  info = g_desktop_app_info_new_from_keyfile (kf);
  g_key_file_free (kf);
  g_assert (info != NULL);

  g_object_get (info, "filename", &file, NULL);
  g_assert (file == NULL);

  file = g_desktop_app_info_get_filename (info);
  g_assert (file == NULL);
  categories = g_desktop_app_info_get_categories (info);
  g_assert_cmpstr (categories, ==, "GNOME;GTK;");
  keywords = (gchar **)g_desktop_app_info_get_keywords (info);
  g_assert_cmpint (g_strv_length (keywords), ==, 2);
  g_assert_cmpstr (keywords[0], ==, "keyword1");
  g_assert_cmpstr (keywords[1], ==, "test keyword");
  name = g_desktop_app_info_get_generic_name (info);
  g_assert_cmpstr (name, ==, "generic-appinfo-test");
  g_assert (!g_desktop_app_info_get_nodisplay (info));

  g_object_unref (info);
}

int
main (int argc, char *argv[])
{
  const gchar *build_dir;

  g_setenv ("XDG_CURRENT_DESKTOP", "GNOME", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://bugzilla.gnome.org/show_bug.cgi?id=");

  /* With Meson build we need to change into right directory, so that the
   * appinfo-test binary can be found. */
  build_dir = g_getenv ("G_TEST_BUILDDIR");
  if (build_dir)
    g_chdir (build_dir);

  g_test_add_func ("/appinfo/basic", test_basic);
  g_test_add_func ("/appinfo/text", test_text);
  g_test_add_func ("/appinfo/launch", test_launch);
  g_test_add_func ("/appinfo/launch/no-appid", test_launch_no_app_id);
  g_test_add_func ("/appinfo/show-in", test_show_in);
  g_test_add_func ("/appinfo/commandline", test_commandline);
  g_test_add_func ("/appinfo/launch-context", test_launch_context);
  g_test_add_func ("/appinfo/launch-context-signals", test_launch_context_signals);
  g_test_add_func ("/appinfo/tryexec", test_tryexec);
  g_test_add_func ("/appinfo/associations", test_associations);
  g_test_add_func ("/appinfo/environment", test_environment);
  g_test_add_func ("/appinfo/startup-wm-class", test_startup_wm_class);
  g_test_add_func ("/appinfo/supported-types", test_supported_types);
  g_test_add_func ("/appinfo/from-keyfile", test_from_keyfile);

  return g_test_run ();
}

