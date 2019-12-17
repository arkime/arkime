/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc.
 * Authors: Tomas Bzatek <tbzatek@redhat.com>
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

#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>

#define DEFAULT_TEST_DIR		"testdir_live-g-file"

#define PATTERN_FILE_SIZE	0x10000
#define TEST_HANDLE_SPECIAL	TRUE

enum StructureExtraFlags
{
  TEST_DELETE_NORMAL = 1 << 0,
  TEST_DELETE_TRASH = 1 << 1,
  TEST_DELETE_NON_EMPTY = 1 << 2,
  TEST_DELETE_FAILURE = 1 << 3,
  TEST_NOT_EXISTS = 1 << 4,
  TEST_ENUMERATE_FILE = 1 << 5,
  TEST_NO_ACCESS = 1 << 6,
  TEST_COPY = 1 << 7,
  TEST_MOVE = 1 << 8,
  TEST_COPY_ERROR_RECURSE = 1 << 9,
  TEST_ALREADY_EXISTS = 1 << 10,
  TEST_TARGET_IS_FILE = 1 << 11,
  TEST_CREATE = 1 << 12,
  TEST_REPLACE = 1 << 13,
  TEST_APPEND = 1 << 14,
  TEST_OPEN = 1 << 15,
  TEST_OVERWRITE = 1 << 16,
  TEST_INVALID_SYMLINK = 1 << 17,
  TEST_HIDDEN = 1 << 18,
  TEST_DOT_HIDDEN = 1 << 19,
};

struct StructureItem
{
  const char *filename;
  const char *link_to;
  GFileType file_type;
  GFileCreateFlags create_flags;
  guint32 mode;
  gboolean handle_special;
  enum StructureExtraFlags extra_flags;
};

#define TEST_DIR_NO_ACCESS		"dir_no-access"
#define TEST_DIR_NO_WRITE		"dir_no-write"
#define TEST_DIR_TARGET			"dir-target"
#define TEST_NAME_NOT_EXISTS	"not_exists"
#define TEST_TARGET_FILE		"target-file"


static const struct StructureItem sample_struct[] = {
/*	 filename				link	file_type				create_flags		mode | handle_special | extra_flags              */
    {"dir1",				NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_NONE, 0, 0, TEST_DELETE_NORMAL | TEST_DELETE_NON_EMPTY | TEST_REPLACE | TEST_OPEN},
    {"dir1/subdir",			NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_NONE, 0, 0, TEST_COPY	| TEST_COPY_ERROR_RECURSE | TEST_APPEND},
    {"dir2",				NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_NONE, 0, 0, TEST_DELETE_NORMAL | TEST_MOVE | TEST_CREATE},
    {TEST_DIR_TARGET,		NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_NONE, 0, 0, TEST_COPY | TEST_COPY_ERROR_RECURSE},
    {TEST_DIR_NO_ACCESS,	NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_PRIVATE, S_IRUSR + S_IWUSR + S_IRGRP + S_IWGRP + S_IROTH + S_IWOTH, 0, TEST_NO_ACCESS | TEST_OPEN},
    {TEST_DIR_NO_WRITE,		NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_PRIVATE, S_IRUSR + S_IXUSR + S_IRGRP + S_IXGRP + S_IROTH + S_IXOTH, 0, 0},
    {TEST_TARGET_FILE,		NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, 0, TEST_COPY | TEST_OPEN},
	{"normal_file",			NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, 0, TEST_ENUMERATE_FILE | TEST_CREATE | TEST_OVERWRITE},
	{"normal_file-symlink",	"normal_file",	G_FILE_TYPE_SYMBOLIC_LINK, G_FILE_CREATE_NONE, 0, 0, TEST_ENUMERATE_FILE | TEST_COPY | TEST_OPEN},
    {"executable_file",		NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, S_IRWXU + S_IRWXG + S_IRWXO, 0, TEST_DELETE_TRASH | TEST_COPY | TEST_OPEN | TEST_OVERWRITE | TEST_REPLACE},
    {"private_file",		NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_PRIVATE, 0, 0, TEST_COPY | TEST_OPEN | TEST_OVERWRITE | TEST_APPEND},
    {"normal_file2",		NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, 0, TEST_COPY | TEST_OVERWRITE | TEST_REPLACE},
    {"readonly_file",		NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, S_IRUSR + S_IRGRP + S_IROTH, 0, TEST_DELETE_NORMAL | TEST_OPEN},
    {"UTF_pr\xcc\x8ci\xcc\x81lis\xcc\x8c z",
    						NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, 0, TEST_COPY | TEST_CREATE | TEST_OPEN | TEST_OVERWRITE},
    {"dir_pr\xcc\x8ci\xcc\x81lis\xcc\x8c z",
    						NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_NONE, 0, 0, TEST_DELETE_NORMAL | TEST_CREATE},
    {"pattern_file",		NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, TEST_COPY | TEST_OPEN | TEST_APPEND},
    {TEST_NAME_NOT_EXISTS,	NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, TEST_DELETE_NORMAL | TEST_NOT_EXISTS | TEST_COPY | TEST_OPEN},
    {TEST_NAME_NOT_EXISTS,	NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, TEST_DELETE_TRASH | TEST_NOT_EXISTS | TEST_MOVE},
    {"not_exists2",			NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, TEST_NOT_EXISTS | TEST_CREATE},
    {"not_exists3",			NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, TEST_NOT_EXISTS | TEST_REPLACE},
    {"not_exists4",			NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, TEST_NOT_EXISTS | TEST_APPEND},
    {"dir_no-execute/file",	NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, TEST_DELETE_NORMAL | TEST_DELETE_FAILURE | TEST_NOT_EXISTS | TEST_OPEN},
	{"lost_symlink",		"nowhere",	G_FILE_TYPE_SYMBOLIC_LINK, G_FILE_CREATE_NONE, 0, 0, TEST_COPY | TEST_DELETE_NORMAL | TEST_OPEN | TEST_INVALID_SYMLINK},
    {"dir_hidden",		NULL,	G_FILE_TYPE_DIRECTORY,	G_FILE_CREATE_NONE, 0, 0, 0},
    {"dir_hidden/.hidden",		NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, TEST_HANDLE_SPECIAL, 0},
    {"dir_hidden/.a-hidden-file",	NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, 0, TEST_HIDDEN},
    {"dir_hidden/file-in-.hidden1",	NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, 0, TEST_HIDDEN | TEST_DOT_HIDDEN},
    {"dir_hidden/file-in-.hidden2",	NULL,	G_FILE_TYPE_REGULAR,	G_FILE_CREATE_NONE, 0, 0, TEST_HIDDEN | TEST_DOT_HIDDEN},
  };

