/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#if G_OS_UNIX
#include <dirent.h>
#include <unistd.h>
#endif

#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#elif HAVE_SYS_MOUNT_H
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/mount.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "gfileattribute.h"
#include "glocalfile.h"
#include "glocalfileprivate.h"
#include "glocalfileinfo.h"
#include "glocalfileenumerator.h"
#include "glocalfileinputstream.h"
#include "glocalfileoutputstream.h"
#include "glocalfileiostream.h"
#include "glocalfilemonitor.h"
#include "gmountprivate.h"
#include "gunixmounts.h"
#include "gioerror.h"
#include <glib/gstdio.h>
#include <glib/gstdioprivate.h>
#include "glibintl.h"
#ifdef G_OS_UNIX
#include "glib-unix.h"
#endif

#include "glib-private.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>

#ifndef FILE_READ_ONLY_VOLUME
#define FILE_READ_ONLY_VOLUME           0x00080000
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) (0)
#endif

#ifndef ECANCELED
#define ECANCELED 105
#endif
#endif


static void g_local_file_file_iface_init (GFileIface *iface);

static GFileAttributeInfoList *local_writable_attributes = NULL;
static /* GFileAttributeInfoList * */ gsize local_writable_namespaces = 0;

struct _GLocalFile
{
  GObject parent_instance;

  char *filename;
};

#define g_local_file_get_type _g_local_file_get_type
G_DEFINE_TYPE_WITH_CODE (GLocalFile, g_local_file, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE,
						g_local_file_file_iface_init))

static char *find_mountpoint_for (const char *file, dev_t dev);

static void
g_local_file_finalize (GObject *object)
{
  GLocalFile *local;

  local = G_LOCAL_FILE (object);

  g_free (local->filename);

  G_OBJECT_CLASS (g_local_file_parent_class)->finalize (object);
}

static void
g_local_file_class_init (GLocalFileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GFileAttributeInfoList *list;

  gobject_class->finalize = g_local_file_finalize;

  /* Set up attribute lists */

  /* Writable attributes: */

  list = g_file_attribute_info_list_new ();

  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_UNIX_MODE,
				  G_FILE_ATTRIBUTE_TYPE_UINT32,
				  G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE |
				  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
  
#ifdef G_OS_UNIX
  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_UNIX_UID,
				  G_FILE_ATTRIBUTE_TYPE_UINT32,
				  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_UNIX_GID,
				  G_FILE_ATTRIBUTE_TYPE_UINT32,
				  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
#endif
  
#ifdef HAVE_SYMLINK
  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_ATTRIBUTE_TYPE_BYTE_STRING,
				  0);
#endif
  
#ifdef HAVE_UTIMES
  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  G_FILE_ATTRIBUTE_TYPE_UINT64,
				  G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE |
				  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
				  G_FILE_ATTRIBUTE_TYPE_UINT32,
				  G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE |
				  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
  /* When copying, the target file is accessed. Replicating
   * the source access time does not make sense in this case.
   */
  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_TIME_ACCESS,
				  G_FILE_ATTRIBUTE_TYPE_UINT64,
				  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
  g_file_attribute_info_list_add (list,
				  G_FILE_ATTRIBUTE_TIME_ACCESS_USEC,
				  G_FILE_ATTRIBUTE_TYPE_UINT32,
				  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
#endif

  local_writable_attributes = list;
}

static void
g_local_file_init (GLocalFile *local)
{
}

const char *
_g_local_file_get_filename (GLocalFile *file)
{
  return file->filename;
}

static char *
canonicalize_filename (const char *filename)
{
  char *canon, *start, *p, *q;
  char *cwd;
  int i;
  
  if (!g_path_is_absolute (filename))
    {
      cwd = g_get_current_dir ();
      canon = g_build_filename (cwd, filename, NULL);
      g_free (cwd);
    }
  else
    canon = g_strdup (filename);

  start = (char *)g_path_skip_root (canon);

  if (start == NULL)
    {
      /* This shouldn't really happen, as g_get_current_dir() should
	 return an absolute pathname, but bug 573843 shows this is
	 not always happening */
      g_free (canon);
      return g_build_filename (G_DIR_SEPARATOR_S, filename, NULL);
    }
  
  /* POSIX allows double slashes at the start to
   * mean something special (as does windows too).
   * So, "//" != "/", but more than two slashes
   * is treated as "/".
   */
  i = 0;
  for (p = start - 1;
       (p >= canon) &&
	 G_IS_DIR_SEPARATOR (*p);
       p--)
    i++;
  if (i > 2)
    {
      i -= 1;
      start -= i;
      memmove (start, start+i, strlen (start+i)+1);
    }

  /* Make sure we're using the canonical dir separator */
  p++;
  while (p < start && G_IS_DIR_SEPARATOR (*p))
    *p++ = G_DIR_SEPARATOR;
  
  p = start;
  while (*p != 0)
    {
      if (p[0] == '.' && (p[1] == 0 || G_IS_DIR_SEPARATOR (p[1])))
	{
	  memmove (p, p+1, strlen (p+1)+1);
	}
      else if (p[0] == '.' && p[1] == '.' && (p[2] == 0 || G_IS_DIR_SEPARATOR (p[2])))
	{
	  q = p + 2;
	  /* Skip previous separator */
	  p = p - 2;
	  if (p < start)
	    p = start;
	  while (p > start && !G_IS_DIR_SEPARATOR (*p))
	    p--;
	  if (G_IS_DIR_SEPARATOR (*p))
	    *p++ = G_DIR_SEPARATOR;
	  memmove (p, q, strlen (q)+1);
	}
      else
	{
	  /* Skip until next separator */
	  while (*p != 0 && !G_IS_DIR_SEPARATOR (*p))
	    p++;
	  
	  if (*p != 0)
	    {
	      /* Canonicalize one separator */
	      *p++ = G_DIR_SEPARATOR;
	    }
	}

      /* Remove additional separators */
      q = p;
      while (*q && G_IS_DIR_SEPARATOR (*q))
	q++;

      if (p != q)
	memmove (p, q, strlen (q)+1);
    }

  /* Remove trailing slashes */
  if (p > start && G_IS_DIR_SEPARATOR (*(p-1)))
    *(p-1) = 0;
  
  return canon;
}

GFile *
_g_local_file_new (const char *filename)
{
  GLocalFile *local;

  local = g_object_new (G_TYPE_LOCAL_FILE, NULL);
  local->filename = canonicalize_filename (filename);
  
  return G_FILE (local);
}

/*< internal >
 * g_local_file_new_from_dirname_and_basename:
 * @dirname: an absolute, canonical directory name
 * @basename: the name of a child inside @dirname
 *
 * Creates a #GFile from @dirname and @basename.
 *
 * This is more efficient than pasting the fields together for yourself
 * and creating a #GFile from the result, and also more efficient than
 * creating a #GFile for the dirname and using g_file_get_child().
 *
 * @dirname must be canonical, as per GLocalFile's opinion of what
 * canonical means.  This means that you should only pass strings that
 * were returned by _g_local_file_get_filename().
 *
 * Returns: a #GFile
 */
GFile *
g_local_file_new_from_dirname_and_basename (const gchar *dirname,
                                            const gchar *basename)
{
  GLocalFile *local;

  g_return_val_if_fail (dirname != NULL, NULL);
  g_return_val_if_fail (basename && basename[0] && !strchr (basename, '/'), NULL);

  local = g_object_new (G_TYPE_LOCAL_FILE, NULL);
  local->filename = g_build_filename (dirname, basename, NULL);

  return G_FILE (local);
}

static gboolean
g_local_file_is_native (GFile *file)
{
  return TRUE;
}

static gboolean
g_local_file_has_uri_scheme (GFile      *file,
			     const char *uri_scheme)
{
  return g_ascii_strcasecmp (uri_scheme, "file") == 0;
}

static char *
g_local_file_get_uri_scheme (GFile *file)
{
  return g_strdup ("file");
}

static char *
g_local_file_get_basename (GFile *file)
{
  return g_path_get_basename (G_LOCAL_FILE (file)->filename);
}

static char *
g_local_file_get_path (GFile *file)
{
  return g_strdup (G_LOCAL_FILE (file)->filename);
}

static char *
g_local_file_get_uri (GFile *file)
{
  return g_filename_to_uri (G_LOCAL_FILE (file)->filename, NULL, NULL);
}

static gboolean
get_filename_charset (const gchar **filename_charset)
{
  const gchar **charsets;
  gboolean is_utf8;
  
  is_utf8 = g_get_filename_charsets (&charsets);

  if (filename_charset)
    *filename_charset = charsets[0];
  
  return is_utf8;
}

static gboolean
name_is_valid_for_display (const char *string,
			   gboolean    is_valid_utf8)
{
  char c;

  if (!is_valid_utf8 &&
      !g_utf8_validate (string, -1, NULL))
    return FALSE;

  while ((c = *string++) != 0)
    {
      if (g_ascii_iscntrl (c))
	return FALSE;
    }

  return TRUE;
}

