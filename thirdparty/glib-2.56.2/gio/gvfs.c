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
#include <string.h>
#include "gvfs.h"
#include "glib-private.h"
#include "glocalvfs.h"
#include "gresourcefile.h"
#include "giomodule-priv.h"
#include "glibintl.h"


/**
 * SECTION:gvfs
 * @short_description: Virtual File System
 * @include: gio/gio.h
 *
 * Entry point for using GIO functionality.
 *
 */

static GRWLock additional_schemes_lock;

typedef struct _GVfsPrivate {
  GHashTable *additional_schemes;
  char const **supported_schemes;
} GVfsPrivate;

typedef struct {
  GVfsFileLookupFunc uri_func;
  gpointer uri_data;
  GDestroyNotify uri_destroy;

  GVfsFileLookupFunc parse_name_func;
  gpointer parse_name_data;
  GDestroyNotify parse_name_destroy;
} GVfsURISchemeData;

G_DEFINE_TYPE_WITH_PRIVATE (GVfs, g_vfs, G_TYPE_OBJECT)

static void
g_vfs_dispose (GObject *object)
{
  GVfs *vfs = G_VFS (object);
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);

  g_clear_pointer (&priv->additional_schemes, g_hash_table_destroy);
  g_clear_pointer (&priv->supported_schemes, g_free);

  G_OBJECT_CLASS (g_vfs_parent_class)->dispose (object);
}

static void
g_vfs_class_init (GVfsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = g_vfs_dispose;
}

static GFile *
resource_parse_name (GVfs       *vfs,
                     const char *parse_name,
                     gpointer    user_data)
{
  if (g_str_has_prefix (parse_name, "resource:"))
    return _g_resource_file_new (parse_name);

  return NULL;
}

static GFile *
resource_get_file_for_uri (GVfs       *vfs,
                           const char *uri,
                           gpointer    user_data)
{
  return _g_resource_file_new (uri);
}

static void
g_vfs_uri_lookup_func_closure_free (gpointer data)
{
  GVfsURISchemeData *closure = data;

  if (closure->uri_destroy)
    closure->uri_destroy (closure->uri_data);
  if (closure->parse_name_destroy)
    closure->parse_name_destroy (closure->parse_name_data);

  g_free (closure);
}

static void
g_vfs_init (GVfs *vfs)
{
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);
  priv->additional_schemes =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           g_free, g_vfs_uri_lookup_func_closure_free);

  g_vfs_register_uri_scheme (vfs, "resource",
                             resource_get_file_for_uri, NULL, NULL,
                             resource_parse_name, NULL, NULL);
}

/**
 * g_vfs_is_active:
 * @vfs: a #GVfs.
 *
 * Checks if the VFS is active.
 *
 * Returns: %TRUE if construction of the @vfs was successful
 *     and it is now active.
 */
gboolean
g_vfs_is_active (GVfs *vfs)
{
  GVfsClass *class;

  g_return_val_if_fail (G_IS_VFS (vfs), FALSE);

  class = G_VFS_GET_CLASS (vfs);

  return (* class->is_active) (vfs);
}


/**
 * g_vfs_get_file_for_path:
 * @vfs: a #GVfs.
 * @path: a string containing a VFS path.
 *
 * Gets a #GFile for @path.
 *
 * Returns: (transfer full): a #GFile.
 *     Free the returned object with g_object_unref().
 */
GFile *
g_vfs_get_file_for_path (GVfs       *vfs,
                         const char *path)
{
  GVfsClass *class;
 
  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  return (* class->get_file_for_path) (vfs, path);
}

static GFile *
parse_name_internal (GVfs       *vfs,
                     const char *parse_name)
{
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);
  GHashTableIter iter;
  GVfsURISchemeData *closure;
  GFile *ret = NULL;

  g_rw_lock_reader_lock (&additional_schemes_lock);
  g_hash_table_iter_init (&iter, priv->additional_schemes);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &closure))
    {
      ret = closure->parse_name_func (vfs, parse_name,
                                      closure->parse_name_data);

      if (ret)
        break;
    }

  g_rw_lock_reader_unlock (&additional_schemes_lock);

  return ret;
}

static GFile *
get_file_for_uri_internal (GVfs       *vfs,
                           const char *uri)
{
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);
  GFile *ret = NULL;
  char *scheme;
  GVfsURISchemeData *closure;

  scheme = g_uri_parse_scheme (uri);
  if (scheme == NULL)
    return NULL;

  g_rw_lock_reader_lock (&additional_schemes_lock);
  closure = g_hash_table_lookup (priv->additional_schemes, scheme);

  if (closure)
    ret = closure->uri_func (vfs, uri, closure->uri_data);

  g_rw_lock_reader_unlock (&additional_schemes_lock);

  g_free (scheme);
  return ret;
}

