/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2008 Novell, Inc.
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
 * Author: Tor Lillqvist <tml@novell.com>
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "gio/gfile.h"
#include "gio/gfileattribute.h"
#include "gio/gfileinfo.h"
#include "gwinhttpfile.h"
#include "gwinhttpfileinputstream.h"
#include "gwinhttpfileoutputstream.h"
#include "gio/gioerror.h"

#include "glibintl.h"

static void g_winhttp_file_file_iface_init (GFileIface *iface);

#define g_winhttp_file_get_type _g_winhttp_file_get_type
G_DEFINE_TYPE_WITH_CODE (GWinHttpFile, g_winhttp_file, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_FILE,
                                                g_winhttp_file_file_iface_init))

static void
g_winhttp_file_finalize (GObject *object)
{
  GWinHttpFile *file;

  file = G_WINHTTP_FILE (object);

  g_free (file->url.lpszScheme);
  g_free (file->url.lpszHostName);
  g_free (file->url.lpszUserName);
  g_free (file->url.lpszPassword);
  g_free (file->url.lpszUrlPath);
  g_free (file->url.lpszExtraInfo);

  g_object_unref (file->vfs);

  G_OBJECT_CLASS (g_winhttp_file_parent_class)->finalize (object);
}

static void
g_winhttp_file_class_init (GWinHttpFileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_winhttp_file_finalize;
}

static void
g_winhttp_file_init (GWinHttpFile *winhttp)
{
}

/*
 * _g_winhttp_file_new:
 * @vfs: GWinHttpVfs to use
 * @uri: URI of the GWinHttpFile to create.
 *
 * Returns: new winhttp #GFile.
 */
GFile *
_g_winhttp_file_new (GWinHttpVfs *vfs,
                     const char  *uri)
{
  wchar_t *wuri;
  GWinHttpFile *file;

  wuri = g_utf8_to_utf16 (uri, -1, NULL, NULL, NULL);

  if (wuri == NULL)
    return NULL;

  file = g_object_new (G_TYPE_WINHTTP_FILE, NULL);
  file->vfs = g_object_ref (vfs);

  memset (&file->url, 0, sizeof (file->url));
  file->url.dwStructSize = sizeof (file->url);
  file->url.dwSchemeLength = 1;
  file->url.dwHostNameLength = 1;
  file->url.dwUserNameLength = 1;
  file->url.dwPasswordLength = 1;
  file->url.dwUrlPathLength = 1;
  file->url.dwExtraInfoLength = 1;

  if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpCrackUrl (wuri, 0, 0, &file->url))
    {
      g_free (wuri);
      return NULL;
    }

  file->url.lpszScheme = g_new (wchar_t, ++file->url.dwSchemeLength);
  file->url.lpszHostName = g_new (wchar_t, ++file->url.dwHostNameLength);
  file->url.lpszUserName = g_new (wchar_t, ++file->url.dwUserNameLength);
  file->url.lpszPassword = g_new (wchar_t, ++file->url.dwPasswordLength);
  file->url.lpszUrlPath = g_new (wchar_t, ++file->url.dwUrlPathLength);
  file->url.lpszExtraInfo = g_new (wchar_t, ++file->url.dwExtraInfoLength);

  if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpCrackUrl (wuri, 0, 0, &file->url))
    {
      g_free (file->url.lpszScheme);
      g_free (file->url.lpszHostName);
      g_free (file->url.lpszUserName);
      g_free (file->url.lpszPassword);
      g_free (file->url.lpszUrlPath);
      g_free (file->url.lpszExtraInfo);
      g_free (wuri);
      return NULL;
    }

  g_free (wuri);
  return G_FILE (file);
}

static gboolean
g_winhttp_file_is_native (GFile *file)
{
  return FALSE;
}

static gboolean
g_winhttp_file_has_uri_scheme (GFile      *file,
                               const char *uri_scheme)
{
  return (g_ascii_strcasecmp (uri_scheme, "http") == 0 ||
          g_ascii_strcasecmp (uri_scheme, "https") == 0);
}

static char *
g_winhttp_file_get_uri_scheme (GFile *file)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);

  return g_utf16_to_utf8 (winhttp_file->url.lpszScheme, -1, NULL, NULL, NULL);
}