static char *
g_local_file_get_parse_name (GFile *file)
{
  const char *filename;
  char *parse_name;
  const gchar *charset;
  char *utf8_filename;
  char *roundtripped_filename;
  gboolean free_utf8_filename;
  gboolean is_valid_utf8;
  char *escaped_path;
  
  filename = G_LOCAL_FILE (file)->filename;
  if (get_filename_charset (&charset))
    {
      utf8_filename = (char *)filename;
      free_utf8_filename = FALSE;
      is_valid_utf8 = FALSE; /* Can't guarantee this */
    }
  else
    {
      utf8_filename = g_convert (filename, -1, 
				 "UTF-8", charset, NULL, NULL, NULL);
      free_utf8_filename = TRUE;
      is_valid_utf8 = TRUE;

      if (utf8_filename != NULL)
	{
	  /* Make sure we can roundtrip: */
	  roundtripped_filename = g_convert (utf8_filename, -1,
					     charset, "UTF-8", NULL, NULL, NULL);
	  
	  if (roundtripped_filename == NULL ||
	      strcmp (filename, roundtripped_filename) != 0)
	    {
	      g_free (utf8_filename);
	      utf8_filename = NULL;
	    }

	  g_free (roundtripped_filename);
	}
    }

  if (utf8_filename != NULL &&
      name_is_valid_for_display (utf8_filename, is_valid_utf8))
    {
      if (free_utf8_filename)
	parse_name = utf8_filename;
      else
	parse_name = g_strdup (utf8_filename);
    }
  else
    {
#ifdef G_OS_WIN32
      char *dup_filename, *p, *backslash;

      /* Turn backslashes into forward slashes like
       * g_filename_to_uri() would do (but we can't use that because
       * it doesn't output IRIs).
       */
      dup_filename = g_strdup (filename);
      filename = p = dup_filename;

      while ((backslash = strchr (p, '\\')) != NULL)
	{
	  *backslash = '/';
	  p = backslash + 1;
	}
#endif

      escaped_path = g_uri_escape_string (filename,
					  G_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT "/",
					  TRUE);
      parse_name = g_strconcat ("file://",
				(*escaped_path != '/') ? "/" : "",
				escaped_path,
				NULL);
      
      g_free (escaped_path);
#ifdef G_OS_WIN32
      g_free (dup_filename);
#endif
      if (free_utf8_filename)
	g_free (utf8_filename);
    }
  
  return parse_name;
}

static GFile *
g_local_file_get_parent (GFile *file)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  const char *non_root;
  char *dirname;
  GFile *parent;

  /* Check for root; local->filename is guaranteed to be absolute, so
   * g_path_skip_root() should never return NULL. */
  non_root = g_path_skip_root (local->filename);
  g_assert (non_root != NULL);

  if (*non_root == 0)
    return NULL;

  dirname = g_path_get_dirname (local->filename);
  parent = _g_local_file_new (dirname);
  g_free (dirname);
  return parent;
}

static GFile *
g_local_file_dup (GFile *file)
{
  GLocalFile *local = G_LOCAL_FILE (file);

  return _g_local_file_new (local->filename);
}

static guint
g_local_file_hash (GFile *file)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  
  return g_str_hash (local->filename);
}

static gboolean
g_local_file_equal (GFile *file1,
		    GFile *file2)
{
  GLocalFile *local1 = G_LOCAL_FILE (file1);
  GLocalFile *local2 = G_LOCAL_FILE (file2);

  return g_str_equal (local1->filename, local2->filename);
}

static const char *
match_prefix (const char *path, 
              const char *prefix)
{
  int prefix_len;

  prefix_len = strlen (prefix);
  if (strncmp (path, prefix, prefix_len) != 0)
    return NULL;
  
  /* Handle the case where prefix is the root, so that
   * the IS_DIR_SEPRARATOR check below works */
  if (prefix_len > 0 &&
      G_IS_DIR_SEPARATOR (prefix[prefix_len-1]))
    prefix_len--;
  
  return path + prefix_len;
}

static gboolean
g_local_file_prefix_matches (GFile *parent,
			     GFile *descendant)
{
  GLocalFile *parent_local = G_LOCAL_FILE (parent);
  GLocalFile *descendant_local = G_LOCAL_FILE (descendant);
  const char *remainder;

  remainder = match_prefix (descendant_local->filename, parent_local->filename);
  if (remainder != NULL && G_IS_DIR_SEPARATOR (*remainder))
    return TRUE;
  return FALSE;
}

static char *
g_local_file_get_relative_path (GFile *parent,
				GFile *descendant)
{
  GLocalFile *parent_local = G_LOCAL_FILE (parent);
  GLocalFile *descendant_local = G_LOCAL_FILE (descendant);
  const char *remainder;

  remainder = match_prefix (descendant_local->filename, parent_local->filename);
  
  if (remainder != NULL && G_IS_DIR_SEPARATOR (*remainder))
    return g_strdup (remainder + 1);
  return NULL;
}

static GFile *
g_local_file_resolve_relative_path (GFile      *file,
				    const char *relative_path)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  char *filename;
  GFile *child;

  if (g_path_is_absolute (relative_path))
    return _g_local_file_new (relative_path);
  
  filename = g_build_filename (local->filename, relative_path, NULL);
  child = _g_local_file_new (filename);
  g_free (filename);
  
  return child;
}

static GFileEnumerator *
g_local_file_enumerate_children (GFile                *file,
				 const char           *attributes,
				 GFileQueryInfoFlags   flags,
				 GCancellable         *cancellable,
				 GError              **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  return _g_local_file_enumerator_new (local,
				       attributes, flags,
				       cancellable, error);
}

static GFile *
g_local_file_get_child_for_display_name (GFile        *file,
					 const char   *display_name,
					 GError      **error)
{
  GFile *new_file;
  char *basename;

  basename = g_filename_from_utf8 (display_name, -1, NULL, NULL, NULL);
  if (basename == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_INVALID_FILENAME,
		   _("Invalid filename %s"), display_name);
      return NULL;
    }

  new_file = g_file_get_child (file, basename);
  g_free (basename);
  
  return new_file;
}

#if defined(USE_STATFS) && !defined(HAVE_STRUCT_STATFS_F_FSTYPENAME)
static const char *
get_fs_type (long f_type)
{
  /* filesystem ids taken from linux manpage */
  switch (f_type) 
    {
    case 0xadf5:
      return "adfs";
    case 0x5346414f:
      return "afs";
    case 0x0187:
      return "autofs";
    case 0xADFF:
      return "affs";
    case 0x62646576:
      return "bdevfs";
    case 0x42465331:
      return "befs";
    case 0x1BADFACE:
      return "bfs";
    case 0x42494e4d:
      return "binfmt_misc";
    case 0x9123683E:
      return "btrfs";
    case 0x73727279:
      return "btrfs_test_fs";
    case 0x27e0eb:
      return "cgroup";
    case 0x63677270:
      return "cgroup2";
    case 0xFF534D42:
      return "cifs";
    case 0x73757245:
      return "coda";
    case 0x012FF7B7:
      return "coh";
    case 0x62656570:
      return "configfs";
    case 0x28cd3d45:
      return "cramfs";
    case 0x64626720:
      return "debugfs";
    case 0x1373:
      return "devfs";
    case 0x1cd1:
      return "devpts";
    case 0xf15f:
      return "ecryptfs";
    case 0xde5e81e4:
      return "efivarfs";
    case 0x00414A53:
      return "efs";
    case 0x137D:
      return "ext";
    case 0xEF51:
      return "ext2";
    case 0xEF53:
      return "ext3/ext4";
    case 0xF2F52010:
      return "f2fs";
    case 0x65735546:
      return "fuse";
    case 0x65735543:
      return "fusectl";
    case 0xBAD1DEA:
      return "futexfs";
    case 0x4244:
      return "hfs";
    case 0x00c0ffee:
      return "hostfs";
    case 0xF995E849:
      return "hpfs";
    case 0x958458f6:
      return "hugetlbfs";
    case 0x9660:
      return "isofs";
    case 0x72b6:
      return "jffs2";
    case 0x3153464a:
      return "jfs";
    case 0x137F:
      return "minix";
    case 0x138F:
      return "minix2";
    case 0x2468:
      return "minix2";
    case 0x2478:
      return "minix22";
    case 0x4d5a:
      return "minix3";
    case 0x19800202:
      return "mqueue";
    case 0x4d44:
      return "msdos";
    case 0x564c:
      return "ncp";
    case 0x6969:
      return "nfs";
    case 0x3434:
      return "nilfs";
    case 0x6e736673:
      return "nsfs";
    case 0x5346544e:
      return "ntfs";
    case 0x7461636f:
      return "ocfs2";
    case 0x9fa1:
      return "openprom";
    case 0x794c7630:
      return "overlay";
    case 0x50495045:
      return "pipefs";
    case 0x9fa0:
      return "proc";
    case 0x6165676C:
      return "pstore";
    case 0x002f:
      return "qnx4";
    case 0x68191122:
      return "qnx6";
    case 0x858458f6:
      return "ramfs";
    case 0x52654973:
      return "reiserfs";
    case 0x7275:
      return "romfs";
    case 0x67596969:
      return "rpc_pipefs";
    case 0x73636673:
      return "securityfs";
    case 0xf97cff8c:
      return "selinuxfs";
    case 0x43415d53:
      return "smackfs";
    case 0x517B:
      return "smb";
    case 0x534F434B:
      return "sockfs";
    case 0x73717368:
      return "squashfs";
    case 0x62656572:
      return "sysfs";
    case 0x012FF7B6:
      return "sysv2";
    case 0x012FF7B5:
      return "sysv4";
    case 0x01021994:
      return "tmpfs";
    case 0x74726163:
      return "tracefs";
    case 0x15013346:
      return "udf";
    case 0x00011954:
      return "ufs";
    case 0x9fa2:
      return "usbdevice";
    case 0x01021997:
      return "v9fs";
    case 0xa501FCF5:
      return "vxfs";
    case 0xabba1974:
      return "xenfs";
    case 0x012FF7B4:
      return "xenix";
    case 0x58465342:
      return "xfs";
    case 0x012FD16D:
      return "xiafs";
    case 0x52345362:
      return "reiser4";
    default:
      return NULL;
    }
}
#endif

#ifndef G_OS_WIN32

G_LOCK_DEFINE_STATIC(mount_info_hash);
static GHashTable *mount_info_hash = NULL;
static guint64 mount_info_hash_cache_time = 0;

typedef enum {
  MOUNT_INFO_READONLY = 1<<0
} MountInfo;