/**
 * g_vfs_get_file_for_uri:
 * @vfs: a#GVfs.
 * @uri: a string containing a URI
 *
 * Gets a #GFile for @uri.
 *
 * This operation never fails, but the returned object
 * might not support any I/O operation if the URI
 * is malformed or if the URI scheme is not supported.
 *
 * Returns: (transfer full): a #GFile.
 *     Free the returned object with g_object_unref().
 */
GFile *
g_vfs_get_file_for_uri (GVfs       *vfs,
                        const char *uri)
{
  GVfsClass *class;
  GFile *ret;
 
  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (uri != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  ret = get_file_for_uri_internal (vfs, uri);
  if (ret)
    return ret;

  return (* class->get_file_for_uri) (vfs, uri);
}

/**
 * g_vfs_get_supported_uri_schemes:
 * @vfs: a #GVfs.
 *
 * Gets a list of URI schemes supported by @vfs.
 *
 * Returns: (transfer none): a %NULL-terminated array of strings.
 *     The returned array belongs to GIO and must
 *     not be freed or modified.
 */
const gchar * const *
g_vfs_get_supported_uri_schemes (GVfs *vfs)
{
  GVfsPrivate *priv;

  g_return_val_if_fail (G_IS_VFS (vfs), NULL);

  priv = g_vfs_get_instance_private (vfs);

  if (!priv->supported_schemes)
    {
      GVfsClass *class;
      const char * const *default_schemes;
      const char *additional_scheme;
      GPtrArray *supported_schemes;
      GHashTableIter iter;

      class = G_VFS_GET_CLASS (vfs);

      default_schemes = (* class->get_supported_uri_schemes) (vfs);
      supported_schemes = g_ptr_array_new ();

      for (; default_schemes && *default_schemes; default_schemes++)
        g_ptr_array_add (supported_schemes, (gpointer) *default_schemes);

      g_rw_lock_reader_lock (&additional_schemes_lock);
      g_hash_table_iter_init (&iter, priv->additional_schemes);

      while (g_hash_table_iter_next
             (&iter, (gpointer *) &additional_scheme, NULL))
        g_ptr_array_add (supported_schemes, (gpointer) additional_scheme);

      g_rw_lock_reader_unlock (&additional_schemes_lock);

      g_ptr_array_add (supported_schemes, NULL);

      g_free (priv->supported_schemes);
      priv->supported_schemes =
        (char const **) g_ptr_array_free (supported_schemes, FALSE);
    }

  return priv->supported_schemes;
}

/**
 * g_vfs_parse_name:
 * @vfs: a #GVfs.
 * @parse_name: a string to be parsed by the VFS module.
 *
 * This operation never fails, but the returned object might
 * not support any I/O operations if the @parse_name cannot
 * be parsed by the #GVfs module.
 *
 * Returns: (transfer full): a #GFile for the given @parse_name.
 *     Free the returned object with g_object_unref().
 */
GFile *
g_vfs_parse_name (GVfs       *vfs,
                  const char *parse_name)
{
  GVfsClass *class;
  GFile *ret;

  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (parse_name != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  ret = parse_name_internal (vfs, parse_name);
  if (ret)
    return ret;

  return (* class->parse_name) (vfs, parse_name);
}

/**
 * g_vfs_get_default:
 *
 * Gets the default #GVfs for the system.
 *
 * Returns: (transfer none): a #GVfs.
 */
GVfs *
g_vfs_get_default (void)
{
  if (GLIB_PRIVATE_CALL (g_check_setuid) ())
    return g_vfs_get_local ();
  return _g_io_module_get_default (G_VFS_EXTENSION_POINT_NAME,
                                   "GIO_USE_VFS",
                                   (GIOModuleVerifyFunc)g_vfs_is_active);
}

/**
 * g_vfs_get_local:
 *
 * Gets the local #GVfs for the system.
 *
 * Returns: (transfer none): a #GVfs.
 */
GVfs *
g_vfs_get_local (void)
{
  static gsize vfs = 0;

  if (g_once_init_enter (&vfs))
    g_once_init_leave (&vfs, (gsize)_g_local_vfs_new ());

  return G_VFS (vfs);
}

/**
 * g_vfs_register_uri_scheme:
 * @vfs: a #GVfs
 * @scheme: an URI scheme, e.g. "http"
 * @uri_func: (scope notified) (nullable): a #GVfsFileLookupFunc
 * @uri_data: (nullable): custom data passed to be passed to @uri_func, or %NULL
 * @uri_destroy: (nullable): function to be called when unregistering the
 *     URI scheme, or when @vfs is disposed, to free the resources used
 *     by the URI lookup function
 * @parse_name_func: (scope notified) (nullable): a #GVfsFileLookupFunc
 * @parse_name_data: (nullable): custom data passed to be passed to
 *     @parse_name_func, or %NULL
 * @parse_name_destroy: (nullable): function to be called when unregistering the
 *     URI scheme, or when @vfs is disposed, to free the resources used
 *     by the parse name lookup function
 *
 * Registers @uri_func and @parse_name_func as the #GFile URI and parse name
 * lookup functions for URIs with a scheme matching @scheme.
 * Note that @scheme is registered only within the running application, as
 * opposed to desktop-wide as it happens with GVfs backends.
 *
 * When a #GFile is requested with an URI containing @scheme (e.g. through
 * g_file_new_for_uri()), @uri_func will be called to allow a custom
 * constructor. The implementation of @uri_func should not be blocking, and
 * must not call g_vfs_register_uri_scheme() or g_vfs_unregister_uri_scheme().
 *
 * When g_file_parse_name() is called with a parse name obtained from such file,
 * @parse_name_func will be called to allow the #GFile to be created again. In
 * that case, it's responsibility of @parse_name_func to make sure the parse
 * name matches what the custom #GFile implementation returned when
 * g_file_get_parse_name() was previously called. The implementation of
 * @parse_name_func should not be blocking, and must not call
 * g_vfs_register_uri_scheme() or g_vfs_unregister_uri_scheme().
 *
 * It's an error to call this function twice with the same scheme. To unregister
 * a custom URI scheme, use g_vfs_unregister_uri_scheme().
 *
 * Returns: %TRUE if @scheme was successfully registered, or %FALSE if a handler
 *     for @scheme already exists.
 *
 * Since: 2.50
 */
gboolean
g_vfs_register_uri_scheme (GVfs              *vfs,
                           const char        *scheme,
                           GVfsFileLookupFunc uri_func,
                           gpointer           uri_data,
                           GDestroyNotify     uri_destroy,
                           GVfsFileLookupFunc parse_name_func,
                           gpointer           parse_name_data,
                           GDestroyNotify     parse_name_destroy)
{
  GVfsPrivate *priv;
  GVfsURISchemeData *closure;

  g_return_val_if_fail (G_IS_VFS (vfs), FALSE);
  g_return_val_if_fail (scheme != NULL, FALSE);

  priv = g_vfs_get_instance_private (vfs);

  g_rw_lock_reader_lock (&additional_schemes_lock);
  closure = g_hash_table_lookup (priv->additional_schemes, scheme);
  g_rw_lock_reader_unlock (&additional_schemes_lock);

  if (closure != NULL)
    return FALSE;

  closure = g_new0 (GVfsURISchemeData, 1);
  closure->uri_func = uri_func;
  closure->uri_data = uri_data;
  closure->uri_destroy = uri_destroy;
  closure->parse_name_func = parse_name_func;
  closure->parse_name_data = parse_name_data;
  closure->parse_name_destroy = parse_name_destroy;

  g_rw_lock_writer_lock (&additional_schemes_lock);
  g_hash_table_insert (priv->additional_schemes, g_strdup (scheme), closure);
  g_rw_lock_writer_unlock (&additional_schemes_lock);

  /* Invalidate supported schemes */
  g_clear_pointer (&priv->supported_schemes, g_free);

  return TRUE;
}

/**
 * g_vfs_unregister_uri_scheme:
 * @vfs: a #GVfs
 * @scheme: an URI scheme, e.g. "http"
 *
 * Unregisters the URI handler for @scheme previously registered with
 * g_vfs_register_uri_scheme().
 *
 * Returns: %TRUE if @scheme was successfully unregistered, or %FALSE if a
 *     handler for @scheme does not exist.
 *
 * Since: 2.50
 */
gboolean
g_vfs_unregister_uri_scheme (GVfs       *vfs,
                             const char *scheme)
{
  GVfsPrivate *priv;
  gboolean res;

  g_return_val_if_fail (G_IS_VFS (vfs), FALSE);
  g_return_val_if_fail (scheme != NULL, FALSE);

  priv = g_vfs_get_instance_private (vfs);

  g_rw_lock_writer_lock (&additional_schemes_lock);
  res = g_hash_table_remove (priv->additional_schemes, scheme);
  g_rw_lock_writer_unlock (&additional_schemes_lock);

  if (res)
    {
      /* Invalidate supported schemes */
      g_clear_pointer (&priv->supported_schemes, g_free);

      return TRUE;
    }

  return FALSE;
}