static char *
g_winhttp_file_get_basename (GFile *file)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);
  char *basename;
  char *last_slash;
  char *retval;

  basename = g_utf16_to_utf8 (winhttp_file->url.lpszUrlPath, -1, NULL, NULL, NULL);
  last_slash = strrchr (basename, '/');
  /* If no slash, or only "/" fallback to full path part of URI */
  if (last_slash == NULL || last_slash[1] == '\0')
    return basename;

  retval = g_strdup (last_slash + 1);
  g_free (basename);

  return retval;
}

static char *
g_winhttp_file_get_path (GFile *file)
{
  return NULL;
}

static char *
g_winhttp_file_get_uri (GFile *file)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);
  DWORD len;
  wchar_t *wuri;
  char *retval;

  len = 0;
  if (!G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpCreateUrl (&winhttp_file->url, ICU_ESCAPE, NULL, &len) &&
      GetLastError () != ERROR_INSUFFICIENT_BUFFER)
    return NULL;

  wuri = g_new (wchar_t, ++len);

  if (!G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpCreateUrl (&winhttp_file->url, ICU_ESCAPE, wuri, &len))
    {
      g_free (wuri);
      return NULL;
    }

  retval = g_utf16_to_utf8 (wuri, -1, NULL, NULL, NULL);
  g_free (wuri);

  if (g_str_has_prefix (retval, "http://:@"))
    {
      memmove (retval + 7, retval + 9, strlen (retval) - 9);
      retval[strlen (retval) - 2] = '\0';
    }
  else if (g_str_has_prefix (retval, "https://:@"))
    {
      memmove (retval + 8, retval + 10, strlen (retval) - 10);
      retval[strlen (retval) - 2] = '\0';
    }

  return retval;
}

static char *
g_winhttp_file_get_parse_name (GFile *file)
{
  /* FIXME: More hair surely needed */

  return g_winhttp_file_get_uri (file);
}

static GFile *
g_winhttp_file_get_parent (GFile *file)
{
  GWinHttpFile *winhttp_file;
  char *uri;
  char *last_slash;
  GFile *parent;

  winhttp_file = G_WINHTTP_FILE (file);

  uri = g_winhttp_file_get_uri (file);
  if (uri == NULL)
    return NULL;

  last_slash = strrchr (uri, '/');
  if (last_slash == NULL || *(last_slash+1) == 0)
    {
      g_free (uri);
      return NULL;
    }

  while (last_slash > uri && *last_slash == '/')
    last_slash--;

  last_slash[1] = '\0';

  parent = _g_winhttp_file_new (winhttp_file->vfs, uri);
  g_free (uri);

  return parent;
}

static GFile *
g_winhttp_file_dup (GFile *file)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);
  char *uri = g_winhttp_file_get_uri (file);
  GFile *retval = _g_winhttp_file_new (winhttp_file->vfs, uri);

  g_free (uri);

  return retval;
}

static guint
g_winhttp_file_hash (GFile *file)
{
  char *uri = g_winhttp_file_get_uri (file);
  guint retval = g_str_hash (uri);

  g_free (uri);

  return retval;
}

static gboolean
g_winhttp_file_equal (GFile *file1,
                      GFile *file2)
{
  char *uri1 = g_winhttp_file_get_uri (file1);
  char *uri2 = g_winhttp_file_get_uri (file2);
  gboolean retval = g_str_equal (uri1, uri2);

  g_free (uri1);
  g_free (uri2);

  return retval;
}

static const char *
match_prefix (const char *path,
              const char *prefix)
{
  int prefix_len;

  prefix_len = strlen (prefix);
  if (strncmp (path, prefix, prefix_len) != 0)
    return NULL;

  if (prefix_len > 0 && prefix[prefix_len-1] == '/')
    prefix_len--;

  return path + prefix_len;
}

static gboolean
g_winhttp_file_prefix_matches (GFile *parent,
                               GFile *descendant)
{
  char *parent_uri = g_winhttp_file_get_uri (parent);
  char *descendant_uri = g_winhttp_file_get_uri (descendant);
  const char *remainder;
  gboolean retval;

  remainder = match_prefix (descendant_uri, parent_uri);

  if (remainder != NULL && *remainder == '/')
    retval = TRUE;
  else
    retval = FALSE;

  g_free (parent_uri);
  g_free (descendant_uri);

  return retval;
}

