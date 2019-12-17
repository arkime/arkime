/* gstdio.c - wrappers for C library functions
 *
 * Copyright 2004 Tor Lillqvist
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "glibconfig.h"

#define G_STDIO_NO_WRAP_ON_UNIX

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#include <errno.h>
#include <wchar.h>
#include <direct.h>
#include <io.h>
#include <sys/utime.h>
#else
#include <utime.h>
#include <errno.h>
#endif

#include "gstdio.h"
#include "gstdioprivate.h"

#if !defined (G_OS_UNIX) && !defined (G_OS_WIN32)
#error Please port this to your operating system
#endif

#if defined (_MSC_VER) && !defined(_WIN64)
#undef _wstat
#define _wstat _wstat32
#endif

#if defined (G_OS_WIN32)

/* We can't include Windows DDK and Windows SDK simultaneously,
 * so let's copy this here from MinGW-w64 DDK.
 * The structure is ultimately documented here:
 * https://msdn.microsoft.com/en-us/library/ff552012(v=vs.85).aspx
 */
typedef struct _REPARSE_DATA_BUFFER
{
  ULONG  ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union
  {
    struct
    {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG  Flags;
      WCHAR  PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct
    {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR  PathBuffer[1];
    } MountPointReparseBuffer;
    struct
    {
      UCHAR  DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

static int
w32_error_to_errno (DWORD error_code)
{
  switch (error_code)
    {
    case ERROR_ACCESS_DENIED:
      return EACCES;
      break;
    case ERROR_INVALID_HANDLE:
      return EBADF;
      break;
    case ERROR_INVALID_FUNCTION:
      return EFAULT;
      break;
    case ERROR_FILE_NOT_FOUND:
      return ENOENT;
      break;
    case ERROR_PATH_NOT_FOUND:
      return ENOENT; /* or ELOOP, or ENAMETOOLONG */
      break;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
      return ENOMEM;
      break;
    default:
      return EIO;
      break;
    }
}

static int
_g_win32_stat_utf16_no_trailing_slashes (const gunichar2    *filename,
                                         int                 fd,
                                         GWin32PrivateStat  *buf,
                                         gboolean            for_symlink)
{
  HANDLE file_handle;
  gboolean succeeded_so_far;
  DWORD error_code;
  struct __stat64 statbuf;
  BY_HANDLE_FILE_INFORMATION handle_info;
  FILE_STANDARD_INFO std_info;
  WIN32_FIND_DATAW finddata;
  DWORD immediate_attributes;
  gboolean is_symlink = FALSE;
  gboolean is_directory;
  DWORD open_flags;
  wchar_t *filename_target = NULL;
  int result;

  if (fd < 0)
    {
      immediate_attributes = GetFileAttributesW (filename);

      if (immediate_attributes == INVALID_FILE_ATTRIBUTES)
        {
          error_code = GetLastError ();
          errno = w32_error_to_errno (error_code);

          return -1;
        }

      is_symlink = (immediate_attributes & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT;
      is_directory = (immediate_attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;

      open_flags = FILE_ATTRIBUTE_NORMAL;

      if (for_symlink && is_symlink)
        open_flags |= FILE_FLAG_OPEN_REPARSE_POINT;

      if (is_directory)
        open_flags |= FILE_FLAG_BACKUP_SEMANTICS;

      file_handle = CreateFileW (filename, FILE_READ_ATTRIBUTES,
                                 FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                 open_flags,
                                 NULL);

      if (file_handle == INVALID_HANDLE_VALUE)
        {
          error_code = GetLastError ();
          errno = w32_error_to_errno (error_code);
          return -1;
        }
    }
  else
    {
      file_handle = (HANDLE) _get_osfhandle (fd);

      if (file_handle == INVALID_HANDLE_VALUE)
        return -1;
    }

  succeeded_so_far = GetFileInformationByHandle (file_handle,
                                                 &handle_info);
  error_code = GetLastError ();

  if (succeeded_so_far)
    {
      succeeded_so_far = GetFileInformationByHandleEx (file_handle,
                                                       FileStandardInfo,
                                                       &std_info,
                                                       sizeof (std_info));
      error_code = GetLastError ();
    }

  if (!succeeded_so_far)
    {
      if (fd < 0)
        CloseHandle (file_handle);
      errno = w32_error_to_errno (error_code);
      return -1;
    }

  /* It's tempting to use GetFileInformationByHandleEx(FileAttributeTagInfo),
   * but it always reports that the ReparseTag is 0.
   */
  if (fd < 0)
    {
      memset (&finddata, 0, sizeof (finddata));

      if (handle_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
          HANDLE tmp = FindFirstFileW (filename,
                                       &finddata);

          if (tmp == INVALID_HANDLE_VALUE)
            {
              error_code = GetLastError ();
              errno = w32_error_to_errno (error_code);
              CloseHandle (file_handle);
              return -1;
            }

          FindClose (tmp);
        }

      if (is_symlink && !for_symlink)
        {
          /* If filename is a symlink, _wstat64 obtains information about
           * the symlink (except that st_size will be 0).
           * To get information about the target we need to resolve
           * the symlink first. And we need _wstat64() to get st_dev,
           * it's a bother to try finding it ourselves.
           */
          DWORD filename_target_len;
          DWORD new_len;

          /* Just in case, give it a real memory location instead of NULL */
          new_len = GetFinalPathNameByHandleW (file_handle,
                                               (wchar_t *) &filename_target_len,
                                               0,
                                               FILE_NAME_NORMALIZED);

#define SANE_LIMIT 1024 * 10
          if (new_len >= SANE_LIMIT)
#undef SANE_LIMIT
            {
              new_len = 0;
              error_code = ERROR_BUFFER_OVERFLOW;
            }
          else if (new_len == 0)
            {
              error_code = GetLastError ();
            }

          if (new_len > 0)
            {
              const wchar_t *extended_prefix = L"\\\\?\\";
              const gsize    extended_prefix_len = wcslen (extended_prefix);
              const gsize    extended_prefix_len_bytes = sizeof (wchar_t) * extended_prefix_len;

              /* Pretend that new_len doesn't count the terminating NUL char,
               * and ask for a bit more space than is needed.
               */
              filename_target_len = new_len + 5;
              filename_target = g_malloc (filename_target_len * sizeof (wchar_t));

              new_len = GetFinalPathNameByHandleW (file_handle,
                                                   filename_target,
                                                   filename_target_len,
                                                   FILE_NAME_NORMALIZED);

              /* filename_target_len is already larger than needed,
               * new_len should be smaller than that, even if the size
               * is off by 1 for some reason.
               */
              if (new_len >= filename_target_len - 1)
                {
                  new_len = 0;
                  error_code = ERROR_BUFFER_OVERFLOW;
                  g_clear_pointer (&filename_target, g_free);
                }
              /* GetFinalPathNameByHandle() is documented to return extended paths,
               * strip the extended prefix.
               */
              else if (new_len > extended_prefix_len &&
                       memcmp (filename_target, extended_prefix, extended_prefix_len_bytes) == 0)
                {
                  new_len -= extended_prefix_len;
                  memmove (filename_target,
                           filename_target + extended_prefix_len,
                           (new_len + 1) * sizeof (wchar_t));
                }
            }

          if (new_len == 0)
            succeeded_so_far = FALSE;
        }

      CloseHandle (file_handle);
    }
  /* else if fd >= 0 the file_handle was obtained via _get_osfhandle()
   * and must not be closed, it is owned by fd.
   */

  if (!succeeded_so_far)
    {
      errno = w32_error_to_errno (error_code);
      return -1;
    }

  if (fd < 0)
    result = _wstat64 (filename_target != NULL ? filename_target : filename, &statbuf);
  else
    result = _fstat64 (fd, &statbuf);

  if (result != 0)
    {
      int errsv = errno;

      g_free (filename_target);
      errno = errsv;

      return -1;
    }

  g_free (filename_target);

  buf->st_dev = statbuf.st_dev;
  buf->st_mode = statbuf.st_mode;
  buf->volume_serial = handle_info.dwVolumeSerialNumber;
  buf->file_index = (((guint64) handle_info.nFileIndexHigh) << 32) | handle_info.nFileIndexLow;
  /* Note that immediate_attributes is for the symlink
   * (if it's a symlink), while handle_info contains info
   * about the symlink or the target, depending on the flags
   * we used earlier.
   */
  buf->attributes = handle_info.dwFileAttributes;
  buf->st_nlink = handle_info.nNumberOfLinks;
  buf->st_size = (((guint64) handle_info.nFileSizeHigh) << 32) | handle_info.nFileSizeLow;
  buf->allocated_size = std_info.AllocationSize.QuadPart;

  if (fd < 0 && buf->attributes & FILE_ATTRIBUTE_REPARSE_POINT)
    buf->reparse_tag = finddata.dwReserved0;
  else
    buf->reparse_tag = 0;

  buf->st_ctime = statbuf.st_ctime;
  buf->st_atime = statbuf.st_atime;
  buf->st_mtime = statbuf.st_mtime;

  return 0;
}

static int
_g_win32_stat_utf8 (const gchar       *filename,
                    GWin32PrivateStat *buf,
                    gboolean           for_symlink)
{
  wchar_t *wfilename;
  int result;
  gsize len;

  if (filename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  len = strlen (filename);

  while (len > 0 && G_IS_DIR_SEPARATOR (filename[len - 1]))
    len--;

  if (len <= 0 ||
      (g_path_is_absolute (filename) && len <= g_path_skip_root (filename) - filename))
    len = strlen (filename);

  wfilename = g_utf8_to_utf16 (filename, len, NULL, NULL, NULL);

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  result = _g_win32_stat_utf16_no_trailing_slashes (wfilename, -1, buf, for_symlink);

  g_free (wfilename);

  return result;
}

int
g_win32_stat_utf8 (const gchar       *filename,
                   GWin32PrivateStat *buf)
{
  return _g_win32_stat_utf8 (filename, buf, FALSE);
}

int
g_win32_lstat_utf8 (const gchar       *filename,
                    GWin32PrivateStat *buf)
{
  return _g_win32_stat_utf8 (filename, buf, TRUE);
}

int
g_win32_fstat (int                fd,
               GWin32PrivateStat *buf)
{
  return _g_win32_stat_utf16_no_trailing_slashes (NULL, fd, buf, FALSE);
}

static int
_g_win32_readlink_utf16_raw (const gunichar2 *filename,
                             gunichar2       *buf,
                             gsize            buf_size)
{
  DWORD returned_bytes;
  BYTE returned_data[MAXIMUM_REPARSE_DATA_BUFFER_SIZE]; /* This is 16k, by the way */
  HANDLE h;
  DWORD attributes;
  REPARSE_DATA_BUFFER *rep_buf;
  DWORD to_copy;
  DWORD error_code;

  if (buf_size > G_MAXSIZE / sizeof (wchar_t))
    {
      /* "buf_size * sizeof (wchar_t)" overflows */
      errno = EFAULT;
      return -1;
    }

  if ((attributes = GetFileAttributesW (filename)) == 0)
    {
      error_code = GetLastError ();
      errno = w32_error_to_errno (error_code);
      return -1;
    }

  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
    {
      errno = EINVAL;
      return -1;
    }

  /* To read symlink target we need to open the file as a reparse
   * point and use DeviceIoControl() on it.
   */
  h = CreateFileW (filename,
                   FILE_READ_ATTRIBUTES | SYNCHRONIZE | GENERIC_READ,
                   FILE_SHARE_READ, NULL, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL
                   | FILE_FLAG_OPEN_REPARSE_POINT
                   | (attributes & FILE_ATTRIBUTE_DIRECTORY ? FILE_FLAG_BACKUP_SEMANTICS : 0),
                   NULL);

  if (h == INVALID_HANDLE_VALUE)
    {
      error_code = GetLastError ();
      errno = w32_error_to_errno (error_code);
      return -1;
    }

  if (!DeviceIoControl (h, FSCTL_GET_REPARSE_POINT, NULL, 0,
                        returned_data, MAXIMUM_REPARSE_DATA_BUFFER_SIZE,
                        &returned_bytes, NULL))
    {
      error_code = GetLastError ();
      errno = w32_error_to_errno (error_code);
      CloseHandle (h);
      return -1;
    }

  rep_buf = (REPARSE_DATA_BUFFER *) returned_data;
  to_copy = 0;

  if (rep_buf->ReparseTag == IO_REPARSE_TAG_SYMLINK)
    {
      to_copy = rep_buf->SymbolicLinkReparseBuffer.SubstituteNameLength;

      if (to_copy > buf_size * sizeof (wchar_t))
        to_copy = buf_size * sizeof (wchar_t);

      memcpy (buf,
              &((BYTE *) rep_buf->SymbolicLinkReparseBuffer.PathBuffer)[rep_buf->SymbolicLinkReparseBuffer.SubstituteNameOffset],
              to_copy);
    }
  else if (rep_buf->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
    {
      to_copy = rep_buf->MountPointReparseBuffer.SubstituteNameLength;

      if (to_copy > buf_size * sizeof (wchar_t))
        to_copy = buf_size * sizeof (wchar_t);

      memcpy (buf,
              &((BYTE *) rep_buf->MountPointReparseBuffer.PathBuffer)[rep_buf->MountPointReparseBuffer.SubstituteNameOffset],
              to_copy);
    }

  CloseHandle (h);

  return to_copy;
}

static int
_g_win32_readlink_utf16 (const gunichar2 *filename,
                         gunichar2       *buf,
                         gsize            buf_size)
{
  const wchar_t *ntobjm_prefix = L"\\??\\";
  const gsize    ntobjm_prefix_len_unichar2 = wcslen (ntobjm_prefix);
  const gsize    ntobjm_prefix_len_bytes = sizeof (gunichar2) * ntobjm_prefix_len_unichar2;
  int            result = _g_win32_readlink_utf16_raw (filename, buf, buf_size);

  if (result <= 0)
    return result;

  /* Ensure that output is a multiple of sizeof (gunichar2),
   * cutting any trailing partial gunichar2, if present.
   */
  result -= result % sizeof (gunichar2);

  if (result <= 0)
    return result;

  /* DeviceIoControl () tends to return filenames as NT Object Manager
   * names , i.e. "\\??\\C:\\foo\\bar".
   * Remove the leading 4-byte \??\ prefix, as glib (as well as many W32 API
   * functions) is unprepared to deal with it.
   */
  if (result > ntobjm_prefix_len_bytes &&
      memcmp (buf, ntobjm_prefix, ntobjm_prefix_len_bytes) == 0)
    {
      result -= ntobjm_prefix_len_bytes;
      memmove (buf, buf + ntobjm_prefix_len_unichar2, result);
    }

  return result;
}

int
g_win32_readlink_utf8 (const gchar *filename,
                       gchar       *buf,
                       gsize        buf_size)
{
  wchar_t *wfilename;
  int result;

  wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  result = _g_win32_readlink_utf16 (wfilename, (gunichar2 *) buf, buf_size);

  g_free (wfilename);

  if (result > 0)
    {
      glong tmp_len;
      gchar *tmp = g_utf16_to_utf8 ((const gunichar2 *) buf,
                                    result / sizeof (gunichar2),
                                    NULL,
                                    &tmp_len,
                                    NULL);

      if (tmp == NULL)
        {
          errno = EINVAL;
          return -1;
        }

      if (tmp_len > buf_size - 1)
        tmp_len = buf_size - 1;

      memcpy (buf, tmp, tmp_len);
      /* readlink() doesn't NUL-terminate, but we do.
       * To be compliant, however, we return the
       * number of bytes without the NUL-terminator.
       */
      buf[tmp_len] = '\0';
      result = tmp_len;
      g_free (tmp);
    }

  return result;
}

#endif

/**
 * g_access:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @mode: as in access()
 *
 * A wrapper for the POSIX access() function. This function is used to
 * test a pathname for one or several of read, write or execute
 * permissions, or just existence.
 *
 * On Windows, the file protection mechanism is not at all POSIX-like,
 * and the underlying function in the C library only checks the
 * FAT-style READONLY attribute, and does not look at the ACL of a
 * file at all. This function is this in practise almost useless on
 * Windows. Software that needs to handle file permissions on Windows
 * more exactly should use the Win32 API.
 *
 * See your C library manual for more details about access().
 *
 * Returns: zero if the pathname refers to an existing file system
 *     object that has all the tested permissions, or -1 otherwise
 *     or on error.
 * 
 * Since: 2.8
 */
int
g_access (const gchar *filename,
	  int          mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;
    
  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

#ifndef X_OK
#define X_OK 1
#endif

  retval = _waccess (wfilename, mode & ~X_OK);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return access (filename, mode);
#endif
}

/**
 * g_chmod:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @mode: as in chmod()
 *
 * A wrapper for the POSIX chmod() function. The chmod() function is
 * used to set the permissions of a file system object.
 * 
 * On Windows the file protection mechanism is not at all POSIX-like,
 * and the underlying chmod() function in the C library just sets or
 * clears the FAT-style READONLY attribute. It does not touch any
 * ACL. Software that needs to manage file permissions on Windows
 * exactly should use the Win32 API.
 *
 * See your C library manual for more details about chmod().
 *
 * Returns: 0 if the operation succeeded, -1 on error
 * 
 * Since: 2.8
 */
int
g_chmod (const gchar *filename,
	 int          mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;
    
  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  retval = _wchmod (wfilename, mode);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return chmod (filename, mode);
#endif
}
/**
 * g_open:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @flags: as in open()
 * @mode: as in open()
 *
 * A wrapper for the POSIX open() function. The open() function is
 * used to convert a pathname into a file descriptor.
 *
 * On POSIX systems file descriptors are implemented by the operating
 * system. On Windows, it's the C library that implements open() and
 * file descriptors. The actual Win32 API for opening files is quite
 * different, see MSDN documentation for CreateFile(). The Win32 API
 * uses file handles, which are more randomish integers, not small
 * integers like file descriptors.
 *
 * Because file descriptors are specific to the C library on Windows,
 * the file descriptor returned by this function makes sense only to
 * functions in the same C library. Thus if the GLib-using code uses a
 * different C library than GLib does, the file descriptor returned by
 * this function cannot be passed to C library functions like write()
 * or read().
 *
 * See your C library manual for more details about open().
 *
 * Returns: a new file descriptor, or -1 if an error occurred.
 *     The return value can be used exactly like the return value
 *     from open().
 * 
 * Since: 2.6
 */
int
g_open (const gchar *filename,
	int          flags,
	int          mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;
    
  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  retval = _wopen (wfilename, flags, mode);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  int fd;
  do
    fd = open (filename, flags, mode);
  while (G_UNLIKELY (fd == -1 && errno == EINTR));
  return fd;
#endif
}

/**
 * g_creat:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @mode: as in creat()
 *
 * A wrapper for the POSIX creat() function. The creat() function is
 * used to convert a pathname into a file descriptor, creating a file
 * if necessary.
 *
 * On POSIX systems file descriptors are implemented by the operating
 * system. On Windows, it's the C library that implements creat() and
 * file descriptors. The actual Windows API for opening files is
 * different, see MSDN documentation for CreateFile(). The Win32 API
 * uses file handles, which are more randomish integers, not small
 * integers like file descriptors.
 *
 * Because file descriptors are specific to the C library on Windows,
 * the file descriptor returned by this function makes sense only to
 * functions in the same C library. Thus if the GLib-using code uses a
 * different C library than GLib does, the file descriptor returned by
 * this function cannot be passed to C library functions like write()
 * or read().
 *
 * See your C library manual for more details about creat().
 *
 * Returns: a new file descriptor, or -1 if an error occurred.
 *     The return value can be used exactly like the return value
 *     from creat().
 * 
 * Since: 2.8
 */
int
g_creat (const gchar *filename,
	 int          mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;
    
  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  retval = _wcreat (wfilename, mode);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return creat (filename, mode);
#endif
}

/**
 * g_rename:
 * @oldfilename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @newfilename: (type filename): a pathname in the GLib file name encoding
 *
 * A wrapper for the POSIX rename() function. The rename() function 
 * renames a file, moving it between directories if required.
 * 
 * See your C library manual for more details about how rename() works
 * on your system. It is not possible in general on Windows to rename
 * a file that is open to some process.
 *
 * Returns: 0 if the renaming succeeded, -1 if an error occurred
 * 
 * Since: 2.6
 */
int
g_rename (const gchar *oldfilename,
	  const gchar *newfilename)
{
#ifdef G_OS_WIN32
  wchar_t *woldfilename = g_utf8_to_utf16 (oldfilename, -1, NULL, NULL, NULL);
  wchar_t *wnewfilename;
  int retval;
  int save_errno = 0;

  if (woldfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  wnewfilename = g_utf8_to_utf16 (newfilename, -1, NULL, NULL, NULL);

  if (wnewfilename == NULL)
    {
      g_free (woldfilename);
      errno = EINVAL;
      return -1;
    }

  if (MoveFileExW (woldfilename, wnewfilename, MOVEFILE_REPLACE_EXISTING))
    retval = 0;
  else
    {
      retval = -1;
      switch (GetLastError ())
	{
#define CASE(a,b) case ERROR_##a: save_errno = b; break
	  CASE (FILE_NOT_FOUND, ENOENT);
	  CASE (PATH_NOT_FOUND, ENOENT);
	  CASE (ACCESS_DENIED, EACCES);
	  CASE (NOT_SAME_DEVICE, EXDEV);
	  CASE (LOCK_VIOLATION, EACCES);
	  CASE (SHARING_VIOLATION, EACCES);
	  CASE (FILE_EXISTS, EEXIST);
	  CASE (ALREADY_EXISTS, EEXIST);
#undef CASE
	default: save_errno = EIO;
	}
    }

  g_free (woldfilename);
  g_free (wnewfilename);
    
  errno = save_errno;
  return retval;
#else
  return rename (oldfilename, newfilename);
#endif
}

/**
 * g_mkdir: 
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @mode: permissions to use for the newly created directory
 *
 * A wrapper for the POSIX mkdir() function. The mkdir() function 
 * attempts to create a directory with the given name and permissions.
 * The mode argument is ignored on Windows.
 * 
 * See your C library manual for more details about mkdir().
 *
 * Returns: 0 if the directory was successfully created, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_mkdir (const gchar *filename,
	 int          mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  retval = _wmkdir (wfilename);
  save_errno = errno;

  g_free (wfilename);
    
  errno = save_errno;
  return retval;
#else
  return mkdir (filename, mode);
#endif
}

/**
 * g_chdir: 
 * @path: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 *
 * A wrapper for the POSIX chdir() function. The function changes the
 * current directory of the process to @path.
 * 
 * See your C library manual for more details about chdir().
 *
 * Returns: 0 on success, -1 if an error occurred.
 * 
 * Since: 2.8
 */
int
g_chdir (const gchar *path)
{
#ifdef G_OS_WIN32
  wchar_t *wpath = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wpath == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  retval = _wchdir (wpath);
  save_errno = errno;

  g_free (wpath);
    
  errno = save_errno;
  return retval;
#else
  return chdir (path);
#endif
}

/**
 * GStatBuf:
 *
 * A type corresponding to the appropriate struct type for the stat()
 * system call, depending on the platform and/or compiler being used.
 *
 * See g_stat() for more information.
 */
/**
 * g_stat: 
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @buf: a pointer to a stat struct, which will be filled with the file
 *     information
 *
 * A wrapper for the POSIX stat() function. The stat() function
 * returns information about a file. On Windows the stat() function in
 * the C library checks only the FAT-style READONLY attribute and does
 * not look at the ACL at all. Thus on Windows the protection bits in
 * the @st_mode field are a fabrication of little use.
 * 
 * On Windows the Microsoft C libraries have several variants of the
 * stat struct and stat() function with names like _stat(), _stat32(),
 * _stat32i64() and _stat64i32(). The one used here is for 32-bit code
 * the one with 32-bit size and time fields, specifically called _stat32().
 *
 * In Microsoft's compiler, by default struct stat means one with
 * 64-bit time fields while in MinGW struct stat is the legacy one
 * with 32-bit fields. To hopefully clear up this messs, the gstdio.h
 * header defines a type #GStatBuf which is the appropriate struct type
 * depending on the platform and/or compiler being used. On POSIX it
 * is just struct stat, but note that even on POSIX platforms, stat()
 * might be a macro.
 *
 * See your C library manual for more details about stat().
 *
 * Returns: 0 if the information was successfully retrieved,
 *     -1 if an error occurred
 * 
 * Since: 2.6
 */
int
g_stat (const gchar *filename,
	GStatBuf    *buf)
{
#ifdef G_OS_WIN32
  GWin32PrivateStat w32_buf;
  int retval = g_win32_stat_utf8 (filename, &w32_buf);

  buf->st_dev = w32_buf.st_dev;
  buf->st_ino = w32_buf.st_ino;
  buf->st_mode = w32_buf.st_mode;
  buf->st_nlink = w32_buf.st_nlink;
  buf->st_uid = w32_buf.st_uid;
  buf->st_gid = w32_buf.st_gid;
  buf->st_rdev = w32_buf.st_dev;
  buf->st_size = w32_buf.st_size;
  buf->st_atime = w32_buf.st_atime;
  buf->st_mtime = w32_buf.st_mtime;
  buf->st_ctime = w32_buf.st_ctime;

  return retval;
#else
  return stat (filename, buf);
#endif
}

/**
 * g_lstat: 
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @buf: a pointer to a stat struct, which will be filled with the file
 *     information
 *
 * A wrapper for the POSIX lstat() function. The lstat() function is
 * like stat() except that in the case of symbolic links, it returns
 * information about the symbolic link itself and not the file that it
 * refers to. If the system does not support symbolic links g_lstat()
 * is identical to g_stat().
 * 
 * See your C library manual for more details about lstat().
 *
 * Returns: 0 if the information was successfully retrieved,
 *     -1 if an error occurred
 * 
 * Since: 2.6
 */
int
g_lstat (const gchar *filename,
	 GStatBuf    *buf)
{
#ifdef HAVE_LSTAT
  /* This can't be Win32, so don't do the widechar dance. */
  return lstat (filename, buf);
#elif defined (G_OS_WIN32)
  GWin32PrivateStat w32_buf;
  int retval = g_win32_lstat_utf8 (filename, &w32_buf);

  buf->st_dev = w32_buf.st_dev;
  buf->st_ino = w32_buf.st_ino;
  buf->st_mode = w32_buf.st_mode;
  buf->st_nlink = w32_buf.st_nlink;
  buf->st_uid = w32_buf.st_uid;
  buf->st_gid = w32_buf.st_gid;
  buf->st_rdev = w32_buf.st_dev;
  buf->st_size = w32_buf.st_size;
  buf->st_atime = w32_buf.st_atime;
  buf->st_mtime = w32_buf.st_mtime;
  buf->st_ctime = w32_buf.st_ctime;

  return retval;
#else
  return g_stat (filename, buf);
#endif
}

/**
 * g_unlink:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 *
 * A wrapper for the POSIX unlink() function. The unlink() function 
 * deletes a name from the filesystem. If this was the last link to the 
 * file and no processes have it opened, the diskspace occupied by the
 * file is freed.
 * 
 * See your C library manual for more details about unlink(). Note
 * that on Windows, it is in general not possible to delete files that
 * are open to some process, or mapped into memory.
 *
 * Returns: 0 if the name was successfully deleted, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_unlink (const gchar *filename)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  retval = _wunlink (wfilename);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return unlink (filename);
#endif
}

/**
 * g_remove:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 *
 * A wrapper for the POSIX remove() function. The remove() function
 * deletes a name from the filesystem.
 * 
 * See your C library manual for more details about how remove() works
 * on your system. On Unix, remove() removes also directories, as it
 * calls unlink() for files and rmdir() for directories. On Windows,
 * although remove() in the C library only works for files, this
 * function tries first remove() and then if that fails rmdir(), and
 * thus works for both files and directories. Note however, that on
 * Windows, it is in general not possible to remove a file that is
 * open to some process, or mapped into memory.
 *
 * If this function fails on Windows you can't infer too much from the
 * errno value. rmdir() is tried regardless of what caused remove() to
 * fail. Any errno value set by remove() will be overwritten by that
 * set by rmdir().
 *
 * Returns: 0 if the file was successfully removed, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_remove (const gchar *filename)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  retval = _wremove (wfilename);
  if (retval == -1)
    retval = _wrmdir (wfilename);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return remove (filename);
#endif
}

/**
 * g_rmdir:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 *
 * A wrapper for the POSIX rmdir() function. The rmdir() function
 * deletes a directory from the filesystem.
 * 
 * See your C library manual for more details about how rmdir() works
 * on your system.
 *
 * Returns: 0 if the directory was successfully removed, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_rmdir (const gchar *filename)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }
  
  retval = _wrmdir (wfilename);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return rmdir (filename);
#endif
}

/**
 * g_fopen:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @mode: a string describing the mode in which the file should be opened
 *
 * A wrapper for the stdio fopen() function. The fopen() function
 * opens a file and associates a new stream with it.
 * 
 * Because file descriptors are specific to the C library on Windows,
 * and a file descriptor is part of the FILE struct, the FILE* returned
 * by this function makes sense only to functions in the same C library.
 * Thus if the GLib-using code uses a different C library than GLib does,
 * the FILE* returned by this function cannot be passed to C library
 * functions like fprintf() or fread().
 *
 * See your C library manual for more details about fopen().
 *
 * Returns: A FILE* if the file was successfully opened, or %NULL if
 *     an error occurred
 * 
 * Since: 2.6
 */
FILE *
g_fopen (const gchar *filename,
	 const gchar *mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  wchar_t *wmode;
  FILE *retval;
  int save_errno;

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return NULL;
    }

  wmode = g_utf8_to_utf16 (mode, -1, NULL, NULL, NULL);

  if (wmode == NULL)
    {
      g_free (wfilename);
      errno = EINVAL;
      return NULL;
    }

  retval = _wfopen (wfilename, wmode);
  save_errno = errno;

  g_free (wfilename);
  g_free (wmode);

  errno = save_errno;
  return retval;
#else
  return fopen (filename, mode);
#endif
}

/**
 * g_freopen:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @mode: a string describing the mode in which the file should be  opened
 * @stream: (nullable): an existing stream which will be reused, or %NULL
 *
 * A wrapper for the POSIX freopen() function. The freopen() function
 * opens a file and associates it with an existing stream.
 * 
 * See your C library manual for more details about freopen().
 *
 * Returns: A FILE* if the file was successfully opened, or %NULL if
 *     an error occurred.
 * 
 * Since: 2.6
 */
FILE *
g_freopen (const gchar *filename,
	   const gchar *mode,
	   FILE        *stream)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  wchar_t *wmode;
  FILE *retval;
  int save_errno;

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return NULL;
    }
  
  wmode = g_utf8_to_utf16 (mode, -1, NULL, NULL, NULL);

  if (wmode == NULL)
    {
      g_free (wfilename);
      errno = EINVAL;
      return NULL;
    }
  
  retval = _wfreopen (wfilename, wmode, stream);
  save_errno = errno;

  g_free (wfilename);
  g_free (wmode);

  errno = save_errno;
  return retval;
#else
  return freopen (filename, mode, stream);
#endif
}

/**
 * g_utime:
 * @filename: (type filename): a pathname in the GLib file name encoding
 *     (UTF-8 on Windows)
 * @utb: a pointer to a struct utimbuf.
 *
 * A wrapper for the POSIX utime() function. The utime() function
 * sets the access and modification timestamps of a file.
 * 
 * See your C library manual for more details about how utime() works
 * on your system.
 *
 * Returns: 0 if the operation was successful, -1 if an error occurred
 * 
 * Since: 2.18
 */
int
g_utime (const gchar    *filename,
	 struct utimbuf *utb)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wfilename == NULL)
    {
      errno = EINVAL;
      return -1;
    }
  
  retval = _wutime (wfilename, (struct _utimbuf*) utb);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return utime (filename, utb);
#endif
}

/**
 * g_close:
 * @fd: A file descriptor
 * @error: a #GError
 *
 * This wraps the close() call; in case of error, %errno will be
 * preserved, but the error will also be stored as a #GError in @error.
 *
 * Besides using #GError, there is another major reason to prefer this
 * function over the call provided by the system; on Unix, it will
 * attempt to correctly handle %EINTR, which has platform-specific
 * semantics.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 *
 * Since: 2.36
 */
gboolean
g_close (gint       fd,
         GError   **error)
{
  int res;
  res = close (fd);
  /* Just ignore EINTR for now; a retry loop is the wrong thing to do
   * on Linux at least.  Anyone who wants to add a conditional check
   * for e.g. HP-UX is welcome to do so later...
   *
   * http://lkml.indiana.edu/hypermail/linux/kernel/0509.1/0877.html
   * https://bugzilla.gnome.org/show_bug.cgi?id=682819
   * http://utcc.utoronto.ca/~cks/space/blog/unix/CloseEINTR
   * https://sites.google.com/site/michaelsafyan/software-engineering/checkforeintrwheninvokingclosethinkagain
   */
  if (G_UNLIKELY (res == -1 && errno == EINTR))
    return TRUE;
  else if (res == -1)
    {
      int errsv = errno;
      g_set_error_literal (error, G_FILE_ERROR,
                           g_file_error_from_errno (errsv),
                           g_strerror (errsv));
      errno = errsv;
      return FALSE;
    }
  return TRUE;
}