static gboolean
device_equal (gconstpointer v1,
              gconstpointer v2)
{
  return *(dev_t *)v1 == *(dev_t *)v2;
}

static guint
device_hash (gconstpointer v)
{
  return (guint) *(dev_t *)v;
}

static void
get_mount_info (GFileInfo             *fs_info,
		const char            *path,
		GFileAttributeMatcher *matcher)
{
  GStatBuf buf;
  gboolean got_info;
  gpointer info_as_ptr;
  guint mount_info;
  char *mountpoint;
  dev_t *dev;
  GUnixMountEntry *mount;
  guint64 cache_time;

  if (g_lstat (path, &buf) != 0)
    return;

  G_LOCK (mount_info_hash);

  if (mount_info_hash == NULL)
    mount_info_hash = g_hash_table_new_full (device_hash, device_equal,
					     g_free, NULL);


  if (g_unix_mounts_changed_since (mount_info_hash_cache_time))
    g_hash_table_remove_all (mount_info_hash);
  
  got_info = g_hash_table_lookup_extended (mount_info_hash,
					   &buf.st_dev,
					   NULL,
					   &info_as_ptr);
  
  G_UNLOCK (mount_info_hash);
  
  mount_info = GPOINTER_TO_UINT (info_as_ptr);
  
  if (!got_info)
    {
      mount_info = 0;

      mountpoint = find_mountpoint_for (path, buf.st_dev);
      if (mountpoint == NULL)
	mountpoint = g_strdup ("/");

      mount = g_unix_mount_at (mountpoint, &cache_time);
      if (mount)
	{
	  if (g_unix_mount_is_readonly (mount))
	    mount_info |= MOUNT_INFO_READONLY;
	  
	  g_unix_mount_free (mount);
	}

      g_free (mountpoint);

      dev = g_new0 (dev_t, 1);
      *dev = buf.st_dev;
      
      G_LOCK (mount_info_hash);
      mount_info_hash_cache_time = cache_time;
      g_hash_table_insert (mount_info_hash, dev, GUINT_TO_POINTER (mount_info));
      G_UNLOCK (mount_info_hash);
    }

  if (mount_info & MOUNT_INFO_READONLY)
    g_file_info_set_attribute_boolean (fs_info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY, TRUE);
}

#endif

#ifdef G_OS_WIN32

static gboolean
is_xp_or_later (void)
{
  static int result = -1;

  if (result == -1)
    {
#ifndef _MSC_VER    
      OSVERSIONINFOEX ver_info = {0};
      DWORDLONG cond_mask = 0;
      int op = VER_GREATER_EQUAL;

      ver_info.dwOSVersionInfoSize = sizeof ver_info;
      ver_info.dwMajorVersion = 5;
      ver_info.dwMinorVersion = 1;

      VER_SET_CONDITION (cond_mask, VER_MAJORVERSION, op);
      VER_SET_CONDITION (cond_mask, VER_MINORVERSION, op);

      result = VerifyVersionInfo (&ver_info,
				  VER_MAJORVERSION | VER_MINORVERSION, 
				  cond_mask) != 0;
#else
      result = ((DWORD)(LOBYTE (LOWORD (GetVersion ())))) >= 5;  
#endif
    }

  return result;
}

static wchar_t *
get_volume_for_path (const char *path)
{
  long len;
  wchar_t *wpath;
  wchar_t *result;

  wpath = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
  result = g_new (wchar_t, MAX_PATH);

  if (!GetVolumePathNameW (wpath, result, MAX_PATH))
    {
      char *msg = g_win32_error_message (GetLastError ());
      g_critical ("GetVolumePathName failed: %s", msg);
      g_free (msg);
      g_free (result);
      g_free (wpath);
      return NULL;
    }

  len = wcslen (result);
  if (len > 0 && result[len-1] != L'\\')
    {
      result = g_renew (wchar_t, result, len + 2);
      result[len] = L'\\';
      result[len + 1] = 0;
    }

  g_free (wpath);
  return result;
}

static char *
find_mountpoint_for (const char *file, dev_t dev)
{
  wchar_t *wpath;
  char *utf8_path;

  wpath = get_volume_for_path (file);
  if (!wpath)
    return NULL;

  utf8_path = g_utf16_to_utf8 (wpath, -1, NULL, NULL, NULL);

  g_free (wpath);
  return utf8_path;
}

static void
get_filesystem_readonly (GFileInfo  *info,
			 const char *path)
{
  wchar_t *rootdir;

  rootdir = get_volume_for_path (path);

  if (rootdir)
    {
      if (is_xp_or_later ())
        {
          DWORD flags;
          if (GetVolumeInformationW (rootdir, NULL, 0, NULL, NULL, &flags, NULL, 0))
	    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY,
					       (flags & FILE_READ_ONLY_VOLUME) != 0);
        }
      else
        {
          if (GetDriveTypeW (rootdir) == DRIVE_CDROM)
	    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY, TRUE);
        }
    }

  g_free (rootdir);
}

#endif /* G_OS_WIN32 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
g_set_io_error (GError      **error,
                const gchar  *msg,
                GFile        *file,
                gint          errsv)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  gchar *display_name;

  display_name = g_filename_display_name (local->filename);
  g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
               msg, display_name, g_strerror (errsv));
  g_free (display_name);
}
#pragma GCC diagnostic pop

static GFileInfo *
g_local_file_query_filesystem_info (GFile         *file,
				    const char    *attributes,
				    GCancellable  *cancellable,
				    GError       **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  GFileInfo *info;
  int statfs_result = 0;
  gboolean no_size;
#ifndef G_OS_WIN32
  const char *fstype;
#ifdef USE_STATFS
  guint64 block_size;
  struct statfs statfs_buffer;
#elif defined(USE_STATVFS)
  guint64 block_size;
  struct statvfs statfs_buffer;
#endif /* USE_STATFS */
#endif /* G_OS_WIN32 */
  GFileAttributeMatcher *attribute_matcher;
	
  no_size = FALSE;
  
#ifdef USE_STATFS
  
#if STATFS_ARGS == 2
  statfs_result = statfs (local->filename, &statfs_buffer);
#elif STATFS_ARGS == 4
  statfs_result = statfs (local->filename, &statfs_buffer,
			  sizeof (statfs_buffer), 0);
#endif /* STATFS_ARGS == 2 */
  block_size = statfs_buffer.f_bsize;
  
  /* Many backends can't report free size (for instance the gvfs fuse
     backend for backend not supporting this), and set f_bfree to 0,
     but it can be 0 for real too. We treat the available == 0 and
     free == 0 case as "both of these are invalid".
   */
#ifndef G_OS_WIN32
  if (statfs_result == 0 &&
      statfs_buffer.f_bavail == 0 && statfs_buffer.f_bfree == 0)
    no_size = TRUE;
#endif /* G_OS_WIN32 */
  
#elif defined(USE_STATVFS)
  statfs_result = statvfs (local->filename, &statfs_buffer);
  block_size = statfs_buffer.f_frsize; 
#endif /* USE_STATFS */

  if (statfs_result == -1)
    {
      int errsv = errno;

      g_set_io_error (error,
                      _("Error getting filesystem info for %s: %s"),
                      file, errsv);
      return NULL;
    }

  info = g_file_info_new ();

  attribute_matcher = g_file_attribute_matcher_new (attributes);
  
  if (!no_size &&
      g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_FILESYSTEM_FREE))
    {
#ifdef G_OS_WIN32
      gchar *localdir = g_path_get_dirname (local->filename);
      wchar_t *wdirname = g_utf8_to_utf16 (localdir, -1, NULL, NULL, NULL);
      ULARGE_INTEGER li;
      
      g_free (localdir);
      if (GetDiskFreeSpaceExW (wdirname, &li, NULL, NULL))
        g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE, (guint64)li.QuadPart);
      g_free (wdirname);
#else
#if defined(USE_STATFS) || defined(USE_STATVFS)
      g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE, block_size * statfs_buffer.f_bavail);
#endif
#endif
    }
  if (!no_size &&
      g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_FILESYSTEM_SIZE))
    {
#ifdef G_OS_WIN32
      gchar *localdir = g_path_get_dirname (local->filename);
      wchar_t *wdirname = g_utf8_to_utf16 (localdir, -1, NULL, NULL, NULL);
      ULARGE_INTEGER li;
      
      g_free (localdir);
      if (GetDiskFreeSpaceExW (wdirname, NULL, &li, NULL))
        g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE,  (guint64)li.QuadPart);
      g_free (wdirname);
#else
#if defined(USE_STATFS) || defined(USE_STATVFS)
      g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE, block_size * statfs_buffer.f_blocks);
#endif
#endif /* G_OS_WIN32 */
    }

  if (!no_size &&
      g_file_attribute_matcher_matches (attribute_matcher,
                                        G_FILE_ATTRIBUTE_FILESYSTEM_USED))
    {
#ifdef G_OS_WIN32
      gchar *localdir = g_path_get_dirname (local->filename);
      wchar_t *wdirname = g_utf8_to_utf16 (localdir, -1, NULL, NULL, NULL);
      ULARGE_INTEGER li_free;
      ULARGE_INTEGER li_total;

      g_free (localdir);
      if (GetDiskFreeSpaceExW (wdirname, &li_free, &li_total, NULL))
        g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED,  (guint64)li_total.QuadPart - (guint64)li_free.QuadPart);
      g_free (wdirname);
#else
      g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED, block_size * (statfs_buffer.f_blocks - statfs_buffer.f_bfree));
#endif /* G_OS_WIN32 */
    }

#ifndef G_OS_WIN32
#ifdef USE_STATFS
#if defined(HAVE_STRUCT_STATFS_F_FSTYPENAME)
  fstype = statfs_buffer.f_fstypename;
#else
  fstype = get_fs_type (statfs_buffer.f_type);
#endif

