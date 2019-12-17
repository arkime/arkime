/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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

/* Prologue {{{1 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifndef HAVE_SYSCTLBYNAME
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#endif
#ifdef HAVE_POLL
#include <poll.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <gstdio.h>
#include <dirent.h>

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

#include "gunixmounts.h"
#include "glocalfileprivate.h"
#include "gfile.h"
#include "gfilemonitor.h"
#include "glibintl.h"
#include "gthemedicon.h"
#include "gcontextspecificgroup.h"


#ifdef HAVE_MNTENT_H
static const char *_resolve_dev_root (void);
#endif

/**
 * SECTION:gunixmounts
 * @include: gio/gunixmounts.h
 * @short_description: UNIX mounts
 *
 * Routines for managing mounted UNIX mount points and paths.
 *
 * Note that `<gio/gunixmounts.h>` belongs to the UNIX-specific GIO
 * interfaces, thus you have to use the `gio-unix-2.0.pc` pkg-config
 * file when using it.
 */

/**
 * GUnixMountType:
 * @G_UNIX_MOUNT_TYPE_UNKNOWN: Unknown UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_FLOPPY: Floppy disk UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_CDROM: CDROM UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_NFS: Network File System (NFS) UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_ZIP: ZIP UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_JAZ: JAZZ UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_MEMSTICK: Memory Stick UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_CF: Compact Flash UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_SM: Smart Media UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_SDMMC: SD/MMC UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_IPOD: iPod UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_CAMERA: Digital camera UNIX mount type.
 * @G_UNIX_MOUNT_TYPE_HD: Hard drive UNIX mount type.
 * 
 * Types of UNIX mounts.
 **/
typedef enum {
  G_UNIX_MOUNT_TYPE_UNKNOWN,
  G_UNIX_MOUNT_TYPE_FLOPPY,
  G_UNIX_MOUNT_TYPE_CDROM,
  G_UNIX_MOUNT_TYPE_NFS,
  G_UNIX_MOUNT_TYPE_ZIP,
  G_UNIX_MOUNT_TYPE_JAZ,
  G_UNIX_MOUNT_TYPE_MEMSTICK,
  G_UNIX_MOUNT_TYPE_CF,
  G_UNIX_MOUNT_TYPE_SM,
  G_UNIX_MOUNT_TYPE_SDMMC,
  G_UNIX_MOUNT_TYPE_IPOD,
  G_UNIX_MOUNT_TYPE_CAMERA,
  G_UNIX_MOUNT_TYPE_HD
} GUnixMountType;

struct _GUnixMountEntry {
  char *mount_path;
  char *device_path;
  char *filesystem_type;
  gboolean is_read_only;
  gboolean is_system_internal;
};

G_DEFINE_BOXED_TYPE (GUnixMountEntry, g_unix_mount_entry,
                     g_unix_mount_copy, g_unix_mount_free)

struct _GUnixMountPoint {
  char *mount_path;
  char *device_path;
  char *filesystem_type;
  char *options;
  gboolean is_read_only;
  gboolean is_user_mountable;
  gboolean is_loopback;
};

G_DEFINE_BOXED_TYPE (GUnixMountPoint, g_unix_mount_point,
                     g_unix_mount_point_copy, g_unix_mount_point_free)

static GList *_g_get_unix_mounts (void);
static GList *_g_get_unix_mount_points (void);
static gboolean proc_mounts_watch_is_running (void);

static guint64 mount_poller_time = 0;

#ifdef HAVE_SYS_MNTTAB_H
#define MNTOPT_RO	"ro"
#endif

#ifdef HAVE_MNTENT_H
#include <mntent.h>
#ifdef HAVE_LIBMOUNT
#include <libmount.h>
#endif
#elif defined (HAVE_SYS_MNTTAB_H)
#include <sys/mnttab.h>
#endif

#ifdef HAVE_SYS_VFSTAB_H
#include <sys/vfstab.h>
#endif

#if defined(HAVE_SYS_MNTCTL_H) && defined(HAVE_SYS_VMOUNT_H) && defined(HAVE_SYS_VFS_H)
#include <sys/mntctl.h>
#include <sys/vfs.h>
#include <sys/vmount.h>
#include <fshelp.h>
#endif

#if (defined(HAVE_GETVFSSTAT) || defined(HAVE_GETFSSTAT)) && defined(HAVE_FSTAB_H) && defined(HAVE_SYS_MOUNT_H)
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <fstab.h>
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#endif

#ifndef HAVE_SETMNTENT
#define setmntent(f,m) fopen(f,m)
#endif
#ifndef HAVE_ENDMNTENT
#define endmntent(f) fclose(f)
#endif

static gboolean
is_in (const char *value, const char *set[])
{
  int i;
  for (i = 0; set[i] != NULL; i++)
    {
      if (strcmp (set[i], value) == 0)
	return TRUE;
    }
  return FALSE;
}

/**
 * g_unix_is_mount_path_system_internal:
 * @mount_path: (type filename): a mount path, e.g. `/media/disk` or `/usr`
 *
 * Determines if @mount_path is considered an implementation of the
 * OS. This is primarily used for hiding mountable and mounted volumes
 * that only are used in the OS and has little to no relevance to the
 * casual user.
 *
 * Returns: %TRUE if @mount_path is considered an implementation detail 
 *     of the OS.
 **/
gboolean
g_unix_is_mount_path_system_internal (const char *mount_path)
{
  const char *ignore_mountpoints[] = {
    /* Includes all FHS 2.3 toplevel dirs and other specialized
     * directories that we want to hide from the user.
     */
    "/",              /* we already have "Filesystem root" in Nautilus */ 
    "/bin",
    "/boot",
    "/compat/linux/proc",
    "/compat/linux/sys",
    "/dev",
    "/etc",
    "/home",
    "/lib",
    "/lib64",
    "/libexec",
    "/live/cow",
    "/live/image",
    "/media",
    "/mnt",
    "/opt",
    "/rescue",
    "/root",
    "/sbin",
    "/srv",
    "/tmp",
    "/usr",
    "/usr/X11R6",
    "/usr/local",
    "/usr/obj",
    "/usr/ports",
    "/usr/src",
    "/usr/xobj",
    "/var",
    "/var/crash",
    "/var/local",
    "/var/log",
    "/var/log/audit", /* https://bugzilla.redhat.com/show_bug.cgi?id=333041 */
    "/var/mail",
    "/var/run",
    "/var/tmp",       /* https://bugzilla.redhat.com/show_bug.cgi?id=335241 */
    "/proc",
    "/sbin",
    "/net",
    "/sys",
    NULL
  };

  if (is_in (mount_path, ignore_mountpoints))
    return TRUE;
  
  if (g_str_has_prefix (mount_path, "/dev/") ||
      g_str_has_prefix (mount_path, "/proc/") ||
      g_str_has_prefix (mount_path, "/sys/"))
    return TRUE;

  if (g_str_has_suffix (mount_path, "/.gvfs"))
    return TRUE;

  return FALSE;
}

/**
 * g_unix_is_system_fs_type:
 * @fs_type: a file system type, e.g. `procfs` or `tmpfs`
 *
 * Determines if @fs_type is considered a type of file system which is only
 * used in implementation of the OS. This is primarily used for hiding
 * mounted volumes that are intended as APIs for programs to read, and system
 * administrators at a shell; rather than something that should, for example,
 * appear in a GUI. For example, the Linux `/proc` filesystem.
 *
 * The list of file system types considered ‘system’ ones may change over time.
 *
 * Returns: %TRUE if @fs_type is considered an implementation detail of the OS.
 * Since: 2.56
 */
gboolean
g_unix_is_system_fs_type (const char *fs_type)
{
  const char *ignore_fs[] = {
    "adfs",
    "afs",
    "auto",
    "autofs",
    "autofs4",
    "cgroup",
    "configfs",
    "cxfs",
    "debugfs",
    "devfs",
    "devpts",
    "devtmpfs",
    "ecryptfs",
    "fdescfs",
    "fusectl",
    "gfs",
    "gfs2",
    "gpfs",
    "hugetlbfs",
    "kernfs",
    "linprocfs",
    "linsysfs",
    "lustre",
    "lustre_lite",
    "mfs",
    "mqueue",
    "ncpfs",
    "nfsd",
    "nullfs",
    "ocfs2",
    "overlay",
    "proc",
    "procfs",
    "pstore",
    "ptyfs",
    "rootfs",
    "rpc_pipefs",
    "securityfs",
    "selinuxfs",
    "sysfs",
    "tmpfs",
    "usbfs",
    "zfs",
    NULL
  };

  g_return_val_if_fail (fs_type != NULL && *fs_type != '\0', FALSE);

  return is_in (fs_type, ignore_fs);
}

/**
 * g_unix_is_system_device_path:
 * @device_path: a device path, e.g. `/dev/loop0` or `nfsd`
 *
 * Determines if @device_path is considered a block device path which is only
 * used in implementation of the OS. This is primarily used for hiding
 * mounted volumes that are intended as APIs for programs to read, and system
 * administrators at a shell; rather than something that should, for example,
 * appear in a GUI. For example, the Linux `/proc` filesystem.
 *
 * The list of device paths considered ‘system’ ones may change over time.
 *
 * Returns: %TRUE if @device_path is considered an implementation detail of
 *    the OS.
 * Since: 2.56
 */
gboolean
g_unix_is_system_device_path (const char *device_path)
{
  const char *ignore_devices[] = {
    "none",
    "sunrpc",
    "devpts",
    "nfsd",
    "/dev/loop",
    "/dev/vn",
    NULL
  };

  g_return_val_if_fail (device_path != NULL && *device_path != '\0', FALSE);

  return is_in (device_path, ignore_devices);
}

static gboolean
guess_system_internal (const char *mountpoint,
		       const char *fs,
		       const char *device)
{
  if (g_unix_is_system_fs_type (fs))
    return TRUE;
  
  if (g_unix_is_system_device_path (device))
    return TRUE;

  if (g_unix_is_mount_path_system_internal (mountpoint))
    return TRUE;
  
  return FALSE;
}

/* GUnixMounts (ie: mtab) implementations {{{1 */

static GUnixMountEntry *
create_unix_mount_entry (const char *device_path,
                         const char *mount_path,
                         const char *filesystem_type,
                         gboolean    is_read_only)
{
  GUnixMountEntry *mount_entry = NULL;

  mount_entry = g_new0 (GUnixMountEntry, 1);
  mount_entry->device_path = g_strdup (device_path);
  mount_entry->mount_path = g_strdup (mount_path);
  mount_entry->filesystem_type = g_strdup (filesystem_type);
  mount_entry->is_read_only = is_read_only;

  mount_entry->is_system_internal =
    guess_system_internal (mount_entry->mount_path,
                           mount_entry->filesystem_type,
                           mount_entry->device_path);
  return mount_entry;
}