static gboolean test_suite;
static gboolean write_test;
static gboolean verbose;
static gboolean posix_compat;

#ifdef G_HAVE_ISO_VARARGS
#define log(...) if (verbose)  g_printerr (__VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
#define log(msg...) if (verbose)  g_printerr (msg)
#else  /* no varargs macros */
static void log (const g_char *format, ...)
{
  va_list args;
  va_start (args, format);
  if (verbose) g_printerr (format, args);
  va_end (args);
}
#endif

static GFile *
create_empty_file (GFile * parent, const char *filename,
		   GFileCreateFlags create_flags)
{
  GFile *child;
  GError *error;
  GFileOutputStream *outs;

  child = g_file_get_child (parent, filename);
  g_assert (child != NULL);

  error = NULL;
  outs = g_file_replace (child, NULL, FALSE, create_flags, NULL, &error);
  g_assert_no_error (error);
  g_assert (outs != NULL);
  error = NULL;
  g_output_stream_close (G_OUTPUT_STREAM (outs), NULL, &error);
  g_object_unref (outs);
  return child;
}

static GFile *
create_empty_dir (GFile * parent, const char *filename)
{
  GFile *child;
  gboolean res;
  GError *error;

  child = g_file_get_child (parent, filename);
  g_assert (child != NULL);
  error = NULL;
  res = g_file_make_directory (child, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert_no_error (error);
  return child;
}

static GFile *
create_symlink (GFile * parent, const char *filename, const char *points_to)
{
  GFile *child;
  gboolean res;
  GError *error;

  child = g_file_get_child (parent, filename);
  g_assert (child != NULL);
  error = NULL;
  res = g_file_make_symbolic_link (child, points_to, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert_no_error (error);
  return child;
}

static void
test_create_structure (gconstpointer test_data)
{
  GFile *root;
  GFile *child;
  gboolean res;
  GError *error;
  GFileOutputStream *outs;
  GDataOutputStream *outds;
  guint i;
  struct StructureItem item;

  g_assert (test_data != NULL);
  log ("\n  Going to create testing structure in '%s'...\n",
       (char *) test_data);

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);

  /*  create root directory  */
  res = g_file_make_directory (root, NULL, NULL);
  /*  don't care about errors here  */

  /*  create any other items  */
  for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
    {
      item = sample_struct[i];
      if ((item.handle_special)
	  || ((!posix_compat)
	      && (item.file_type == G_FILE_TYPE_SYMBOLIC_LINK)))
	continue;

      child = NULL;
      switch (item.file_type)
	{
	case G_FILE_TYPE_REGULAR:
	  log ("    Creating file '%s'...\n", item.filename);
	  child = create_empty_file (root, item.filename, item.create_flags);
	  break;
	case G_FILE_TYPE_DIRECTORY:
	  log ("    Creating directory '%s'...\n", item.filename);
	  child = create_empty_dir (root, item.filename);
	  break;
	case G_FILE_TYPE_SYMBOLIC_LINK:
	  log ("    Creating symlink '%s' --> '%s'...\n", item.filename,
	       item.link_to);
	  child = create_symlink (root, item.filename, item.link_to);
	  break;
        case G_FILE_TYPE_UNKNOWN:
        case G_FILE_TYPE_SPECIAL:
        case G_FILE_TYPE_SHORTCUT:
        case G_FILE_TYPE_MOUNTABLE:
	default:
	  break;
	}
      g_assert (child != NULL);

      if ((item.mode > 0) && (posix_compat))
	{
	  error = NULL;
	  res =
	    g_file_set_attribute_uint32 (child, G_FILE_ATTRIBUTE_UNIX_MODE,
					 item.mode,
					 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					 NULL, &error);
	  g_assert_cmpint (res, ==, TRUE);
	  g_assert_no_error (error);
	}

      if ((item.extra_flags & TEST_DOT_HIDDEN) == TEST_DOT_HIDDEN)
	{
	  gchar *dir, *path, *basename;
	  FILE *f;

	  dir = g_path_get_dirname (item.filename);
	  basename = g_path_get_basename (item.filename);
	  path = g_build_filename (test_data, dir, ".hidden", NULL);

	  f = fopen (path, "a");
	  fprintf (f, "%s\n", basename);
	  fclose (f);

	  g_free (dir);
	  g_free (path);
	  g_free (basename);
	}

      g_object_unref (child);
    }

  /*  create a pattern file  */
  log ("    Creating pattern file...");
  child = g_file_get_child (root, "pattern_file");
  g_assert (child != NULL);

  error = NULL;
  outs =
    g_file_replace (child, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
  g_assert_no_error (error);

  g_assert (outs != NULL);
  outds = g_data_output_stream_new (G_OUTPUT_STREAM (outs));
  g_assert (outds != NULL);
  for (i = 0; i < PATTERN_FILE_SIZE; i++)
    {
      error = NULL;
      res = g_data_output_stream_put_byte (outds, i % 256, NULL, &error);
      g_assert_no_error (error);
    }
  error = NULL;
  res = g_output_stream_close (G_OUTPUT_STREAM (outs), NULL, &error);
  g_assert_no_error (error);
  g_object_unref (outds);
  g_object_unref (outs);
  g_object_unref (child);
  log (" done.\n");

  g_object_unref (root);
}

static GFile *
file_exists (GFile * parent, const char *filename, gboolean * result)
{
  GFile *child;
  gboolean res;

  if (result)
    *result = FALSE;

  child = g_file_get_child (parent, filename);
  g_assert (child != NULL);
  res = g_file_query_exists (child, NULL);
  if (result)
    *result = res;

  return child;
}

static void
test_attributes (struct StructureItem item, GFileInfo * info)
{
  GFileType ftype;
  guint32 mode;
  const char *name, *display_name, *edit_name, *copy_name, *symlink_target;
  gboolean utf8_valid;
  gboolean has_attr;
  gboolean is_symlink;
  gboolean is_hidden;
  gboolean can_read, can_write;

  /*  standard::type  */
  has_attr = g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_TYPE);
  g_assert_cmpint (has_attr, ==, TRUE);
  ftype = g_file_info_get_file_type (info);
  g_assert_cmpint (ftype, !=, G_FILE_TYPE_UNKNOWN);
  g_assert_cmpint (ftype, ==, item.file_type);

  /*  unix::mode  */
  if ((item.mode > 0) && (posix_compat))
    {
      mode =
	g_file_info_get_attribute_uint32 (info,
					  G_FILE_ATTRIBUTE_UNIX_MODE) & 0xFFF;
      g_assert_cmpint (mode, ==, item.mode);
    }

  /*  access::can-read  */
  if (item.file_type != G_FILE_TYPE_SYMBOLIC_LINK)
    {
      can_read =
	g_file_info_get_attribute_boolean (info,
					   G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
      g_assert_cmpint (can_read, ==, TRUE);
    }

  /*  access::can-write  */
  if ((write_test) && ((item.extra_flags & TEST_OVERWRITE) == TEST_OVERWRITE))
    {
      can_write =
	g_file_info_get_attribute_boolean (info,
					   G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
      g_assert_cmpint (can_write, ==, TRUE);
    }

  /*  standard::name  */
  name = g_file_info_get_name (info);
  g_assert (name != NULL);

  /*  standard::display-name  */
  display_name = g_file_info_get_display_name (info);
  g_assert (display_name != NULL);
  utf8_valid = g_utf8_validate (display_name, -1, NULL);
  g_assert_cmpint (utf8_valid, ==, TRUE);

  /*  standard::edit-name  */
  edit_name = g_file_info_get_edit_name (info);
  if (edit_name)
    {
      utf8_valid = g_utf8_validate (edit_name, -1, NULL);
      g_assert_cmpint (utf8_valid, ==, TRUE);
    }

  /*  standard::copy-name  */
  copy_name =
    g_file_info_get_attribute_string (info,
				      G_FILE_ATTRIBUTE_STANDARD_COPY_NAME);
  if (copy_name)
    {
      utf8_valid = g_utf8_validate (copy_name, -1, NULL);
      g_assert_cmpint (utf8_valid, ==, TRUE);
    }

  /*  standard::is-symlink  */
  if (posix_compat)
    {
      is_symlink = g_file_info_get_is_symlink (info);
      g_assert_cmpint (is_symlink, ==,
		       item.file_type == G_FILE_TYPE_SYMBOLIC_LINK);
    }

  /*  standard::symlink-target  */
  if ((item.file_type == G_FILE_TYPE_SYMBOLIC_LINK) && (posix_compat))
    {
      symlink_target = g_file_info_get_symlink_target (info);
      g_assert_cmpstr (symlink_target, ==, item.link_to);
    }

  /*  standard::is-hidden  */
  if ((item.extra_flags & TEST_HIDDEN) == TEST_HIDDEN)
    {
      is_hidden =
	g_file_info_get_attribute_boolean (info,
					   G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN);
      g_assert_cmpint (is_hidden, ==, TRUE);
    }
}

static void
test_initial_structure (gconstpointer test_data)
{
  GFile *root;
  GFile *child;
  gboolean res;
  GError *error;
  GFileInputStream *ins;
  guint i;
  GFileInfo *info;
  guint32 size;
  guchar *buffer;
  gssize read, total_read;
  struct StructureItem item;


  g_assert (test_data != NULL);
  log ("\n  Testing sample structure in '%s'...\n", (char *) test_data);

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);

  /*  test the structure  */
  for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
    {
      item = sample_struct[i];
      if (((!posix_compat) && (item.file_type == G_FILE_TYPE_SYMBOLIC_LINK))
	  || (item.handle_special))
	continue;

      log ("    Testing file '%s'...\n", item.filename);

      child = file_exists (root, item.filename, &res);
      g_assert (child != NULL);
      g_assert_cmpint (res, ==, TRUE);

      error = NULL;
      info =
	g_file_query_info (child, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
			   NULL, &error);
      g_assert_no_error (error);
      g_assert (info != NULL);

      test_attributes (item, info);

      g_object_unref (child);
      g_object_unref (info);
    }

  /*  read and test the pattern file  */
  log ("    Testing pattern file...\n");
  child = file_exists (root, "pattern_file", &res);
  g_assert (child != NULL);
  g_assert_cmpint (res, ==, TRUE);

  error = NULL;
  info =
    g_file_query_info (child, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
		       &error);
  g_assert_no_error (error);
  g_assert (info != NULL);
  size = g_file_info_get_size (info);
  g_assert_cmpint (size, ==, PATTERN_FILE_SIZE);
  g_object_unref (info);

  error = NULL;
  ins = g_file_read (child, NULL, &error);
  g_assert (ins != NULL);
  g_assert_no_error (error);

  buffer = g_malloc (PATTERN_FILE_SIZE);
  total_read = 0;

  while (total_read < PATTERN_FILE_SIZE)
    {
      error = NULL;
      read =
	g_input_stream_read (G_INPUT_STREAM (ins), buffer + total_read,
			     PATTERN_FILE_SIZE, NULL, &error);
      g_assert_no_error (error);
      total_read += read;
      log ("      read %"G_GSSIZE_FORMAT" bytes, total = %"G_GSSIZE_FORMAT" of %d.\n",
	   read, total_read, PATTERN_FILE_SIZE);
    }
  g_assert_cmpint (total_read, ==, PATTERN_FILE_SIZE);

  error = NULL;
  res = g_input_stream_close (G_INPUT_STREAM (ins), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (res, ==, TRUE);

  for (i = 0; i < PATTERN_FILE_SIZE; i++)
    g_assert_cmpint (*(buffer + i), ==, i % 256);

  g_object_unref (ins);
  g_object_unref (child);
  g_free (buffer);
  g_object_unref (root);
}

static void
traverse_recurse_dirs (GFile * parent, GFile * root)
{
  gboolean res;
  GError *error;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GFile *descend;
  char *relative_path;
  guint i;
  gboolean found;

  g_assert (root != NULL);

  error = NULL;
  enumerator =
    g_file_enumerate_children (parent, "*",
			       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
			       &error);
  g_assert (enumerator != NULL);
  g_assert_no_error (error);

  g_assert (g_file_enumerator_get_container (enumerator) == parent);

  error = NULL;
  info = g_file_enumerator_next_file (enumerator, NULL, &error);
  while ((info) && (!error))
    {
      descend = g_file_enumerator_get_child (enumerator, info);
      g_assert (descend != NULL);
      relative_path = g_file_get_relative_path (root, descend);
      g_assert (relative_path != NULL);

      found = FALSE;
      for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
	{
	  if (strcmp (sample_struct[i].filename, relative_path) == 0)
	    {
	      /*  test the attributes again  */
	      test_attributes (sample_struct[i], info);

	      found = TRUE;
	      break;
	    }
	}
      g_assert_cmpint (found, ==, TRUE);

      log ("  Found file %s, relative to root: %s\n",
	   g_file_info_get_display_name (info), relative_path);

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
	traverse_recurse_dirs (descend, root);

      g_object_unref (descend);
      error = NULL;
      g_object_unref (info);
      g_free (relative_path);

      info = g_file_enumerator_next_file (enumerator, NULL, &error);
    }
  g_assert_no_error (error);

  error = NULL;
  res = g_file_enumerator_close (enumerator, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert_no_error (error);
  g_assert (g_file_enumerator_is_closed (enumerator));

  g_object_unref (enumerator);
}

static void
test_traverse_structure (gconstpointer test_data)
{
  GFile *root;
  gboolean res;

  g_assert (test_data != NULL);
  log ("\n  Traversing through the sample structure in '%s'...\n",
       (char *) test_data);

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);

  traverse_recurse_dirs (root, root);

  g_object_unref (root);
}




static void
test_enumerate (gconstpointer test_data)
{
  GFile *root, *child;
  gboolean res;
  GError *error;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  guint i;
  struct StructureItem item;


  g_assert (test_data != NULL);
  log ("\n  Test enumerate '%s'...\n", (char *) test_data);

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);


  for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
    {
      item = sample_struct[i];
      if ((!posix_compat) && (item.file_type == G_FILE_TYPE_SYMBOLIC_LINK))
	continue;

      if (((item.extra_flags & TEST_NOT_EXISTS) == TEST_NOT_EXISTS) ||
	  (((item.extra_flags & TEST_NO_ACCESS) == TEST_NO_ACCESS)
	   && posix_compat)
	  || ((item.extra_flags & TEST_ENUMERATE_FILE) ==
	      TEST_ENUMERATE_FILE))
	{
	  log ("    Testing file '%s'\n", item.filename);
	  child = g_file_get_child (root, item.filename);
	  g_assert (child != NULL);
	  error = NULL;
	  enumerator =
	    g_file_enumerate_children (child, "*",
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL, &error);

	  if ((item.extra_flags & TEST_NOT_EXISTS) == TEST_NOT_EXISTS)
	    {
	      g_assert (enumerator == NULL);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	    }
	  if ((item.extra_flags & TEST_ENUMERATE_FILE) == TEST_ENUMERATE_FILE)
	    {
	      g_assert (enumerator == NULL);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY);
	    }
	  if ((item.extra_flags & TEST_NO_ACCESS) == TEST_NO_ACCESS)
	    {
	      g_assert (enumerator != NULL);

	      error = NULL;
	      info = g_file_enumerator_next_file (enumerator, NULL, &error);
	      g_assert (info == NULL);
	      g_assert_no_error (error);
	      /*  no items should be found, no error should be logged  */
	    }

	  if (error)
	    g_error_free (error);

	  if (enumerator)
	    {
	      error = NULL;
	      res = g_file_enumerator_close (enumerator, NULL, &error);
	      g_assert_cmpint (res, ==, TRUE);
	      g_assert_no_error (error);

              g_object_unref (enumerator);
	    }
	  g_object_unref (child);
	}
    }
  g_object_unref (root);
}

static void
do_copy_move (GFile * root, struct StructureItem item, const char *target_dir,
	      enum StructureExtraFlags extra_flags)
{
  GFile *dst_dir, *src_file, *dst_file;
  gboolean res;
  GError *error;

  log ("    do_copy_move: '%s' --> '%s'\n", item.filename, target_dir);

  dst_dir = g_file_get_child (root, target_dir);
  g_assert (dst_dir != NULL);
  src_file = g_file_get_child (root, item.filename);
  g_assert (src_file != NULL);
  dst_file = g_file_get_child (dst_dir, item.filename);
  g_assert (dst_file != NULL);

  error = NULL;
  if ((item.extra_flags & TEST_COPY) == TEST_COPY)
    res =
      g_file_copy (src_file, dst_file,
		   G_FILE_COPY_NOFOLLOW_SYMLINKS |
		   ((extra_flags ==
		     TEST_OVERWRITE) ? G_FILE_COPY_OVERWRITE :
		    G_FILE_COPY_NONE), NULL, NULL, NULL, &error);
  else
    res =
      g_file_move (src_file, dst_file, G_FILE_COPY_NOFOLLOW_SYMLINKS, NULL,
		   NULL, NULL, &error);

  if (error)
    log ("       res = %d, error code %d = %s\n", res, error->code,
	 error->message);

  /*  copying file/directory to itself (".")  */
  if (((item.extra_flags & TEST_NOT_EXISTS) != TEST_NOT_EXISTS) &&
      (extra_flags == TEST_ALREADY_EXISTS))
    {
      g_assert_cmpint (res, ==, FALSE);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS);
    }
  /*  target file is a file, overwrite is not set  */
  else if (((item.extra_flags & TEST_NOT_EXISTS) != TEST_NOT_EXISTS) &&
	   (extra_flags == TEST_TARGET_IS_FILE))
    {
      g_assert_cmpint (res, ==, FALSE);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY);
    }
  /*  source file is directory  */
  else if ((item.extra_flags & TEST_COPY_ERROR_RECURSE) ==
	   TEST_COPY_ERROR_RECURSE)
    {
      g_assert_cmpint (res, ==, FALSE);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_RECURSE);
    }
  /*  source or target path doesn't exist  */
  else if (((item.extra_flags & TEST_NOT_EXISTS) == TEST_NOT_EXISTS) ||
	   (extra_flags == TEST_NOT_EXISTS))
    {
      g_assert_cmpint (res, ==, FALSE);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    }
  /*  source or target path permission denied  */
  else if (((item.extra_flags & TEST_NO_ACCESS) == TEST_NO_ACCESS) ||
	   (extra_flags == TEST_NO_ACCESS))
    {
      /* This works for root, see bug #552912 */
      if (test_suite && getuid () == 0)
	{
	  g_assert_cmpint (res, ==, TRUE);
	  g_assert_no_error (error);
	}
      else
	{
	  g_assert_cmpint (res, ==, FALSE);
	  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
	}
    }
  /*  no error should be found, all exceptions defined above  */
  else
    {
      g_assert_cmpint (res, ==, TRUE);
      g_assert_no_error (error);
    }

  if (error)
    g_error_free (error);


  g_object_unref (dst_dir);
  g_object_unref (src_file);
  g_object_unref (dst_file);
}