static char *
g_winhttp_file_get_relative_path (GFile *parent,
                                  GFile *descendant)
{
  char *parent_uri = g_winhttp_file_get_uri (parent);
  char *descendant_uri = g_winhttp_file_get_uri (descendant);
  const char *remainder;
  char *retval;

  remainder = match_prefix (descendant_uri, parent_uri);

  if (remainder != NULL && *remainder == '/')
    retval = g_strdup (remainder + 1);
  else
    retval = NULL;

  g_free (parent_uri);
  g_free (descendant_uri);

  return retval;
}

static GFile *
g_winhttp_file_resolve_relative_path (GFile      *file,
                                      const char *relative_path)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);
  GWinHttpFile *child;
  wchar_t *wnew_path = g_utf8_to_utf16 (relative_path, -1, NULL, NULL, NULL);

  if (wnew_path == NULL)
    return NULL;

  if (*wnew_path != '/')
    {
      wchar_t *tmp = NULL;
      int trailing_slash = winhttp_file->url.lpszUrlPath[winhttp_file->url.dwUrlPathLength-1] == L'/'? 1 : 0;
      if (trailing_slash)
	{
	  tmp = g_new (wchar_t, wcslen (winhttp_file->url.lpszUrlPath) + wcslen (wnew_path) + 1);
	  wcscpy (tmp, winhttp_file->url.lpszUrlPath);
	}
      else
	{
	  tmp = g_new (wchar_t, wcslen (winhttp_file->url.lpszUrlPath) + 1 + wcslen (wnew_path) + 1);
	  wcscpy (tmp, winhttp_file->url.lpszUrlPath);
	  wcscat (tmp, L"/");
	}
      wcscat (tmp, wnew_path);

      g_free (wnew_path);
      wnew_path = tmp;
    }

  child = g_object_new (G_TYPE_WINHTTP_FILE, NULL);
  child->vfs = winhttp_file->vfs;
  child->url = winhttp_file->url;
  child->url.lpszScheme = g_memdup (winhttp_file->url.lpszScheme, (winhttp_file->url.dwSchemeLength+1)*2);
  child->url.lpszHostName = g_memdup (winhttp_file->url.lpszHostName, (winhttp_file->url.dwHostNameLength+1)*2);
  child->url.lpszUserName = g_memdup (winhttp_file->url.lpszUserName, (winhttp_file->url.dwUserNameLength+1)*2);
  child->url.lpszPassword = g_memdup (winhttp_file->url.lpszPassword, (winhttp_file->url.dwPasswordLength+1)*2);
  child->url.lpszUrlPath = wnew_path;
  child->url.dwUrlPathLength = wcslen (wnew_path);
  child->url.lpszExtraInfo = NULL;
  child->url.dwExtraInfoLength = 0;

  return (GFile *) child;
}

static GFile *
g_winhttp_file_get_child_for_display_name (GFile        *file,
                                           const char   *display_name,
                                           GError      **error)
{
  GFile *new_file;
  char *basename;

  basename = g_locale_from_utf8 (display_name, -1, NULL, NULL, NULL);
  if (basename == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
                   _("Invalid filename %s"), display_name);
      return NULL;
    }

  new_file = g_file_get_child (file, basename);
  g_free (basename);

  return new_file;
}

static GFile *
g_winhttp_file_set_display_name (GFile         *file,
                                 const char    *display_name,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("Operation not supported"));

  return NULL;
}

static time_t
mktime_utc (SYSTEMTIME *t)
{
  time_t retval;

  static const gint days_before[] =
  {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };

  if (t->wMonth < 1 || t->wMonth > 12)
    return (time_t) -1;

  retval = (t->wYear - 1970) * 365;
  retval += (t->wYear - 1968) / 4;
  retval += days_before[t->wMonth-1] + t->wDay - 1;

  if (t->wYear % 4 == 0 && t->wMonth < 3)
    retval -= 1;

  retval = ((((retval * 24) + t->wHour) * 60) + t->wMinute) * 60 + t->wSecond;

  return retval;
}