static GUnixMountPoint *
create_unix_mount_point (const char *device_path,
                         const char *mount_path,
                         const char *filesystem_type,
                         const char *options,
                         gboolean    is_read_only,
                         gboolean    is_user_mountable,
                         gboolean    is_loopback)
{
  GUnixMountPoint *mount_point = NULL;

  mount_point = g_new0 (GUnixMountPoint, 1);
  mount_point->device_path = g_strdup (device_path);
  mount_point->mount_path = g_strdup (mount_path);
  mount_point->filesystem_type = g_strdup (filesystem_type);
  mount_point->options = g_strdup (options);
  mount_point->is_read_only = is_read_only;
  mount_point->is_user_mountable = is_user_mountable;
  mount_point->is_loopback = is_loopback;

  return mount_point;
}

/* mntent.h (Linux, GNU, NSS) {{{2 */
#ifdef HAVE_MNTENT_H

#ifdef HAVE_LIBMOUNT

/* For documentation on /proc/self/mountinfo see
 * http://www.kernel.org/doc/Documentation/filesystems/proc.txt
 */
#define PROC_MOUNTINFO_PATH "/proc/self/mountinfo"

static GList *
_g_get_unix_mounts (void)
{
  struct libmnt_table *table = NULL;
  struct libmnt_iter* iter = NULL;
  struct libmnt_fs *fs = NULL;
  GUnixMountEntry *mount_entry = NULL;
  GList *return_list = NULL;

  table = mnt_new_table ();
  if (mnt_table_parse_mtab (table, NULL) < 0)
    goto out;

  iter = mnt_new_iter (MNT_ITER_FORWARD);
  while (mnt_table_next_fs (table, iter, &fs) == 0)
    {
      const char *device_path = NULL;
      char *mount_options = NULL;
      unsigned long mount_flags = 0;
      gboolean is_read_only = FALSE;

      device_path = mnt_fs_get_source (fs);
      if (g_strcmp0 (device_path, "/dev/root") == 0)
        device_path = _resolve_dev_root ();

      mount_options = mnt_fs_strdup_options (fs);
      if (mount_options)
        {
          mnt_optstr_get_flags (mount_options, &mount_flags, mnt_get_builtin_optmap (MNT_LINUX_MAP));
          g_free (mount_options);
        }
      is_read_only = (mount_flags & MS_RDONLY) ? TRUE : FALSE;

      mount_entry = create_unix_mount_entry (device_path,
                                             mnt_fs_get_target (fs),
                                             mnt_fs_get_fstype (fs),
                                             is_read_only);

      return_list = g_list_prepend (return_list, mount_entry);
    }
  mnt_free_iter (iter);

 out:
  mnt_free_table (table);

  return g_list_reverse (return_list);
}

#else

static const char *
get_mtab_read_file (void)
{
#ifdef _PATH_MOUNTED
# ifdef __linux__
  return "/proc/mounts";
# else
  return _PATH_MOUNTED;
# endif
#else
  return "/etc/mtab";
#endif
}

#ifndef HAVE_GETMNTENT_R
G_LOCK_DEFINE_STATIC(getmntent);
#endif

static GList *
_g_get_unix_mounts (void)
{
#ifdef HAVE_GETMNTENT_R
  struct mntent ent;
  char buf[1024];
#endif
  struct mntent *mntent;
  FILE *file;
  const char *read_file;
  GUnixMountEntry *mount_entry;
  GHashTable *mounts_hash;
  GList *return_list;
  
  read_file = get_mtab_read_file ();

  file = setmntent (read_file, "r");
  if (file == NULL)
    return NULL;

  return_list = NULL;
  
  mounts_hash = g_hash_table_new (g_str_hash, g_str_equal);
  
#ifdef HAVE_GETMNTENT_R
  while ((mntent = getmntent_r (file, &ent, buf, sizeof (buf))) != NULL)
#else
  G_LOCK (getmntent);
  while ((mntent = getmntent (file)) != NULL)
#endif
    {
      const char *device_path = NULL;
      gboolean is_read_only = FALSE;

      /* ignore any mnt_fsname that is repeated and begins with a '/'
       *
       * We do this to avoid being fooled by --bind mounts, since
       * these have the same device as the location they bind to.
       * It's not an ideal solution to the problem, but it's likely that
       * the most important mountpoint is first and the --bind ones after
       * that aren't as important. So it should work.
       *
       * The '/' is to handle procfs, tmpfs and other no device mounts.
       */
      if (mntent->mnt_fsname != NULL &&
	  mntent->mnt_fsname[0] == '/' &&
	  g_hash_table_lookup (mounts_hash, mntent->mnt_fsname))
        continue;

      if (g_strcmp0 (mntent->mnt_fsname, "/dev/root") == 0)
        device_path = _resolve_dev_root ();
      else
        device_path = mntent->mnt_fsname;

#if defined (HAVE_HASMNTOPT)
      if (hasmntopt (mntent, MNTOPT_RO) != NULL)
	is_read_only = TRUE;
#endif

      mount_entry = create_unix_mount_entry (device_path,
                                             mntent->mnt_dir,
                                             mntent->mnt_type,
                                             is_read_only);

      g_hash_table_insert (mounts_hash,
			   mount_entry->device_path,
			   mount_entry->device_path);

      return_list = g_list_prepend (return_list, mount_entry);
    }
  g_hash_table_destroy (mounts_hash);
  
  endmntent (file);

#ifndef HAVE_GETMNTENT_R
  G_UNLOCK (getmntent);
#endif
  
  return g_list_reverse (return_list);
}

#endif /* HAVE_LIBMOUNT */

static const char *
get_mtab_monitor_file (void)
{
  static const char *mountinfo_path = NULL;
#ifdef HAVE_LIBMOUNT
  struct stat buf;
#endif

  if (mountinfo_path != NULL)
    return mountinfo_path;

#ifdef HAVE_LIBMOUNT
  /* The mtab file is still used by some distros, so it has to be monitored in
   * order to avoid races between g_unix_mounts_get and "mounts-changed" signal:
   * https://bugzilla.gnome.org/show_bug.cgi?id=782814
   */
  if (mnt_has_regular_mtab (&mountinfo_path, NULL))
    {
      return mountinfo_path;
    }

  if (stat (PROC_MOUNTINFO_PATH, &buf) == 0)
    {
      mountinfo_path = PROC_MOUNTINFO_PATH;
      return mountinfo_path;
    }
#endif

#ifdef _PATH_MOUNTED
# ifdef __linux__
  mountinfo_path = "/proc/mounts";
# else
  mountinfo_path = _PATH_MOUNTED;
# endif
#else
  mountinfo_path = "/etc/mtab";
#endif

  return mountinfo_path;
}

/* mnttab.h {{{2 */
#elif defined (HAVE_SYS_MNTTAB_H)

G_LOCK_DEFINE_STATIC(getmntent);

static const char *
get_mtab_read_file (void)
{
#ifdef _PATH_MOUNTED
  return _PATH_MOUNTED;
#else	
  return "/etc/mnttab";
#endif
}

static const char *
get_mtab_monitor_file (void)
{
  return get_mtab_read_file ();
}

static GList *
_g_get_unix_mounts (void)
{
  struct mnttab mntent;
  FILE *file;
  const char *read_file;
  GUnixMountEntry *mount_entry;
  GList *return_list;
  
  read_file = get_mtab_read_file ();
  
  file = setmntent (read_file, "r");
  if (file == NULL)
    return NULL;
  
  return_list = NULL;
  
  G_LOCK (getmntent);
  while (! getmntent (file, &mntent))
    {
      gboolean is_read_only = FALSE;

#if defined (HAVE_HASMNTOPT)
      if (hasmntopt (&mntent, MNTOPT_RO) != NULL)
	is_read_only = TRUE;
#endif

      mount_entry = create_unix_mount_entry (mntent.mnt_special,
                                             mntent.mnt_mountp,
                                             mntent.mnt_fstype,
                                             is_read_only);

      return_list = g_list_prepend (return_list, mount_entry);
    }
  
  endmntent (file);
  
  G_UNLOCK (getmntent);
  
  return g_list_reverse (return_list);
}

/* mntctl.h (AIX) {{{2 */
#elif defined(HAVE_SYS_MNTCTL_H) && defined(HAVE_SYS_VMOUNT_H) && defined(HAVE_SYS_VFS_H)

static const char *
get_mtab_monitor_file (void)
{
  return NULL;
}

static GList *
_g_get_unix_mounts (void)
{
  struct vfs_ent *fs_info;
  struct vmount *vmount_info;
  int vmount_number;
  unsigned int vmount_size;
  int current;
  GList *return_list;
  
  if (mntctl (MCTL_QUERY, sizeof (vmount_size), &vmount_size) != 0)
    {
      g_warning ("Unable to know the number of mounted volumes\n");
      
      return NULL;
    }

  vmount_info = (struct vmount*)g_malloc (vmount_size);

  vmount_number = mntctl (MCTL_QUERY, vmount_size, vmount_info);
  
  if (vmount_info->vmt_revision != VMT_REVISION)
    g_warning ("Bad vmount structure revision number, want %d, got %d\n", VMT_REVISION, vmount_info->vmt_revision);

  if (vmount_number < 0)
    {
      g_warning ("Unable to recover mounted volumes information\n");
      
      g_free (vmount_info);
      return NULL;
    }
  
  return_list = NULL;
  while (vmount_number > 0)
    {
      gboolean is_read_only = FALSE;

      fs_info = getvfsbytype (vmount_info->vmt_gfstype);

      /* is_removable = (vmount_info->vmt_flags & MNT_REMOVABLE) ? 1 : 0; */
      is_read_only = (vmount_info->vmt_flags & MNT_READONLY) ? 1 : 0;

      mount_entry = create_unix_mount_entry (vmt2dataptr (vmount_info, VMT_OBJECT),
                                             vmt2dataptr (vmount_info, VMT_STUB),
                                             fs_info == NULL ? "unknown" : fs_info->vfsent_name,
                                             is_read_only);

      return_list = g_list_prepend (return_list, mount_entry);
      
      vmount_info = (struct vmount *)( (char*)vmount_info 
				       + vmount_info->vmt_length);
      vmount_number--;
    }
  
  g_free (vmount_info);
  
  return g_list_reverse (return_list);
}

