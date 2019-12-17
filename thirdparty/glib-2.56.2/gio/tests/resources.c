/* GLib testing framework examples and tests
 *
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <gio/gio.h>
#include "gconstructor.h"
#include "test_resources2.h"

static void
test_resource (GResource *resource)
{
  GError *error = NULL;
  gboolean found, success;
  gsize size;
  guint32 flags;
  GBytes *data;
  char **children;
  GInputStream *in;
  char buffer[128];

  found = g_resource_get_info (resource,
			       "/not/there",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (!found);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  found = g_resource_get_info (resource,
			       "/test1.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, G_RESOURCE_FLAGS_COMPRESSED);

  found = g_resource_get_info (resource,
			       "/a_prefix/test2.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  found = g_resource_get_info (resource,
			       "/a_prefix/test2-alias.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  data = g_resource_lookup_data (resource,
				 "/test1.txt",
				 G_RESOURCE_LOOKUP_FLAGS_NONE,
				 &error);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_assert_no_error (error);
  g_bytes_unref (data);

  in = g_resource_open_stream (resource,
			       "/test1.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &error);
  g_assert (in != NULL);
  g_assert_no_error (error);

  success = g_input_stream_read_all (in, buffer, sizeof (buffer) - 1,
				     &size,
				     NULL, &error);
  g_assert (success);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  buffer[size] = 0;
  g_assert_cmpstr (buffer, ==, "test1\n");

  g_input_stream_close (in, NULL, &error);
  g_assert_no_error (error);
  g_clear_object (&in);

  data = g_resource_lookup_data (resource,
				 "/a_prefix/test2.txt",
				 G_RESOURCE_LOOKUP_FLAGS_NONE,
				 &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  data = g_resource_lookup_data (resource,
				 "/a_prefix/test2-alias.txt",
				 G_RESOURCE_LOOKUP_FLAGS_NONE,
				 &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  children = g_resource_enumerate_children  (resource,
					     "/not/here",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children == NULL);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  children = g_resource_enumerate_children  (resource,
					     "/a_prefix",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children != NULL);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (children), ==, 2);
  g_strfreev (children);

  /* Test the preferred lookup where we have a trailing slash. */
  children = g_resource_enumerate_children  (resource,
					     "/a_prefix/",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children != NULL);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (children), ==, 2);
  g_strfreev (children);

  /* test with a path > 256 and no trailing slash to test the
   * slow path of resources where we allocate a modified path.
   */
  children = g_resource_enumerate_children  (resource,
					     "/not/here/not/here/not/here/not/here/not/here/not/here/not/here"
					     "/not/here/not/here/not/here/not/here/not/here/not/here/not/here"
					     "/not/here/not/here/not/here/not/here/not/here/not/here/not/here"
					     "/not/here/not/here/not/here/not/here/not/here/not/here/not/here"
					     "/not/here/not/here/not/here/not/here/not/here/not/here/not/here"
					     "/with/no/trailing/slash",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children == NULL);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);
}

static void
test_resource_file (void)
{
  GResource *resource;
  GError *error = NULL;

  resource = g_resource_load ("not-there", &error);
  g_assert (resource == NULL);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
  g_clear_error (&error);

  resource = g_resource_load (g_test_get_filename (G_TEST_BUILT, "test.gresource", NULL), &error);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  test_resource (resource);
  g_resource_unref (resource);
}

static void
test_resource_file_path (void)
{
  static const struct {
    const gchar *input;
    const gchar *expected;
  } test_uris[] = {
    { "resource://", "resource:///" },
    { "resource:///", "resource:///" },
    { "resource://////", "resource:///" },
    { "resource:///../../../", "resource:///" },
    { "resource:///../../..", "resource:///" },
    { "resource://abc", "resource:///abc" },
    { "resource:///abc/", "resource:///abc" },
    { "resource:/a/b/../c/", "resource:///a/c" },
    { "resource://../a/b/../c/../", "resource:///a" },
    { "resource://a/b/cc//bb//a///", "resource:///a/b/cc/bb/a" },
    { "resource://././././", "resource:///" },
    { "resource://././././../", "resource:///" },
    { "resource://a/b/c/d.png", "resource:///a/b/c/d.png" },
    { "resource://a/b/c/..png", "resource:///a/b/c/..png" },
    { "resource://a/b/c/./png", "resource:///a/b/c/png" },
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (test_uris); i++)
    {
      GFile *file;
      gchar *uri;

      file = g_file_new_for_uri (test_uris[i].input);
      g_assert (file != NULL);

      uri = g_file_get_uri (file);
      g_assert (uri != NULL);

      g_assert_cmpstr (uri, ==, test_uris[i].expected);

      g_object_unref (file);
      g_free (uri);
    }
}