static void
test_copy_move (gconstpointer test_data)
{
  GFile *root;
  gboolean res;
  guint i;
  struct StructureItem item;

  log ("\n");

  g_assert (test_data != NULL);
  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);


  for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
    {
      item = sample_struct[i];

      if ((!posix_compat) && (item.file_type == G_FILE_TYPE_SYMBOLIC_LINK))
	continue;

      if (((item.extra_flags & TEST_COPY) == TEST_COPY) ||
	  ((item.extra_flags & TEST_MOVE) == TEST_MOVE))
	{
	  /*  test copy/move to a directory, expecting no errors if source files exist  */
	  do_copy_move (root, item, TEST_DIR_TARGET, 0);

	  /*  some files have been already moved so we can't count with them in the tests  */
	  if ((item.extra_flags & TEST_COPY) == TEST_COPY)
	    {
	      /*  test overwrite for flagged files  */
	      if ((item.extra_flags & TEST_OVERWRITE) == TEST_OVERWRITE)
		{
		  do_copy_move (root, item, TEST_DIR_TARGET, TEST_OVERWRITE);
		}
	      /*  source = target, should return G_IO_ERROR_EXISTS  */
	      do_copy_move (root, item, ".", TEST_ALREADY_EXISTS);
	      /*  target is file  */
	      do_copy_move (root, item, TEST_TARGET_FILE,
			    TEST_TARGET_IS_FILE);
	      /*  target path is invalid  */
	      do_copy_move (root, item, TEST_NAME_NOT_EXISTS,
			    TEST_NOT_EXISTS);

	      /*  tests on POSIX-compatible filesystems  */
	      if (posix_compat)
		{
		  /*  target directory is not accessible (no execute flag)  */
		  do_copy_move (root, item, TEST_DIR_NO_ACCESS,
				TEST_NO_ACCESS);
		  /*  target directory is readonly  */
		  do_copy_move (root, item, TEST_DIR_NO_WRITE,
				TEST_NO_ACCESS);
		}
	    }
	}
    }
  g_object_unref (root);
}