/* sys/mount.h {{{2 */
#elif (defined(HAVE_GETVFSSTAT) || defined(HAVE_GETFSSTAT)) && defined(HAVE_FSTAB_H) && defined(HAVE_SYS_MOUNT_H)

static const char *
get_mtab_monitor_file (void)
{
  return NULL;
}

static GList *
_g_get_unix_mounts (void)
{
#if defined(USE_STATVFS)
  struct statvfs *mntent = NULL;
#elif defined(USE_STATFS)
  struct statfs *mntent = NULL;
#else
  #error statfs juggling failed
#endif
  size_t bufsize;
  int num_mounts, i;
  GUnixMountEntry *mount_entry;
  GList *return_list;
  
  /* Pass NOWAIT to avoid blocking trying to update NFS mounts. */
#if defined(USE_STATVFS) && defined(HAVE_GETVFSSTAT)
  num_mounts = getvfsstat (NULL, 0, ST_NOWAIT);
#elif defined(USE_STATFS) && defined(HAVE_GETFSSTAT)
  num_mounts = getfsstat (NULL, 0, MNT_NOWAIT);
#endif
  if (num_mounts == -1)
    return NULL;

  bufsize = num_mounts * sizeof (*mntent);
  mntent = g_malloc (bufsize);
#if defined(USE_STATVFS) && defined(HAVE_GETVFSSTAT)
  num_mounts = getvfsstat (mntent, bufsize, ST_NOWAIT);
#elif defined(USE_STATFS) && defined(HAVE_GETFSSTAT)
  num_mounts = getfsstat (mntent, bufsize, MNT_NOWAIT);
#endif
  if (num_mounts == -1)
    return NULL;
  
  return_list = NULL;
  
  for (i = 0; i < num_mounts; i++)
    {
      gboolean is_read_only = FALSE;

#if defined(USE_STATVFS)
      if (mntent[i].f_flag & ST_RDONLY)
#elif defined(USE_STATFS)
      if (mntent[i].f_flags & MNT_RDONLY)
#else
      #error statfs juggling failed
#endif
        is_read_only = TRUE;

      mount_entry = create_unix_mount_entry (mntent[i].f_mntfromname,
                                             mntent[i].f_mntonname,
                                             mntent[i].f_fstypename,
                                             is_read_only);

      return_list = g_list_prepend (return_list, mount_entry);
    }

  g_free (mntent);
  
  return g_list_reverse (return_list);
}

/* Interix {{{2 */
#elif defined(__INTERIX)

static const char *
get_mtab_monitor_file (void)
{
  return NULL;
}

static GList *
_g_get_unix_mounts (void)
{
  DIR *dirp;
  GList* return_list = NULL;
  char filename[9 + NAME_MAX];

  dirp = opendir ("/dev/fs");
  if (!dirp)
    {
      g_warning ("unable to read /dev/fs!");
      return NULL;
    }

  while (1)
    {
      struct statvfs statbuf;
      struct dirent entry;
      struct dirent* result;
      
      if (readdir_r (dirp, &entry, &result) || result == NULL)
        break;
      
      strcpy (filename, "/dev/fs/");
      strcat (filename, entry.d_name);
      
      if (statvfs (filename, &statbuf) == 0)
        {
          GUnixMountEntry* mount_entry = g_new0(GUnixMountEntry, 1);
          
          mount_entry->mount_path = g_strdup (statbuf.f_mntonname);
          mount_entry->device_path = g_strdup (statbuf.f_mntfromname);
          mount_entry->filesystem_type = g_strdup (statbuf.f_fstypename);
          
          if (statbuf.f_flag & ST_RDONLY)
            mount_entry->is_read_only = TRUE;
          
          return_list = g_list_prepend(return_list, mount_entry);
        }
    }
  
  return_list = g_list_reverse (return_list);
  
  closedir (dirp);

  return return_list;
}

/* Common code {{{2 */
#else
#error No _g_get_unix_mounts() implementation for system
#endif

/* GUnixMountPoints (ie: fstab) implementations {{{1 */

/* _g_get_unix_mount_points():
 * read the fstab.
 * don't return swap and ignore mounts.
 */

static char *
get_fstab_file (void)
{
#ifdef HAVE_LIBMOUNT
  return (char *) mnt_get_fstab_path ();
#else
#if defined(HAVE_SYS_MNTCTL_H) && defined(HAVE_SYS_VMOUNT_H) && defined(HAVE_SYS_VFS_H)
  /* AIX */
  return "/etc/filesystems";
#elif defined(_PATH_MNTTAB)
  return _PATH_MNTTAB;
#elif defined(VFSTAB)
  return VFSTAB;
#else
  return "/etc/fstab";
#endif
#endif
}

/* mntent.h (Linux, GNU, NSS) {{{2 */
#ifdef HAVE_MNTENT_H

#ifdef HAVE_LIBMOUNT

static GList *
_g_get_unix_mount_points (void)
{
  struct libmnt_table *table = NULL;
  struct libmnt_iter* iter = NULL;
  struct libmnt_fs *fs = NULL;
  GUnixMountPoint *mount_point = NULL;
  GList *return_list = NULL;

  table = mnt_new_table ();
  if (mnt_table_parse_fstab (table, NULL) < 0)
    goto out;

  iter = mnt_new_iter (MNT_ITER_FORWARD);
  while (mnt_table_next_fs (table, iter, &fs) == 0)
    {
      const char *device_path = NULL;
      const char *mount_path = NULL;
      const char *mount_fstype = NULL;
      char *mount_options = NULL;
      gboolean is_read_only = FALSE;
      gboolean is_user_mountable = FALSE;
      gboolean is_loopback = FALSE;

      mount_path = mnt_fs_get_target (fs);
      if ((strcmp (mount_path, "ignore") == 0) ||
          (strcmp (mount_path, "swap") == 0) ||
          (strcmp (mount_path, "none") == 0))
        continue;

      mount_fstype = mnt_fs_get_fstype (fs);
      mount_options = mnt_fs_strdup_options (fs);
      if (mount_options)
        {
          unsigned long mount_flags = 0;
          unsigned long userspace_flags = 0;

          mnt_optstr_get_flags (mount_options, &mount_flags, mnt_get_builtin_optmap (MNT_LINUX_MAP));
          mnt_optstr_get_flags (mount_options, &userspace_flags, mnt_get_builtin_optmap (MNT_USERSPACE_MAP));

          /* We ignore bind fstab entries, as we ignore bind mounts anyway */
          if (mount_flags & MS_BIND)
            {
              g_free (mount_options);
              continue;
            }

          is_read_only = (mount_flags & MS_RDONLY) != 0;
          is_loopback = (userspace_flags & MNT_MS_LOOP) != 0;

          if ((mount_fstype != NULL && g_strcmp0 ("supermount", mount_fstype) == 0) ||
              ((userspace_flags & MNT_MS_USER) &&
               (g_strstr_len (mount_options, -1, "user_xattr") == NULL)) ||
              (g_strstr_len (mount_options, -1, "pamconsole") == NULL) ||
              (userspace_flags & MNT_MS_USERS) ||
              (userspace_flags & MNT_MS_OWNER))
            {
              is_user_mountable = TRUE;
            }
        }

      device_path = mnt_fs_get_source (fs);
      if (g_strcmp0 (device_path, "/dev/root") == 0)
        device_path = _resolve_dev_root ();

      mount_point = create_unix_mount_point (device_path,
                                             mount_path,
                                             mount_fstype,
                                             mount_options,
                                             is_read_only,
                                             is_user_mountable,
                                             is_loopback);
      if (mount_options)
        g_free (mount_options);

      return_list = g_list_prepend (return_list, mount_point);
    }
  mnt_free_iter (iter);

 out:
  mnt_free_table (table);

  return g_list_reverse (return_list);
}

#else

static GList *
_g_get_unix_mount_points (void)
{
#ifdef HAVE_GETMNTENT_R
  struct mntent ent;
  char buf[1024];
#endif
  struct mntent *mntent;
  FILE *file;
  char *read_file;
  GUnixMountPoint *mount_point;
  GList *return_list;
  
  read_file = get_fstab_file ();
  
  file = setmntent (read_file, "r");
  if (file == NULL)
    return NULL;

  return_list = NULL;
  
#ifdef HAVE_GETMNTENT_R
  while ((mntent = getmntent_r (file, &ent, buf, sizeof (buf))) != NULL)
#else
  G_LOCK (getmntent);
  while ((mntent = getmntent (file)) != NULL)
#endif
    {
      const char *device_path = NULL;
      gboolean is_read_only = FALSE;
      gboolean is_user_mountable = FALSE;
      gboolean is_loopback = FALSE;

      if ((strcmp (mntent->mnt_dir, "ignore") == 0) ||
          (strcmp (mntent->mnt_dir, "swap") == 0) ||
          (strcmp (mntent->mnt_dir, "none") == 0))
	continue;

#ifdef HAVE_HASMNTOPT
      /* We ignore bind fstab entries, as we ignore bind mounts anyway */
      if (hasmntopt (mntent, "bind"))
        continue;
#endif

      if (strcmp (mntent->mnt_fsname, "/dev/root") == 0)
        device_path = _resolve_dev_root ();
      else
        device_path = mntent->mnt_fsname;

#ifdef HAVE_HASMNTOPT
      if (hasmntopt (mntent, MNTOPT_RO) != NULL)
	is_read_only = TRUE;

      if (hasmntopt (mntent, "loop") != NULL)
	is_loopback = TRUE;

#endif

      if ((mntent->mnt_type != NULL && strcmp ("supermount", mntent->mnt_type) == 0)
#ifdef HAVE_HASMNTOPT
	  || (hasmntopt (mntent, "user") != NULL
	      && hasmntopt (mntent, "user") != hasmntopt (mntent, "user_xattr"))
	  || hasmntopt (mntent, "pamconsole") != NULL
	  || hasmntopt (mntent, "users") != NULL
	  || hasmntopt (mntent, "owner") != NULL
#endif
	  )
	is_user_mountable = TRUE;

      mount_point = create_unix_mount_point (device_path,
                                             mntent->mnt_dir,
                                             mntent->mnt_type,
                                             mntent->mnt_opts,
                                             is_read_only,
                                             is_user_mountable,
                                             is_loopback);

      return_list = g_list_prepend (return_list, mount_point);
    }
  
  endmntent (file);

#ifndef HAVE_GETMNTENT_R
  G_UNLOCK (getmntent);
#endif
  
  return g_list_reverse (return_list);
}

#endif /* HAVE_LIBMOUNT */

