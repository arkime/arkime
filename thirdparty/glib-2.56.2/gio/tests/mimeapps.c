#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

static gboolean
strv_equal (gchar **strv, ...)
{
  gint count;
  va_list list;
  const gchar *str;
  gboolean res;

  res = TRUE;
  count = 0;
  va_start (list, strv);
  while (1)
    {
      str = va_arg (list, const gchar *);
      if (str == NULL)
        break;
      if (g_strcmp0 (str, strv[count]) != 0)
        {
          res = FALSE;
          break;
        }
      count++;
    }
  va_end (list);

  if (res)
    res = g_strv_length (strv) == count;

  return res;
}

const gchar *myapp_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=true %f\n"
  "Name=my app\n";

const gchar *myapp2_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=sleep %f\n"
  "Name=my app 2\n";

const gchar *myapp3_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=sleep 1\n"
  "Name=my app 3\n"
  "MimeType=image/png;";

const gchar *myapp4_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=echo %f\n"
  "Name=my app 4\n"
  "MimeType=image/bmp;";

const gchar *myapp5_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=true %f\n"
  "Name=my app 5\n"
  "MimeType=image/bmp;x-scheme-handler/ftp;";

const gchar *nosuchapp_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=no_such_application %f\n"
  "Name=no such app\n";

const gchar *defaults_data =
  "[Default Applications]\n"
  "image/bmp=myapp4.desktop;\n"
  "image/png=myapp3.desktop;\n"
  "x-scheme-handler/ftp=myapp5.desktop;\n";

const gchar *mimecache_data =
  "[MIME Cache]\n"
  "image/bmp=myapp4.desktop;myapp5.desktop;\n"
  "image/png=myapp3.desktop;\n";

/* Set up XDG_DATA_HOME and XDG_DATA_DIRS.
 * XDG_DATA_DIRS/applications will contain mimeapps.list
 * XDG_DATA_HOME/applications will contain myapp.desktop
 * and myapp2.desktop, and no mimeapps.list
 */
