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

#include "config.h"

#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#ifdef G_OS_WIN32
#include <stdio.h>
#include <glib/gstdio.h>
#include <Windows.h>
#include <Shlobj.h>
#endif

#define TEST_NAME			"Prilis zlutoucky kun"
#define TEST_DISPLAY_NAME	        "UTF-8 p\xc5\x99\xc3\xadli\xc5\xa1 \xc5\xbelu\xc5\xa5ou\xc4\x8dk\xc3\xbd k\xc5\xaf\xc5\x88"
#define TEST_SIZE			0xFFFFFFF0

static void
test_assigned_values (GFileInfo *info)
{
  const char *name, *display_name, *mistake;
  guint64 size;
  GFileType type;
  
  /*  Test for attributes presence */
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_NAME) == TRUE);
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME) == TRUE);
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE) == TRUE);
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_COPY_NAME) == FALSE);
	
  /*  Retrieve data back and compare */
  
  name = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_STANDARD_NAME);
  display_name = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
  mistake = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_COPY_NAME);
  size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
  type = g_file_info_get_file_type (info);
  
  g_assert_cmpstr (name, ==, TEST_NAME);
  g_assert_cmpstr (display_name, ==, TEST_DISPLAY_NAME);
  g_assert (mistake == NULL);
  g_assert_cmpint (size, ==, TEST_SIZE);
  g_assert_cmpstr (name, ==, g_file_info_get_name (info));
  g_assert_cmpstr (display_name, ==, g_file_info_get_display_name (info));
  g_assert_cmpint (size, ==, g_file_info_get_size (info)	);
  g_assert_cmpint (type, ==, G_FILE_TYPE_DIRECTORY);
}



static void
test_g_file_info (void)
{
  GFileInfo *info;
  GFileInfo *info_dup;
  GFileInfo *info_copy;
  char **attr_list;
  GFileAttributeMatcher *matcher;
  
  info = g_file_info_new ();
  
  /*  Test for empty instance */
  attr_list = g_file_info_list_attributes (info, NULL);
  g_assert (attr_list != NULL);
  g_assert (*attr_list == NULL);
  g_strfreev (attr_list);

  g_file_info_set_attribute_byte_string (info, G_FILE_ATTRIBUTE_STANDARD_NAME, TEST_NAME);
  g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, TEST_DISPLAY_NAME);
  g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE, TEST_SIZE);
  g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
	
  /*  The attr list should not be empty now */
  attr_list = g_file_info_list_attributes (info, NULL);
  g_assert (attr_list != NULL);
  g_assert (*attr_list != NULL);
  g_strfreev (attr_list);

  test_assigned_values (info);
	
  /*  Test dups */
  info_dup = g_file_info_dup (info);
  g_assert (info_dup != NULL);
  test_assigned_values (info_dup);
  
  info_copy = g_file_info_new ();
  g_file_info_copy_into (info_dup, info_copy);
  g_assert (info_copy != NULL);
  test_assigned_values (info_copy);

  /*  Test remove attribute */
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER) == FALSE);
  g_file_info_set_attribute_int32 (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER, 10);
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER) == TRUE);

  g_assert (g_file_info_get_attribute_type (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER) == G_FILE_ATTRIBUTE_TYPE_INT32);
  g_assert (g_file_info_get_attribute_status (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER) != G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING);

  g_file_info_remove_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER);
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER) == FALSE);
  g_assert (g_file_info_get_attribute_type (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER) == G_FILE_ATTRIBUTE_TYPE_INVALID);

  matcher = g_file_attribute_matcher_new (G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

  g_assert (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_NAME) == TRUE);
  g_assert (g_file_attribute_matcher_matches_only (matcher, G_FILE_ATTRIBUTE_STANDARD_NAME) == FALSE);
  g_assert (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_STANDARD_SIZE) == FALSE);

  g_file_info_set_attribute_mask (info, matcher);
  g_file_attribute_matcher_unref (matcher);

  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE) == FALSE);
  g_assert (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_NAME) == TRUE);

  g_object_unref (info);
  g_object_unref (info_dup);
  g_object_unref (info_copy);
}