/* mnttab.h {{{2 */
#elif defined (HAVE_SYS_MNTTAB_H)

static GList *
_g_get_unix_mount_points (void)
{
  struct mnttab mntent;
  FILE *file;
  char *read_file;
  GUnixMountPoint *mount_point;
  GList *return_list;
  
  read_file = get_fstab_file ();
  
  file = setmntent (read_file, "r");
  if (file == NULL)
    return NULL;

  return_list = NULL;
  
  G_LOCK (getmntent);
  while (! getmntent (file, &mntent))
    {
      gboolean is_read_only = FALSE;
      gboolean is_user_mountable = FALSE;
      gboolean is_loopback = FALSE;

      if ((strcmp (mntent.mnt_mountp, "ignore") == 0) ||
          (strcmp (mntent.mnt_mountp, "swap") == 0) ||
          (strcmp (mntent.mnt_mountp, "none") == 0))
	continue;

#ifdef HAVE_HASMNTOPT
      if (hasmntopt (&mntent, MNTOPT_RO) != NULL)
	is_read_only = TRUE;

      if (hasmntopt (&mntent, "lofs") != NULL)
	is_loopback = TRUE;
#endif

      if ((mntent.mnt_fstype != NULL)
#ifdef HAVE_HASMNTOPT
	  || (hasmntopt (&mntent, "user") != NULL
	      && hasmntopt (&mntent, "user") != hasmntopt (&mntent, "user_xattr"))
	  || hasmntopt (&mntent, "pamconsole") != NULL
	  || hasmntopt (&mntent, "users") != NULL
	  || hasmntopt (&mntent, "owner") != NULL
#endif
	  )
	is_user_mountable = TRUE;

      mount_point = create_unix_mount_point (mntent.mnt_special,
                                             mntent.mnt_mountp,
                                             mntent.mnt_fstype,
                                             mntent.mnt_mntopts,
                                             is_read_only,
                                             is_user_mountable,
                                             is_loopback);

      return_list = g_list_prepend (return_list, mount_point);
    }
  
  endmntent (file);
  G_UNLOCK (getmntent);
  
  return g_list_reverse (return_list);
}

/* mntctl.h (AIX) {{{2 */
#elif defined(HAVE_SYS_MNTCTL_H) && defined(HAVE_SYS_VMOUNT_H) && defined(HAVE_SYS_VFS_H)

/* functions to parse /etc/filesystems on aix */

/* read character, ignoring comments (begin with '*', end with '\n' */
static int
aix_fs_getc (FILE *fd)
{
  int c;
  
  while ((c = getc (fd)) == '*')
    {
      while (((c = getc (fd)) != '\n') && (c != EOF))
	;
    }
}

/* eat all continuous spaces in a file */
static int
aix_fs_ignorespace (FILE *fd)
{
  int c;
  
  while ((c = aix_fs_getc (fd)) != EOF)
    {
      if (!g_ascii_isspace (c))
	{
	  ungetc (c,fd);
	  return c;
	}
    }
  
  return EOF;
}

/* read one word from file */
static int
aix_fs_getword (FILE *fd, 
                char *word)
{
  int c;
  
  aix_fs_ignorespace (fd);

  while (((c = aix_fs_getc (fd)) != EOF) && !g_ascii_isspace (c))
    {
      if (c == '"')
	{
	  while (((c = aix_fs_getc (fd)) != EOF) && (c != '"'))
	    *word++ = c;
	  else
	    *word++ = c;
	}
    }
  *word = 0;
  
  return c;
}

typedef struct {
  char mnt_mount[PATH_MAX];
  char mnt_special[PATH_MAX];
  char mnt_fstype[16];
  char mnt_options[128];
} AixMountTableEntry;

/* read mount points properties */
static int
aix_fs_get (FILE               *fd, 
            AixMountTableEntry *prop)
{
  static char word[PATH_MAX] = { 0 };
  char value[PATH_MAX];
  
  /* read stanza */
  if (word[0] == 0)
    {
      if (aix_fs_getword (fd, word) == EOF)
	return EOF;
    }

  word[strlen(word) - 1] = 0;
  strcpy (prop->mnt_mount, word);
  
  /* read attributes and value */
  
  while (aix_fs_getword (fd, word) != EOF)
    {
      /* test if is attribute or new stanza */
      if (word[strlen(word) - 1] == ':')
	return 0;
      
      /* read "=" */
      aix_fs_getword (fd, value);
      
      /* read value */
      aix_fs_getword (fd, value);
      
      if (strcmp (word, "dev") == 0)
	strcpy (prop->mnt_special, value);
      else if (strcmp (word, "vfs") == 0)
	strcpy (prop->mnt_fstype, value);
      else if (strcmp (word, "options") == 0)
	strcpy(prop->mnt_options, value);
    }
  
  return 0;
}

static GList *
_g_get_unix_mount_points (void)
{
  struct mntent *mntent;
  FILE *file;
  char *read_file;
  GUnixMountPoint *mount_point;
  AixMountTableEntry mntent;
  GList *return_list;
  
  read_file = get_fstab_file ();
  
  file = setmntent (read_file, "r");
  if (file == NULL)
    return NULL;
  
  return_list = NULL;
  
  while (!aix_fs_get (file, &mntent))
    {
      if (strcmp ("cdrfs", mntent.mnt_fstype) == 0)
	{
          mount_point = create_unix_mount_point (mntent.mnt_special,
                                                 mntent.mnt_mount,
                                                 mntent.mnt_fstype,
                                                 mntent.mnt_options,
                                                 TRUE,
                                                 TRUE,
                                                 FALSE);

	  return_list = g_list_prepend (return_list, mount_point);
	}
    }
	
  endmntent (file);
  
  return g_list_reverse (return_list);
}

#elif (defined(HAVE_GETVFSSTAT) || defined(HAVE_GETFSSTAT)) && defined(HAVE_FSTAB_H) && defined(HAVE_SYS_MOUNT_H)

static GList *
_g_get_unix_mount_points (void)
{
  struct fstab *fstab = NULL;
  GUnixMountPoint *mount_point;
  GList *return_list;
#ifdef HAVE_SYS_SYSCTL_H
  int usermnt = 0;
  struct stat sb;
#endif
  
  if (!setfsent ())
    return NULL;

  return_list = NULL;
  
#ifdef HAVE_SYS_SYSCTL_H
#if defined(HAVE_SYSCTLBYNAME)
  {
    size_t len = sizeof(usermnt);

    sysctlbyname ("vfs.usermount", &usermnt, &len, NULL, 0);
  }
#elif defined(CTL_VFS) && defined(VFS_USERMOUNT)
  {
    int mib[2];
    size_t len = sizeof(usermnt);
    
    mib[0] = CTL_VFS;
    mib[1] = VFS_USERMOUNT;
    sysctl (mib, 2, &usermnt, &len, NULL, 0);
  }
#elif defined(CTL_KERN) && defined(KERN_USERMOUNT)
  {
    int mib[2];
    size_t len = sizeof(usermnt);
    
    mib[0] = CTL_KERN;
    mib[1] = KERN_USERMOUNT;
    sysctl (mib, 2, &usermnt, &len, NULL, 0);
  }
#endif
#endif
  
  while ((fstab = getfsent ()) != NULL)
    {
      gboolean is_read_only = FALSE;
      gboolean is_user_mountable = FALSE;

      if (strcmp (fstab->fs_vfstype, "swap") == 0)
	continue;

      if (strcmp (fstab->fs_type, "ro") == 0)
	is_read_only = TRUE;

#ifdef HAVE_SYS_SYSCTL_H
      if (usermnt != 0)
	{
	  uid_t uid = getuid ();
	  if (stat (fstab->fs_file, &sb) == 0)
	    {
	      if (uid == 0 || sb.st_uid == uid)
		is_user_mountable = TRUE;
	    }
	}
#endif

      mount_point = create_unix_mount_point (fstab->fs_spec,
                                             fstab->fs_file,
                                             fstab->fs_vfstype,
                                             fstab->fs_mntops,
                                             is_read_only,
                                             is_user_mountable,
                                             FALSE);

      return_list = g_list_prepend (return_list, mount_point);
    }
  
  endfsent ();
  
  return g_list_reverse (return_list);
}
/* Interix {{{2 */
#elif defined(__INTERIX)
static GList *
_g_get_unix_mount_points (void)
{
  return _g_get_unix_mounts ();
}

/* Common code {{{2 */
#else
#error No g_get_mount_table() implementation for system
#endif

static guint64
get_mounts_timestamp (void)
{
  const char *monitor_file;
  struct stat buf;

  monitor_file = get_mtab_monitor_file ();
  /* Don't return mtime for /proc/ files */
  if (monitor_file && !g_str_has_prefix (monitor_file, "/proc/"))
    {
      if (stat (monitor_file, &buf) == 0)
        return (guint64)buf.st_mtime;
    }
  else if (proc_mounts_watch_is_running ())
    {
      /* it's being monitored by poll, so return mount_poller_time */
      return mount_poller_time;
    }
  else
    {
      /* Case of /proc/ file not being monitored - Be on the safe side and
       * send a new timestamp to force g_unix_mounts_changed_since() to
       * return TRUE so any application caches depending on it (like eg.
       * the one in GIO) get invalidated and don't hold possibly outdated
       * data - see Bug 787731 */
     return (guint64) g_get_monotonic_time ();
    }
  return 0;
}

static guint64
get_mount_points_timestamp (void)
{
  const char *monitor_file;
  struct stat buf;

  monitor_file = get_fstab_file ();
  if (monitor_file)
    {
      if (stat (monitor_file, &buf) == 0)
        return (guint64)buf.st_mtime;
    }
  return 0;
}

/**
 * g_unix_mounts_get:
 * @time_read: (out) (optional): guint64 to contain a timestamp, or %NULL
 *
 * Gets a #GList of #GUnixMountEntry containing the unix mounts.
 * If @time_read is set, it will be filled with the mount
 * timestamp, allowing for checking if the mounts have changed
 * with g_unix_mounts_changed_since().
 *
 * Returns: (element-type GUnixMountEntry) (transfer full):
 *     a #GList of the UNIX mounts.
 **/
GList *
g_unix_mounts_get (guint64 *time_read)
{
  if (time_read)
    *time_read = get_mounts_timestamp ();

  return _g_get_unix_mounts ();
}

/**
 * g_unix_mount_at:
 * @mount_path: (type filename): path for a possible unix mount.
 * @time_read: (out) (optional): guint64 to contain a timestamp.
 * 
 * Gets a #GUnixMountEntry for a given mount path. If @time_read
 * is set, it will be filled with a unix timestamp for checking
 * if the mounts have changed since with g_unix_mounts_changed_since().
 * 
 * Returns: (transfer full): a #GUnixMountEntry.
 **/