#elif defined(USE_STATVFS)
#if defined(HAVE_STRUCT_STATVFS_F_FSTYPENAME)
  fstype = statfs_buffer.f_fstypename;
#elif defined(HAVE_STRUCT_STATVFS_F_BASETYPE)
  fstype = statfs_buffer.f_basetype;
#else
  fstype = NULL;
#endif
#endif /* USE_STATFS */

  if (fstype &&
      g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_FILESYSTEM_TYPE))
    g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE, fstype);
#endif /* G_OS_WIN32 */

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_FILESYSTEM_READONLY))
    {
#ifdef G_OS_WIN32
      get_filesystem_readonly (info, local->filename);
#else
      get_mount_info (info, local->filename, attribute_matcher);
#endif /* G_OS_WIN32 */
    }
  
  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE))
      g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE,
					 g_local_file_is_remote (local->filename));

  g_file_attribute_matcher_unref (attribute_matcher);
  
  return info;
}

static GMount *
g_local_file_find_enclosing_mount (GFile         *file,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  GStatBuf buf;
  char *mountpoint;
  GMount *mount;

  if (g_lstat (local->filename, &buf) != 0)
    goto error;

  mountpoint = find_mountpoint_for (local->filename, buf.st_dev);
  if (mountpoint == NULL)
    goto error;

  mount = _g_mount_get_for_mount_path (mountpoint, cancellable);
  g_free (mountpoint);
  if (mount)
    return mount;

error:
  g_set_io_error (error,
		  /* Translators: This is an error message when trying to find
		   * the enclosing (user visible) mount of a file, but none
		   * exists.
		   */
		  _("Containing mount for file %s not found"),
                  file, 0);

  return NULL;
}

static GFile *
g_local_file_set_display_name (GFile         *file,
			       const char    *display_name,
			       GCancellable  *cancellable,
			       GError       **error)
{
  GLocalFile *local, *new_local;
  GFile *new_file, *parent;
  GStatBuf statbuf;
  GVfsClass *class;
  GVfs *vfs;
  int errsv;

  parent = g_file_get_parent (file);
  if (parent == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Can’t rename root directory"));
      return NULL;
    }
  
  new_file = g_file_get_child_for_display_name (parent, display_name, error);
  g_object_unref (parent);
  
  if (new_file == NULL)
    return NULL;
  local = G_LOCAL_FILE (file);
  new_local = G_LOCAL_FILE (new_file);

  if (g_lstat (new_local->filename, &statbuf) == -1) 
    {
      errsv = errno;

      if (errsv != ENOENT)
        {
          g_set_io_error (error, _("Error renaming file %s: %s"), new_file, errsv);
          return NULL;
        }
    }
  else
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                           _("Can’t rename file, filename already exists"));
      return NULL;
    }

  if (g_rename (local->filename, new_local->filename) == -1)
    {
      errsv = errno;

      if (errsv == EINVAL)
	/* We can't get a rename file into itself error here,
	 * so this must be an invalid filename, on e.g. FAT
         */
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
                             _("Invalid filename"));
      else
        g_set_io_error (error,
		        _("Error renaming file %s: %s"),
                        file, errsv);
      g_object_unref (new_file);
      return NULL;
    }

  vfs = g_vfs_get_default ();
  class = G_VFS_GET_CLASS (vfs);
  if (class->local_file_moved)
    class->local_file_moved (vfs, local->filename, new_local->filename);

  return new_file;
}

static GFileInfo *
g_local_file_query_info (GFile                *file,
			 const char           *attributes,
			 GFileQueryInfoFlags   flags,
			 GCancellable         *cancellable,
			 GError              **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  GFileInfo *info;
  GFileAttributeMatcher *matcher;
  char *basename, *dirname;
  GLocalParentFileInfo parent_info;

  matcher = g_file_attribute_matcher_new (attributes);
  
  basename = g_path_get_basename (local->filename);
  
  dirname = g_path_get_dirname (local->filename);
  _g_local_file_info_get_parent_info (dirname, matcher, &parent_info);
  g_free (dirname);
  
  info = _g_local_file_info_get (basename, local->filename,
				 matcher, flags, &parent_info,
				 error);
  

  _g_local_file_info_free_parent_info (&parent_info);
  g_free (basename);

  g_file_attribute_matcher_unref (matcher);

  return info;
}

static GFileAttributeInfoList *
g_local_file_query_settable_attributes (GFile         *file,
					GCancellable  *cancellable,
					GError       **error)
{
  return g_file_attribute_info_list_ref (local_writable_attributes);
}

static GFileAttributeInfoList *
g_local_file_query_writable_namespaces (GFile         *file,
					GCancellable  *cancellable,
					GError       **error)
{
  GFileAttributeInfoList *list;
  GVfsClass *class;
  GVfs *vfs;

  if (g_once_init_enter (&local_writable_namespaces))
    {
      /* Writable namespaces: */

      list = g_file_attribute_info_list_new ();

#ifdef HAVE_XATTR
      g_file_attribute_info_list_add (list,
				      "xattr",
				      G_FILE_ATTRIBUTE_TYPE_STRING,
				      G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE |
				      G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
      g_file_attribute_info_list_add (list,
				      "xattr-sys",
				      G_FILE_ATTRIBUTE_TYPE_STRING,
				      G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED);
#endif

      vfs = g_vfs_get_default ();
      class = G_VFS_GET_CLASS (vfs);
      if (class->add_writable_namespaces)
	class->add_writable_namespaces (vfs, list);

      g_once_init_leave (&local_writable_namespaces, (gsize)list);
    }
  list = (GFileAttributeInfoList *)local_writable_namespaces;

  return g_file_attribute_info_list_ref (list);
}

static gboolean
g_local_file_set_attribute (GFile                *file,
			    const char           *attribute,
			    GFileAttributeType    type,
			    gpointer              value_p,
			    GFileQueryInfoFlags   flags,
			    GCancellable         *cancellable,
			    GError              **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);

  return _g_local_file_info_set_attribute (local->filename,
					   attribute,
					   type,
					   value_p,
					   flags,
					   cancellable,
					   error);
}

static gboolean
g_local_file_set_attributes_from_info (GFile                *file,
				       GFileInfo            *info,
				       GFileQueryInfoFlags   flags,
				       GCancellable         *cancellable,
				       GError              **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  int res, chained_res;
  GFileIface *default_iface;

  res = _g_local_file_info_set_attributes (local->filename,
					   info, flags, 
					   cancellable,
					   error);

  if (!res)
    error = NULL; /* Don't write over error if further errors */

  default_iface = g_type_default_interface_peek (G_TYPE_FILE);

  chained_res = (default_iface->set_attributes_from_info) (file, info, flags, cancellable, error);
  
  return res && chained_res;
}

static GFileInputStream *
g_local_file_read (GFile         *file,
		   GCancellable  *cancellable,
		   GError       **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  int fd, ret;
  GLocalFileStat buf;
  
  fd = g_open (local->filename, O_RDONLY|O_BINARY, 0);
  if (fd == -1)
    {
      int errsv = errno;

#ifdef G_OS_WIN32
      if (errsv == EACCES)
	{
	  /* Exploit the fact that on W32 the glib filename encoding is UTF8 */
	  ret = GLIB_PRIVATE_CALL (g_win32_stat_utf8) (local->filename, &buf);
	  if (ret == 0 && S_ISDIR (buf.st_mode))
            errsv = EISDIR;
	}
#endif
      g_set_io_error (error,
		      _("Error opening file %s: %s"),
                      file, errsv);
      return NULL;
    }

#ifdef G_OS_WIN32
  ret = GLIB_PRIVATE_CALL (g_win32_fstat) (fd, &buf);
#else
  ret = fstat (fd, &buf);
#endif

  if (ret == 0 && S_ISDIR (buf.st_mode))
    {
      (void) g_close (fd, NULL);
      g_set_io_error (error,
		      _("Error opening file %s: %s"),
                      file, EISDIR);
      return NULL;
    }
  
  return _g_local_file_input_stream_new (fd);
}

static GFileOutputStream *
g_local_file_append_to (GFile             *file,
			GFileCreateFlags   flags,
			GCancellable      *cancellable,
			GError           **error)
{
  return _g_local_file_output_stream_append (G_LOCAL_FILE (file)->filename,
					     flags, cancellable, error);
}

static GFileOutputStream *
g_local_file_create (GFile             *file,
		     GFileCreateFlags   flags,
		     GCancellable      *cancellable,
		     GError           **error)
{
  return _g_local_file_output_stream_create (G_LOCAL_FILE (file)->filename,
                                             FALSE, flags, NULL,
                                             cancellable, error);
}

static GFileOutputStream *
g_local_file_replace (GFile             *file,
		      const char        *etag,
		      gboolean           make_backup,
		      GFileCreateFlags   flags,
		      GCancellable      *cancellable,
		      GError           **error)
{
  return _g_local_file_output_stream_replace (G_LOCAL_FILE (file)->filename,
                                              FALSE,
                                              etag, make_backup, flags, NULL,
                                              cancellable, error);
}

static GFileIOStream *
g_local_file_open_readwrite (GFile                      *file,
			     GCancellable               *cancellable,
			     GError                    **error)
{
  GFileOutputStream *output;
  GFileIOStream *res;

  output = _g_local_file_output_stream_open (G_LOCAL_FILE (file)->filename,
					     TRUE,
					     cancellable, error);
  if (output == NULL)
    return NULL;

  res = _g_local_file_io_stream_new (G_LOCAL_FILE_OUTPUT_STREAM (output));
  g_object_unref (output);
  return res;
}