#ifdef G_OS_WIN32
static void
test_internal_enhanced_stdio (void)
{
  char *p0, *p1, *ps;
  gboolean try_sparse;
  gchar *tmp_dir_root;
  wchar_t *tmp_dir_root_w;
  gchar *c;
  DWORD fsflags;
  FILE *f;
  SYSTEMTIME st;
  FILETIME ft;
  HANDLE h;
  GStatBuf statbuf_p0, statbuf_p1, statbuf_ps;
  GFile *gf_p0, *gf_p1, *gf_ps;
  GFileInfo *fi_p0, *fi_p1, *fi_ps;
  guint64 size_p0, alsize_p0, size_ps, alsize_ps;
  const gchar *id_p0;
  const gchar *id_p1;
  volatile guint64 time_p0;
  gchar *tmp_dir;
  wchar_t *programdata_dir_w;
  wchar_t *users_dir_w;
  static const GUID folder_id_programdata = 
    { 0x62AB5D82, 0xFDC1, 0x4DC3, { 0xA9, 0xDD, 0x07, 0x0D, 0x1D, 0x49, 0x5D, 0x97 } };
  static const GUID folder_id_users = 
    { 0x0762D272, 0xC50A, 0x4BB0, { 0xA3, 0x82, 0x69, 0x7D, 0xCD, 0x72, 0x9B, 0x80 } };

  programdata_dir_w = NULL;
  SHGetKnownFolderPath (&folder_id_programdata, 0, NULL, &programdata_dir_w);

  users_dir_w = NULL;
  SHGetKnownFolderPath (&folder_id_users, 0, NULL, &users_dir_w);

  if (programdata_dir_w != NULL && users_dir_w != NULL)
    {
      gchar *programdata;
      gchar *users_dir;
      gchar *allusers;
      GFile *gf_programdata, *gf_allusers;
      GFileInfo *fi_programdata, *fi_allusers, *fi_allusers_target;
      GFileType ft_allusers;
      GFileType ft_allusers_target;
      GFileType ft_programdata;
      gboolean allusers_is_symlink;
      const gchar *id_allusers;
      const gchar *id_allusers_target;
      const gchar *id_programdata;
      const gchar *allusers_target;

      /* C:/ProgramData */
      programdata = g_utf16_to_utf8 (programdata_dir_w, -1, NULL, NULL, NULL);
      g_assert_nonnull (programdata);
      /* C:/Users */
      users_dir = g_utf16_to_utf8 (users_dir_w, -1, NULL, NULL, NULL);
      g_assert_nonnull (users_dir);
      /* "C:/Users/All Users" is a known directory symlink
       * for "C:/ProgramData".
       */
      allusers = g_build_filename (users_dir, "All Users", NULL);
      g_assert_nonnull (allusers);

      /* We don't test g_stat() and g_lstat() on these directories,
       * because it is pointless - there's no way to tell that these
       * functions behave correctly in this case
       * (st_ino is useless, so we can't tell apart g_stat() and g_lstat()
       *  results; st_mode is also useless as it does not support S_ISLNK;
       *  and these directories have no interesting properties other
       *  than [not] being symlinks).
       */
      gf_programdata = g_file_new_for_path (programdata);
      gf_allusers = g_file_new_for_path (allusers);

      fi_programdata = g_file_query_info (gf_programdata,
                                          G_FILE_ATTRIBUTE_ID_FILE ","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL, NULL);

      fi_allusers_target = g_file_query_info (gf_allusers,
                                              G_FILE_ATTRIBUTE_ID_FILE ","
                                              G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                              G_FILE_QUERY_INFO_NONE,
                                              NULL, NULL);

      fi_allusers = g_file_query_info (gf_allusers,
                                       G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET ","
                                       G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                                       G_FILE_ATTRIBUTE_ID_FILE ","
                                       G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);

      g_assert (g_file_info_has_attribute (fi_programdata, G_FILE_ATTRIBUTE_ID_FILE));
      g_assert (g_file_info_has_attribute (fi_programdata, G_FILE_ATTRIBUTE_STANDARD_TYPE));

      g_assert (g_file_info_has_attribute (fi_allusers_target, G_FILE_ATTRIBUTE_ID_FILE));
      g_assert (g_file_info_has_attribute (fi_allusers_target, G_FILE_ATTRIBUTE_STANDARD_TYPE));

      g_assert (g_file_info_has_attribute (fi_allusers, G_FILE_ATTRIBUTE_ID_FILE));
      g_assert (g_file_info_has_attribute (fi_allusers, G_FILE_ATTRIBUTE_STANDARD_TYPE));
      g_assert (g_file_info_has_attribute (fi_allusers, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK));
      g_assert (g_file_info_has_attribute (fi_allusers, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET));

      ft_allusers = g_file_info_get_file_type (fi_allusers);
      ft_allusers_target = g_file_info_get_file_type (fi_allusers_target);
      ft_programdata = g_file_info_get_file_type (fi_programdata);

      g_assert (ft_allusers == G_FILE_TYPE_SYMBOLIC_LINK);
      g_assert (ft_allusers_target == G_FILE_TYPE_DIRECTORY);
      g_assert (ft_programdata == G_FILE_TYPE_DIRECTORY);

      allusers_is_symlink = g_file_info_get_attribute_boolean (fi_allusers, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);

      g_assert_true (allusers_is_symlink);

      id_allusers = g_file_info_get_attribute_string (fi_allusers, G_FILE_ATTRIBUTE_ID_FILE);
      id_allusers_target = g_file_info_get_attribute_string (fi_allusers_target, G_FILE_ATTRIBUTE_ID_FILE);
      id_programdata = g_file_info_get_attribute_string (fi_programdata, G_FILE_ATTRIBUTE_ID_FILE);

      g_assert_cmpstr (id_allusers_target, ==, id_programdata);
      g_assert_cmpstr (id_allusers, !=, id_programdata);

      allusers_target = g_file_info_get_symlink_target (fi_allusers);

      g_assert_true (g_str_has_suffix (allusers_target, "ProgramData"));

      g_object_unref (fi_allusers);
      g_object_unref (fi_allusers_target);
      g_object_unref (fi_programdata);
      g_object_unref (gf_allusers);
      g_object_unref (gf_programdata);

      g_free (allusers);
      g_free (users_dir);
      g_free (programdata);
    }

  if (programdata_dir_w)
    CoTaskMemFree (programdata_dir_w);

  if (users_dir_w)
    CoTaskMemFree (users_dir_w);

  tmp_dir = g_dir_make_tmp ("glib_stdio_testXXXXXX", NULL);
  g_assert_nonnull (tmp_dir);

  /* Technically, this isn't necessary - we already assume NTFS, because of
   * symlink support, and NTFS also supports sparse files. Still, given
   * the amount of unusual I/O APIs called in this test, checking for
   * sparse file support of the filesystem where temp directory is
   * doesn't seem to be out of place.
   */
  try_sparse = FALSE;
  tmp_dir_root = g_strdup (tmp_dir);
  /* We need "C:\\" or "C:/", with a trailing [back]slash */
  for (c = tmp_dir_root; c && c[0] && c[1]; c++)
    if (c[0] == ':')
      {
        c[2] = '\0';
        break;
      }
  tmp_dir_root_w = g_utf8_to_utf16 (tmp_dir_root, -1, NULL, NULL, NULL);
  g_assert_nonnull (tmp_dir_root_w);
  g_free (tmp_dir_root);
  g_assert_true (GetVolumeInformationW (tmp_dir_root_w, NULL, 0, NULL, NULL, &fsflags, NULL, 0));
  g_free (tmp_dir_root_w);
  try_sparse = (fsflags & FILE_SUPPORTS_SPARSE_FILES) == FILE_SUPPORTS_SPARSE_FILES;

  p0 = g_build_filename (tmp_dir, "zool", NULL);
  p1 = g_build_filename (tmp_dir, "looz", NULL);
  ps = g_build_filename (tmp_dir, "sparse", NULL);

  if (try_sparse)
    {
      FILE_SET_SPARSE_BUFFER ssb;
      FILE_ZERO_DATA_INFORMATION zdi;

      g_remove (ps);

      f = g_fopen (ps, "wb");
      g_assert_nonnull (f);

      h = (HANDLE) _get_osfhandle (fileno (f));
      g_assert (h != INVALID_HANDLE_VALUE);

      ssb.SetSparse = TRUE;
      g_assert_true (DeviceIoControl (h,
                     FSCTL_SET_SPARSE,
                     &ssb, sizeof (ssb),
                     NULL, 0, NULL, NULL));

      /* Make it a sparse file that starts with 4GBs of zeros */
      zdi.FileOffset.QuadPart = 0;
      zdi.BeyondFinalZero.QuadPart = 0xFFFFFFFFULL + 1;
      g_assert_true (DeviceIoControl (h,
                     FSCTL_SET_ZERO_DATA,
                     &zdi, sizeof (zdi),
                     NULL, 0, NULL, NULL));

      /* Let's not keep this seemingly 4GB monster around
       * longer than we absolutely need to. Do all operations
       * without assertions, then remove the file immediately.
       */
      _fseeki64 (f, 0xFFFFFFFFULL, SEEK_SET);
      fprintf (f, "Hello 4GB World!");
      fflush (f);
      fclose (f);

      memset (&statbuf_ps, 0, sizeof (statbuf_ps));

      g_stat (ps, &statbuf_ps);

      gf_ps = g_file_new_for_path (ps);

      fi_ps = g_file_query_info (gf_ps,
                                 G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                 G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL, NULL);

      g_remove (ps);

      g_assert (g_file_info_has_attribute (fi_ps, G_FILE_ATTRIBUTE_STANDARD_SIZE));
      g_assert (g_file_info_has_attribute (fi_ps, G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE));

      size_ps = g_file_info_get_attribute_uint64 (fi_ps, G_FILE_ATTRIBUTE_STANDARD_SIZE);
      alsize_ps = g_file_info_get_attribute_uint64 (fi_ps, G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE);

      /* allocated size should small (usually - size of the FS cluster,
       * let's assume it's less than 1 gigabyte),
       * size should be more than 4 gigabytes,
       * st_size should not exceed its 0xFFFFFFFF 32-bit limit,
       * and should be nonzero (this also detects a failed g_stat() earlier).
       */
      g_assert_cmpuint (alsize_ps, <, 0x40000000);
      g_assert_cmpuint (size_ps, >, G_GUINT64_CONSTANT (0xFFFFFFFF));
      g_assert_cmpuint (statbuf_ps.st_size, >, 0);
      g_assert_cmpuint (statbuf_ps.st_size, <=, 0xFFFFFFFF);

      g_object_unref (fi_ps);
      g_object_unref (gf_ps);
    }

  /* Wa-a-ay past 02/07/2106 @ 6:28am (UTC),
   * which is the date corresponding to 0xFFFFFFFF + 1.
   * This is easier to check than Y2038 (0x80000000 + 1),
   * since there's no need to worry about signedness this way.
   */
  st.wYear = 2106;
  st.wMonth = 2;
  st.wDay = 9;
  st.wHour = 0;
  st.wMinute = 0;
  st.wSecond = 0;
  st.wMilliseconds = 0;

  g_assert_true (SystemTimeToFileTime (&st, &ft));

  f = g_fopen (p0, "w");
  g_assert_nonnull (f);

  h = (HANDLE) _get_osfhandle (fileno (f));
  g_assert (h != INVALID_HANDLE_VALUE);

  fprintf (f, "1");
  fflush (f);

  g_assert_true (SetFileTime (h, &ft, &ft, &ft));

  fclose (f);

  f = g_fopen (p1, "w");
  g_assert_nonnull (f);

  fclose (f);

  memset (&statbuf_p0, 0, sizeof (statbuf_p0));
  memset (&statbuf_p1, 0, sizeof (statbuf_p1));

  g_assert_cmpint (g_stat (p0, &statbuf_p0), ==, 0);
  g_assert_cmpint (g_stat (p1, &statbuf_p1), ==, 0);

  gf_p0 = g_file_new_for_path (p0);
  gf_p1 = g_file_new_for_path (p1);

  fi_p0 = g_file_query_info (gf_p0,
                             G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                             G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE ","
                             G_FILE_ATTRIBUTE_ID_FILE ","
                             G_FILE_ATTRIBUTE_TIME_MODIFIED,
                             G_FILE_QUERY_INFO_NONE,
                             NULL, NULL);

  fi_p1 = g_file_query_info (gf_p1,
                             G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                             G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE ","
                             G_FILE_ATTRIBUTE_ID_FILE ","
                             G_FILE_ATTRIBUTE_TIME_MODIFIED,
                             G_FILE_QUERY_INFO_NONE,
                             NULL, NULL);

  g_assert (g_file_info_has_attribute (fi_p0, G_FILE_ATTRIBUTE_STANDARD_SIZE));
  g_assert (g_file_info_has_attribute (fi_p0, G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE));
  g_assert (g_file_info_has_attribute (fi_p0, G_FILE_ATTRIBUTE_ID_FILE));
  g_assert (g_file_info_has_attribute (fi_p0, G_FILE_ATTRIBUTE_TIME_MODIFIED));

  g_assert (g_file_info_has_attribute (fi_p1, G_FILE_ATTRIBUTE_STANDARD_SIZE));
  g_assert (g_file_info_has_attribute (fi_p1, G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE));
  g_assert (g_file_info_has_attribute (fi_p1, G_FILE_ATTRIBUTE_ID_FILE));
  g_assert (g_file_info_has_attribute (fi_p1, G_FILE_ATTRIBUTE_TIME_MODIFIED));

  size_p0 = g_file_info_get_attribute_uint64 (fi_p0, G_FILE_ATTRIBUTE_STANDARD_SIZE);
  alsize_p0 = g_file_info_get_attribute_uint64 (fi_p0, G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE);

  /* size should be 1, allocated size should be something else
   * (could be 0 or the size of the FS cluster, but never 1).
   */
  g_assert_cmpuint (size_p0, ==, statbuf_p0.st_size);
  g_assert_cmpuint (size_p0, ==, 1);
  g_assert_cmpuint (alsize_p0, !=, size_p0);

  id_p0 = g_file_info_get_attribute_string (fi_p0, G_FILE_ATTRIBUTE_ID_FILE);
  id_p1 = g_file_info_get_attribute_string (fi_p1, G_FILE_ATTRIBUTE_ID_FILE);

  /* st_ino from W32 stat() is useless for file identification.
   * It will be either 0, or it will be the same for both files.
   */
  g_assert (statbuf_p0.st_ino == statbuf_p1.st_ino);
  g_assert_cmpstr (id_p0, !=, id_p1);

  time_p0 = g_file_info_get_attribute_uint64 (fi_p0, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  /* Check that GFileInfo doesn't suffer from Y2106 problem.
   * Don't check stat(), as its contents may vary depending on
   * the host platform architecture
   * (time fields are 32-bit on 32-bit Windows,
   *  and 64-bit on 64-bit Windows, usually),
   * so it *can* pass this test in some cases.
   */
  g_assert (time_p0 > G_GUINT64_CONSTANT (0xFFFFFFFF));

  g_object_unref (fi_p0);
  g_object_unref (fi_p1);
  g_object_unref (gf_p0);
  g_object_unref (gf_p1);
  g_remove (p0);
  g_remove (p1);
  g_free (p0);
  g_free (p1);
  g_rmdir (tmp_dir);
}
#endif


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/g-file-info/test_g_file_info", test_g_file_info);
#ifdef G_OS_WIN32
  g_test_add_func ("/g-file-info/internal-enhanced-stdio", test_internal_enhanced_stdio);
#endif
  
  return g_test_run();
}