GUnixMountEntry *
g_unix_mount_at (const char *mount_path,
		 guint64    *time_read)
{
  GList *mounts, *l;
  GUnixMountEntry *mount_entry, *found;
  
  mounts = g_unix_mounts_get (time_read);

  found = NULL;
  for (l = mounts; l != NULL; l = l->next)
    {
      mount_entry = l->data;

      if (!found && strcmp (mount_path, mount_entry->mount_path) == 0)
        found = mount_entry;
      else
        g_unix_mount_free (mount_entry);
    }
  g_list_free (mounts);

  return found;
}

/**
 * g_unix_mount_for:
 * @file_path: (type filename): file path on some unix mount.
 * @time_read: (out) (optional): guint64 to contain a timestamp.
 *
 * Gets a #GUnixMountEntry for a given file path. If @time_read
 * is set, it will be filled with a unix timestamp for checking
 * if the mounts have changed since with g_unix_mounts_changed_since().
 *
 * Returns: (transfer full): a #GUnixMountEntry.
 *
 * Since: 2.52
 **/
GUnixMountEntry *
g_unix_mount_for (const char *file_path,
                  guint64    *time_read)
{
  GUnixMountEntry *entry;

  g_return_val_if_fail (file_path != NULL, NULL);

  entry = g_unix_mount_at (file_path, time_read);
  if (entry == NULL)
    {
      char *topdir;

      topdir = _g_local_file_find_topdir_for (file_path);
      if (topdir != NULL)
        {
          entry = g_unix_mount_at (topdir, time_read);
          g_free (topdir);
        }
    }

  return entry;
}

/**
 * g_unix_mount_points_get:
 * @time_read: (out) (optional): guint64 to contain a timestamp.
 *
 * Gets a #GList of #GUnixMountPoint containing the unix mount points.
 * If @time_read is set, it will be filled with the mount timestamp,
 * allowing for checking if the mounts have changed with
 * g_unix_mount_points_changed_since().
 *
 * Returns: (element-type GUnixMountPoint) (transfer full):
 *     a #GList of the UNIX mountpoints.
 **/
GList *
g_unix_mount_points_get (guint64 *time_read)
{
  if (time_read)
    *time_read = get_mount_points_timestamp ();

  return _g_get_unix_mount_points ();
}

/**
 * g_unix_mounts_changed_since:
 * @time: guint64 to contain a timestamp.
 * 
 * Checks if the unix mounts have changed since a given unix time.
 * 
 * Returns: %TRUE if the mounts have changed since @time. 
 **/
gboolean
g_unix_mounts_changed_since (guint64 time)
{
  return get_mounts_timestamp () != time;
}

/**
 * g_unix_mount_points_changed_since:
 * @time: guint64 to contain a timestamp.
 * 
 * Checks if the unix mount points have changed since a given unix time.
 * 
 * Returns: %TRUE if the mount points have changed since @time. 
 **/
gboolean
g_unix_mount_points_changed_since (guint64 time)
{
  return get_mount_points_timestamp () != time;
}

/* GUnixMountMonitor {{{1 */

enum {
  MOUNTS_CHANGED,
  MOUNTPOINTS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _GUnixMountMonitor {
  GObject parent;

  GMainContext *context;
};

struct _GUnixMountMonitorClass {
  GObjectClass parent_class;
};


G_DEFINE_TYPE (GUnixMountMonitor, g_unix_mount_monitor, G_TYPE_OBJECT)

static GContextSpecificGroup  mount_monitor_group;
static GFileMonitor          *fstab_monitor;
static GFileMonitor          *mtab_monitor;
static GSource               *proc_mounts_watch_source;
static GList                 *mount_poller_mounts;
static guint                  mtab_file_changed_id;

static gboolean
proc_mounts_watch_is_running (void)
{
  return proc_mounts_watch_source != NULL &&
         !g_source_is_destroyed (proc_mounts_watch_source);
}

static void
fstab_file_changed (GFileMonitor      *monitor,
                    GFile             *file,
                    GFile             *other_file,
                    GFileMonitorEvent  event_type,
                    gpointer           user_data)
{
  if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
      event_type != G_FILE_MONITOR_EVENT_CREATED &&
      event_type != G_FILE_MONITOR_EVENT_DELETED)
    return;

  g_context_specific_group_emit (&mount_monitor_group, signals[MOUNTPOINTS_CHANGED]);
}

static gboolean
mtab_file_changed_cb (gpointer user_data)
{
  mtab_file_changed_id = 0;
  g_context_specific_group_emit (&mount_monitor_group, signals[MOUNTS_CHANGED]);

  return G_SOURCE_REMOVE;
}

static void
mtab_file_changed (GFileMonitor      *monitor,
                   GFile             *file,
                   GFile             *other_file,
                   GFileMonitorEvent  event_type,
                   gpointer           user_data)
{
  if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
      event_type != G_FILE_MONITOR_EVENT_CREATED &&
      event_type != G_FILE_MONITOR_EVENT_DELETED)
    return;

  /* Skip accumulated events from file monitor which we are not able to handle
   * in a real time instead of emitting mounts_changed signal several times.
   * This should behave equally to GIOChannel based monitoring. See Bug 792235.
   */
  if (mtab_file_changed_id > 0)
    return;

  mtab_file_changed_id = g_idle_add (mtab_file_changed_cb, NULL);
}

static gboolean
proc_mounts_changed (GIOChannel   *channel,
                     GIOCondition  cond,
                     gpointer      user_data)
{
  if (cond & G_IO_ERR)
    {
      mount_poller_time = (guint64) g_get_monotonic_time ();
      g_context_specific_group_emit (&mount_monitor_group, signals[MOUNTS_CHANGED]);
    }

  return TRUE;
}

static gboolean
mount_change_poller (gpointer user_data)
{
  GList *current_mounts, *new_it, *old_it;
  gboolean has_changed = FALSE;

  current_mounts = _g_get_unix_mounts ();

  for ( new_it = current_mounts, old_it = mount_poller_mounts;
        new_it != NULL && old_it != NULL;
        new_it = g_list_next (new_it), old_it = g_list_next (old_it) )
    {
      if (g_unix_mount_compare (new_it->data, old_it->data) != 0)
        {
          has_changed = TRUE;
          break;
        }
    }
  if (!(new_it == NULL && old_it == NULL))
    has_changed = TRUE;

  g_list_free_full (mount_poller_mounts, (GDestroyNotify) g_unix_mount_free);

  mount_poller_mounts = current_mounts;

  if (has_changed)
    {
      mount_poller_time = (guint64) g_get_monotonic_time ();
      g_context_specific_group_emit (&mount_monitor_group, signals[MOUNTPOINTS_CHANGED]);
    }

  return TRUE;
}


static void
mount_monitor_stop (void)
{
  if (fstab_monitor)
    {
      g_file_monitor_cancel (fstab_monitor);
      g_object_unref (fstab_monitor);
    }

  if (proc_mounts_watch_source != NULL)
    {
      g_source_destroy (proc_mounts_watch_source);
      proc_mounts_watch_source = NULL;
    }

  if (mtab_monitor)
    {
      g_file_monitor_cancel (mtab_monitor);
      g_object_unref (mtab_monitor);
    }

  g_list_free_full (mount_poller_mounts, (GDestroyNotify) g_unix_mount_free);
}

static void
mount_monitor_start (void)
{
  GFile *file;

  if (get_fstab_file () != NULL)
    {
      file = g_file_new_for_path (get_fstab_file ());
      fstab_monitor = g_file_monitor_file (file, 0, NULL, NULL);
      g_object_unref (file);

      g_signal_connect (fstab_monitor, "changed", (GCallback)fstab_file_changed, NULL);
    }

  if (get_mtab_monitor_file () != NULL)
    {
      const gchar *mtab_path;

      mtab_path = get_mtab_monitor_file ();
      /* Monitoring files in /proc/ is special - can't just use GFileMonitor.
       * See 'man proc' for more details.
       */
      if (g_str_has_prefix (mtab_path, "/proc/"))
        {
          GIOChannel *proc_mounts_channel;
          GError *error = NULL;
          proc_mounts_channel = g_io_channel_new_file (mtab_path, "r", &error);
          if (proc_mounts_channel == NULL)
            {
              g_warning ("Error creating IO channel for %s: %s (%s, %d)", mtab_path,
                         error->message, g_quark_to_string (error->domain), error->code);
              g_error_free (error);
            }
          else
            {
              proc_mounts_watch_source = g_io_create_watch (proc_mounts_channel, G_IO_ERR);
              g_source_set_callback (proc_mounts_watch_source,
                                     (GSourceFunc) proc_mounts_changed,
                                     NULL, NULL);
              g_source_attach (proc_mounts_watch_source,
                               g_main_context_get_thread_default ());
              g_source_unref (proc_mounts_watch_source);
              g_io_channel_unref (proc_mounts_channel);
            }
        }
      else
        {
          file = g_file_new_for_path (mtab_path);
          mtab_monitor = g_file_monitor_file (file, 0, NULL, NULL);
          g_object_unref (file);
          g_signal_connect (mtab_monitor, "changed", (GCallback)mtab_file_changed, NULL);
        }
    }
  else
    {
      proc_mounts_watch_source = g_timeout_source_new_seconds (3);
      mount_poller_mounts = _g_get_unix_mounts ();
      mount_poller_time = (guint64)g_get_monotonic_time ();
      g_source_set_callback (proc_mounts_watch_source,
                             mount_change_poller,
                             NULL, NULL);
      g_source_attach (proc_mounts_watch_source,
                       g_main_context_get_thread_default ());
      g_source_unref (proc_mounts_watch_source);
    }
}

static void
g_unix_mount_monitor_finalize (GObject *object)
{
  GUnixMountMonitor *monitor;

  monitor = G_UNIX_MOUNT_MONITOR (object);

  g_context_specific_group_remove (&mount_monitor_group, monitor->context, monitor, mount_monitor_stop);

  G_OBJECT_CLASS (g_unix_mount_monitor_parent_class)->finalize (object);
}

