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
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include "glibintl.h"
#include "gioerror.h"
#include "gcancellable.h"
#include "glocalfileoutputstream.h"
#include "gfileinfo.h"
#include "glocalfileinfo.h"

#ifdef G_OS_UNIX
#include <unistd.h>
#include "gfiledescriptorbased.h"
#endif

#include "glib-private.h"

#ifdef G_OS_WIN32
#include <io.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct _GLocalFileOutputStreamPrivate {
  char *tmp_filename;
  char *original_filename;
  char *backup_filename;
  char *etag;
  guint sync_on_close : 1;
  guint do_close : 1;
  int fd;
};

#ifdef G_OS_UNIX
static void       g_file_descriptor_based_iface_init   (GFileDescriptorBasedIface *iface);
#endif

#define g_local_file_output_stream_get_type _g_local_file_output_stream_get_type
#ifdef G_OS_UNIX
G_DEFINE_TYPE_WITH_CODE (GLocalFileOutputStream, g_local_file_output_stream, G_TYPE_FILE_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GLocalFileOutputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE_DESCRIPTOR_BASED,
						g_file_descriptor_based_iface_init))
#else
G_DEFINE_TYPE_WITH_CODE (GLocalFileOutputStream, g_local_file_output_stream, G_TYPE_FILE_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GLocalFileOutputStream))
#endif


/* Some of the file replacement code was based on the code from gedit,
 * relicenced to LGPL with permissions from the authors.
 */

#define BACKUP_EXTENSION "~"

static gssize     g_local_file_output_stream_write        (GOutputStream      *stream,
							   const void         *buffer,
							   gsize               count,
							   GCancellable       *cancellable,
							   GError            **error);
static gboolean   g_local_file_output_stream_close        (GOutputStream      *stream,
							   GCancellable       *cancellable,
							   GError            **error);
static GFileInfo *g_local_file_output_stream_query_info   (GFileOutputStream  *stream,
							   const char         *attributes,
							   GCancellable       *cancellable,
							   GError            **error);
static char *     g_local_file_output_stream_get_etag     (GFileOutputStream  *stream);
static goffset    g_local_file_output_stream_tell         (GFileOutputStream  *stream);
static gboolean   g_local_file_output_stream_can_seek     (GFileOutputStream  *stream);
static gboolean   g_local_file_output_stream_seek         (GFileOutputStream  *stream,
							   goffset             offset,
							   GSeekType           type,
							   GCancellable       *cancellable,
							   GError            **error);
static gboolean   g_local_file_output_stream_can_truncate (GFileOutputStream  *stream);
static gboolean   g_local_file_output_stream_truncate     (GFileOutputStream  *stream,
							   goffset             size,
							   GCancellable       *cancellable,
							   GError            **error);
#ifdef G_OS_UNIX
static int        g_local_file_output_stream_get_fd       (GFileDescriptorBased *stream);
#endif

static void
g_local_file_output_stream_finalize (GObject *object)
{
  GLocalFileOutputStream *file;
  
  file = G_LOCAL_FILE_OUTPUT_STREAM (object);
  
  g_free (file->priv->tmp_filename);
  g_free (file->priv->original_filename);
  g_free (file->priv->backup_filename);
  g_free (file->priv->etag);

  G_OBJECT_CLASS (g_local_file_output_stream_parent_class)->finalize (object);
}

static void
g_local_file_output_stream_class_init (GLocalFileOutputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);
  GFileOutputStreamClass *file_stream_class = G_FILE_OUTPUT_STREAM_CLASS (klass);

  gobject_class->finalize = g_local_file_output_stream_finalize;

  stream_class->write_fn = g_local_file_output_stream_write;
  stream_class->close_fn = g_local_file_output_stream_close;
  file_stream_class->query_info = g_local_file_output_stream_query_info;
  file_stream_class->get_etag = g_local_file_output_stream_get_etag;
  file_stream_class->tell = g_local_file_output_stream_tell;
  file_stream_class->can_seek = g_local_file_output_stream_can_seek;
  file_stream_class->seek = g_local_file_output_stream_seek;
  file_stream_class->can_truncate = g_local_file_output_stream_can_truncate;
  file_stream_class->truncate_fn = g_local_file_output_stream_truncate;
}