static GFileIOStream *
g_local_file_create_readwrite (GFile                      *file,
			       GFileCreateFlags            flags,
			       GCancellable               *cancellable,
			       GError                    **error)
{
  GFileOutputStream *output;
  GFileIOStream *res;

  output = _g_local_file_output_stream_create (G_LOCAL_FILE (file)->filename,
					       TRUE, flags, NULL,
					       cancellable, error);
  if (output == NULL)
    return NULL;

  res = _g_local_file_io_stream_new (G_LOCAL_FILE_OUTPUT_STREAM (output));
  g_object_unref (output);
  return res;
}

static GFileIOStream *
g_local_file_replace_readwrite (GFile                      *file,
				const char                 *etag,
				gboolean                    make_backup,
				GFileCreateFlags            flags,
				GCancellable               *cancellable,
				GError                    **error)
{
  GFileOutputStream *output;
  GFileIOStream *res;

  output = _g_local_file_output_stream_replace (G_LOCAL_FILE (file)->filename,
                                                TRUE,
                                                etag, make_backup, flags, NULL,
                                                cancellable, error);
  if (output == NULL)
    return NULL;

  res = _g_local_file_io_stream_new (G_LOCAL_FILE_OUTPUT_STREAM (output));
  g_object_unref (output);
  return res;
}

static gboolean
g_local_file_delete (GFile         *file,
		     GCancellable  *cancellable,
		     GError       **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  GVfsClass *class;
  GVfs *vfs;

  if (g_remove (local->filename) == -1)
    {
      int errsv = errno;

      /* Posix allows EEXIST too, but the more sane error
	 is G_IO_ERROR_NOT_FOUND, and it's what nautilus
	 expects */
      if (errsv == EEXIST)
	errsv = ENOTEMPTY;

      g_set_io_error (error,
		      _("Error removing file %s: %s"),
                      file, errsv);
      return FALSE;
    }

  vfs = g_vfs_get_default ();
  class = G_VFS_GET_CLASS (vfs);
  if (class->local_file_removed)
    class->local_file_removed (vfs, local->filename);

  return TRUE;
}

#ifndef G_OS_WIN32

static char *
strip_trailing_slashes (const char *path)
{
  char *path_copy;
  int len;

  path_copy = g_strdup (path);
  len = strlen (path_copy);
  while (len > 1 && path_copy[len-1] == '/')
    path_copy[--len] = 0;

  return path_copy;
 }

static char *
expand_symlink (const char *link)
{
  char *resolved, *canonical, *parent, *link2;
  char symlink_value[4096];
#ifdef G_OS_WIN32
#else
  gssize res;
#endif
  
#ifdef G_OS_WIN32
#else
  res = readlink (link, symlink_value, sizeof (symlink_value) - 1);
  
  if (res == -1)
    return g_strdup (link);
  symlink_value[res] = 0;
#endif
  
  if (g_path_is_absolute (symlink_value))
    return canonicalize_filename (symlink_value);
  else
    {
      link2 = strip_trailing_slashes (link);
      parent = g_path_get_dirname (link2);
      g_free (link2);
      
      resolved = g_build_filename (parent, symlink_value, NULL);
      g_free (parent);
      
      canonical = canonicalize_filename (resolved);
      
      g_free (resolved);

      return canonical;
    }
}

static char *
get_parent (const char *path, 
            dev_t      *parent_dev)
{
  char *parent, *tmp;
  GStatBuf parent_stat;
  int num_recursions;
  char *path_copy;

  path_copy = strip_trailing_slashes (path);
  
  parent = g_path_get_dirname (path_copy);
  if (strcmp (parent, ".") == 0 ||
      strcmp (parent, path_copy) == 0)
    {
      g_free (parent);
      g_free (path_copy);
      return NULL;
    }
  g_free (path_copy);

  num_recursions = 0;
  do {
    if (g_lstat (parent, &parent_stat) != 0)
      {
	g_free (parent);
	return NULL;
      }
    
    if (S_ISLNK (parent_stat.st_mode))
      {
	tmp = parent;
	parent = expand_symlink (parent);
	g_free (tmp);
      }
    
    num_recursions++;
    if (num_recursions > 12)
      {
	g_free (parent);
	return NULL;
      }
  } while (S_ISLNK (parent_stat.st_mode));

  *parent_dev = parent_stat.st_dev;
  
  return parent;
}

static char *
expand_all_symlinks (const char *path)
{
  char *parent, *parent_expanded;
  char *basename, *res;
  dev_t parent_dev;

  parent = get_parent (path, &parent_dev);
  if (parent)
    {
      parent_expanded = expand_all_symlinks (parent);
      g_free (parent);
      basename = g_path_get_basename (path);
      res = g_build_filename (parent_expanded, basename, NULL);
      g_free (basename);
      g_free (parent_expanded);
    }
  else
    res = g_strdup (path);
  
  return res;
}

static char *
find_mountpoint_for (const char *file, 
                     dev_t       dev)
{
  char *dir, *parent;
  dev_t dir_dev, parent_dev;

  dir = g_strdup (file);
  dir_dev = dev;

  while (1) 
    {
      parent = get_parent (dir, &parent_dev);
      if (parent == NULL)
        return dir;
    
      if (parent_dev != dir_dev)
        {
          g_free (parent);
          return dir;
        }
    
      g_free (dir);
      dir = parent;
    }
}

char *
_g_local_file_find_topdir_for (const char *file)
{
  char *dir;
  char *mountpoint = NULL;
  dev_t dir_dev;

  dir = get_parent (file, &dir_dev);
  if (dir == NULL)
    return NULL;

  mountpoint = find_mountpoint_for (dir, dir_dev);
  g_free (dir);

  return mountpoint;
}

static char *
get_unique_filename (const char *basename, 
                     int         id)
{
  const char *dot;
      
  if (id == 1)
    return g_strdup (basename);

  dot = strchr (basename, '.');
  if (dot)
    return g_strdup_printf ("%.*s.%d%s", (int)(dot - basename), basename, id, dot);
  else
    return g_strdup_printf ("%s.%d", basename, id);
}

static gboolean
path_has_prefix (const char *path, 
                 const char *prefix)
{
  int prefix_len;

  if (prefix == NULL)
    return TRUE;

  prefix_len = strlen (prefix);
  
  if (strncmp (path, prefix, prefix_len) == 0 &&
      (prefix_len == 0 || /* empty prefix always matches */
       prefix[prefix_len - 1] == '/' || /* last char in prefix was a /, so it must be in path too */
       path[prefix_len] == 0 ||
       path[prefix_len] == '/'))
    return TRUE;
  
  return FALSE;
}

static char *
try_make_relative (const char *path, 
                   const char *base)
{
  char *path2, *base2;
  char *relative;

  path2 = expand_all_symlinks (path);
  base2 = expand_all_symlinks (base);

  relative = NULL;
  if (path_has_prefix (path2, base2))
    {
      relative = path2 + strlen (base2);
      while (*relative == '/')
	relative ++;
      relative = g_strdup (relative);
    }
  g_free (path2);
  g_free (base2);

  if (relative)
    return relative;
  
  /* Failed, use abs path */
  return g_strdup (path);
}

gboolean
_g_local_file_has_trash_dir (const char *dirname, dev_t dir_dev)
{
  static gsize home_dev_set = 0;
  static dev_t home_dev;
  char *topdir, *globaldir, *trashdir, *tmpname;
  uid_t uid;
  char uid_str[32];
  GStatBuf global_stat, trash_stat;
  gboolean res;

  if (g_once_init_enter (&home_dev_set))
    {
      GStatBuf home_stat;

      g_stat (g_get_home_dir (), &home_stat);
      home_dev = home_stat.st_dev;
      g_once_init_leave (&home_dev_set, 1);
    }

  /* Assume we can trash to the home */
  if (dir_dev == home_dev)
    return TRUE;

  topdir = find_mountpoint_for (dirname, dir_dev);
  if (topdir == NULL)
    return FALSE;

  globaldir = g_build_filename (topdir, ".Trash", NULL);
  if (g_lstat (globaldir, &global_stat) == 0 &&
      S_ISDIR (global_stat.st_mode) &&
      (global_stat.st_mode & S_ISVTX) != 0)
    {
      /* got a toplevel sysadmin created dir, assume we
       * can trash to it (we should be able to create a dir)
       * This fails for the FAT case where the ownership of
       * that dir would be wrong though..
       */
      g_free (globaldir);
      g_free (topdir);
      return TRUE;
    }
  g_free (globaldir);

  /* No global trash dir, or it failed the tests, fall back to $topdir/.Trash-$uid */
  uid = geteuid ();
  g_snprintf (uid_str, sizeof (uid_str), "%lu", (unsigned long) uid);

  tmpname = g_strdup_printf (".Trash-%s", uid_str);
  trashdir = g_build_filename (topdir, tmpname, NULL);
  g_free (tmpname);

  if (g_lstat (trashdir, &trash_stat) == 0)
    {
      g_free (topdir);
      g_free (trashdir);
      return S_ISDIR (trash_stat.st_mode) &&
	     trash_stat.st_uid == uid;
    }
  g_free (trashdir);

  /* User specific trash didn't exist, can we create it? */
  res = g_access (topdir, W_OK) == 0;
  g_free (topdir);

  return res;
}

#ifdef G_OS_UNIX
gboolean
_g_local_file_is_lost_found_dir (const char *path, dev_t path_dev)
{
  gboolean ret = FALSE;
  gchar *mount_dir = NULL;
  size_t mount_dir_len;
  GStatBuf statbuf;

  if (!g_str_has_suffix (path, "/lost+found"))
    goto out;

  mount_dir = find_mountpoint_for (path, path_dev);
  if (mount_dir == NULL)
    goto out;

  mount_dir_len = strlen (mount_dir);
  /* We special-case rootfs ('/') since it's the only case where
   * mount_dir ends in '/'
   */
  if (mount_dir_len == 1)
    mount_dir_len--;
  if (mount_dir_len + strlen ("/lost+found") != strlen (path))
    goto out;

  if (g_lstat (path, &statbuf) != 0)
    goto out;

  if (!(S_ISDIR (statbuf.st_mode) &&
        statbuf.st_uid == 0 &&
        statbuf.st_gid == 0))
    goto out;

  ret = TRUE;

 out:
  g_free (mount_dir);
  return ret;
}
#endif