static void
g_unix_mount_monitor_class_init (GUnixMountMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_unix_mount_monitor_finalize;
 
  /**
   * GUnixMountMonitor::mounts-changed:
   * @monitor: the object on which the signal is emitted
   * 
   * Emitted when the unix mounts have changed.
   */ 
  signals[MOUNTS_CHANGED] =
    g_signal_new (I_("mounts-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GUnixMountMonitor::mountpoints-changed:
   * @monitor: the object on which the signal is emitted
   * 
   * Emitted when the unix mount points have changed.
   */
  signals[MOUNTPOINTS_CHANGED] =
    g_signal_new (I_("mountpoints-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
g_unix_mount_monitor_init (GUnixMountMonitor *monitor)
{
}

/**
 * g_unix_mount_monitor_set_rate_limit:
 * @mount_monitor: a #GUnixMountMonitor
 * @limit_msec: a integer with the limit in milliseconds to
 *     poll for changes.
 *
 * This function does nothing.
 *
 * Before 2.44, this was a partially-effective way of controlling the
 * rate at which events would be reported under some uncommon
 * circumstances.  Since @mount_monitor is a singleton, it also meant
 * that calling this function would have side effects for other users of
 * the monitor.
 *
 * Since: 2.18
 *
 * Deprecated:2.44:This function does nothing.  Don't call it.
 */
void
g_unix_mount_monitor_set_rate_limit (GUnixMountMonitor *mount_monitor,
                                     gint               limit_msec)
{
}

/**
 * g_unix_mount_monitor_get:
 *
 * Gets the #GUnixMountMonitor for the current thread-default main
 * context.
 *
 * The mount monitor can be used to monitor for changes to the list of
 * mounted filesystems as well as the list of mount points (ie: fstab
 * entries).
 *
 * You must only call g_object_unref() on the return value from under
 * the same main context as you called this function.
 *
 * Returns: (transfer full): the #GUnixMountMonitor.
 *
 * Since: 2.44
 **/
GUnixMountMonitor *
g_unix_mount_monitor_get (void)
{
  return g_context_specific_group_get (&mount_monitor_group,
                                       G_TYPE_UNIX_MOUNT_MONITOR,
                                       G_STRUCT_OFFSET(GUnixMountMonitor, context),
                                       mount_monitor_start);
}

/**
 * g_unix_mount_monitor_new:
 *
 * Deprecated alias for g_unix_mount_monitor_get().
 *
 * This function was never a true constructor, which is why it was
 * renamed.
 *
 * Returns: a #GUnixMountMonitor.
 *
 * Deprecated:2.44:Use g_unix_mount_monitor_get() instead.
 */
GUnixMountMonitor *
g_unix_mount_monitor_new (void)
{
  return g_unix_mount_monitor_get ();
}

/* GUnixMount {{{1 */
/**
 * g_unix_mount_free:
 * @mount_entry: a #GUnixMountEntry.
 * 
 * Frees a unix mount.
 */
void
g_unix_mount_free (GUnixMountEntry *mount_entry)
{
  g_return_if_fail (mount_entry != NULL);

  g_free (mount_entry->mount_path);
  g_free (mount_entry->device_path);
  g_free (mount_entry->filesystem_type);
  g_free (mount_entry);
}

/**
 * g_unix_mount_copy:
 * @mount_entry: a #GUnixMountEntry.
 *
 * Makes a copy of @mount_entry.
 *
 * Returns: (transfer full): a new #GUnixMountEntry
 *
 * Since: 2.54
 */
GUnixMountEntry *
g_unix_mount_copy (GUnixMountEntry *mount_entry)
{
  GUnixMountEntry *copy;

  g_return_val_if_fail (mount_entry != NULL, NULL);

  copy = g_new0 (GUnixMountEntry, 1);
  copy->mount_path = g_strdup (mount_entry->mount_path);
  copy->device_path = g_strdup (mount_entry->device_path);
  copy->filesystem_type = g_strdup (mount_entry->filesystem_type);
  copy->is_read_only = mount_entry->is_read_only;
  copy->is_system_internal = mount_entry->is_system_internal;

  return copy;
}

/**
 * g_unix_mount_point_free:
 * @mount_point: unix mount point to free.
 * 
 * Frees a unix mount point.
 */
void
g_unix_mount_point_free (GUnixMountPoint *mount_point)
{
  g_return_if_fail (mount_point != NULL);

  g_free (mount_point->mount_path);
  g_free (mount_point->device_path);
  g_free (mount_point->filesystem_type);
  g_free (mount_point->options);
  g_free (mount_point);
}

/**
 * g_unix_mount_point_copy:
 * @mount_point: a #GUnixMountPoint.
 *
 * Makes a copy of @mount_point.
 *
 * Returns: (transfer full): a new #GUnixMountPoint
 *
 * Since: 2.54
 */
GUnixMountPoint*
g_unix_mount_point_copy (GUnixMountPoint *mount_point)
{
  GUnixMountPoint *copy;

  g_return_val_if_fail (mount_point != NULL, NULL);

  copy = g_new0 (GUnixMountPoint, 1);
  copy->mount_path = g_strdup (mount_point->mount_path);
  copy->device_path = g_strdup (mount_point->device_path);
  copy->filesystem_type = g_strdup (mount_point->filesystem_type);
  copy->options = g_strdup (mount_point->options);
  copy->is_read_only = mount_point->is_read_only;
  copy->is_user_mountable = mount_point->is_user_mountable;
  copy->is_loopback = mount_point->is_loopback;

  return copy;
}

/**
 * g_unix_mount_compare:
 * @mount1: first #GUnixMountEntry to compare.
 * @mount2: second #GUnixMountEntry to compare.
 * 
 * Compares two unix mounts.
 * 
 * Returns: 1, 0 or -1 if @mount1 is greater than, equal to,
 * or less than @mount2, respectively. 
 */
gint
g_unix_mount_compare (GUnixMountEntry *mount1,
		      GUnixMountEntry *mount2)
{
  int res;

  g_return_val_if_fail (mount1 != NULL && mount2 != NULL, 0);
  
  res = g_strcmp0 (mount1->mount_path, mount2->mount_path);
  if (res != 0)
    return res;
	
  res = g_strcmp0 (mount1->device_path, mount2->device_path);
  if (res != 0)
    return res;
	
  res = g_strcmp0 (mount1->filesystem_type, mount2->filesystem_type);
  if (res != 0)
    return res;

  res =  mount1->is_read_only - mount2->is_read_only;
  if (res != 0)
    return res;
  
  return 0;
}

/**
 * g_unix_mount_get_mount_path:
 * @mount_entry: input #GUnixMountEntry to get the mount path for.
 * 
 * Gets the mount path for a unix mount.
 * 
 * Returns: (type filename): the mount path for @mount_entry.
 */
const gchar *
g_unix_mount_get_mount_path (GUnixMountEntry *mount_entry)
{
  g_return_val_if_fail (mount_entry != NULL, NULL);

  return mount_entry->mount_path;
}

/**
 * g_unix_mount_get_device_path:
 * @mount_entry: a #GUnixMount.
 * 
 * Gets the device path for a unix mount.
 * 
 * Returns: (type filename): a string containing the device path.
 */
const gchar *
g_unix_mount_get_device_path (GUnixMountEntry *mount_entry)
{
  g_return_val_if_fail (mount_entry != NULL, NULL);

  return mount_entry->device_path;
}

/**
 * g_unix_mount_get_fs_type:
 * @mount_entry: a #GUnixMount.
 * 
 * Gets the filesystem type for the unix mount.
 * 
 * Returns: a string containing the file system type.
 */
const gchar *
g_unix_mount_get_fs_type (GUnixMountEntry *mount_entry)
{
  g_return_val_if_fail (mount_entry != NULL, NULL);

  return mount_entry->filesystem_type;
}

/**
 * g_unix_mount_is_readonly:
 * @mount_entry: a #GUnixMount.
 * 
 * Checks if a unix mount is mounted read only.
 * 
 * Returns: %TRUE if @mount_entry is read only.
 */
gboolean
g_unix_mount_is_readonly (GUnixMountEntry *mount_entry)
{
  g_return_val_if_fail (mount_entry != NULL, FALSE);

  return mount_entry->is_read_only;
}

/**
 * g_unix_mount_is_system_internal:
 * @mount_entry: a #GUnixMount.
 *
 * Checks if a Unix mount is a system mount. This is the Boolean OR of
 * g_unix_is_system_fs_type(), g_unix_is_system_device_path() and
 * g_unix_is_mount_path_system_internal() on @mount_entry’s properties.
 * 
 * The definition of what a ‘system’ mount entry is may change over time as new
 * file system types and device paths are ignored.
 *
 * Returns: %TRUE if the unix mount is for a system path.
 */
gboolean
g_unix_mount_is_system_internal (GUnixMountEntry *mount_entry)
{
  g_return_val_if_fail (mount_entry != NULL, FALSE);

  return mount_entry->is_system_internal;
}

/* GUnixMountPoint {{{1 */
/**
 * g_unix_mount_point_compare:
 * @mount1: a #GUnixMount.
 * @mount2: a #GUnixMount.
 * 
 * Compares two unix mount points.
 * 
 * Returns: 1, 0 or -1 if @mount1 is greater than, equal to,
 * or less than @mount2, respectively.
 */
gint
g_unix_mount_point_compare (GUnixMountPoint *mount1,
			    GUnixMountPoint *mount2)
{
  int res;

  g_return_val_if_fail (mount1 != NULL && mount2 != NULL, 0);

  res = g_strcmp0 (mount1->mount_path, mount2->mount_path);
  if (res != 0) 
    return res;
	
  res = g_strcmp0 (mount1->device_path, mount2->device_path);
  if (res != 0) 
    return res;
	
  res = g_strcmp0 (mount1->filesystem_type, mount2->filesystem_type);
  if (res != 0) 
    return res;

  res = g_strcmp0 (mount1->options, mount2->options);
  if (res != 0) 
    return res;

  res =  mount1->is_read_only - mount2->is_read_only;
  if (res != 0) 
    return res;

  res = mount1->is_user_mountable - mount2->is_user_mountable;
  if (res != 0) 
    return res;

  res = mount1->is_loopback - mount2->is_loopback;
  if (res != 0)
    return res;
  
  return 0;
}

/**
 * g_unix_mount_point_get_mount_path:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Gets the mount path for a unix mount point.
 * 
 * Returns: (type filename): a string containing the mount path.
 */
const gchar *
g_unix_mount_point_get_mount_path (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, NULL);

  return mount_point->mount_path;
}

/**
 * g_unix_mount_point_get_device_path:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Gets the device path for a unix mount point.
 * 
 * Returns: (type filename): a string containing the device path.
 */
const gchar *
g_unix_mount_point_get_device_path (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, NULL);

  return mount_point->device_path;
}

/**
 * g_unix_mount_point_get_fs_type:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Gets the file system type for the mount point.
 * 
 * Returns: a string containing the file system type.
 */
const gchar *
g_unix_mount_point_get_fs_type (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, NULL);

  return mount_point->filesystem_type;
}

/**
 * g_unix_mount_point_get_options:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Gets the options for the mount point.
 * 
 * Returns: a string containing the options.
 *
 * Since: 2.32
 */
const gchar *
g_unix_mount_point_get_options (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, NULL);

  return mount_point->options;
}

/**
 * g_unix_mount_point_is_readonly:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Checks if a unix mount point is read only.
 * 
 * Returns: %TRUE if a mount point is read only.
 */
gboolean
g_unix_mount_point_is_readonly (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, FALSE);

  return mount_point->is_read_only;
}