#ifdef G_OS_UNIX
static void
g_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface)
{
  iface->get_fd = g_local_file_output_stream_get_fd;
}
#endif

static void
g_local_file_output_stream_init (GLocalFileOutputStream *stream)
{
  stream->priv = g_local_file_output_stream_get_instance_private (stream);
  stream->priv->do_close = TRUE;
}

static gssize
g_local_file_output_stream_write (GOutputStream  *stream,
				  const void     *buffer,
				  gsize           count,
				  GCancellable   *cancellable,
				  GError        **error)
{
  GLocalFileOutputStream *file;
  gssize res;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);

  while (1)
    {
      if (g_cancellable_set_error_if_cancelled (cancellable, error))
	return -1;
      res = write (file->priv->fd, buffer, count);
      if (res == -1)
	{
          int errsv = errno;

	  if (errsv == EINTR)
	    continue;
	  
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error writing to file: %s"),
		       g_strerror (errsv));
	}
      
      break;
    }
  
  return res;
}

void
_g_local_file_output_stream_set_do_close (GLocalFileOutputStream *out,
					  gboolean do_close)
{
  out->priv->do_close = do_close;
}

gboolean
_g_local_file_output_stream_really_close (GLocalFileOutputStream *file,
					  GCancellable   *cancellable,
					  GError        **error)
{
  GLocalFileStat final_stat;

#ifdef HAVE_FSYNC
  if (file->priv->sync_on_close &&
      fsync (file->priv->fd) != 0)
    {
      int errsv = errno;
      
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error writing to file: %s"),
		   g_strerror (errsv));
      goto err_out;
    }
#endif
 
#ifdef G_OS_WIN32

  /* Must close before renaming on Windows, so just do the close first
   * in all cases for now.
   */
  if (GLIB_PRIVATE_CALL (g_win32_fstat) (file->priv->fd, &final_stat) == 0)
    file->priv->etag = _g_local_file_info_create_etag (&final_stat);

  if (!g_close (file->priv->fd, NULL))
    {
      int errsv = errno;
      
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error closing file: %s"),
		   g_strerror (errsv));
      return FALSE;
    }

#endif

  if (file->priv->tmp_filename)
    {
      /* We need to move the temp file to its final place,
       * and possibly create the backup file
       */

      if (file->priv->backup_filename)
	{
	  if (g_cancellable_set_error_if_cancelled (cancellable, error))
	    goto err_out;
	  
#ifdef G_OS_UNIX
	  /* create original -> backup link, the original is then renamed over */
	  if (g_unlink (file->priv->backup_filename) != 0 &&
	      errno != ENOENT)
	    {
              int errsv = errno;

	      g_set_error (error, G_IO_ERROR,
			   G_IO_ERROR_CANT_CREATE_BACKUP,
			   _("Error removing old backup link: %s"),
			   g_strerror (errsv));
	      goto err_out;
	    }

	  if (link (file->priv->original_filename, file->priv->backup_filename) != 0)
	    {
	      /*  link failed or is not supported, try rename  */
	      if (g_rename (file->priv->original_filename, file->priv->backup_filename) != 0)
		{
                  int errsv = errno;

	    	  g_set_error (error, G_IO_ERROR,
		    	       G_IO_ERROR_CANT_CREATE_BACKUP,
			       _("Error creating backup copy: %s"),
			       g_strerror (errsv));
	          goto err_out;
		}
	    }
#else
	    /* If link not supported, just rename... */
	  if (g_rename (file->priv->original_filename, file->priv->backup_filename) != 0)
	    {
              int errsv = errno;

	      g_set_error (error, G_IO_ERROR,
			   G_IO_ERROR_CANT_CREATE_BACKUP,
			   _("Error creating backup copy: %s"),
			   g_strerror (errsv));
	      goto err_out;
	    }
#endif
	}
      

      if (g_cancellable_set_error_if_cancelled (cancellable, error))
	goto err_out;

      /* tmp -> original */
      if (g_rename (file->priv->tmp_filename, file->priv->original_filename) != 0)
	{
          int errsv = errno;

	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error renaming temporary file: %s"),
		       g_strerror (errsv));
	  goto err_out;
	}

      g_clear_pointer (&file->priv->tmp_filename, g_free);
    }
  
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    goto err_out;
      