static void
test_create (gconstpointer test_data)
{
  GFile *root, *child;
  gboolean res;
  GError *error;
  guint i;
  struct StructureItem item;
  GFileOutputStream *os;

  g_assert (test_data != NULL);
  log ("\n");

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);

  for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
    {
      item = sample_struct[i];

      if (((item.extra_flags & TEST_CREATE) == TEST_CREATE) ||
	  ((item.extra_flags & TEST_REPLACE) == TEST_REPLACE) ||
	  ((item.extra_flags & TEST_APPEND) == TEST_APPEND))
	{
	  log ("  test_create: '%s'\n", item.filename);

	  child = g_file_get_child (root, item.filename);
	  g_assert (child != NULL);
	  error = NULL;
	  os = NULL;

	  if ((item.extra_flags & TEST_CREATE) == TEST_CREATE)
	    os = g_file_create (child, item.create_flags, NULL, &error);
	  else if ((item.extra_flags & TEST_REPLACE) == TEST_REPLACE)
	    os =
	      g_file_replace (child, NULL, TRUE, item.create_flags, NULL,
			      &error);
	  else if ((item.extra_flags & TEST_APPEND) == TEST_APPEND)
	    os = g_file_append_to (child, item.create_flags, NULL, &error);


	  if (error)
	    log ("       error code %d = %s\n", error->code, error->message);

	  if (((item.extra_flags & TEST_NOT_EXISTS) == 0) &&
	      ((item.extra_flags & TEST_CREATE) == TEST_CREATE))
	    {
	      g_assert (os == NULL);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS);
	    }
	  else if (item.file_type == G_FILE_TYPE_DIRECTORY)
	    {
	      g_assert (os == NULL);
	      if ((item.extra_flags & TEST_CREATE) == TEST_CREATE)
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS);
	      else
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY);
	    }
	  else
	    {
	      g_assert (os != NULL);
	      g_assert_no_error (error);
	    }

	  if (error)
	    g_error_free (error);

	  if (os)
	    {
	      error = NULL;
	      res =
		g_output_stream_close (G_OUTPUT_STREAM (os), NULL, &error);
	      if (error)
		log ("         g_output_stream_close: error %d = %s\n",
		     error->code, error->message);
	      g_assert_cmpint (res, ==, TRUE);
	      g_assert_no_error (error);
              g_object_unref (os);
	    }
	  g_object_unref (child);
	}
    }
  g_object_unref (root);
}

