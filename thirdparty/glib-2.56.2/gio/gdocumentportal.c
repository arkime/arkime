/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Endless Mobile, Inc.
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

#include "config.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "gdocumentportal.h"
#include "xdp-dbus.h"
#include "gstdio.h"

#ifdef G_OS_UNIX
#include "gunixfdlist.h"
#endif

#ifndef O_PATH
#define O_PATH 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#else
#define HAVE_O_CLOEXEC 1
#endif

static GXdpDocuments *documents;
static char *documents_mountpoint;

static gboolean
init_document_portal (void)
{
  static gsize documents_inited = 0;

  if (g_once_init_enter (&documents_inited))
    {
      GError *error = NULL;
      GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

      if (connection != NULL)
        {
          documents = gxdp_documents_proxy_new_sync (connection, 0,
                                                     "org.freedesktop.portal.Documents",
                                                     "/org/freedesktop/portal/documents",
                                                     NULL, &error);
          if (documents != NULL)
            {
              gxdp_documents_call_get_mount_point_sync (documents,
                                                        &documents_mountpoint,
                                                        NULL, &error);

              if (error != NULL)
                {
                  g_warning ("Cannot get document portal mount point: %s", error->message);
                  g_error_free (error);
                }
            }
          else
            {
              g_warning ("Cannot create document portal proxy: %s", error->message);
              g_error_free (error);
            }

          g_object_unref (connection);
        }
      else
        {
          g_warning ("Cannot connect to session bus when initializing document portal: %s",
                     error->message);
          g_error_free (error);
        }

      g_once_init_leave (&documents_inited, 1);
    }

  return (documents != NULL && documents_mountpoint != NULL);
}

char *
g_document_portal_add_document (GFile   *file,
                                GError **error)
{
  char *doc_path, *basename;
  char *doc_id = NULL;
  char *doc_uri = NULL;
  char *path = NULL;
  GUnixFDList *fd_list = NULL;
  int fd, fd_in, errsv;
  gboolean ret;

  if (!init_document_portal ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "Document portal is not available");
      goto out;
    }

  path = g_file_get_path (file);
  fd = g_open (path, O_PATH | O_CLOEXEC);
  errsv = errno;

  if (fd == -1)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Failed to open %s", path);
      goto out;
    }

#ifndef HAVE_O_CLOEXEC
  fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif

  fd_list = g_unix_fd_list_new ();
  fd_in = g_unix_fd_list_append (fd_list, fd, error);
  g_close (fd, NULL);

  if (fd_in == -1)
    goto out;

  ret = gxdp_documents_call_add_sync (documents,
                                      g_variant_new_handle (fd_in),
                                      TRUE,
                                      TRUE,
                                      fd_list,
                                      &doc_id,
                                      NULL,
                                      NULL,
                                      error);

  if (!ret)
    goto out;

  basename = g_path_get_basename (path);
  doc_path = g_build_filename (documents_mountpoint, doc_id, basename, NULL);
  g_free (basename);

  doc_uri = g_filename_to_uri (doc_path, NULL, NULL);
  g_free (doc_path);

 out:
  if (fd_list)
    g_object_unref (fd_list);
  g_free (path);
  g_free (doc_id);

  return doc_uri;
}

/* Flags accepted by org.freedesktop.portal.Documents.AddFull */
enum {
  XDP_ADD_FLAGS_REUSE_EXISTING             =  (1 << 0),
  XDP_ADD_FLAGS_PERSISTENT                 =  (1 << 1),
  XDP_ADD_FLAGS_AS_NEEDED_BY_APP           =  (1 << 2),
  XDP_ADD_FLAGS_FLAGS_ALL                  = ((1 << 3) - 1)
};

GList *
g_document_portal_add_documents (GList       *uris,
                                 const char  *app_id,
                                 GError     **error)
{
  int length;
  GList *ruris = NULL;
  gboolean *as_is;
  GVariantBuilder builder;
  GUnixFDList *fd_list = NULL;
  GList *l;
  gsize i, j;
  const char *permissions[] = { "read", "write", NULL };
  char **doc_ids = NULL;
  GVariant *extra_out = NULL;

  if (!init_document_portal ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "Document portal is not available");
      return NULL;
    }

  length = g_list_length (uris);
  as_is = g_new0 (gboolean, length);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("ah"));

  fd_list = g_unix_fd_list_new ();
  for (l = uris, i = 0; l; l = l->next, i++)
    {
      const char *uri = l->data;
      int idx = -1;
      g_autofree char *path = NULL;

      path = g_filename_from_uri (uri, NULL, NULL);
      if (path != NULL)
        {
          int fd;

          fd = g_open (path, O_CLOEXEC | O_PATH);
          if (fd >= 0)
            {
#ifndef HAVE_O_CLOEXEC
              fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
              idx = g_unix_fd_list_append (fd_list, fd, NULL);
              close (fd);
            }
        }

      if (idx != -1)
        g_variant_builder_add (&builder, "h", idx);
      else
        as_is[i] = TRUE;
    }

  if (g_unix_fd_list_get_length (fd_list) > 0)
    {
      if (!gxdp_documents_call_add_full_sync (documents,
                                              g_variant_builder_end (&builder),
                                              XDP_ADD_FLAGS_AS_NEEDED_BY_APP,
                                              app_id,
                                              permissions,
                                              fd_list,
                                              &doc_ids,
                                              &extra_out,
                                              NULL,
                                              NULL,
                                              error))
        goto out;

      for (l = uris, i = 0, j = 0; l; l = l->next, i++)
        {
          const char *uri = l->data;
          char *ruri;

          if (as_is[i]) /* use as-is, not a file uri */
            {
              ruri = g_strdup (uri);
            }
          else if (strcmp (doc_ids[j], "") == 0) /* not rewritten */
            {
              ruri = g_strdup (uri);
              j++;
            }
          else
            {
              char *basename = g_path_get_basename (uri + strlen ("file:"));
              char *doc_path = g_build_filename (documents_mountpoint, doc_ids[j], basename, NULL);
              ruri = g_strconcat ("file:", doc_path, NULL);
              g_free (basename);
              g_free (doc_path);
              j++;
            }

          ruris = g_list_prepend (ruris, ruri);
        }

      ruris = g_list_reverse (ruris);
    }
  else
    {
      ruris = g_list_copy_deep (uris, (GCopyFunc)g_strdup, NULL);
    }

out:
  g_clear_object (&fd_list);
  g_clear_pointer (&extra_out, g_variant_unref);
  g_clear_pointer (&doc_ids, g_strfreev);
  g_free (as_is);

  return ruris;
}