#ifndef G_OS_WIN32		/* Already did the fstat() and close() above on Win32 */

  if (fstat (file->priv->fd, &final_stat) == 0)
    file->priv->etag = _g_local_file_info_create_etag (&final_stat);

  if (!g_close (file->priv->fd, NULL))
    {
      int errsv = errno;
      
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   _("Error closing file: %s"),
                   g_strerror (errsv));
      goto err_out;
    }

#endif
  
  return TRUE;
 err_out:

#ifndef G_OS_WIN32
  /* A simple try to close the fd in case we fail before the actual close */
  g_close (file->priv->fd, NULL);
#endif
  if (file->priv->tmp_filename)
    g_unlink (file->priv->tmp_filename);

  return FALSE;
}


static gboolean
g_local_file_output_stream_close (GOutputStream  *stream,
				  GCancellable   *cancellable,
				  GError        **error)
{
  GLocalFileOutputStream *file;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);

  if (file->priv->do_close)
    return _g_local_file_output_stream_really_close (file,
						     cancellable,
						     error);
  return TRUE;
}

static char *
g_local_file_output_stream_get_etag (GFileOutputStream *stream)
{
  GLocalFileOutputStream *file;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);
  
  return g_strdup (file->priv->etag);
}

static goffset
g_local_file_output_stream_tell (GFileOutputStream *stream)
{
  GLocalFileOutputStream *file;
  off_t pos;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);
  
  pos = lseek (file->priv->fd, 0, SEEK_CUR);

  if (pos == (off_t)-1)
    return 0;
  
  return pos;
}

static gboolean
g_local_file_output_stream_can_seek (GFileOutputStream *stream)
{
  GLocalFileOutputStream *file;
  off_t pos;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);
  
  pos = lseek (file->priv->fd, 0, SEEK_CUR);

  if (pos == (off_t)-1 && errno == ESPIPE)
    return FALSE;
  
  return TRUE;
}

static int
seek_type_to_lseek (GSeekType type)
{
  switch (type)
    {
    default:
    case G_SEEK_CUR:
      return SEEK_CUR;
      
    case G_SEEK_SET:
      return SEEK_SET;
      
    case G_SEEK_END:
      return SEEK_END;
    }
}

static gboolean
g_local_file_output_stream_seek (GFileOutputStream  *stream,
				 goffset             offset,
				 GSeekType           type,
				 GCancellable       *cancellable,
				 GError            **error)
{
  GLocalFileOutputStream *file;
  off_t pos;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);

  pos = lseek (file->priv->fd, offset, seek_type_to_lseek (type));

  if (pos == (off_t)-1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error seeking in file: %s"),
		   g_strerror (errsv));
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
g_local_file_output_stream_can_truncate (GFileOutputStream *stream)
{
  /* We can't truncate pipes and stuff where we can't seek */
  return g_local_file_output_stream_can_seek (stream);
}

static gboolean
g_local_file_output_stream_truncate (GFileOutputStream  *stream,
				     goffset             size,
				     GCancellable       *cancellable,
				     GError            **error)
{
  GLocalFileOutputStream *file;
  int res;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);

 restart:
#ifdef G_OS_WIN32
  res = g_win32_ftruncate (file->priv->fd, size);
#else
  res = ftruncate (file->priv->fd, size);
#endif
  
  if (res == -1)
    {
      int errsv = errno;

      if (errsv == EINTR)
	{
	  if (g_cancellable_set_error_if_cancelled (cancellable, error))
	    return FALSE;
	  goto restart;
	}

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error truncating file: %s"),
		   g_strerror (errsv));
      return FALSE;
    }
  
  return TRUE;
}


static GFileInfo *
g_local_file_output_stream_query_info (GFileOutputStream  *stream,
				       const char         *attributes,
				       GCancellable       *cancellable,
				       GError            **error)
{
  GLocalFileOutputStream *file;

  file = G_LOCAL_FILE_OUTPUT_STREAM (stream);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  return _g_local_file_info_get_from_fd (file->priv->fd,
					 attributes,
					 error);
}

GFileOutputStream *
_g_local_file_output_stream_new (int fd)
{
  GLocalFileOutputStream *stream;

  stream = g_object_new (G_TYPE_LOCAL_FILE_OUTPUT_STREAM, NULL);
  stream->priv->fd = fd;
  return G_FILE_OUTPUT_STREAM (stream);
}