/**
 * g_unix_mount_point_is_user_mountable:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Checks if a unix mount point is mountable by the user.
 * 
 * Returns: %TRUE if the mount point is user mountable.
 */
gboolean
g_unix_mount_point_is_user_mountable (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, FALSE);

  return mount_point->is_user_mountable;
}

/**
 * g_unix_mount_point_is_loopback:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Checks if a unix mount point is a loopback device.
 * 
 * Returns: %TRUE if the mount point is a loopback. %FALSE otherwise. 
 */
gboolean
g_unix_mount_point_is_loopback (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, FALSE);

  return mount_point->is_loopback;
}

static GUnixMountType
guess_mount_type (const char *mount_path,
		  const char *device_path,
		  const char *filesystem_type)
{
  GUnixMountType type;
  char *basename;

  type = G_UNIX_MOUNT_TYPE_UNKNOWN;
  
  if ((strcmp (filesystem_type, "udf") == 0) ||
      (strcmp (filesystem_type, "iso9660") == 0) ||
      (strcmp (filesystem_type, "cd9660") == 0))
    type = G_UNIX_MOUNT_TYPE_CDROM;
  else if ((strcmp (filesystem_type, "nfs") == 0) ||
           (strcmp (filesystem_type, "nfs4") == 0))
    type = G_UNIX_MOUNT_TYPE_NFS;
  else if (g_str_has_prefix (device_path, "/vol/dev/diskette/") ||
	   g_str_has_prefix (device_path, "/dev/fd") ||
	   g_str_has_prefix (device_path, "/dev/floppy"))
    type = G_UNIX_MOUNT_TYPE_FLOPPY;
  else if (g_str_has_prefix (device_path, "/dev/cdrom") ||
	   g_str_has_prefix (device_path, "/dev/acd") ||
	   g_str_has_prefix (device_path, "/dev/cd"))
    type = G_UNIX_MOUNT_TYPE_CDROM;
  else if (g_str_has_prefix (device_path, "/vol/"))
    {
      const char *name = mount_path + strlen ("/");
      
      if (g_str_has_prefix (name, "cdrom"))
	type = G_UNIX_MOUNT_TYPE_CDROM;
      else if (g_str_has_prefix (name, "floppy") ||
	       g_str_has_prefix (device_path, "/vol/dev/diskette/")) 
	type = G_UNIX_MOUNT_TYPE_FLOPPY;
      else if (g_str_has_prefix (name, "rmdisk")) 
	type = G_UNIX_MOUNT_TYPE_ZIP;
      else if (g_str_has_prefix (name, "jaz"))
	type = G_UNIX_MOUNT_TYPE_JAZ;
      else if (g_str_has_prefix (name, "memstick"))
	type = G_UNIX_MOUNT_TYPE_MEMSTICK;
    }
  else
    {
      basename = g_path_get_basename (mount_path);
      
      if (g_str_has_prefix (basename, "cdr") ||
	  g_str_has_prefix (basename, "cdwriter") ||
	  g_str_has_prefix (basename, "burn") ||
	  g_str_has_prefix (basename, "dvdr"))
	type = G_UNIX_MOUNT_TYPE_CDROM;
      else if (g_str_has_prefix (basename, "floppy"))
	type = G_UNIX_MOUNT_TYPE_FLOPPY;
      else if (g_str_has_prefix (basename, "zip"))
	type = G_UNIX_MOUNT_TYPE_ZIP;
      else if (g_str_has_prefix (basename, "jaz"))
	type = G_UNIX_MOUNT_TYPE_JAZ;
      else if (g_str_has_prefix (basename, "camera"))
	type = G_UNIX_MOUNT_TYPE_CAMERA;
      else if (g_str_has_prefix (basename, "memstick") ||
	       g_str_has_prefix (basename, "memory_stick") ||
	       g_str_has_prefix (basename, "ram"))
	type = G_UNIX_MOUNT_TYPE_MEMSTICK;
      else if (g_str_has_prefix (basename, "compact_flash"))
	type = G_UNIX_MOUNT_TYPE_CF;
      else if (g_str_has_prefix (basename, "smart_media"))
	type = G_UNIX_MOUNT_TYPE_SM;
      else if (g_str_has_prefix (basename, "sd_mmc"))
	type = G_UNIX_MOUNT_TYPE_SDMMC;
      else if (g_str_has_prefix (basename, "ipod"))
	type = G_UNIX_MOUNT_TYPE_IPOD;
      
      g_free (basename);
    }
  
  if (type == G_UNIX_MOUNT_TYPE_UNKNOWN)
    type = G_UNIX_MOUNT_TYPE_HD;
  
  return type;
}

/**
 * g_unix_mount_guess_type:
 * @mount_entry: a #GUnixMount.
 * 
 * Guesses the type of a unix mount. If the mount type cannot be 
 * determined, returns %G_UNIX_MOUNT_TYPE_UNKNOWN.
 * 
 * Returns: a #GUnixMountType. 
 */
static GUnixMountType
g_unix_mount_guess_type (GUnixMountEntry *mount_entry)
{
  g_return_val_if_fail (mount_entry != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);
  g_return_val_if_fail (mount_entry->mount_path != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);
  g_return_val_if_fail (mount_entry->device_path != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);
  g_return_val_if_fail (mount_entry->filesystem_type != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);

  return guess_mount_type (mount_entry->mount_path,
			   mount_entry->device_path,
			   mount_entry->filesystem_type);
}

/**
 * g_unix_mount_point_guess_type:
 * @mount_point: a #GUnixMountPoint.
 * 
 * Guesses the type of a unix mount point. 
 * If the mount type cannot be determined, 
 * returns %G_UNIX_MOUNT_TYPE_UNKNOWN.
 * 
 * Returns: a #GUnixMountType.
 */
static GUnixMountType
g_unix_mount_point_guess_type (GUnixMountPoint *mount_point)
{
  g_return_val_if_fail (mount_point != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);
  g_return_val_if_fail (mount_point->mount_path != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);
  g_return_val_if_fail (mount_point->device_path != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);
  g_return_val_if_fail (mount_point->filesystem_type != NULL, G_UNIX_MOUNT_TYPE_UNKNOWN);

  return guess_mount_type (mount_point->mount_path,
			   mount_point->device_path,
			   mount_point->filesystem_type);
}

static const char *
type_to_icon (GUnixMountType type, gboolean is_mount_point, gboolean use_symbolic)
{
  const char *icon_name;
  
  switch (type)
    {
    case G_UNIX_MOUNT_TYPE_HD:
      if (is_mount_point)
        icon_name = use_symbolic ? "drive-removable-media-symbolic" : "drive-removable-media";
      else
        icon_name = use_symbolic ? "drive-harddisk-symbolic" : "drive-harddisk";
      break;
    case G_UNIX_MOUNT_TYPE_FLOPPY:
    case G_UNIX_MOUNT_TYPE_ZIP:
    case G_UNIX_MOUNT_TYPE_JAZ:
      if (is_mount_point)
        icon_name = use_symbolic ? "drive-removable-media-symbolic" : "drive-removable-media";
      else
        icon_name = use_symbolic ? "media-removable-symbolic" : "media-floppy";
      break;
    case G_UNIX_MOUNT_TYPE_CDROM:
      if (is_mount_point)
        icon_name = use_symbolic ? "drive-optical-symbolic" : "drive-optical";
      else
        icon_name = use_symbolic ? "media-optical-symbolic" : "media-optical";
      break;
    case G_UNIX_MOUNT_TYPE_NFS:
        icon_name = use_symbolic ? "folder-remote-symbolic" : "folder-remote";
      break;
    case G_UNIX_MOUNT_TYPE_MEMSTICK:
      if (is_mount_point)
        icon_name = use_symbolic ? "drive-removable-media-symbolic" : "drive-removable-media";
      else
        icon_name = use_symbolic ? "media-removable-symbolic" : "media-flash";
      break;
    case G_UNIX_MOUNT_TYPE_CAMERA:
      if (is_mount_point)
        icon_name = use_symbolic ? "drive-removable-media-symbolic" : "drive-removable-media";
      else
        icon_name = use_symbolic ? "camera-photo-symbolic" : "camera-photo";
      break;
    case G_UNIX_MOUNT_TYPE_IPOD:
      if (is_mount_point)
        icon_name = use_symbolic ? "drive-removable-media-symbolic" : "drive-removable-media";
      else
        icon_name = use_symbolic ? "multimedia-player-symbolic" : "multimedia-player";
      break;
    case G_UNIX_MOUNT_TYPE_UNKNOWN:
    default:
      if (is_mount_point)
        icon_name = use_symbolic ? "drive-removable-media-symbolic" : "drive-removable-media";
      else
        icon_name = use_symbolic ? "drive-harddisk-symbolic" : "drive-harddisk";
      break;
    }

  return icon_name;
}

/**
 * g_unix_mount_guess_name:
 * @mount_entry: a #GUnixMountEntry
 * 
 * Guesses the name of a Unix mount. 
 * The result is a translated string.
 *
 * Returns: A newly allocated string that must
 *     be freed with g_free()
 */
gchar *
g_unix_mount_guess_name (GUnixMountEntry *mount_entry)
{
  char *name;

  if (strcmp (mount_entry->mount_path, "/") == 0)
    name = g_strdup (_("Filesystem root"));
  else
    name = g_filename_display_basename (mount_entry->mount_path);

  return name;
}

/**
 * g_unix_mount_guess_icon:
 * @mount_entry: a #GUnixMountEntry
 * 
 * Guesses the icon of a Unix mount. 
 *
 * Returns: (transfer full): a #GIcon
 */
GIcon *
g_unix_mount_guess_icon (GUnixMountEntry *mount_entry)
{
  return g_themed_icon_new_with_default_fallbacks (type_to_icon (g_unix_mount_guess_type (mount_entry), FALSE, FALSE));
}