static void
test_open (gconstpointer test_data)
{
  GFile *root, *child;
  gboolean res;
  GError *error;
  guint i;
  struct StructureItem item;
  GFileInputStream *input_stream;

  g_assert (test_data != NULL);
  log ("\n");

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);

  for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
    {
      item = sample_struct[i];

      if ((!posix_compat) && (item.file_type == G_FILE_TYPE_SYMBOLIC_LINK))
	continue;

      if ((item.extra_flags & TEST_OPEN) == TEST_OPEN)
	{
	  log ("  test_open: '%s'\n", item.filename);

	  child = g_file_get_child (root, item.filename);
	  g_assert (child != NULL);
	  error = NULL;
	  input_stream = g_file_read (child, NULL, &error);

	  if (((item.extra_flags & TEST_NOT_EXISTS) == TEST_NOT_EXISTS) ||
	      ((item.extra_flags & TEST_INVALID_SYMLINK) ==
	       TEST_INVALID_SYMLINK))
	    {
	      g_assert (input_stream == NULL);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	    }
	  else if (item.file_type == G_FILE_TYPE_DIRECTORY)
	    {
	      g_assert (input_stream == NULL);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY);
	    }
	  else
	    {
	      g_assert (input_stream != NULL);
	      g_assert_no_error (error);
	    }

	  if (error)
	    g_error_free (error);

	  if (input_stream)
	    {
	      error = NULL;
	      res =
		g_input_stream_close (G_INPUT_STREAM (input_stream), NULL,
				      &error);
	      g_assert_cmpint (res, ==, TRUE);
	      g_assert_no_error (error);
              g_object_unref (input_stream);
	    }
	  g_object_unref (child);
	}
    }
  g_object_unref (root);
}