static void
set_error_from_open_errno (const char *filename,
                           GError    **error)
{
  int errsv = errno;
  
  if (errsv == EINVAL)
    /* This must be an invalid filename, on e.g. FAT */
    g_set_error_literal (error, G_IO_ERROR,
                         G_IO_ERROR_INVALID_FILENAME,
                         _("Invalid filename"));
  else
    {
      char *display_name = g_filename_display_name (filename);
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   _("Error opening file “%s”: %s"),
		       display_name, g_strerror (errsv));
      g_free (display_name);
    }
}

static GFileOutputStream *
output_stream_open (const char    *filename,
                    gint           open_flags,
                    guint          mode,
                    GCancellable  *cancellable,
                    GError       **error)
{
  GLocalFileOutputStream *stream;
  gint fd;

  fd = g_open (filename, open_flags, mode);
  if (fd == -1)
    {
      set_error_from_open_errno (filename, error);
      return NULL;
    }
  
  stream = g_object_new (G_TYPE_LOCAL_FILE_OUTPUT_STREAM, NULL);
  stream->priv->fd = fd;
  return G_FILE_OUTPUT_STREAM (stream);
}

GFileOutputStream *
_g_local_file_output_stream_open  (const char        *filename,
				   gboolean          readable,
				   GCancellable      *cancellable,
				   GError           **error)
{
  int open_flags;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  open_flags = O_BINARY;
  if (readable)
    open_flags |= O_RDWR;
  else
    open_flags |= O_WRONLY;

  return output_stream_open (filename, open_flags, 0666, cancellable, error);
}

static gint
mode_from_flags_or_info (GFileCreateFlags   flags,
                         GFileInfo         *reference_info)
{
  if (flags & G_FILE_CREATE_PRIVATE)
    return 0600;
  else if (reference_info && g_file_info_has_attribute (reference_info, "unix::mode"))
    return g_file_info_get_attribute_uint32 (reference_info, "unix::mode") & (~S_IFMT);
  else
    return 0666;
}

GFileOutputStream *
_g_local_file_output_stream_create  (const char        *filename,
				     gboolean          readable,
				     GFileCreateFlags   flags,
                                     GFileInfo         *reference_info,
				     GCancellable      *cancellable,
				     GError           **error)
{
  int mode;
  int open_flags;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  mode = mode_from_flags_or_info (flags, reference_info);

  open_flags = O_CREAT | O_EXCL | O_BINARY;
  if (readable)
    open_flags |= O_RDWR;
  else
    open_flags |= O_WRONLY;

  return output_stream_open (filename, open_flags, mode, cancellable, error);
}

GFileOutputStream *
_g_local_file_output_stream_append  (const char        *filename,
				     GFileCreateFlags   flags,
				     GCancellable      *cancellable,
				     GError           **error)
{
  int mode;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  if (flags & G_FILE_CREATE_PRIVATE)
    mode = 0600;
  else
    mode = 0666;

  return output_stream_open (filename, O_CREAT | O_APPEND | O_WRONLY | O_BINARY, mode,
                             cancellable, error);
}

static char *
create_backup_filename (const char *filename)
{
  return g_strconcat (filename, BACKUP_EXTENSION, NULL);
}

#define BUFSIZE	8192 /* size of normal write buffer */

static gboolean
copy_file_data (gint     sfd,
		gint     dfd,
		GError **error)
{
  gboolean ret = TRUE;
  gpointer buffer;
  const gchar *write_buffer;
  gssize bytes_read;
  gssize bytes_to_write;
  gssize bytes_written;
  
  buffer = g_malloc (BUFSIZE);
  
  do
    {
      bytes_read = read (sfd, buffer, BUFSIZE);
      if (bytes_read == -1)
	{
          int errsv = errno;

	  if (errsv == EINTR)
	    continue;
	  
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error reading from file: %s"),
		       g_strerror (errsv));
	  ret = FALSE;
	  break;
	}
      
      bytes_to_write = bytes_read;
      write_buffer = buffer;
      
      do
	{
	  bytes_written = write (dfd, write_buffer, bytes_to_write);
	  if (bytes_written == -1)
	    {
              int errsv = errno;

	      if (errsv == EINTR)
		continue;
	      
	      g_set_error (error, G_IO_ERROR,
			   g_io_error_from_errno (errsv),
			   _("Error writing to file: %s"),
			   g_strerror (errsv));
	      ret = FALSE;
	      break;
	    }
	  
	  bytes_to_write -= bytes_written;
	  write_buffer += bytes_written;
	}
      while (bytes_to_write > 0);
      
    } while ((bytes_read != 0) && (ret == TRUE));

  g_free (buffer);
  
  return ret;
}

