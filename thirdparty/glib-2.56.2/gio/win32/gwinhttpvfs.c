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

#include <wchar.h>

#include "gio/gioerror.h"
#include "gio/giomodule.h"
#include "gio/gvfs.h"

#include "gwinhttpfile.h"
#include "gwinhttpvfs.h"

static gboolean lookup_done = FALSE;
static gboolean funcs_found = FALSE;
static GWinHttpDllFuncs funcs;

static void
lookup_funcs (void)
{
  HMODULE winhttp = NULL;
  char winhttp_dll[MAX_PATH + 100];
  int n;

  if (lookup_done)
    return;

  n = GetSystemDirectory (winhttp_dll, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
    {
        if (winhttp_dll[n-1] != '\\' &&
            winhttp_dll[n-1] != '/')
            strcat (winhttp_dll, "\\");
        strcat (winhttp_dll, "winhttp.dll");
        winhttp = LoadLibrary (winhttp_dll);
    }

  if (winhttp != NULL)
    {
      funcs.pWinHttpCloseHandle = (BOOL (WINAPI *) (HINTERNET)) GetProcAddress (winhttp, "WinHttpCloseHandle");
      funcs.pWinHttpCrackUrl = (BOOL (WINAPI *) (LPCWSTR,DWORD,DWORD,LPURL_COMPONENTS)) GetProcAddress (winhttp, "WinHttpCrackUrl");
      funcs.pWinHttpConnect = (HINTERNET (WINAPI *) (HINTERNET,LPCWSTR,INTERNET_PORT,DWORD)) GetProcAddress (winhttp, "WinHttpConnect");
      funcs.pWinHttpCreateUrl = (BOOL (WINAPI *) (LPURL_COMPONENTS,DWORD,LPWSTR,LPDWORD)) GetProcAddress (winhttp, "WinHttpCreateUrl");
      funcs.pWinHttpOpen = (HINTERNET (WINAPI *) (LPCWSTR,DWORD,LPCWSTR,LPCWSTR,DWORD)) GetProcAddress (winhttp, "WinHttpOpen");
      funcs.pWinHttpOpenRequest = (HINTERNET (WINAPI *) (HINTERNET,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR*,DWORD)) GetProcAddress (winhttp, "WinHttpOpenRequest");
      funcs.pWinHttpQueryDataAvailable = (BOOL (WINAPI *) (HINTERNET,LPDWORD)) GetProcAddress (winhttp, "WinHttpQueryDataAvailable");
      funcs.pWinHttpQueryHeaders = (BOOL (WINAPI *) (HINTERNET,DWORD,LPCWSTR,LPVOID,LPDWORD,LPDWORD)) GetProcAddress (winhttp, "WinHttpQueryHeaders");
      funcs.pWinHttpReadData = (BOOL (WINAPI *) (HINTERNET,LPVOID,DWORD,LPDWORD)) GetProcAddress (winhttp, "WinHttpReadData");
      funcs.pWinHttpReceiveResponse = (BOOL (WINAPI *) (HINTERNET,LPVOID)) GetProcAddress (winhttp, "WinHttpReceiveResponse");
      funcs.pWinHttpSendRequest = (BOOL (WINAPI *) (HINTERNET,LPCWSTR,DWORD,LPVOID,DWORD,DWORD,DWORD_PTR)) GetProcAddress (winhttp, "WinHttpSendRequest");
      funcs.pWinHttpWriteData = (BOOL (WINAPI *) (HINTERNET,LPCVOID,DWORD,LPDWORD)) GetProcAddress (winhttp, "WinHttpWriteData");

      if (funcs.pWinHttpCloseHandle &&
	  funcs.pWinHttpCrackUrl &&
	  funcs.pWinHttpConnect &&
	  funcs.pWinHttpCreateUrl &&
	  funcs.pWinHttpOpen &&
	  funcs.pWinHttpOpenRequest &&
	  funcs.pWinHttpQueryDataAvailable &&
	  funcs.pWinHttpQueryHeaders &&
	  funcs.pWinHttpReadData &&
	  funcs.pWinHttpReceiveResponse &&
	  funcs.pWinHttpSendRequest &&
	  funcs.pWinHttpWriteData)
	funcs_found = TRUE;
    }
  lookup_done = TRUE;
}

#define g_winhttp_vfs_get_type _g_winhttp_vfs_get_type
G_DEFINE_TYPE_WITH_CODE (GWinHttpVfs, g_winhttp_vfs, G_TYPE_VFS,
			 {
			   lookup_funcs ();
			   if (funcs_found)
			     g_io_extension_point_implement (G_VFS_EXTENSION_POINT_NAME,
							     g_define_type_id,
							     "winhttp",
							     10);
			 })

static const gchar *winhttp_uri_schemes[] = { "http", "https" };

static void
g_winhttp_vfs_finalize (GObject *object)
{
  GWinHttpVfs *vfs;

  vfs = G_WINHTTP_VFS (object);

  (G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpCloseHandle) (vfs->session);
  vfs->session = NULL;

  if (vfs->wrapped_vfs)
    g_object_unref (vfs->wrapped_vfs);
  vfs->wrapped_vfs = NULL;

  G_OBJECT_CLASS (g_winhttp_vfs_parent_class)->finalize (object);
}

static void
g_winhttp_vfs_init (GWinHttpVfs *vfs)
{
  wchar_t *wagent;

  vfs->wrapped_vfs = g_vfs_get_local ();

  wagent = g_utf8_to_utf16 (g_get_prgname (), -1, NULL, NULL, NULL);

  if (!wagent)
    wagent = g_utf8_to_utf16 ("GWinHttpVfs", -1, NULL, NULL, NULL);

  vfs->session = (G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpOpen)
    (wagent,
     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
     WINHTTP_NO_PROXY_NAME,
     WINHTTP_NO_PROXY_BYPASS,
     0);

  g_free (wagent);
}

/**
 * g_winhttp_vfs_new:
 *
 * Returns a new #GVfs handle for a WinHttp vfs.
 *
 * Returns: a new #GVfs handle.
 **/
GVfs *
_g_winhttp_vfs_new (void)
{
  return g_object_new (G_TYPE_WINHTTP_VFS, NULL);
}

static GFile *
g_winhttp_vfs_get_file_for_path (GVfs       *vfs,
                                 const char *path)
{
  return g_vfs_get_file_for_path (G_WINHTTP_VFS (vfs)->wrapped_vfs, path);
}

static GFile *
g_winhttp_vfs_get_file_for_uri (GVfs       *vfs,
                                const char *uri)
{
  GWinHttpVfs *winhttp_vfs = G_WINHTTP_VFS (vfs);
  int i;

  /* If it matches one of "our" schemes, handle it */
  for (i = 0; i < G_N_ELEMENTS (winhttp_uri_schemes); i++)
    if (g_ascii_strncasecmp (uri, winhttp_uri_schemes[i], strlen (winhttp_uri_schemes[i])) == 0 &&
        uri[strlen (winhttp_uri_schemes[i])] == ':')
      return _g_winhttp_file_new (winhttp_vfs, uri);

  /* For other URIs fallback to the wrapped GVfs */
  return g_vfs_parse_name (winhttp_vfs->wrapped_vfs, uri);
}

static const gchar * const *
g_winhttp_vfs_get_supported_uri_schemes (GVfs *vfs)
{
  GWinHttpVfs *winhttp_vfs = G_WINHTTP_VFS (vfs);
  const gchar * const *wrapped_vfs_uri_schemes = g_vfs_get_supported_uri_schemes (winhttp_vfs->wrapped_vfs);
  int i, n;
  const gchar **retval;

  n = 0;
  while (wrapped_vfs_uri_schemes[n] != NULL)
    n++;

  retval = g_new (const gchar *, n + G_N_ELEMENTS (winhttp_uri_schemes) + 1);
  n = 0;
  while (wrapped_vfs_uri_schemes[n] != NULL)
    {
      retval[n] = wrapped_vfs_uri_schemes[n];
      n++;
    }

  for (i = 0; i < G_N_ELEMENTS (winhttp_uri_schemes); i++)
    {
      retval[n] = winhttp_uri_schemes[i];
      n++;
    }

  retval[n] = NULL;

  return retval;
}

static GFile *
g_winhttp_vfs_parse_name (GVfs       *vfs,
                          const char *parse_name)
{
  GWinHttpVfs *winhttp_vfs = G_WINHTTP_VFS (vfs);

  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (parse_name != NULL, NULL);

  /* For plain file paths fallback to the wrapped GVfs */
  if (g_path_is_absolute (parse_name))
    return g_vfs_parse_name (winhttp_vfs->wrapped_vfs, parse_name);

  /* Otherwise assume it is an URI, so pass on to
   * g_winhttp_vfs_get_file_for_uri().
   */
  return g_winhttp_vfs_get_file_for_uri (vfs, parse_name);
}

static gboolean
g_winhttp_vfs_is_active (GVfs *vfs)
{
  return TRUE;
}

static void
g_winhttp_vfs_class_init (GWinHttpVfsClass *class)
{
  GObjectClass *object_class;
  GVfsClass *vfs_class;

  object_class = (GObjectClass *) class;

  object_class->finalize = g_winhttp_vfs_finalize;

  vfs_class = G_VFS_CLASS (class);

  vfs_class->is_active = g_winhttp_vfs_is_active;
  vfs_class->get_file_for_path = g_winhttp_vfs_get_file_for_path;
  vfs_class->get_file_for_uri = g_winhttp_vfs_get_file_for_uri;
  vfs_class->get_supported_uri_schemes = g_winhttp_vfs_get_supported_uri_schemes;
  vfs_class->parse_name = g_winhttp_vfs_parse_name;

  lookup_funcs ();
  if (funcs_found)
    class->funcs = &funcs;
  else
    class->funcs = NULL;
}

char *
_g_winhttp_error_message (DWORD error_code)
{
  /* The FormatMessage() API that g_win32_error_message() uses doesn't
   * seem to know about WinHttp errors, unfortunately.
   */
  if (error_code >= WINHTTP_ERROR_BASE && error_code < WINHTTP_ERROR_BASE + 200)
    {
      switch (error_code)
        {
          /* FIXME: Use meaningful error messages */
#define CASE(x) case ERROR_WINHTTP_##x: return g_strdup ("WinHttp error: " #x);
          CASE (AUTO_PROXY_SERVICE_ERROR);
          CASE (AUTODETECTION_FAILED);
          CASE (BAD_AUTO_PROXY_SCRIPT);
          CASE (CANNOT_CALL_AFTER_OPEN);
          CASE (CANNOT_CALL_AFTER_SEND);
          CASE (CANNOT_CALL_BEFORE_OPEN);
          CASE (CANNOT_CALL_BEFORE_SEND);
          CASE (CANNOT_CONNECT);
          CASE (CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW);
          CASE (CLIENT_AUTH_CERT_NEEDED);
          CASE (CONNECTION_ERROR);
          CASE (HEADER_ALREADY_EXISTS);
          CASE (HEADER_COUNT_EXCEEDED);
          CASE (HEADER_NOT_FOUND);
          CASE (HEADER_SIZE_OVERFLOW);
          CASE (INCORRECT_HANDLE_STATE);
          CASE (INCORRECT_HANDLE_TYPE);
          CASE (INTERNAL_ERROR);
          CASE (INVALID_OPTION);
          CASE (INVALID_QUERY_REQUEST);
          CASE (INVALID_SERVER_RESPONSE);
          CASE (INVALID_URL);
          CASE (LOGIN_FAILURE);
          CASE (NAME_NOT_RESOLVED);
          CASE (NOT_INITIALIZED);
          CASE (OPERATION_CANCELLED);
          CASE (OPTION_NOT_SETTABLE);
          CASE (OUT_OF_HANDLES);
          CASE (REDIRECT_FAILED);
          CASE (RESEND_REQUEST);
          CASE (RESPONSE_DRAIN_OVERFLOW);
          CASE (SECURE_CERT_CN_INVALID);
          CASE (SECURE_CERT_DATE_INVALID);
          CASE (SECURE_CERT_REV_FAILED);
          CASE (SECURE_CERT_REVOKED);
          CASE (SECURE_CERT_WRONG_USAGE);
          CASE (SECURE_CHANNEL_ERROR);
          CASE (SECURE_FAILURE);
          CASE (SECURE_INVALID_CA);
          CASE (SECURE_INVALID_CERT);
          CASE (SHUTDOWN);
          CASE (TIMEOUT);
          CASE (UNABLE_TO_DOWNLOAD_SCRIPT);
          CASE (UNRECOGNIZED_SCHEME);
          #undef CASE
        default:
          return g_strdup_printf ("WinHttp error %ld", error_code);
        }
    }
  else
    return g_win32_error_message (error_code);
}

void
_g_winhttp_set_error (GError     **error,
                      DWORD        error_code,
                      const char  *what)
{
  char *emsg = _g_winhttp_error_message (error_code);

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "%s failed: %s", what, emsg);
  g_free (emsg);
}