static gboolean
g_local_file_trash (GFile         *file,
		    GCancellable  *cancellable,
		    GError       **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  GStatBuf file_stat, home_stat;
  const char *homedir;
  char *trashdir, *topdir, *infodir, *filesdir;
  char *basename, *trashname, *trashfile, *infoname, *infofile;
  char *original_name, *original_name_escaped;
  int i;
  char *data;
  gboolean is_homedir_trash;
  char delete_time[32];
  int fd;
  GStatBuf trash_stat, global_stat;
  char *dirname, *globaldir;
  GVfsClass *class;
  GVfs *vfs;
  int errsv;

  if (g_lstat (local->filename, &file_stat) != 0)
    {
      errsv = errno;

      g_set_io_error (error,
		      _("Error trashing file %s: %s"),
                      file, errsv);
      return FALSE;
    }
    
  homedir = g_get_home_dir ();
  g_stat (homedir, &home_stat);

  is_homedir_trash = FALSE;
  trashdir = NULL;
  if (file_stat.st_dev == home_stat.st_dev)
    {
      is_homedir_trash = TRUE;
      errno = 0;
      trashdir = g_build_filename (g_get_user_data_dir (), "Trash", NULL);
      if (g_mkdir_with_parents (trashdir, 0700) < 0)
	{
          char *display_name;
          errsv = errno;

          display_name = g_filename_display_name (trashdir);
          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       _("Unable to create trash dir %s: %s"),
                       display_name, g_strerror (errsv));
          g_free (display_name);
          g_free (trashdir);
          return FALSE;
	}
      topdir = g_strdup (g_get_user_data_dir ());
    }
  else
    {
      uid_t uid;
      char uid_str[32];

      uid = geteuid ();
      g_snprintf (uid_str, sizeof (uid_str), "%lu", (unsigned long)uid);

      topdir = _g_local_file_find_topdir_for (local->filename);
      if (topdir == NULL)
	{
          g_set_io_error (error,
                          _("Unable to find toplevel directory to trash %s"),
                          file, G_IO_ERROR_NOT_SUPPORTED);
	  return FALSE;
	}
      
      /* Try looking for global trash dir $topdir/.Trash/$uid */
      globaldir = g_build_filename (topdir, ".Trash", NULL);
      if (g_lstat (globaldir, &global_stat) == 0 &&
	  S_ISDIR (global_stat.st_mode) &&
	  (global_stat.st_mode & S_ISVTX) != 0)
	{
	  trashdir = g_build_filename (globaldir, uid_str, NULL);

	  if (g_lstat (trashdir, &trash_stat) == 0)
	    {
	      if (!S_ISDIR (trash_stat.st_mode) ||
		  trash_stat.st_uid != uid)
		{
		  /* Not a directory or not owned by user, ignore */
		  g_free (trashdir);
		  trashdir = NULL;
		}
	    }
	  else if (g_mkdir (trashdir, 0700) == -1)
	    {
	      g_free (trashdir);
	      trashdir = NULL;
	    }
	}
      g_free (globaldir);

      if (trashdir == NULL)
	{
	  gboolean tried_create;
	  
	  /* No global trash dir, or it failed the tests, fall back to $topdir/.Trash-$uid */
	  dirname = g_strdup_printf (".Trash-%s", uid_str);
	  trashdir = g_build_filename (topdir, dirname, NULL);
	  g_free (dirname);

	  tried_create = FALSE;

	retry:
	  if (g_lstat (trashdir, &trash_stat) == 0)
	    {
	      if (!S_ISDIR (trash_stat.st_mode) ||
		  trash_stat.st_uid != uid)
		{
		  /* Remove the failed directory */
		  if (tried_create)
		    g_remove (trashdir);
		  
		  /* Not a directory or not owned by user, ignore */
		  g_free (trashdir);
		  trashdir = NULL;
		}
	    }
	  else
	    {
	      if (!tried_create &&
		  g_mkdir (trashdir, 0700) != -1)
		{
		  /* Ensure that the created dir has the right uid etc.
		     This might fail on e.g. a FAT dir */
		  tried_create = TRUE;
		  goto retry;
		}
	      else
		{
		  g_free (trashdir);
		  trashdir = NULL;
		}
	    }
	}

      if (trashdir == NULL)
	{
	  g_free (topdir);
          g_set_io_error (error,
                          _("Unable to find or create trash directory for %s"),
                          file, G_IO_ERROR_NOT_SUPPORTED);
	  return FALSE;
	}
    }

  /* Trashdir points to the trash dir with the "info" and "files" subdirectories */

  infodir = g_build_filename (trashdir, "info", NULL);
  filesdir = g_build_filename (trashdir, "files", NULL);
  g_free (trashdir);

  /* Make sure we have the subdirectories */
  if ((g_mkdir (infodir, 0700) == -1 && errno != EEXIST) ||
      (g_mkdir (filesdir, 0700) == -1 && errno != EEXIST))
    {
      g_free (topdir);
      g_free (infodir);
      g_free (filesdir);
      g_set_io_error (error,
                      _("Unable to find or create trash directory for %s"),
                      file, G_IO_ERROR_NOT_SUPPORTED);
      return FALSE;
    }

  basename = g_path_get_basename (local->filename);
  i = 1;
  trashname = NULL;
  infofile = NULL;
  do {
    g_free (trashname);
    g_free (infofile);
    
    trashname = get_unique_filename (basename, i++);
    infoname = g_strconcat (trashname, ".trashinfo", NULL);
    infofile = g_build_filename (infodir, infoname, NULL);
    g_free (infoname);

    fd = g_open (infofile, O_CREAT | O_EXCL, 0666);
    errsv = errno;
  } while (fd == -1 && errsv == EEXIST);

  g_free (basename);
  g_free (infodir);

  if (fd == -1)
    {
      errsv = errno;

      g_free (filesdir);
      g_free (topdir);
      g_free (trashname);
      g_free (infofile);

      g_set_io_error (error,
		      _("Unable to create trashing info file for %s: %s"),
                      file, errsv);
      return FALSE;
    }

  (void) g_close (fd, NULL);

  /* Write the full content of the info file before trashing to make
   * sure someone doesn't read an empty file.  See #749314
   */

  /* Use absolute names for homedir */
  if (is_homedir_trash)
    original_name = g_strdup (local->filename);
  else
    original_name = try_make_relative (local->filename, topdir);
  original_name_escaped = g_uri_escape_string (original_name, "/", FALSE);
  
  g_free (original_name);
  g_free (topdir);
  
  {
    time_t t;
    struct tm now;
    t = time (NULL);
    localtime_r (&t, &now);
    delete_time[0] = 0;
    strftime(delete_time, sizeof (delete_time), "%Y-%m-%dT%H:%M:%S", &now);
  }

  data = g_strdup_printf ("[Trash Info]\nPath=%s\nDeletionDate=%s\n",
			  original_name_escaped, delete_time);

  g_file_set_contents (infofile, data, -1, NULL);

  /* TODO: Maybe we should verify that you can delete the file from the trash
   * before moving it? OTOH, that is hard, as it needs a recursive scan
   */

  trashfile = g_build_filename (filesdir, trashname, NULL);

  g_free (filesdir);

  if (g_rename (local->filename, trashfile) == -1)
    {
      errsv = errno;

      g_unlink (infofile);

      g_free (trashname);
      g_free (infofile);
      g_free (trashfile);

      if (errsv == EXDEV)
	/* The trash dir was actually on another fs anyway!?
	 * This can happen when the same device is mounted multiple
	 * times, or with bind mounts of the same fs.
	 */
        g_set_io_error (error,
                        _("Unable to trash file %s across filesystem boundaries"),
                        file, ENOTSUP);
      else
        g_set_io_error (error,
		        _("Unable to trash file %s: %s"),
                        file, errsv);
      return FALSE;
    }

  vfs = g_vfs_get_default ();
  class = G_VFS_GET_CLASS (vfs);
  if (class->local_file_moved)
    class->local_file_moved (vfs, local->filename, trashfile);

  g_free (trashfile);

  /* TODO: Do we need to update mtime/atime here after the move? */

  g_free (infofile);
  g_free (data);
  
  g_free (original_name_escaped);
  g_free (trashname);
  
  return TRUE;
}
#else /* G_OS_WIN32 */
gboolean
_g_local_file_has_trash_dir (const char *dirname, dev_t dir_dev)
{
  return FALSE;			/* XXX ??? */
}

static gboolean
g_local_file_trash (GFile         *file,
		    GCancellable  *cancellable,
		    GError       **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  SHFILEOPSTRUCTW op = {0};
  gboolean success;
  wchar_t *wfilename;
  long len;

  wfilename = g_utf8_to_utf16 (local->filename, -1, NULL, &len, NULL);
  /* SHFILEOPSTRUCT.pFrom is double-zero-terminated */
  wfilename = g_renew (wchar_t, wfilename, len + 2);
  wfilename[len + 1] = 0;

  op.wFunc = FO_DELETE;
  op.pFrom = wfilename;
  op.fFlags = FOF_ALLOWUNDO;

  success = SHFileOperationW (&op) == 0;

  if (success && op.fAnyOperationsAborted)
    {
      if (cancellable && !g_cancellable_is_cancelled (cancellable))
	g_cancellable_cancel (cancellable);
      g_set_io_error (error,
                      _("Unable to trash file %s: %s"),
                      file, ECANCELED);
      success = FALSE;
    }
  else if (!success)
    g_set_io_error (error,
                    _("Unable to trash file %s"),
                    file, 0);

  g_free (wfilename);
  return success;
}
#endif /* G_OS_WIN32 */