/**
 * g_unix_mount_guess_symbolic_icon:
 * @mount_entry: a #GUnixMountEntry
 *
 * Guesses the symbolic icon of a Unix mount.
 *
 * Returns: (transfer full): a #GIcon
 *
 * Since: 2.34
 */
GIcon *
g_unix_mount_guess_symbolic_icon (GUnixMountEntry *mount_entry)
{
  return g_themed_icon_new_with_default_fallbacks (type_to_icon (g_unix_mount_guess_type (mount_entry), FALSE, TRUE));
}

/**
 * g_unix_mount_point_guess_name:
 * @mount_point: a #GUnixMountPoint
 * 
 * Guesses the name of a Unix mount point. 
 * The result is a translated string.
 *
 * Returns: A newly allocated string that must 
 *     be freed with g_free()
 */
gchar *
g_unix_mount_point_guess_name (GUnixMountPoint *mount_point)
{
  char *name;

  if (strcmp (mount_point->mount_path, "/") == 0)
    name = g_strdup (_("Filesystem root"));
  else
    name = g_filename_display_basename (mount_point->mount_path);

  return name;
}

/**
 * g_unix_mount_point_guess_icon:
 * @mount_point: a #GUnixMountPoint
 * 
 * Guesses the icon of a Unix mount point. 
 *
 * Returns: (transfer full): a #GIcon
 */
GIcon *
g_unix_mount_point_guess_icon (GUnixMountPoint *mount_point)
{
  return g_themed_icon_new_with_default_fallbacks (type_to_icon (g_unix_mount_point_guess_type (mount_point), TRUE, FALSE));
}

/**
 * g_unix_mount_point_guess_symbolic_icon:
 * @mount_point: a #GUnixMountPoint
 *
 * Guesses the symbolic icon of a Unix mount point.
 *
 * Returns: (transfer full): a #GIcon
 *
 * Since: 2.34
 */
GIcon *
g_unix_mount_point_guess_symbolic_icon (GUnixMountPoint *mount_point)
{
  return g_themed_icon_new_with_default_fallbacks (type_to_icon (g_unix_mount_point_guess_type (mount_point), TRUE, TRUE));
}

/**
 * g_unix_mount_guess_can_eject:
 * @mount_entry: a #GUnixMountEntry
 * 
 * Guesses whether a Unix mount can be ejected.
 *
 * Returns: %TRUE if @mount_entry is deemed to be ejectable.
 */
gboolean
g_unix_mount_guess_can_eject (GUnixMountEntry *mount_entry)
{
  GUnixMountType guessed_type;

  guessed_type = g_unix_mount_guess_type (mount_entry);
  if (guessed_type == G_UNIX_MOUNT_TYPE_IPOD ||
      guessed_type == G_UNIX_MOUNT_TYPE_CDROM)
    return TRUE;

  return FALSE;
}

/**
 * g_unix_mount_guess_should_display:
 * @mount_entry: a #GUnixMountEntry
 * 
 * Guesses whether a Unix mount should be displayed in the UI.
 *
 * Returns: %TRUE if @mount_entry is deemed to be displayable.
 */
gboolean
g_unix_mount_guess_should_display (GUnixMountEntry *mount_entry)
{
  const char *mount_path;
  const gchar *user_name;
  gsize user_name_len;

  /* Never display internal mountpoints */
  if (g_unix_mount_is_system_internal (mount_entry))
    return FALSE;
  
  /* Only display things in /media (which are generally user mountable)
     and home dir (fuse stuff) and /run/media/$USER */
  mount_path = mount_entry->mount_path;
  if (mount_path != NULL)
    {
      gboolean is_in_runtime_dir = FALSE;
      /* Hide mounts within a dot path, suppose it was a purpose to hide this mount */
      if (g_strstr_len (mount_path, -1, "/.") != NULL)
        return FALSE;

      /* Check /run/media/$USER/ */
      user_name = g_get_user_name ();
      user_name_len = strlen (user_name);
      if (strncmp (mount_path, "/run/media/", sizeof ("/run/media/") - 1) == 0 &&
          strncmp (mount_path + sizeof ("/run/media/") - 1, user_name, user_name_len) == 0 &&
          mount_path[sizeof ("/run/media/") - 1 + user_name_len] == '/')
        is_in_runtime_dir = TRUE;

      if (is_in_runtime_dir || g_str_has_prefix (mount_path, "/media/"))
        {
          char *path;
          /* Avoid displaying mounts that are not accessible to the user.
           *
           * See http://bugzilla.gnome.org/show_bug.cgi?id=526320 for why we
           * want to avoid g_access() for mount points which can potentially
           * block or fail stat()'ing, such as network mounts.
           */
          path = g_path_get_dirname (mount_path);
          if (g_str_has_prefix (path, "/media/"))
            {
              if (g_access (path, R_OK|X_OK) != 0) 
                {
                  g_free (path);
                  return FALSE;
                }
            }
          g_free (path);

          if (mount_entry->device_path && mount_entry->device_path[0] == '/')
           {
             struct stat st;
             if (g_stat (mount_entry->device_path, &st) == 0 &&
                 S_ISBLK(st.st_mode) &&
                 g_access (mount_path, R_OK|X_OK) != 0)
               return FALSE;
           }
          return TRUE;
        }
      
      if (g_str_has_prefix (mount_path, g_get_home_dir ()) && 
          mount_path[strlen (g_get_home_dir())] == G_DIR_SEPARATOR)
        return TRUE;
    }
  
  return FALSE;
}

/**
 * g_unix_mount_point_guess_can_eject:
 * @mount_point: a #GUnixMountPoint
 * 
 * Guesses whether a Unix mount point can be ejected.
 *
 * Returns: %TRUE if @mount_point is deemed to be ejectable.
 */
gboolean
g_unix_mount_point_guess_can_eject (GUnixMountPoint *mount_point)
{
  GUnixMountType guessed_type;

  guessed_type = g_unix_mount_point_guess_type (mount_point);
  if (guessed_type == G_UNIX_MOUNT_TYPE_IPOD ||
      guessed_type == G_UNIX_MOUNT_TYPE_CDROM)
    return TRUE;

  return FALSE;
}

/* Utility functions {{{1 */

#ifdef HAVE_MNTENT_H
/* borrowed from gtk/gtkfilesystemunix.c in GTK+ on 02/23/2006 */
static void
_canonicalize_filename (gchar *filename)
{
  gchar *p, *q;
  gboolean last_was_slash = FALSE;
  
  p = filename;
  q = filename;
  
  while (*p)
    {
      if (*p == G_DIR_SEPARATOR)
        {
          if (!last_was_slash)
            *q++ = G_DIR_SEPARATOR;
          
          last_was_slash = TRUE;
        }
      else
        {
          if (last_was_slash && *p == '.')
            {
              if (*(p + 1) == G_DIR_SEPARATOR ||
                  *(p + 1) == '\0')
                {
                  if (*(p + 1) == '\0')
                    break;
                  
                  p += 1;
                }
              else if (*(p + 1) == '.' &&
                       (*(p + 2) == G_DIR_SEPARATOR ||
                        *(p + 2) == '\0'))
                {
                  if (q > filename + 1)
                    {
                      q--;
                      while (q > filename + 1 &&
                             *(q - 1) != G_DIR_SEPARATOR)
                        q--;
                    }
                  
                  if (*(p + 2) == '\0')
                    break;
                  
                  p += 2;
                }
              else
                {
                  *q++ = *p;
                  last_was_slash = FALSE;
                }
            }
          else
            {
              *q++ = *p;
              last_was_slash = FALSE;
            }
        }
      
      p++;
    }
  
  if (q > filename + 1 && *(q - 1) == G_DIR_SEPARATOR)
    q--;
  
  *q = '\0';
}

static char *
_resolve_symlink (const char *file)
{
  GError *error;
  char *dir;
  char *link;
  char *f;
  char *f1;
  
  f = g_strdup (file);
  
  while (g_file_test (f, G_FILE_TEST_IS_SYMLINK)) 
    {
      link = g_file_read_link (f, &error);
      if (link == NULL) 
        {
          g_error_free (error);
          g_free (f);
          f = NULL;
          goto out;
        }
    
      dir = g_path_get_dirname (f);
      f1 = g_strdup_printf ("%s/%s", dir, link);
      g_free (dir);
      g_free (link);
      g_free (f);
      f = f1;
    }
  
 out:
  if (f != NULL)
    _canonicalize_filename (f);
  return f;
}

static const char *
_resolve_dev_root (void)
{
  static gboolean have_real_dev_root = FALSE;
  static char real_dev_root[256];
  struct stat statbuf;
  
  /* see if it's cached already */
  if (have_real_dev_root)
    goto found;
  
  /* otherwise we're going to find it right away.. */
  have_real_dev_root = TRUE;
  
  if (stat ("/dev/root", &statbuf) == 0) 
    {
      if (! S_ISLNK (statbuf.st_mode)) 
        {
          dev_t root_dev = statbuf.st_dev;
          FILE *f;
      
          /* see if device with similar major:minor as /dev/root is mention
           * in /etc/mtab (it usually is) 
           */
          f = fopen ("/etc/mtab", "r");
          if (f != NULL) 
            {
	      struct mntent *entp;
#ifdef HAVE_GETMNTENT_R        
              struct mntent ent;
              char buf[1024];
              while ((entp = getmntent_r (f, &ent, buf, sizeof (buf))) != NULL) 
                {
#else
	      G_LOCK (getmntent);
	      while ((entp = getmntent (f)) != NULL) 
                { 
#endif          
                  if (stat (entp->mnt_fsname, &statbuf) == 0 &&
                      statbuf.st_dev == root_dev) 
                    {
                      strncpy (real_dev_root, entp->mnt_fsname, sizeof (real_dev_root) - 1);
                      real_dev_root[sizeof (real_dev_root) - 1] = '\0';
                      fclose (f);
                      goto found;
                    }
                }

              endmntent (f);

#ifndef HAVE_GETMNTENT_R
	      G_UNLOCK (getmntent);
#endif
            }                                        
      
          /* no, that didn't work.. next we could scan /dev ... but I digress.. */
      
        } 
       else 
        {
          char *resolved;
          resolved = _resolve_symlink ("/dev/root");
          if (resolved != NULL)
            {
              strncpy (real_dev_root, resolved, sizeof (real_dev_root) - 1);
              real_dev_root[sizeof (real_dev_root) - 1] = '\0';
              g_free (resolved);
              goto found;
            }
        }
    }
  
  /* bah sucks.. */
  strcpy (real_dev_root, "/dev/root");
  
found:
  return real_dev_root;
}
#endif

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