static int
handle_overwrite_open (const char    *filename,
		       gboolean       readable,
		       const char    *etag,
		       gboolean       create_backup,
		       char         **temp_filename,
		       GFileCreateFlags flags,
                       GFileInfo       *reference_info,
		       GCancellable  *cancellable,
		       GError       **error)
{
  int fd = -1;
  GLocalFileStat original_stat;
  char *current_etag;
  gboolean is_symlink;
  int open_flags;
  int res;
  int mode;
  int errsv;

  mode = mode_from_flags_or_info (flags, reference_info);

  /* We only need read access to the original file if we are creating a backup.
   * We also add O_CREATE to avoid a race if the file was just removed */
  if (create_backup || readable)
    open_flags = O_RDWR | O_CREAT | O_BINARY;
  else
    open_flags = O_WRONLY | O_CREAT | O_BINARY;
  
  /* Some systems have O_NOFOLLOW, which lets us avoid some races
   * when finding out if the file we opened was a symlink */
#ifdef O_NOFOLLOW
  is_symlink = FALSE;
  fd = g_open (filename, open_flags | O_NOFOLLOW, mode);
  errsv = errno;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
  if (fd == -1 && errsv == EMLINK)
#elif defined(__NetBSD__)
  if (fd == -1 && errsv == EFTYPE)
#else
  if (fd == -1 && errsv == ELOOP)
#endif
    {
      /* Could be a symlink, or it could be a regular ELOOP error,
       * but then the next open will fail too. */
      is_symlink = TRUE;
      fd = g_open (filename, open_flags, mode);
    }
#else
  fd = g_open (filename, open_flags, mode);
  errsv = errno;
  /* This is racy, but we do it as soon as possible to minimize the race */
  is_symlink = g_file_test (filename, G_FILE_TEST_IS_SYMLINK);
#endif

  if (fd == -1)
    {
      char *display_name = g_filename_display_name (filename);
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error opening file “%s”: %s"),
		   display_name, g_strerror (errsv));
      g_free (display_name);
      return -1;
    }
  
#ifdef G_OS_WIN32
  res = GLIB_PRIVATE_CALL (g_win32_fstat) (fd, &original_stat);
#else
  res = fstat (fd, &original_stat);