gboolean
_g_winhttp_response (GWinHttpVfs *vfs,
                     HINTERNET    request,
                     GError     **error,
                     const char  *what)
{
  wchar_t *status_code;
  DWORD status_code_len;

  if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpReceiveResponse (request, NULL))
    {
      _g_winhttp_set_error (error, GetLastError (), what);

      return FALSE;
    }

  status_code_len = 0;
  if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpQueryHeaders
      (request,
       WINHTTP_QUERY_STATUS_CODE,
       NULL,
       NULL,
       &status_code_len,
       NULL) &&
      GetLastError () != ERROR_INSUFFICIENT_BUFFER)
    {
      _g_winhttp_set_error (error, GetLastError (), what);

      return FALSE;
    }

  status_code = g_malloc (status_code_len);

  if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpQueryHeaders
      (request,
       WINHTTP_QUERY_STATUS_CODE,
       NULL,
       status_code,
       &status_code_len,
       NULL))
    {
      _g_winhttp_set_error (error, GetLastError (), what);
      g_free (status_code);

      return FALSE;
    }

  if (status_code[0] != L'2')
    {
      wchar_t *status_text = NULL;
      DWORD status_text_len;

      if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpQueryHeaders
          (request,
           WINHTTP_QUERY_STATUS_TEXT,
           NULL,
           NULL,
           &status_text_len,
           NULL) &&
          GetLastError () == ERROR_INSUFFICIENT_BUFFER)
        {
          status_text = g_malloc (status_text_len);

          if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpQueryHeaders
              (request,
               WINHTTP_QUERY_STATUS_TEXT,
               NULL,
               status_text,
               &status_text_len,
               NULL))
            {
              g_free (status_text);
              status_text = NULL;
            }
        }

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "%s failed: %S %S",
                   what, status_code, status_text ? status_text : L"");
      g_free (status_code);
      g_free (status_text);

      return FALSE;
    }

  g_free (status_code);

  return TRUE;
}

gboolean
_g_winhttp_query_header (GWinHttpVfs *vfs,
                         HINTERNET    request,
                         const char  *request_description,
                         DWORD        which_header,
                         wchar_t    **header,
                         GError     **error)
{
  DWORD header_len = 0;

  if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpQueryHeaders
      (request,
       which_header,
       NULL,
       NULL,
       &header_len,
       NULL) &&
      GetLastError () != ERROR_INSUFFICIENT_BUFFER)
    {
      _g_winhttp_set_error (error, GetLastError (), request_description);

      return FALSE;
    }

  *header = g_malloc (header_len);
  if (!G_WINHTTP_VFS_GET_CLASS (vfs)->funcs->pWinHttpQueryHeaders
      (request,
       which_header,
       NULL,
       *header,
       &header_len,
       NULL))
    {
      _g_winhttp_set_error (error, GetLastError (), request_description);
      g_free (*header);
      *header = NULL;

      return FALSE;
    }

  return TRUE;
}