static void
test_delete (gconstpointer test_data)
{
  GFile *root;
  GFile *child;
  gboolean res;
  GError *error;
  guint i;
  struct StructureItem item;
  gchar *path;

  g_assert (test_data != NULL);
  log ("\n");

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);

  for (i = 0; i < G_N_ELEMENTS (sample_struct); i++)
    {
      item = sample_struct[i];

      if ((!posix_compat) && (item.file_type == G_FILE_TYPE_SYMBOLIC_LINK))
	continue;

      if (((item.extra_flags & TEST_DELETE_NORMAL) == TEST_DELETE_NORMAL) ||
	  ((item.extra_flags & TEST_DELETE_TRASH) == TEST_DELETE_TRASH))
	{
	  child = file_exists (root, item.filename, &res);
	  g_assert (child != NULL);
	  /*  we don't care about result here  */

          path = g_file_get_path (child);
	  log ("  Deleting %s, path = %s\n", item.filename, path);
          g_free (path);

	  error = NULL;
	  if ((item.extra_flags & TEST_DELETE_NORMAL) == TEST_DELETE_NORMAL)
	    res = g_file_delete (child, NULL, &error);
	  else
	    res = g_file_trash (child, NULL, &error);

	  if ((item.extra_flags & TEST_DELETE_NON_EMPTY) ==
	      TEST_DELETE_NON_EMPTY)
	    {
	      g_assert_cmpint (res, ==, FALSE);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_EMPTY);
	    }
	  if ((item.extra_flags & TEST_DELETE_FAILURE) == TEST_DELETE_FAILURE)
	    {
	      g_assert_cmpint (res, ==, FALSE);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	    }
	  if ((item.extra_flags & TEST_NOT_EXISTS) == TEST_NOT_EXISTS)
	    {
	      g_assert_cmpint (res, ==, FALSE);
	      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	    }

	  if (error)
	    {
	      log ("      result = %d, error = %s\n", res, error->message);
	      g_error_free (error);
	    }

	  g_object_unref (child);
	}
    }
  g_object_unref (root);
}