static gboolean
g_local_file_make_directory (GFile         *file,
			     GCancellable  *cancellable,
			     GError       **error)
{
  GLocalFile *local = G_LOCAL_FILE (file);
  
  if (g_mkdir (local->filename, 0777) == -1)
    {
      int errsv = errno;

      if (errsv == EINVAL)
	/* This must be an invalid filename, on e.g. FAT */
	g_set_error_literal (error, G_IO_ERROR,
                             G_IO_ERROR_INVALID_FILENAME,
                             _("Invalid filename"));
      else
        g_set_io_error (error,
		        _("Error creating directory %s: %s"),
                        file, errsv);
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
g_local_file_make_symbolic_link (GFile         *file,
				 const char    *symlink_value,
				 GCancellable  *cancellable,
				 GError       **error)
{
#ifdef HAVE_SYMLINK
  GLocalFile *local = G_LOCAL_FILE (file);
  
  if (symlink (symlink_value, local->filename) == -1)
    {
      int errsv = errno;

      if (errsv == EINVAL)
	/* This must be an invalid filename, on e.g. FAT */
	g_set_error_literal (error, G_IO_ERROR,
                             G_IO_ERROR_INVALID_FILENAME,
                             _("Invalid filename"));
      else if (errsv == EPERM)
	g_set_error (error, G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     _("Filesystem does not support symbolic links"));
      else
        g_set_io_error (error,
		        _("Error making symbolic link %s: %s"),
                        file, errsv);
      return FALSE;
    }
  return TRUE;
#else
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, _("Symbolic links not supported"));
  return FALSE;
#endif
}


static gboolean
g_local_file_copy (GFile                  *source,
		   GFile                  *destination,
		   GFileCopyFlags          flags,
		   GCancellable           *cancellable,
		   GFileProgressCallback   progress_callback,
		   gpointer                progress_callback_data,
		   GError                **error)
{
  /* Fall back to default copy */
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Copy not supported");
  return FALSE;
}

static gboolean
g_local_file_move (GFile                  *source,
		   GFile                  *destination,
		   GFileCopyFlags          flags,
		   GCancellable           *cancellable,
		   GFileProgressCallback   progress_callback,
		   gpointer                progress_callback_data,
		   GError                **error)
{
  GLocalFile *local_source, *local_destination;
  GStatBuf statbuf;
  gboolean destination_exist, source_is_dir;
  char *backup_name;
  int res;
  off_t source_size;
  GVfsClass *class;
  GVfs *vfs;

  if (!G_IS_LOCAL_FILE (source) ||
      !G_IS_LOCAL_FILE (destination))
    {
      /* Fall back to default move */
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Move not supported");
      return FALSE;
    }
  
  local_source = G_LOCAL_FILE (source);
  local_destination = G_LOCAL_FILE (destination);
  
  res = g_lstat (local_source->filename, &statbuf);
  if (res == -1)
    {
      int errsv = errno;

      g_set_io_error (error,
                      _("Error moving file %s: %s"),
                      source, errsv);
      return FALSE;
    }

  source_is_dir = S_ISDIR (statbuf.st_mode);
  source_size = statbuf.st_size;
  
  destination_exist = FALSE;
  res = g_lstat (local_destination->filename, &statbuf);
  if (res == 0)
    {
      destination_exist = TRUE; /* Target file exists */

      if (flags & G_FILE_COPY_OVERWRITE)
	{
	  /* Always fail on dirs, even with overwrite */
	  if (S_ISDIR (statbuf.st_mode))
	    {
	      if (source_is_dir)
		g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_WOULD_MERGE,
                                     _("Can’t move directory over directory"));
              else
		g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_IS_DIRECTORY,
                                     _("Can’t copy over directory"));
	      return FALSE;
	    }
	}
      else
	{
          g_set_io_error (error,
                          _("Error moving file %s: %s"),
                          source, EEXIST);
	  return FALSE;
	}
    }
  
  if (flags & G_FILE_COPY_BACKUP && destination_exist)
    {
      backup_name = g_strconcat (local_destination->filename, "~", NULL);
      if (g_rename (local_destination->filename, backup_name) == -1)
	{
      	  g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_CANT_CREATE_BACKUP,
                               _("Backup file creation failed"));
	  g_free (backup_name);
	  return FALSE;
	}
      g_free (backup_name);
      destination_exist = FALSE; /* It did, but no more */
    }

  if (source_is_dir && destination_exist && (flags & G_FILE_COPY_OVERWRITE))
    {
      /* Source is a dir, destination exists (and is not a dir, because that would have failed
	 earlier), and we're overwriting. Manually remove the target so we can do the rename. */
      res = g_unlink (local_destination->filename);
      if (res == -1)
	{
          int errsv = errno;

	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error removing target file: %s"),
		       g_strerror (errsv));
	  return FALSE;
	}
    }
  
  if (g_rename (local_source->filename, local_destination->filename) == -1)
    {
      int errsv = errno;

      if (errsv == EXDEV)
	/* This will cause the fallback code to run */
	g_set_error_literal (error, G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             _("Move between mounts not supported"));
      else if (errsv == EINVAL)
	/* This must be an invalid filename, on e.g. FAT, or
	   we're trying to move the file into itself...
	   We return invalid filename for both... */
	g_set_error_literal (error, G_IO_ERROR,
                             G_IO_ERROR_INVALID_FILENAME,
                             _("Invalid filename"));
      else
        g_set_io_error (error,
                        _("Error moving file %s: %s"),
                        source, errsv);
      return FALSE;
    }

  vfs = g_vfs_get_default ();
  class = G_VFS_GET_CLASS (vfs);
  if (class->local_file_moved)
    class->local_file_moved (vfs, local_source->filename, local_destination->filename);

  /* Make sure we send full copied size */
  if (progress_callback)
    progress_callback (source_size, source_size, progress_callback_data);
  
  return TRUE;
}

#ifdef G_OS_WIN32

gboolean
g_local_file_is_remote (const gchar *filename)
{
  return FALSE;
}

#else

static gboolean
is_remote_fs (const gchar *filename)
{
  const char *fsname = NULL;

#ifdef USE_STATFS
  struct statfs statfs_buffer;
  int statfs_result = 0;

#if STATFS_ARGS == 2
  statfs_result = statfs (filename, &statfs_buffer);
#elif STATFS_ARGS == 4
  statfs_result = statfs (filename, &statfs_buffer, sizeof (statfs_buffer), 0);
#endif

#elif defined(USE_STATVFS)
  struct statvfs statfs_buffer;
  int statfs_result = 0;

  statfs_result = statvfs (filename, &statfs_buffer);
#else
  return FALSE;
#endif

  if (statfs_result == -1)
    return FALSE;

#ifdef USE_STATFS
#if defined(HAVE_STRUCT_STATFS_F_FSTYPENAME)
  fsname = statfs_buffer.f_fstypename;
#else
  fsname = get_fs_type (statfs_buffer.f_type);
#endif

#elif defined(USE_STATVFS) && defined(HAVE_STRUCT_STATVFS_F_BASETYPE)
  fsname = statfs_buffer.f_basetype;
#endif

  if (fsname != NULL)
    {
      if (strcmp (fsname, "nfs") == 0)
        return TRUE;
      if (strcmp (fsname, "nfs4") == 0)
        return TRUE;
    }

  return FALSE;
}

gboolean
g_local_file_is_remote (const gchar *filename)
{
  static gboolean remote_home;
  static gsize initialized;
  const gchar *home;

  home = g_get_home_dir ();
  if (path_has_prefix (filename, home))
    {
      if (g_once_init_enter (&initialized))
        {
          remote_home = is_remote_fs (home);
          g_once_init_leave (&initialized, TRUE);
        }
      return remote_home;
    }

  return FALSE;
}
#endif /* !G_OS_WIN32 */

static GFileMonitor*
g_local_file_monitor_dir (GFile             *file,
			  GFileMonitorFlags  flags,
			  GCancellable      *cancellable,
			  GError           **error)
{
  GLocalFile *local_file = G_LOCAL_FILE (file);

  return g_local_file_monitor_new_for_path (local_file->filename, TRUE, flags, error);
}

static GFileMonitor*
g_local_file_monitor_file (GFile             *file,
			   GFileMonitorFlags  flags,
			   GCancellable      *cancellable,
			   GError           **error)
{
  GLocalFile *local_file = G_LOCAL_FILE (file);

  return g_local_file_monitor_new_for_path (local_file->filename, FALSE, flags, error);
}

/* Here is the GLocalFile implementation of g_file_measure_disk_usage().
 *
 * If available, we use fopenat() in preference to filenames for
 * efficiency and safety reasons.  We know that fopenat() is available
 * based on if AT_FDCWD is defined.  POSIX guarantees that this will be
 * defined as a macro.
 *
 * We use a linked list of stack-allocated GSList nodes in order to be
 * able to reconstruct the filename for error messages.  We actually
 * pass the filename to operate on through the top node of the list.
 *
 * In case we're using openat(), this top filename will be a basename
 * which should be opened in the directory which has also had its fd
 * passed along.  If we're not using openat() then it will be a full
 * absolute filename.
 */