static GFileInfo *
g_winhttp_file_query_info (GFile                *file,
                           const char           *attributes,
                           GFileQueryInfoFlags   flags,
                           GCancellable         *cancellable,
                           GError              **error)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);
  HINTERNET connection, request;
  const wchar_t *accept_types[] =
    {
      L"*/*",
      NULL,
    };
  GFileInfo *info;
  GFileAttributeMatcher *matcher;
  char *basename;
  wchar_t *content_length;
  wchar_t *content_type;
  SYSTEMTIME last_modified;
  DWORD last_modified_len;

  connection = G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpConnect
    (G_WINHTTP_VFS (winhttp_file->vfs)->session,
     winhttp_file->url.lpszHostName,
     winhttp_file->url.nPort,
     0);

  if (connection == NULL)
    {
      _g_winhttp_set_error (error, GetLastError (), "HTTP connection");

      return NULL;
    }

  request = G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpOpenRequest
    (connection,
     L"HEAD",
     winhttp_file->url.lpszUrlPath,
     NULL,
     WINHTTP_NO_REFERER,
     accept_types,
     winhttp_file->url.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);

  if (request == NULL)
    {
      _g_winhttp_set_error (error, GetLastError (), "HEAD request");

      return NULL;
    }

  if (!G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpSendRequest
      (request,
       NULL, 0,
       NULL, 0,
       0,
       0))
    {
      _g_winhttp_set_error (error, GetLastError (), "HEAD request");

      return NULL;
    }

  if (!_g_winhttp_response (winhttp_file->vfs, request, error, "HEAD request"))
    return NULL;

  matcher = g_file_attribute_matcher_new (attributes);
  info = g_file_info_new ();
  g_file_info_set_attribute_mask (info, matcher);

  basename = g_winhttp_file_get_basename (file);
  g_file_info_set_name (info, basename);
  g_free (basename);

  content_length = NULL;
  if (_g_winhttp_query_header (winhttp_file->vfs,
                               request,
                               "HEAD request",
                               WINHTTP_QUERY_CONTENT_LENGTH,
                               &content_length,
                               NULL))
    {
      gint64 cl;
      int n;
      const char *gint64_format = "%"G_GINT64_FORMAT"%n";
      wchar_t *gint64_format_w = g_utf8_to_utf16 (gint64_format, -1, NULL, NULL, NULL);

      if (swscanf (content_length, gint64_format_w, &cl, &n) == 1 &&
          n == wcslen (content_length))
        g_file_info_set_size (info, cl);

      g_free (content_length);
      g_free (gint64_format_w);
    }

  if (matcher == NULL)
    return info;

  content_type = NULL;
  if (_g_winhttp_query_header (winhttp_file->vfs,
                               request,
                               "HEAD request",
                               WINHTTP_QUERY_CONTENT_TYPE,
                               &content_type,
                               NULL))
    {
      char *ct = g_utf16_to_utf8 (content_type, -1, NULL, NULL, NULL);

      if (ct != NULL)
        {
          char *p = strchr (ct, ';');

          if (p != NULL)
            {
              char *tmp = g_strndup (ct, p - ct);

              g_file_info_set_content_type (info, tmp);
              g_free (tmp);
            }
          else
            g_file_info_set_content_type (info, ct);
        }

      g_free (ct);
    }

  last_modified_len = sizeof (last_modified);
  if (G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpQueryHeaders
      (request,
       WINHTTP_QUERY_LAST_MODIFIED | WINHTTP_QUERY_FLAG_SYSTEMTIME,
       NULL,
       &last_modified,
       &last_modified_len,
       NULL) &&
      last_modified_len == sizeof (last_modified) &&
      /* Don't bother comparing to the exact Y2038 moment */
      last_modified.wYear >= 1970 &&
      last_modified.wYear < 2038)
    {
      GTimeVal tv;

      tv.tv_sec = mktime_utc (&last_modified);
      tv.tv_usec = last_modified.wMilliseconds * 1000;

      g_file_info_set_modification_time (info, &tv);
    }

  g_file_attribute_matcher_unref (matcher);

  return info;
}

static GFileInputStream *
g_winhttp_file_read (GFile         *file,
                     GCancellable  *cancellable,
                     GError       **error)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);
  HINTERNET connection, request;
  const wchar_t *accept_types[] =
    {
      L"*/*",
      NULL,
    };

  connection = G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpConnect
    (G_WINHTTP_VFS (winhttp_file->vfs)->session,
     winhttp_file->url.lpszHostName,
     winhttp_file->url.nPort,
     0);

  if (connection == NULL)
    {
      _g_winhttp_set_error (error, GetLastError (), "HTTP connection");

      return NULL;
    }

  request = G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpOpenRequest
    (connection,
     L"GET",
     winhttp_file->url.lpszUrlPath,
     NULL,
     WINHTTP_NO_REFERER,
     accept_types,
     winhttp_file->url.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);

  if (request == NULL)
    {
      _g_winhttp_set_error (error, GetLastError (), "GET request");

      return NULL;
    }

  return _g_winhttp_file_input_stream_new (winhttp_file, connection, request);
}