#endif
  errsv = errno;

  if (res != 0)
    {
      char *display_name = g_filename_display_name (filename);
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error when getting information for file “%s”: %s"),
		   display_name, g_strerror (errsv));
      g_free (display_name);
      goto err_out;
    }
  
  /* not a regular file */
  if (!S_ISREG (original_stat.st_mode))
    {
      if (S_ISDIR (original_stat.st_mode))
	g_set_error_literal (error,
                             G_IO_ERROR,
                             G_IO_ERROR_IS_DIRECTORY,
                             _("Target file is a directory"));
      else
	g_set_error_literal (error,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_REGULAR_FILE,
                             _("Target file is not a regular file"));
      goto err_out;
    }
  
  if (etag != NULL)
    {
      current_etag = _g_local_file_info_create_etag (&original_stat);
      if (strcmp (etag, current_etag) != 0)
	{
	  g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_WRONG_ETAG,
                               _("The file was externally modified"));
	  g_free (current_etag);
	  goto err_out;
	}
      g_free (current_etag);
    }

  /* We use two backup strategies.
   * The first one (which is faster) consist in saving to a
   * tmp file then rename the original file to the backup and the
   * tmp file to the original name. This is fast but doesn't work
   * when the file is a link (hard or symbolic) or when we can't
   * write to the current dir or can't set the permissions on the
   * new file. 
   * The second strategy consist simply in copying the old file
   * to a backup file and rewrite the contents of the file.
   */
  
  if ((flags & G_FILE_CREATE_REPLACE_DESTINATION) ||
      (!(original_stat.st_nlink > 1) && !is_symlink))
    {
      char *dirname, *tmp_filename;
      int tmpfd;
      
      dirname = g_path_get_dirname (filename);
      tmp_filename = g_build_filename (dirname, ".goutputstream-XXXXXX", NULL);
      g_free (dirname);

      tmpfd = g_mkstemp_full (tmp_filename, (readable ? O_RDWR : O_WRONLY) | O_BINARY, mode);
      if (tmpfd == -1)
	{
	  g_free (tmp_filename);
	  goto fallback_strategy;
	}
      
      /* try to keep permissions (unless replacing) */

      if ( ! (flags & G_FILE_CREATE_REPLACE_DESTINATION) &&
	   (
#ifdef HAVE_FCHOWN
	    fchown (tmpfd, original_stat.st_uid, original_stat.st_gid) == -1 ||
#endif
#ifdef HAVE_FCHMOD
	    fchmod (tmpfd, original_stat.st_mode) == -1 ||
#endif
	    0
	    )
	  )
	{
          GLocalFileStat tmp_statbuf;
          int tres;

#ifdef G_OS_WIN32
          tres = GLIB_PRIVATE_CALL (g_win32_fstat) (tmpfd, &tmp_statbuf);
#else
          tres = fstat (tmpfd, &tmp_statbuf);
#endif
	  /* Check that we really needed to change something */
	  if (tres != 0 ||
	      original_stat.st_uid != tmp_statbuf.st_uid ||
	      original_stat.st_gid != tmp_statbuf.st_gid ||
	      original_stat.st_mode != tmp_statbuf.st_mode)
	    {
	      g_close (tmpfd, NULL);
	      g_unlink (tmp_filename);
	      g_free (tmp_filename);
	      goto fallback_strategy;
	    }
	}

      g_close (fd, NULL);
      *temp_filename = tmp_filename;
      return tmpfd;
    }

 fallback_strategy:

  if (create_backup)
    {
#if defined(HAVE_FCHOWN) && defined(HAVE_FCHMOD)
      struct stat tmp_statbuf;      
#endif
      char *backup_filename;
      int bfd;
      
      backup_filename = create_backup_filename (filename);

      if (g_unlink (backup_filename) == -1 && errno != ENOENT)
	{
	  g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_CANT_CREATE_BACKUP,
                               _("Backup file creation failed"));
	  g_free (backup_filename);
	  goto err_out;
	}

      bfd = g_open (backup_filename,
		    O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
		    original_stat.st_mode & 0777);

      if (bfd == -1)
	{
	  g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_CANT_CREATE_BACKUP,
                               _("Backup file creation failed"));
	  g_free (backup_filename);
	  goto err_out;
	}

      /* If needed, Try to set the group of the backup same as the
       * original file. If this fails, set the protection
       * bits for the group same as the protection bits for
       * others. */
#if defined(HAVE_FCHOWN) && defined(HAVE_FCHMOD)
      if (fstat (bfd, &tmp_statbuf) != 0)
	{
	  g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_CANT_CREATE_BACKUP,
                               _("Backup file creation failed"));
	  g_unlink (backup_filename);
	  g_close (bfd, NULL);
	  g_free (backup_filename);
	  goto err_out;
	}
      
      if ((original_stat.st_gid != tmp_statbuf.st_gid)  &&
	  fchown (bfd, (uid_t) -1, original_stat.st_gid) != 0)
	{
	  if (fchmod (bfd,
		      (original_stat.st_mode & 0707) |
		      ((original_stat.st_mode & 07) << 3)) != 0)
	    {
	      g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_CANT_CREATE_BACKUP,
                                   _("Backup file creation failed"));
	      g_unlink (backup_filename);
	      g_close (bfd, NULL);
	      g_free (backup_filename);
	      goto err_out;
	    }
	}