static void
test_resource_data (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean loaded_file;
  char *content;
  gsize content_size;
  GBytes *data;

  loaded_file = g_file_get_contents (g_test_get_filename (G_TEST_BUILT, "test.gresource", NULL),
                                     &content, &content_size, NULL);
  g_assert (loaded_file);

  data = g_bytes_new_take (content, content_size);
  resource = g_resource_new_from_data (data, &error);
  g_bytes_unref (data);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  test_resource (resource);

  g_resource_unref (resource);
}

static void
test_resource_data_unaligned (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean loaded_file;
  char *content, *content_copy;
  gsize content_size;
  GBytes *data;

  loaded_file = g_file_get_contents (g_test_get_filename (G_TEST_BUILT, "test.gresource", NULL),
                                     &content, &content_size, NULL);
  g_assert (loaded_file);

  content_copy = g_new (char, content_size + 1);
  memcpy (content_copy + 1, content, content_size);

  data = g_bytes_new_with_free_func (content_copy + 1, content_size,
                                     (GDestroyNotify) g_free, content_copy);
  g_free (content);
  resource = g_resource_new_from_data (data, &error);
  g_bytes_unref (data);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  test_resource (resource);

  g_resource_unref (resource);
}

static void
test_resource_registered (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean found, success;
  gsize size;
  guint32 flags;
  GBytes *data;
  char **children;
  GInputStream *in;
  char buffer[128];

  resource = g_resource_load (g_test_get_filename (G_TEST_BUILT, "test.gresource", NULL), &error);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  found = g_resources_get_info ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (!found);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  g_resources_register (resource);

  found = g_resources_get_info ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert (flags == (G_RESOURCE_FLAGS_COMPRESSED));

  found = g_resources_get_info ("/a_prefix/test2.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpint (flags, ==, 0);

  found = g_resources_get_info ("/a_prefix/test2-alias.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  data = g_resources_lookup_data ("/test1.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_assert_no_error (error);
  g_bytes_unref (data);

  in = g_resources_open_stream ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&error);
  g_assert (in != NULL);
  g_assert_no_error (error);

  success = g_input_stream_read_all (in, buffer, sizeof (buffer) - 1,
				     &size,
				     NULL, &error);
  g_assert (success);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  buffer[size] = 0;
  g_assert_cmpstr (buffer, ==, "test1\n");

  g_input_stream_close (in, NULL, &error);
  g_assert_no_error (error);
  g_clear_object (&in);

  data = g_resources_lookup_data ("/a_prefix/test2.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  data = g_resources_lookup_data ("/a_prefix/test2-alias.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  children = g_resources_enumerate_children ("/not/here",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children == NULL);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  children = g_resources_enumerate_children ("/a_prefix",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children != NULL);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (children), ==, 2);
  g_strfreev (children);

  g_resources_unregister (resource);
  g_resource_unref (resource);

  found = g_resources_get_info ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (!found);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);
}