static void
test_make_directory_with_parents (gconstpointer test_data)
{
  GFile *root, *child, *grandchild, *greatgrandchild;
  gboolean res;
  GError *error = NULL;

  g_assert (test_data != NULL);

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  res = g_file_query_exists (root, NULL);
  g_assert_cmpint (res, ==, TRUE);

  child = g_file_get_child (root, "a");
  grandchild = g_file_get_child (child, "b");
  greatgrandchild = g_file_get_child (grandchild, "c");

  /* Check that we can successfully make directory hierarchies of
   * depth 1, 2, or 3
   */
  res = g_file_make_directory_with_parents (child, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert_no_error (error);
  res = g_file_query_exists (child, NULL);
  g_assert_cmpint (res, ==, TRUE);

  g_file_delete (child, NULL, NULL);

  res = g_file_make_directory_with_parents (grandchild, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert_no_error (error);
  res = g_file_query_exists (grandchild, NULL);
  g_assert_cmpint (res, ==, TRUE);

  g_file_delete (grandchild, NULL, NULL);
  g_file_delete (child, NULL, NULL);

  res = g_file_make_directory_with_parents (greatgrandchild, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert_no_error (error);
  res = g_file_query_exists (greatgrandchild, NULL);
  g_assert_cmpint (res, ==, TRUE);

  g_file_delete (greatgrandchild, NULL, NULL);
  g_file_delete (grandchild, NULL, NULL);
  g_file_delete (child, NULL, NULL);

  /* Now test failure by trying to create a directory hierarchy
   * where a ancestor exists but is read-only
   */

  /* No obvious way to do this on Windows */
  if (!posix_compat)
    goto out;

#ifndef G_PLATFORM_WIN32
  if (getuid() == 0) /* permissions are ignored for root */
    goto out;
#endif

  g_file_make_directory (child, NULL, NULL);
  g_assert_cmpint (res, ==, TRUE);

  res = g_file_set_attribute_uint32 (child,
                                     G_FILE_ATTRIBUTE_UNIX_MODE,
                                     S_IRUSR + S_IXUSR, /* -r-x------ */
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                     NULL, NULL);
  g_assert_cmpint (res, ==, TRUE);

  res = g_file_make_directory_with_parents (grandchild, NULL, &error);
  g_assert_cmpint (res, ==, FALSE);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
  g_clear_error (&error);

  res = g_file_make_directory_with_parents (greatgrandchild, NULL, &error);
  g_assert_cmpint (res, ==, FALSE);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
  g_clear_error (&error);

out:
  g_object_unref (greatgrandchild);
  g_object_unref (grandchild);
  g_object_unref (child);
  g_object_unref (root);
}


static void
cleanup_dir_recurse (GFile *parent, GFile *root)
{
  gboolean res;
  GError *error;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GFile *descend;
  char *relative_path;

  g_assert (root != NULL);

  enumerator =
    g_file_enumerate_children (parent, "*",
			       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
			       NULL);
  if (! enumerator)
	  return;

  error = NULL;
  info = g_file_enumerator_next_file (enumerator, NULL, &error);
  while ((info) && (!error))
    {
      descend = g_file_enumerator_get_child (enumerator, info);
      g_assert (descend != NULL);
      relative_path = g_file_get_relative_path (root, descend);
      g_assert (relative_path != NULL);
      g_free (relative_path);

      log ("    deleting '%s'\n", g_file_info_get_display_name (info));

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    	  cleanup_dir_recurse (descend, root);
      
      error = NULL;
      res = g_file_delete (descend, NULL, &error);
      g_assert_cmpint (res, ==, TRUE);

      g_object_unref (descend);
      error = NULL;
      g_object_unref (info);

      info = g_file_enumerator_next_file (enumerator, NULL, &error);
    }
  g_assert_no_error (error);

  error = NULL;
  res = g_file_enumerator_close (enumerator, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert_no_error (error);

  g_object_unref (enumerator);
}

static void
prep_clean_structure (gconstpointer test_data)
{
  GFile *root;
  
  g_assert (test_data != NULL);
  log ("\n  Cleaning target testing structure in '%s'...\n",
       (char *) test_data);

  root = g_file_new_for_commandline_arg ((char *) test_data);
  g_assert (root != NULL);
  
  cleanup_dir_recurse (root, root);

  g_file_delete (root, NULL, NULL);
  
  g_object_unref (root);
}

int
main (int argc, char *argv[])
{
  static gboolean only_create_struct;
  const char *target_path;
  GError *error;
  GOptionContext *context;

  static GOptionEntry cmd_entries[] = {
    {"read-write", 'w', 0, G_OPTION_ARG_NONE, &write_test,
     "Perform write tests (incl. structure creation)", NULL},
    {"create-struct", 'c', 0, G_OPTION_ARG_NONE, &only_create_struct,
     "Only create testing structure (no tests)", NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
    {"posix", 'x', 0, G_OPTION_ARG_NONE, &posix_compat,
     "Test POSIX-specific features (unix permissions, symlinks)", NULL},
    {NULL}
  };

  test_suite = FALSE;
  verbose = FALSE;
  write_test = FALSE;
  only_create_struct = FALSE;
  target_path = NULL;
  posix_compat = FALSE;

  /*  strip all gtester-specific args  */
  g_test_init (&argc, &argv, NULL);

  /*  no extra parameters specified, assume we're executed from glib test suite  */ 
  if (argc < 2)
    {
	  test_suite = TRUE;
	  verbose = TRUE;
	  write_test = TRUE;
	  only_create_struct = FALSE;
	  target_path = DEFAULT_TEST_DIR;
#ifdef G_PLATFORM_WIN32
	  posix_compat = FALSE;
#else
	  posix_compat = TRUE;
#endif
    }
  
  /*  add trailing args  */
  error = NULL;
  context = g_option_context_new ("target_path");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("option parsing failed: %s\n", error->message);
      return g_test_run ();
    }

  /*  remaining arg should is the target path; we don't care of the extra args here  */ 
  if (argc >= 2)
    target_path = strdup (argv[1]);
  
  if (! target_path) 
    {
      g_printerr ("error: target path was not specified\n");
      g_printerr ("%s", g_option_context_get_help (context, TRUE, NULL));
      return g_test_run ();
    }

  g_option_context_free (context);
  
  /*  Write test - clean target directory first  */
  /*    this can be also considered as a test - enumerate + delete  */ 
  if (write_test || only_create_struct)
    g_test_add_data_func ("/live-g-file/prep_clean_structure", target_path,
    	  	  prep_clean_structure);
  
  /*  Write test - create new testing structure  */
  if (write_test || only_create_struct)
    g_test_add_data_func ("/live-g-file/create_structure", target_path,
			  test_create_structure);

  /*  Read test - test the sample structure - expect defined attributes to be there  */
  if (!only_create_struct)
    g_test_add_data_func ("/live-g-file/test_initial_structure", target_path,
			  test_initial_structure);

  /*  Read test - test traverse the structure - no special file should appear  */
  if (!only_create_struct)
    g_test_add_data_func ("/live-g-file/test_traverse_structure", target_path,
			  test_traverse_structure);

  /*  Read test - enumerate  */
  if (!only_create_struct)
    g_test_add_data_func ("/live-g-file/test_enumerate", target_path,
			  test_enumerate);

  /*  Read test - open (g_file_read())  */
  if (!only_create_struct)
    g_test_add_data_func ("/live-g-file/test_open", target_path, test_open);

  /*  Write test - create  */
  if (write_test && (!only_create_struct))
    g_test_add_data_func ("/live-g-file/test_create", target_path,
			  test_create);

  /*  Write test - copy, move  */
  if (write_test && (!only_create_struct))
    g_test_add_data_func ("/live-g-file/test_copy_move", target_path,
			  test_copy_move);

  /*  Write test - delete, trash  */
  if (write_test && (!only_create_struct))
    g_test_add_data_func ("/live-g-file/test_delete", target_path,
			  test_delete);

  /*  Write test - make_directory_with_parents */
  if (write_test && (!only_create_struct))
    g_test_add_data_func ("/live-g-file/test_make_directory_with_parents", target_path,
			  test_make_directory_with_parents);

  if (write_test || only_create_struct)
    g_test_add_data_func ("/live-g-file/final_clean", target_path,
    	  	  prep_clean_structure);

  return g_test_run ();

}