static GFileOutputStream *
g_winhttp_file_create (GFile             *file,
                       GFileCreateFlags   flags,
                       GCancellable      *cancellable,
                       GError           **error)
{
  GWinHttpFile *winhttp_file = G_WINHTTP_FILE (file);
  HINTERNET connection;

  connection = G_WINHTTP_VFS_GET_CLASS (winhttp_file->vfs)->funcs->pWinHttpConnect
    (G_WINHTTP_VFS (winhttp_file->vfs)->session,
     winhttp_file->url.lpszHostName,
     winhttp_file->url.nPort,
     0);

  if (connection == NULL)
    {
      _g_winhttp_set_error (error, GetLastError (), "HTTP connection");

      return NULL;
    }

  return _g_winhttp_file_output_stream_new (winhttp_file, connection);
}

#if 0

static GFileOutputStream *
g_winhttp_file_replace (GFile             *file,
                        const char        *etag,
                        gboolean           make_backup,
                        GFileCreateFlags   flags,
                        GCancellable      *cancellable,
                        GError           **error)
{
  /* FIXME: Implement */

  return NULL;
}


static gboolean
g_winhttp_file_delete (GFile         *file,
                       GCancellable  *cancellable,
                       GError       **error)
{
  /* FIXME: Implement */

  return FALSE;
}

static gboolean
g_winhttp_file_make_directory (GFile         *file,
                               GCancellable  *cancellable,
                               GError       **error)
{
  /* FIXME: Implement */

  return FALSE;
}

static gboolean
g_winhttp_file_copy (GFile                  *source,
                     GFile                  *destination,
                     GFileCopyFlags          flags,
                     GCancellable           *cancellable,
                     GFileProgressCallback   progress_callback,
                     gpointer                progress_callback_data,
                     GError                **error)
{
  /* Fall back to default copy?? */
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       "Copy not supported");

  return FALSE;
}

static gboolean
g_winhttp_file_move (GFile                  *source,
                     GFile                  *destination,
                     GFileCopyFlags          flags,
                     GCancellable           *cancellable,
                     GFileProgressCallback   progress_callback,
                     gpointer                progress_callback_data,
                     GError                **error)
{
  /* FIXME: Implement */

  return FALSE;
}

#endif

static void
g_winhttp_file_file_iface_init (GFileIface *iface)
{
  iface->dup = g_winhttp_file_dup;
  iface->hash = g_winhttp_file_hash;
  iface->equal = g_winhttp_file_equal;
  iface->is_native = g_winhttp_file_is_native;
  iface->has_uri_scheme = g_winhttp_file_has_uri_scheme;
  iface->get_uri_scheme = g_winhttp_file_get_uri_scheme;
  iface->get_basename = g_winhttp_file_get_basename;
  iface->get_path = g_winhttp_file_get_path;
  iface->get_uri = g_winhttp_file_get_uri;
  iface->get_parse_name = g_winhttp_file_get_parse_name;
  iface->get_parent = g_winhttp_file_get_parent;
  iface->prefix_matches = g_winhttp_file_prefix_matches;
  iface->get_relative_path = g_winhttp_file_get_relative_path;
  iface->resolve_relative_path = g_winhttp_file_resolve_relative_path;
  iface->get_child_for_display_name = g_winhttp_file_get_child_for_display_name;
  iface->set_display_name = g_winhttp_file_set_display_name;
  iface->query_info = g_winhttp_file_query_info;
  iface->read_fn = g_winhttp_file_read;
  iface->create = g_winhttp_file_create;
#if 0
  iface->replace = g_winhttp_file_replace;
  iface->delete_file = g_winhttp_file_delete;
  iface->make_directory = g_winhttp_file_make_directory;
  iface->copy = g_winhttp_file_copy;
  iface->move = g_winhttp_file_move;
#endif
}