static void
setup (void)
{
  gchar *dir;
  gchar *xdgconfighome;
  gchar *xdgdatahome;
  gchar *xdgdatadir;
  gchar *appdir;
  gchar *apphome;
  gchar *mimeapps;
  gchar *name;
  gboolean res;
  GError *error = NULL;

  dir = g_get_current_dir ();
  xdgconfighome = g_build_filename (dir, "xdgconfighome", NULL);
  xdgdatahome = g_build_filename (dir, "xdgdatahome", NULL);
  xdgdatadir = g_build_filename (dir, "xdgdatadir", NULL);
  g_test_message ("setting XDG_CONFIG_HOME to '%s'\n", xdgconfighome);
  g_setenv ("XDG_CONFIG_HOME", xdgconfighome, TRUE);
  g_test_message ("setting XDG_DATA_HOME to '%s'\n", xdgdatahome);
  g_setenv ("XDG_DATA_HOME", xdgdatahome, TRUE);
  g_test_message ("setting XDG_DATA_DIRS to '%s'\n", xdgdatadir);
  g_setenv ("XDG_DATA_DIRS", xdgdatadir, TRUE);

  appdir = g_build_filename (xdgdatadir, "applications", NULL);
  g_test_message ("creating '%s'\n", appdir);
  res = g_mkdir_with_parents (appdir, 0700);
  g_assert (res == 0);

  name = g_build_filename (appdir, "mimeapps.list", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, defaults_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  apphome = g_build_filename (xdgdatahome, "applications", NULL);
  g_test_message ("creating '%s'\n", apphome);
  res = g_mkdir_with_parents (apphome, 0700);
  g_assert (res == 0);

  name = g_build_filename (apphome, "myapp.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, myapp_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  name = g_build_filename (apphome, "myapp2.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, myapp2_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  name = g_build_filename (apphome, "myapp3.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, myapp3_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  name = g_build_filename (apphome, "myapp4.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, myapp4_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  name = g_build_filename (apphome, "myapp5.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, myapp5_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  name = g_build_filename (apphome, "nosuchapp.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, nosuchapp_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  mimeapps = g_build_filename (apphome, "mimeapps.list", NULL);
  g_test_message ("removing '%s'\n", mimeapps);
  g_remove (mimeapps);

  name = g_build_filename (apphome, "mimeinfo.cache", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, mimecache_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  g_free (dir);
  g_free (xdgconfighome);
  g_free (xdgdatahome);
  g_free (xdgdatadir);
  g_free (apphome);
  g_free (appdir);
  g_free (mimeapps);
}

static void
test_mime_api (void)
{
  GAppInfo *appinfo;
  GAppInfo *appinfo2;
  GError *error = NULL;
  GAppInfo *def;
  GList *list;
  const gchar *contenttype = "application/pdf";

  /* clear things out */
  g_app_info_reset_type_associations (contenttype);

  appinfo = (GAppInfo*)g_desktop_app_info_new ("myapp.desktop");
  appinfo2 = (GAppInfo*)g_desktop_app_info_new ("myapp2.desktop");

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (def == NULL);
  g_assert (list == NULL);

  /* 1. add a non-default association */
  g_app_info_add_supports_type (appinfo, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 1);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 2. add another non-default association */
  g_app_info_add_supports_type (appinfo2, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo2));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 3. make the first app the default */
  g_app_info_set_as_default_for_type (appinfo, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo2));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 4. make the second app the last used one */
  g_app_info_set_as_last_used_for_type (appinfo2, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo2));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 5. reset everything */
  g_app_info_reset_type_associations (contenttype);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (def == NULL);
  g_assert (list == NULL);

  g_object_unref (appinfo);
  g_object_unref (appinfo2);
}

/* Repeat the same tests, this time checking that we handle
 * mimeapps.list as expected. These tests are different from
 * the ones in test_mime_api() in that we directly parse
 * mimeapps.list to verify the results.
 */
static void
test_mime_file (void)
{
  gchar **assoc;
  GAppInfo *appinfo;
  GAppInfo *appinfo2;
  GError *error = NULL;
  GKeyFile *keyfile;
  gchar *str;
  gboolean res;
  GAppInfo *def;
  GList *list;
  gchar *mimeapps;
  gchar *dir;
  const gchar *contenttype = "application/pdf";

  dir = g_get_current_dir ();
  mimeapps = g_build_filename (dir, "xdgconfighome", "mimeapps.list", NULL);

  /* clear things out */
  g_app_info_reset_type_associations (contenttype);

  appinfo = (GAppInfo*)g_desktop_app_info_new ("myapp.desktop");
  appinfo2 = (GAppInfo*)g_desktop_app_info_new ("myapp2.desktop");

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (def == NULL);
  g_assert (list == NULL);

  /* 1. add a non-default association */
  g_app_info_add_supports_type (appinfo, contenttype, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", contenttype, NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp.desktop", NULL));
  g_strfreev (assoc);

  /* we've unset XDG_DATA_DIRS so there should be no default */
  assoc = g_key_file_get_string_list (keyfile, "Default Applications", contenttype, NULL, &error);
  g_assert (error != NULL);
  g_clear_error (&error);

  g_key_file_free (keyfile);

  /* 2. add another non-default association */
  g_app_info_add_supports_type (appinfo2, contenttype, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", contenttype, NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp.desktop", "myapp2.desktop", NULL));
  g_strfreev (assoc);

  assoc = g_key_file_get_string_list (keyfile, "Default Applications", contenttype, NULL, &error);
  g_assert (error != NULL);
  g_clear_error (&error);

  g_key_file_free (keyfile);

  /* 3. make the first app the default */
  g_app_info_set_as_default_for_type (appinfo, contenttype, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", contenttype, NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp.desktop", "myapp2.desktop", NULL));
  g_strfreev (assoc);

  str = g_key_file_get_string (keyfile, "Default Applications", contenttype, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (str, ==, "myapp.desktop");
  g_free (str);

  g_key_file_free (keyfile);

  /* 4. make the second app the last used one */
  g_app_info_set_as_last_used_for_type (appinfo2, contenttype, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", contenttype, NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp2.desktop", "myapp.desktop", NULL));
  g_strfreev (assoc);

  g_key_file_free (keyfile);

  /* 5. reset everything */
  g_app_info_reset_type_associations (contenttype);

  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  res = g_key_file_has_key (keyfile, "Added Associations", contenttype, NULL);
  g_assert (!res);

  res = g_key_file_has_key (keyfile, "Default Applications", contenttype, NULL);
  g_assert (!res);

  g_key_file_free (keyfile);

  g_object_unref (appinfo);
  g_object_unref (appinfo2);

  g_free (mimeapps);
  g_free (dir);
}