static void
test_resource_automatic (void)
{
  GError *error = NULL;
  gboolean found;
  gsize size;
  guint32 flags;
  GBytes *data;

  found = g_resources_get_info ("/auto_loaded/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpint (flags, ==, 0);

  data = g_resources_lookup_data ("/auto_loaded/test1.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_bytes_unref (data);
}

static void
test_resource_manual (void)
{
  GError *error = NULL;
  gboolean found;
  gsize size;
  guint32 flags;
  GBytes *data;

  found = g_resources_get_info ("/manual_loaded/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  data = g_resources_lookup_data ("/manual_loaded/test1.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_bytes_unref (data);
}

static void
test_resource_manual2 (void)
{
  GResource *resource;
  GBytes *data;
  gsize size;
  GError *error = NULL;

  resource = _g_test2_get_resource ();

  data = g_resource_lookup_data (resource,
                                 "/manual_loaded/test1.txt",
				 G_RESOURCE_LOOKUP_FLAGS_NONE,
				 &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_bytes_unref (data);

  g_resource_unref (resource);
}

static void
test_resource_module (void)
{
  GIOModule *module;
  gboolean found;
  gsize size;
  guint32 flags;
  GBytes *data;
  GError *error;

  if (g_module_supported ())
    {
      /* For in-tree, this will find the .la file and use it to get to the .so in .libs/ */
      module = g_io_module_new (g_test_get_filename (G_TEST_BUILT, "libresourceplugin",  NULL));

      error = NULL;

      found = g_resources_get_info ("/resourceplugin/test1.txt",
				    G_RESOURCE_LOOKUP_FLAGS_NONE,
				    &size, &flags, &error);
      g_assert (!found);
      g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
      g_clear_error (&error);

      g_type_module_use (G_TYPE_MODULE (module));

      found = g_resources_get_info ("/resourceplugin/test1.txt",
				    G_RESOURCE_LOOKUP_FLAGS_NONE,
				    &size, &flags, &error);
      g_assert (found);
      g_assert_no_error (error);
      g_assert_cmpint (size, ==, 6);
      g_assert_cmpuint (flags, ==, 0);

      data = g_resources_lookup_data ("/resourceplugin/test1.txt",
				      G_RESOURCE_LOOKUP_FLAGS_NONE,
				      &error);
      g_assert (data != NULL);
      g_assert_no_error (error);
      size = g_bytes_get_size (data);
      g_assert_cmpint (size, ==, 6);
      g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
      g_bytes_unref (data);

      g_type_module_unuse (G_TYPE_MODULE (module));

      found = g_resources_get_info ("/resourceplugin/test1.txt",
				    G_RESOURCE_LOOKUP_FLAGS_NONE,
				    &size, &flags, &error);
      g_assert (!found);
      g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
      g_clear_error (&error);
    }
}

static void
test_uri_query_info (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean loaded_file;
  char *content;
  gsize content_size;
  GBytes *data;
  GFile *file;
  GFileInfo *info;
  const char *content_type, *mime_type;
  const char *fs_type;
  gboolean readonly;

  loaded_file = g_file_get_contents (g_test_get_filename (G_TEST_BUILT, "test.gresource", NULL),
                                     &content, &content_size, NULL);
  g_assert (loaded_file);

  data = g_bytes_new_take (content, content_size);
  resource = g_resource_new_from_data (data, &error);
  g_bytes_unref (data);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  g_resources_register (resource);

  file = g_file_new_for_uri ("resource://" "/a_prefix/test2-alias.txt");

  info = g_file_query_info (file, "*", 0, NULL, &error);
  g_assert_no_error (error);

  content_type = g_file_info_get_content_type (info);
  g_assert (content_type);
  mime_type = g_content_type_get_mime_type (content_type);
  g_assert (mime_type);
  g_assert_cmpstr (mime_type, ==, "text/plain");

  g_object_unref (info);

  info = g_file_query_filesystem_info (file, "*", NULL, &error);
  g_assert_no_error (error);

  fs_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
  g_assert_cmpstr (fs_type, ==, "resource");
  readonly = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY);
  g_assert_true (readonly);

  g_object_unref (info);

  g_assert_cmpuint  (g_file_hash (file), !=, 0);

  g_object_unref (file);

  g_resources_unregister (resource);
  g_resource_unref (resource);
}

static void
test_uri_file (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean loaded_file;
  char *content;
  gsize content_size;
  GBytes *data;
  GFile *file;
  GFileInfo *info;
  gchar *name;
  GFile *file2, *parent;
  GFileEnumerator *enumerator;
  gchar *scheme;
  GFileAttributeInfoList *attrs;
  GInputStream *stream;
  gchar buf[1024];
  gboolean ret;
  gssize skipped;

  loaded_file = g_file_get_contents (g_test_get_filename (G_TEST_BUILT, "test.gresource", NULL),
                                     &content, &content_size, NULL);
  g_assert (loaded_file);

  data = g_bytes_new_take (content, content_size);
  resource = g_resource_new_from_data (data, &error);
  g_bytes_unref (data);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  g_resources_register (resource);

  file = g_file_new_for_uri ("resource://" "/a_prefix/test2-alias.txt");

  g_assert (g_file_get_path (file) == NULL);

  name = g_file_get_parse_name (file);
  g_assert_cmpstr (name, ==, "resource:///a_prefix/test2-alias.txt");
  g_free (name);

  name = g_file_get_uri (file);
  g_assert_cmpstr (name, ==, "resource:///a_prefix/test2-alias.txt");
  g_free (name);

  g_assert (!g_file_is_native (file));
  g_assert (!g_file_has_uri_scheme (file, "http"));
  g_assert (g_file_has_uri_scheme (file, "resource"));
  scheme = g_file_get_uri_scheme (file);
  g_assert_cmpstr (scheme, ==, "resource");
  g_free (scheme);

  file2 = g_file_dup (file);
  g_assert (g_file_equal (file, file2));
  g_object_unref (file2);

  parent = g_file_get_parent (file);
  enumerator = g_file_enumerate_children (parent, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);

  file2 = g_file_get_child_for_display_name (parent, "test2-alias.txt", &error);
  g_assert_no_error (error);
  g_assert (g_file_equal (file, file2));
  g_object_unref (file2);

  info = g_file_enumerator_next_file (enumerator, NULL, &error);
  g_assert_no_error (error);
  g_assert (info != NULL);
  g_object_unref (info);

  info = g_file_enumerator_next_file (enumerator, NULL, &error);
  g_assert_no_error (error);
  g_assert (info != NULL);
  g_object_unref (info);

  info = g_file_enumerator_next_file (enumerator, NULL, &error);
  g_assert_no_error (error);
  g_assert (info == NULL);

  g_file_enumerator_close (enumerator, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (enumerator);

  file2 = g_file_new_for_uri ("resource://" "a_prefix/../a_prefix//test2-alias.txt");
  g_assert (g_file_equal (file, file2));

  g_assert (g_file_has_prefix (file, parent));

  name = g_file_get_relative_path (parent, file);
  g_assert_cmpstr (name, ==, "test2-alias.txt");
  g_free (name);

  g_object_unref (parent);

  attrs = g_file_query_settable_attributes (file, NULL, &error);
  g_assert_no_error (error);
  g_file_attribute_info_list_unref (attrs);

  attrs = g_file_query_writable_namespaces (file, NULL, &error);
  g_assert_no_error (error);
  g_file_attribute_info_list_unref (attrs);

  stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (stream)), ==, 0);
  g_assert (g_seekable_can_seek (G_SEEKABLE (G_SEEKABLE (stream))));
  ret = g_seekable_seek (G_SEEKABLE (stream), 1, G_SEEK_SET, NULL, &error);
  g_assert (ret);
  g_assert_no_error (error);
  skipped = g_input_stream_skip (stream, 1, NULL, &error);
  g_assert_cmpint (skipped, ==, 1);
  g_assert_no_error (error);

  memset (buf, 0, 1024);
  ret = g_input_stream_read_all (stream, &buf, 1024, NULL, NULL, &error);
  g_assert (ret);
  g_assert_no_error (error);
  g_assert_cmpstr (buf, ==, "st2\n");
  info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (stream),
                                         G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                         NULL,
                                         &error);
  g_assert_no_error (error);
  g_assert (info != NULL);
  g_assert_cmpint (g_file_info_get_size (info), ==, 6);
  g_object_unref (info);

  ret = g_input_stream_close (stream, NULL, &error);
  g_assert (ret);
  g_assert_no_error (error);
  g_object_unref (stream);

  g_object_unref (file);
  g_object_unref (file2);

  g_resources_unregister (resource);
  g_resource_unref (resource);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  _g_test2_register_resource ();

  g_test_add_func ("/resource/file", test_resource_file);
  g_test_add_func ("/resource/file-path", test_resource_file_path);
  g_test_add_func ("/resource/data", test_resource_data);
  g_test_add_func ("/resource/data_unaligned", test_resource_data_unaligned);
  g_test_add_func ("/resource/registered", test_resource_registered);
  g_test_add_func ("/resource/manual", test_resource_manual);
  g_test_add_func ("/resource/manual2", test_resource_manual2);
#ifdef G_HAS_CONSTRUCTORS
  g_test_add_func ("/resource/automatic", test_resource_automatic);
  /* This only uses automatic resources too, so it tests the constructors and destructors */
  g_test_add_func ("/resource/module", test_resource_module);
#endif
  g_test_add_func ("/resource/uri/query-info", test_uri_query_info);
  g_test_add_func ("/resource/uri/file", test_uri_file);

  return g_test_run();
}