#endif

      if (!copy_file_data (fd, bfd, NULL))
	{
	  g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_CANT_CREATE_BACKUP,
                               _("Backup file creation failed"));
	  g_unlink (backup_filename);
          g_close (bfd, NULL);
	  g_free (backup_filename);
	  
	  goto err_out;
	}
      
      g_close (bfd, NULL);
      g_free (backup_filename);

      /* Seek back to the start of the file after the backup copy */
      if (lseek (fd, 0, SEEK_SET) == -1)
	{
          int errsv = errno;

	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error seeking in file: %s"),
		       g_strerror (errsv));
	  goto err_out;
	}
    }

  if (flags & G_FILE_CREATE_REPLACE_DESTINATION)
    {
      g_close (fd, NULL);
      
      if (g_unlink (filename) != 0)
	{
	  int errsv = errno;
	  
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error removing old file: %s"),
		       g_strerror (errsv));
	  goto err_out2;
	}

      if (readable)
	open_flags = O_RDWR | O_CREAT | O_BINARY;
      else
	open_flags = O_WRONLY | O_CREAT | O_BINARY;
      fd = g_open (filename, open_flags, mode);
      if (fd == -1)
	{
	  int errsv = errno;
	  char *display_name = g_filename_display_name (filename);
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error opening file “%s”: %s"),
		       display_name, g_strerror (errsv));
	  g_free (display_name);
	  goto err_out2;
	}
    }
  else
    {
      /* Truncate the file at the start */
#ifdef G_OS_WIN32
      if (g_win32_ftruncate (fd, 0) == -1)
#else
	if (ftruncate (fd, 0) == -1)
#endif
	  {
	    int errsv = errno;
	    
	    g_set_error (error, G_IO_ERROR,
			 g_io_error_from_errno (errsv),
			 _("Error truncating file: %s"),
			 g_strerror (errsv));
	    goto err_out;
	  }
    }
    
  return fd;

 err_out:
  g_close (fd, NULL);
 err_out2:
  return -1;
}

GFileOutputStream *
_g_local_file_output_stream_replace (const char        *filename,
				     gboolean          readable,
				     const char        *etag,
				     gboolean           create_backup,
				     GFileCreateFlags   flags,
                                     GFileInfo         *reference_info,
				     GCancellable      *cancellable,
				     GError           **error)
{
  GLocalFileOutputStream *stream;
  int mode;
  int fd;
  char *temp_file;
  gboolean sync_on_close;
  int open_flags;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  temp_file = NULL;

  mode = mode_from_flags_or_info (flags, reference_info);
  sync_on_close = FALSE;

  /* If the file doesn't exist, create it */
  open_flags = O_CREAT | O_EXCL | O_BINARY;
  if (readable)
    open_flags |= O_RDWR;
  else
    open_flags |= O_WRONLY;
  fd = g_open (filename, open_flags, mode);

  if (fd == -1 && errno == EEXIST)
    {
      /* The file already exists */
      fd = handle_overwrite_open (filename, readable, etag,
                                  create_backup, &temp_file,
                                  flags, reference_info,
                                  cancellable, error);
      if (fd == -1)
	return NULL;

      /* If the final destination exists, we want to sync the newly written
       * file to ensure the data is on disk when we rename over the destination.
       * otherwise if we get a system crash we can lose both the new and the
       * old file on some filesystems. (I.E. those that don't guarantee the
       * data is written to the disk before the metadata.)
       */
      sync_on_close = TRUE;
    }
  else if (fd == -1)
    {
      set_error_from_open_errno (filename, error);
      return NULL;
    }
  
 
  stream = g_object_new (G_TYPE_LOCAL_FILE_OUTPUT_STREAM, NULL);
  stream->priv->fd = fd;
  stream->priv->sync_on_close = sync_on_close;
  stream->priv->tmp_filename = temp_file;
  if (create_backup)
    stream->priv->backup_filename = create_backup_filename (filename);
  stream->priv->original_filename =  g_strdup (filename);
  
  return G_FILE_OUTPUT_STREAM (stream);
}

gint
_g_local_file_output_stream_get_fd (GLocalFileOutputStream *stream)
{
  g_return_val_if_fail (G_IS_LOCAL_FILE_OUTPUT_STREAM (stream), -1);
  return stream->priv->fd;
}

#ifdef G_OS_UNIX
static int
g_local_file_output_stream_get_fd (GFileDescriptorBased *fd_based)
{
  GLocalFileOutputStream *stream = G_LOCAL_FILE_OUTPUT_STREAM (fd_based);
  return _g_local_file_output_stream_get_fd (stream);
}
#endif