static gboolean
g_local_file_measure_size_error (GFileMeasureFlags   flags,
                                 gint                saved_errno,
                                 GSList             *name,
                                 GError            **error)
{
  /* Only report an error if we were at the toplevel or if the caller
   * requested reporting of all errors.
   */
  if ((name->next == NULL) || (flags & G_FILE_MEASURE_REPORT_ANY_ERROR))
    {
      GString *filename;
      GSList *node;

      /* Skip some work if there is no error return */
      if (!error)
        return FALSE;

#ifdef AT_FDCWD
      /* If using openat() we need to rebuild the filename for the message */
      filename = g_string_new (name->data);
      for (node = name->next; node; node = node->next)
        {
          gchar *utf8;

          g_string_prepend_c (filename, G_DIR_SEPARATOR);
          utf8 = g_filename_display_name (node->data);
          g_string_prepend (filename, utf8);
          g_free (utf8);
        }
#else
      {
        gchar *utf8;

        /* Otherwise, we already have it, so just use it. */
        node = name;
        filename = g_string_new (NULL);
        utf8 = g_filename_display_name (node->data);
        g_string_append (filename, utf8);
        g_free (utf8);
      }
#endif

      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (saved_errno),
                   _("Could not determine the disk usage of %s: %s"),
                   filename->str, g_strerror (saved_errno));

      g_string_free (filename, TRUE);

      return FALSE;
    }

  else
    /* We're not reporting this error... */
    return TRUE;
}

typedef struct
{
  GFileMeasureFlags  flags;
  dev_t              contained_on;
  GCancellable      *cancellable;

  GFileMeasureProgressCallback progress_callback;
  gpointer                     progress_data;

  guint64 disk_usage;
  guint64 num_dirs;
  guint64 num_files;

  guint64 last_progress_report;
} MeasureState;

static gboolean
g_local_file_measure_size_of_contents (gint           fd,
                                       GSList        *dir_name,
                                       MeasureState  *state,
                                       GError       **error);

static gboolean
g_local_file_measure_size_of_file (gint           parent_fd,
                                   GSList        *name,
                                   MeasureState  *state,
                                   GError       **error)
{
  GLocalFileStat buf;

  if (g_cancellable_set_error_if_cancelled (state->cancellable, error))
    return FALSE;

#if defined (AT_FDCWD)
  if (fstatat (parent_fd, name->data, &buf, AT_SYMLINK_NOFOLLOW) != 0)
    {
      int errsv = errno;
      return g_local_file_measure_size_error (state->flags, errsv, name, error);
    }
#elif defined (HAVE_LSTAT) || !defined (G_OS_WIN32)
  if (g_lstat (name->data, &buf) != 0)
    {
      int errsv = errno;
      return g_local_file_measure_size_error (state->flags, errsv, name, error);
    }
#else /* !AT_FDCWD && !HAVE_LSTAT && G_OS_WIN32 */
  if (GLIB_PRIVATE_CALL (g_win32_lstat_utf8) (name->data, &buf) != 0)
    {
      int errsv = errno;
      return g_local_file_measure_size_error (state->flags, errsv, name, error);
    }
#endif

  if (name->next)
    {
      /* If not at the toplevel, check for a device boundary. */

      if (state->flags & G_FILE_MEASURE_NO_XDEV)
        if (state->contained_on != buf.st_dev)
          return TRUE;
    }
  else
    {
      /* If, however, this is the toplevel, set the device number so
       * that recursive invocations can compare against it.
       */
      state->contained_on = buf.st_dev;
    }

#if defined (G_OS_WIN32)
  if (~state->flags & G_FILE_MEASURE_APPARENT_SIZE)
    state->disk_usage += buf.allocated_size;
  else
#elif defined (HAVE_STRUCT_STAT_ST_BLOCKS)
  if (~state->flags & G_FILE_MEASURE_APPARENT_SIZE)
    state->disk_usage += buf.st_blocks * G_GUINT64_CONSTANT (512);
  else
#endif
    state->disk_usage += buf.st_size;

  if (S_ISDIR (buf.st_mode))
    state->num_dirs++;
  else
    state->num_files++;

  if (state->progress_callback)
    {
      /* We could attempt to do some cleverness here in order to avoid
       * calling clock_gettime() so much, but we're doing stats and opens
       * all over the place already...
       */
      if (state->last_progress_report)
        {
          guint64 now;

          now = g_get_monotonic_time ();

          if (state->last_progress_report + 200 * G_TIME_SPAN_MILLISECOND < now)
            {
              (* state->progress_callback) (TRUE,
                                            state->disk_usage, state->num_dirs, state->num_files,
                                            state->progress_data);
              state->last_progress_report = now;
            }
        }
      else
        {
          /* We must do an initial report to inform that more reports
           * will be coming.
           */
          (* state->progress_callback) (TRUE, 0, 0, 0, state->progress_data);
          state->last_progress_report = g_get_monotonic_time ();
        }
    }

  if (S_ISDIR (buf.st_mode))
    {
      int dir_fd = -1;
      int errsv;

      if (g_cancellable_set_error_if_cancelled (state->cancellable, error))
        return FALSE;

#ifdef AT_FDCWD
#ifdef HAVE_OPEN_O_DIRECTORY
      dir_fd = openat (parent_fd, name->data, O_RDONLY|O_DIRECTORY);
#else
      dir_fd = openat (parent_fd, name->data, O_RDONLY);
#endif
      errsv = errno;
      if (dir_fd < 0)
        return g_local_file_measure_size_error (state->flags, errsv, name, error);
#endif

      if (!g_local_file_measure_size_of_contents (dir_fd, name, state, error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
g_local_file_measure_size_of_contents (gint           fd,
                                       GSList        *dir_name,
                                       MeasureState  *state,
                                       GError       **error)
{
  gboolean success = TRUE;
  const gchar *name;
  GDir *dir;

#ifdef AT_FDCWD
  {
    /* If this fails, we want to preserve the errno from fopendir() */
    DIR *dirp;
    dirp = fdopendir (fd);
    dir = dirp ? GLIB_PRIVATE_CALL(g_dir_new_from_dirp) (dirp) : NULL;
  }
#else
  dir = GLIB_PRIVATE_CALL(g_dir_open_with_errno) (dir_name->data, 0);
#endif

  if (dir == NULL)
    {
      gint saved_errno = errno;

#ifdef AT_FDCWD
      close (fd);
#endif

      return g_local_file_measure_size_error (state->flags, saved_errno, dir_name, error);
    }

  while (success && (name = g_dir_read_name (dir)))
    {
      GSList node;

      node.next = dir_name;
#ifdef AT_FDCWD
      node.data = (gchar *) name;
#else
      node.data = g_build_filename (dir_name->data, name, NULL);
#endif

      success = g_local_file_measure_size_of_file (fd, &node, state, error);

#ifndef AT_FDCWD
      g_free (node.data);
#endif
    }

  g_dir_close (dir);

  return success;
}

static gboolean
g_local_file_measure_disk_usage (GFile                         *file,
                                 GFileMeasureFlags              flags,
                                 GCancellable                  *cancellable,
                                 GFileMeasureProgressCallback   progress_callback,
                                 gpointer                       progress_data,
                                 guint64                       *disk_usage,
                                 guint64                       *num_dirs,
                                 guint64                       *num_files,
                                 GError                       **error)
{
  GLocalFile *local_file = G_LOCAL_FILE (file);
  MeasureState state = { 0, };
  gint root_fd = -1;
  GSList node;

  state.flags = flags;
  state.cancellable = cancellable;
  state.progress_callback = progress_callback;
  state.progress_data = progress_data;

#ifdef AT_FDCWD
  root_fd = AT_FDCWD;
#endif

  node.data = local_file->filename;
  node.next = NULL;

  if (!g_local_file_measure_size_of_file (root_fd, &node, &state, error))
    return FALSE;

  if (disk_usage)
    *disk_usage = state.disk_usage;

  if (num_dirs)
    *num_dirs = state.num_dirs;

  if (num_files)
    *num_files = state.num_files;

  return TRUE;
}

static void
g_local_file_file_iface_init (GFileIface *iface)
{
  iface->dup = g_local_file_dup;
  iface->hash = g_local_file_hash;
  iface->equal = g_local_file_equal;
  iface->is_native = g_local_file_is_native;
  iface->has_uri_scheme = g_local_file_has_uri_scheme;
  iface->get_uri_scheme = g_local_file_get_uri_scheme;
  iface->get_basename = g_local_file_get_basename;
  iface->get_path = g_local_file_get_path;
  iface->get_uri = g_local_file_get_uri;
  iface->get_parse_name = g_local_file_get_parse_name;
  iface->get_parent = g_local_file_get_parent;
  iface->prefix_matches = g_local_file_prefix_matches;
  iface->get_relative_path = g_local_file_get_relative_path;
  iface->resolve_relative_path = g_local_file_resolve_relative_path;
  iface->get_child_for_display_name = g_local_file_get_child_for_display_name;
  iface->set_display_name = g_local_file_set_display_name;
  iface->enumerate_children = g_local_file_enumerate_children;
  iface->query_info = g_local_file_query_info;
  iface->query_filesystem_info = g_local_file_query_filesystem_info;
  iface->find_enclosing_mount = g_local_file_find_enclosing_mount;
  iface->query_settable_attributes = g_local_file_query_settable_attributes;
  iface->query_writable_namespaces = g_local_file_query_writable_namespaces;
  iface->set_attribute = g_local_file_set_attribute;
  iface->set_attributes_from_info = g_local_file_set_attributes_from_info;
  iface->read_fn = g_local_file_read;
  iface->append_to = g_local_file_append_to;
  iface->create = g_local_file_create;
  iface->replace = g_local_file_replace;
  iface->open_readwrite = g_local_file_open_readwrite;
  iface->create_readwrite = g_local_file_create_readwrite;
  iface->replace_readwrite = g_local_file_replace_readwrite;
  iface->delete_file = g_local_file_delete;
  iface->trash = g_local_file_trash;
  iface->make_directory = g_local_file_make_directory;
  iface->make_symbolic_link = g_local_file_make_symbolic_link;
  iface->copy = g_local_file_copy;
  iface->move = g_local_file_move;
  iface->monitor_dir = g_local_file_monitor_dir;
  iface->monitor_file = g_local_file_monitor_file;
  iface->measure_disk_usage = g_local_file_measure_disk_usage;

  iface->supports_thread_contexts = TRUE;
}