/* test interaction between mimeapps.list at different levels */
static void
test_mime_default (void)
{
  GAppInfo *appinfo;
  GAppInfo *appinfo2;
  GAppInfo *appinfo3;
  GError *error = NULL;
  GAppInfo *def;
  GList *list;
  const gchar *contenttype = "image/png";

  /* clear things out */
  g_app_info_reset_type_associations (contenttype);

  appinfo = (GAppInfo*)g_desktop_app_info_new ("myapp.desktop");
  appinfo2 = (GAppInfo*)g_desktop_app_info_new ("myapp2.desktop");
  appinfo3 = (GAppInfo*)g_desktop_app_info_new ("myapp3.desktop");

  /* myapp3 is set as the default in defaults.list */
  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo3));
  g_assert_cmpint (g_list_length (list), ==, 1);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo3));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 1. add a non-default association */
  g_app_info_add_supports_type (appinfo, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo3)); /* default is unaffected */
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo3));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 2. add another non-default association */
  g_app_info_add_supports_type (appinfo2, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo3));
  g_assert_cmpint (g_list_length (list), ==, 3);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo2));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->next->data, appinfo3));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 3. make the first app the default */
  g_app_info_set_as_default_for_type (appinfo, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 3);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo2));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->next->data, appinfo3));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  g_object_unref (appinfo);
  g_object_unref (appinfo2);
  g_object_unref (appinfo3);
}

/* test interaction between mimeinfo.cache, defaults.list and mimeapps.list
 * to ensure g_app_info_set_as_last_used_for_type doesn't incorrectly
 * change the default
 */
static void
test_mime_default_last_used (void)
{
  GAppInfo *appinfo4;
  GAppInfo *appinfo5;
  GError *error = NULL;
  GAppInfo *def;
  GList *list;
  const gchar *contenttype = "image/bmp";

  /* clear things out */
  g_app_info_reset_type_associations (contenttype);

  appinfo4 = (GAppInfo*)g_desktop_app_info_new ("myapp4.desktop");
  appinfo5 = (GAppInfo*)g_desktop_app_info_new ("myapp5.desktop");

  /* myapp4 is set as the default in defaults.list */
  /* myapp4 and myapp5 can both handle image/bmp */
  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo4));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo4));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo5));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 1. set default (myapp4) as last used */
  g_app_info_set_as_last_used_for_type (appinfo4, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo4)); /* default is unaffected */
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo4));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo5));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 2. set other (myapp5) as last used */
  g_app_info_set_as_last_used_for_type (appinfo5, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo4));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo5));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo4));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 3. change the default to myapp5 */
  g_app_info_set_as_default_for_type (appinfo5, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo5));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo5));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo4));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 4. set myapp4 as last used */
  g_app_info_set_as_last_used_for_type (appinfo4, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo5));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo4));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo5));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* 5. set myapp5 as last used again */
  g_app_info_set_as_last_used_for_type (appinfo5, contenttype, &error);
  g_assert_no_error (error);

  def = g_app_info_get_default_for_type (contenttype, FALSE);
  list = g_app_info_get_recommended_for_type (contenttype);
  g_assert (g_app_info_equal (def, appinfo5));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo5));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo4));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  g_object_unref (appinfo4);
  g_object_unref (appinfo5);
}

static void
test_scheme_handler (void)
{
  GAppInfo *info, *info5;

  info5 = (GAppInfo*)g_desktop_app_info_new ("myapp5.desktop");
  info = g_app_info_get_default_for_uri_scheme ("ftp");
  g_assert (g_app_info_equal (info, info5));

  g_object_unref (info);
  g_object_unref (info5);
}

/* test that g_app_info_* ignores desktop files with nonexisting executables
 */
static void
test_mime_ignore_nonexisting (void)
{
  GAppInfo *appinfo;

  appinfo = (GAppInfo*)g_desktop_app_info_new ("nosuchapp.desktop");
  g_assert (appinfo == NULL);
}

static void
test_all (void)
{
  GList *all, *l;

  all = g_app_info_get_all ();

  for (l = all; l; l = l->next)
    g_assert (G_IS_APP_INFO (l->data));

  g_list_free_full (all, g_object_unref);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  setup ();

  g_test_add_func ("/appinfo/mime/api", test_mime_api);
  g_test_add_func ("/appinfo/mime/default", test_mime_default);
  g_test_add_func ("/appinfo/mime/file", test_mime_file);
  g_test_add_func ("/appinfo/mime/scheme-handler", test_scheme_handler);
  g_test_add_func ("/appinfo/mime/default-last-used", test_mime_default_last_used);
  g_test_add_func ("/appinfo/mime/ignore-nonexisting", test_mime_ignore_nonexisting);
  g_test_add_func ("/appinfo/all", test_all);

  return g_test_run ();
}
