
/* Unit tests for GVfs
 * Copyright (C) 2011 Red Hat, Inc
 * Author: Matthias Clasen
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

#include <gio/gio.h>

static GFile *
test_vfs_parse_name (GVfs       *vfs,
                     const char *parse_name,
                     gpointer    user_data)
{
  GFile *file = NULL;

  if (g_strcmp0 ((parse_name), "testfile") == 0)
    {
      file = g_file_new_for_uri ("file:///");
      g_object_set_data (G_OBJECT (file), "testfile", GINT_TO_POINTER (1));
    }

  return file;
}

static GFile *
test_vfs_lookup (GVfs       *vfs,
                 const char *uri,
                 gpointer    user_data)
{
  GFile *file;
  file = g_file_new_for_uri ("file:///");
  g_object_set_data (G_OBJECT (file), "testfile", GINT_TO_POINTER (1));

  return file;
}

static void
test_register_scheme (void)
{
  GVfs *vfs;
  GFile *file;
  const gchar * const *schemes;
  gboolean res;

  vfs = g_vfs_get_default ();
  g_assert_nonnull (vfs);
  g_assert_true (g_vfs_is_active (vfs));

  schemes = g_vfs_get_supported_uri_schemes (vfs);
  g_assert_false (g_strv_contains (schemes, "test"));

  res = g_vfs_unregister_uri_scheme (vfs, "test");
  g_assert_false (res);

  res = g_vfs_register_uri_scheme (vfs, "test",
                                   test_vfs_lookup, NULL, NULL,
                                   test_vfs_parse_name, NULL, NULL);
  g_assert_true (res);

  schemes = g_vfs_get_supported_uri_schemes (vfs);
  g_assert_true (g_strv_contains (schemes, "test"));

  file = g_file_new_for_uri ("test:///foo");
  g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (file), "testfile")), ==, 1);
  g_object_unref (file);

  file = g_file_parse_name ("testfile");
  g_assert_cmpint (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (file), "testfile")), ==, 1);
  g_object_unref (file);

  res = g_vfs_register_uri_scheme (vfs, "test",
                                   test_vfs_lookup, NULL, NULL,
                                   test_vfs_parse_name, NULL, NULL);
  g_assert_false (res);

  res = g_vfs_unregister_uri_scheme (vfs, "test");
  g_assert_true (res);

  file = g_file_new_for_uri ("test:///foo");
  g_assert_null (g_object_get_data (G_OBJECT (file), "testfile"));
  g_object_unref (file);
}

static void
test_local (void)
{
  GVfs *vfs;
  GFile *file;
  gchar **schemes;

  vfs = g_vfs_get_local ();
  g_assert (g_vfs_is_active (vfs));

  file = g_vfs_get_file_for_uri (vfs, "not a good uri");
  g_assert (G_IS_FILE (file));
  g_object_unref (file);

  schemes = (gchar **)g_vfs_get_supported_uri_schemes (vfs);

  g_assert (g_strv_length (schemes) > 0);
  g_assert_cmpstr (schemes[0], ==, "file");
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gvfs/local", test_local);
  g_test_add_func ("/gvfs/register-scheme", test_register_scheme);

  return g_test_run ();
}
