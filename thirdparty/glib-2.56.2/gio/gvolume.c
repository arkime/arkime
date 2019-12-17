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
 *         David Zeuthen <davidz@redhat.com>
 */

#include "config.h"
#include "gmount.h"
#include "gvolume.h"
#include "gthemedicon.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gvolume
 * @short_description: Volume management
 * @include: gio/gio.h
 * 
 * The #GVolume interface represents user-visible objects that can be
 * mounted. Note, when porting from GnomeVFS, #GVolume is the moral
 * equivalent of #GnomeVFSDrive.
 *
 * Mounting a #GVolume instance is an asynchronous operation. For more
 * information about asynchronous operations, see #GAsyncResult and
 * #GTask. To mount a #GVolume, first call g_volume_mount() with (at
 * least) the #GVolume instance, optionally a #GMountOperation object
 * and a #GAsyncReadyCallback.
 *
 * Typically, one will only want to pass %NULL for the
 * #GMountOperation if automounting all volumes when a desktop session
 * starts since it's not desirable to put up a lot of dialogs asking
 * for credentials.
 *
 * The callback will be fired when the operation has resolved (either
 * with success or failure), and a #GAsyncReady structure will be
 * passed to the callback.  That callback should then call
 * g_volume_mount_finish() with the #GVolume instance and the
 * #GAsyncReady data to see if the operation was completed
 * successfully.  If an @error is present when g_volume_mount_finish()
 * is called, then it will be filled with any error information.
 *
 * ## Volume Identifiers # {#volume-identifier}
 *
 * It is sometimes necessary to directly access the underlying
 * operating system object behind a volume (e.g. for passing a volume
 * to an application via the commandline). For this purpose, GIO
 * allows to obtain an 'identifier' for the volume. There can be
 * different kinds of identifiers, such as Hal UDIs, filesystem labels,
 * traditional Unix devices (e.g. `/dev/sda2`), UUIDs. GIO uses predefined
 * strings as names for the different kinds of identifiers:
 * #G_VOLUME_IDENTIFIER_KIND_HAL_UDI, #G_VOLUME_IDENTIFIER_KIND_LABEL, etc.
 * Use g_volume_get_identifier() to obtain an identifier for a volume.
 *
 *
 * Note that #G_VOLUME_IDENTIFIER_KIND_HAL_UDI will only be available
 * when the gvfs hal volume monitor is in use. Other volume monitors
 * will generally be able to provide the #G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE
 * identifier, which can be used to obtain a hal device by means of
 * libhal_manager_find_device_string_match().
 */

typedef GVolumeIface GVolumeInterface;
G_DEFINE_INTERFACE(GVolume, g_volume, G_TYPE_OBJECT)

static void
g_volume_default_init (GVolumeInterface *iface)
{
  /**
   * GVolume::changed:
   * 
   * Emitted when the volume has been changed.
   */
  g_signal_new (I_("changed"),
		G_TYPE_VOLUME,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GVolumeIface, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /**
   * GVolume::removed:
   * 
   * This signal is emitted when the #GVolume have been removed. If
   * the recipient is holding references to the object they should
   * release them so the object can be finalized.
   */
  g_signal_new (I_("removed"),
		G_TYPE_VOLUME,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GVolumeIface, removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

/**
 * g_volume_get_name:
 * @volume: a #GVolume
 * 
 * Gets the name of @volume.
 * 
 * Returns: the name for the given @volume. The returned string should 
 *     be freed with g_free() when no longer needed.
 */
char *
g_volume_get_name (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_name) (volume);
}

/**
 * g_volume_get_icon:
 * @volume: a #GVolume
 * 
 * Gets the icon for @volume.
 * 
 * Returns: (transfer full): a #GIcon.
 *     The returned object should be unreffed with g_object_unref()
 *     when no longer needed.
 */
GIcon *
g_volume_get_icon (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_icon) (volume);
}

/**
 * g_volume_get_symbolic_icon:
 * @volume: a #GVolume
 * 
 * Gets the symbolic icon for @volume.
 * 
 * Returns: (transfer full): a #GIcon.
 *     The returned object should be unreffed with g_object_unref()
 *     when no longer needed.
 *
 * Since: 2.34
 */
GIcon *
g_volume_get_symbolic_icon (GVolume *volume)
{
  GVolumeIface *iface;
  GIcon *ret;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->get_symbolic_icon != NULL)
    ret = iface->get_symbolic_icon (volume);
  else
    ret = g_themed_icon_new_with_default_fallbacks ("folder-remote-symbolic");

  return ret;

}

/**
 * g_volume_get_uuid:
 * @volume: a #GVolume
 * 
 * Gets the UUID for the @volume. The reference is typically based on
 * the file system UUID for the volume in question and should be
 * considered an opaque string. Returns %NULL if there is no UUID
 * available.
 * 
 * Returns: the UUID for @volume or %NULL if no UUID can be computed.
 *     The returned string should be freed with g_free() 
 *     when no longer needed.
 */
char *
g_volume_get_uuid (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_uuid) (volume);
}
  
/**
 * g_volume_get_drive:
 * @volume: a #GVolume
 * 
 * Gets the drive for the @volume.
 * 
 * Returns: (transfer full): a #GDrive or %NULL if @volume is not
 *     associated with a drive. The returned object should be unreffed
 *     with g_object_unref() when no longer needed.
 */
GDrive *
g_volume_get_drive (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_drive) (volume);
}

/**
 * g_volume_get_mount:
 * @volume: a #GVolume
 * 
 * Gets the mount for the @volume.
 * 
 * Returns: (transfer full): a #GMount or %NULL if @volume isn't mounted.
 *     The returned object should be unreffed with g_object_unref()
 *     when no longer needed.
 */
GMount *
g_volume_get_mount (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_mount) (volume);
}


/**
 * g_volume_can_mount:
 * @volume: a #GVolume
 * 
 * Checks if a volume can be mounted.
 * 
 * Returns: %TRUE if the @volume can be mounted. %FALSE otherwise
 */
gboolean
g_volume_can_mount (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->can_mount == NULL)
    return FALSE;

  return (* iface->can_mount) (volume);
}

/**
 * g_volume_can_eject:
 * @volume: a #GVolume
 * 
 * Checks if a volume can be ejected.
 * 
 * Returns: %TRUE if the @volume can be ejected. %FALSE otherwise
 */
gboolean
g_volume_can_eject (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->can_eject == NULL)
    return FALSE;

  return (* iface->can_eject) (volume);
}

/**
 * g_volume_should_automount:
 * @volume: a #GVolume
 *
 * Returns whether the volume should be automatically mounted.
 * 
 * Returns: %TRUE if the volume should be automatically mounted
 */
gboolean
g_volume_should_automount (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->should_automount == NULL)
    return FALSE;

  return (* iface->should_automount) (volume);
}


/**
 * g_volume_mount:
 * @volume: a #GVolume
 * @flags: flags affecting the operation
 * @mount_operation: (nullable): a #GMountOperation or %NULL to avoid user interaction
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL
 * @user_data: user data that gets passed to @callback
 * 
 * Mounts a volume. This is an asynchronous operation, and is
 * finished by calling g_volume_mount_finish() with the @volume
 * and #GAsyncResult returned in the @callback.
 *
 * Virtual: mount_fn
 */
void
g_volume_mount (GVolume             *volume,
		GMountMountFlags     flags,
                GMountOperation     *mount_operation,
                GCancellable        *cancellable,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
  GVolumeIface *iface;

  g_return_if_fail (G_IS_VOLUME (volume));

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->mount_fn == NULL)
    {
      g_task_report_new_error (volume, callback, user_data,
                               g_volume_mount,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("volume doesn’t implement mount"));
      return;
    }
  
  (* iface->mount_fn) (volume, flags, mount_operation, cancellable, callback, user_data);
}

/**
 * g_volume_mount_finish:
 * @volume: a #GVolume
 * @result: a #GAsyncResult
 * @error: a #GError location to store an error, or %NULL to ignore
 * 
 * Finishes mounting a volume. If any errors occurred during the operation,
 * @error will be set to contain the errors and %FALSE will be returned.
 *
 * If the mount operation succeeded, g_volume_get_mount() on @volume
 * is guaranteed to return the mount right after calling this
 * function; there's no need to listen for the 'mount-added' signal on
 * #GVolumeMonitor.
 * 
 * Returns: %TRUE, %FALSE if operation failed
 */
gboolean
g_volume_mount_finish (GVolume       *volume,
                       GAsyncResult  *result,
                       GError       **error)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_volume_mount))
    return g_task_propagate_boolean (G_TASK (result), error);
  
  iface = G_VOLUME_GET_IFACE (volume);
  return (* iface->mount_finish) (volume, result, error);
}

/**
 * g_volume_eject:
 * @volume: a #GVolume
 * @flags: flags affecting the unmount if required for eject
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL
 * @user_data: user data that gets passed to @callback
 * 
 * Ejects a volume. This is an asynchronous operation, and is
 * finished by calling g_volume_eject_finish() with the @volume
 * and #GAsyncResult returned in the @callback.
 *
 * Deprecated: 2.22: Use g_volume_eject_with_operation() instead.
 */
void
g_volume_eject (GVolume             *volume,
		GMountUnmountFlags   flags,
                GCancellable        *cancellable,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
  GVolumeIface *iface;

  g_return_if_fail (G_IS_VOLUME (volume));

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->eject == NULL)
    {
      g_task_report_new_error (volume, callback, user_data,
                               g_volume_eject_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("volume doesn’t implement eject"));
      return;
    }
  
  (* iface->eject) (volume, flags, cancellable, callback, user_data);
}

/**
 * g_volume_eject_finish:
 * @volume: pointer to a #GVolume
 * @result: a #GAsyncResult
 * @error: a #GError location to store an error, or %NULL to ignore
 * 
 * Finishes ejecting a volume. If any errors occurred during the operation,
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE, %FALSE if operation failed
 *
 * Deprecated: 2.22: Use g_volume_eject_with_operation_finish() instead.
 **/
gboolean
g_volume_eject_finish (GVolume       *volume,
                       GAsyncResult  *result,
                       GError       **error)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  if (g_async_result_is_tagged (result, g_volume_eject_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);
  
  iface = G_VOLUME_GET_IFACE (volume);
  return (* iface->eject_finish) (volume, result, error);
}

/**
 * g_volume_eject_with_operation:
 * @volume: a #GVolume
 * @flags: flags affecting the unmount if required for eject
 * @mount_operation: (nullable): a #GMountOperation or %NULL to
 *     avoid user interaction
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL
 * @user_data: user data passed to @callback
 *
 * Ejects a volume. This is an asynchronous operation, and is
 * finished by calling g_volume_eject_with_operation_finish() with the @volume
 * and #GAsyncResult data returned in the @callback.
 *
 * Since: 2.22
 **/
void
g_volume_eject_with_operation (GVolume              *volume,
                               GMountUnmountFlags   flags,
                               GMountOperation     *mount_operation,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GVolumeIface *iface;

  g_return_if_fail (G_IS_VOLUME (volume));

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->eject == NULL && iface->eject_with_operation == NULL)
    {
      g_task_report_new_error (volume, callback, user_data,
                               g_volume_eject_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for volume objects that
                                * don't implement any of eject or eject_with_operation. */
                               _("volume doesn’t implement eject or eject_with_operation"));
      return;
    }

  if (iface->eject_with_operation != NULL)
    (* iface->eject_with_operation) (volume, flags, mount_operation, cancellable, callback, user_data);
  else
    (* iface->eject) (volume, flags, cancellable, callback, user_data);
}

/**
 * g_volume_eject_with_operation_finish:
 * @volume: a #GVolume
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL
 *
 * Finishes ejecting a volume. If any errors occurred during the operation,
 * @error will be set to contain the errors and %FALSE will be returned.
 *
 * Returns: %TRUE if the volume was successfully ejected. %FALSE otherwise
 *
 * Since: 2.22
 **/
gboolean
g_volume_eject_with_operation_finish (GVolume        *volume,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_volume_eject_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_VOLUME_GET_IFACE (volume);
  if (iface->eject_with_operation_finish != NULL)
    return (* iface->eject_with_operation_finish) (volume, result, error);
  else
    return (* iface->eject_finish) (volume, result, error);
}

/**
 * g_volume_get_identifier:
 * @volume: a #GVolume
 * @kind: the kind of identifier to return
 *
 * Gets the identifier of the given kind for @volume. 
 * See the [introduction][volume-identifier] for more
 * information about volume identifiers.
 *
 * Returns: a newly allocated string containing the
 *     requested identfier, or %NULL if the #GVolume
 *     doesn't have this kind of identifier
 */
char *
g_volume_get_identifier (GVolume    *volume,
			 const char *kind)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);
  g_return_val_if_fail (kind != NULL, NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->get_identifier == NULL)
    return NULL;
  
  return (* iface->get_identifier) (volume, kind);
}

/**
 * g_volume_enumerate_identifiers:
 * @volume: a #GVolume
 * 
 * Gets the kinds of [identifiers][volume-identifier] that @volume has.
 * Use g_volume_get_identifier() to obtain the identifiers themselves.
 *
 * Returns: (array zero-terminated=1) (transfer full): a %NULL-terminated array
 *   of strings containing kinds of identifiers. Use g_strfreev() to free.
 */
char **
g_volume_enumerate_identifiers (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);
  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->enumerate_identifiers == NULL)
    return NULL;
  
  return (* iface->enumerate_identifiers) (volume);
}

/**
 * g_volume_get_activation_root:
 * @volume: a #GVolume
 *
 * Gets the activation root for a #GVolume if it is known ahead of
 * mount time. Returns %NULL otherwise. If not %NULL and if @volume
 * is mounted, then the result of g_mount_get_root() on the
 * #GMount object obtained from g_volume_get_mount() will always
 * either be equal or a prefix of what this function returns. In
 * other words, in code
 *
 * |[<!-- language="C" -->
 *   GMount *mount;
 *   GFile *mount_root
 *   GFile *volume_activation_root;
 *
 *   mount = g_volume_get_mount (volume); // mounted, so never NULL
 *   mount_root = g_mount_get_root (mount);
 *   volume_activation_root = g_volume_get_activation_root (volume); // assume not NULL
 * ]|
 * then the expression
 * |[<!-- language="C" -->
 *   (g_file_has_prefix (volume_activation_root, mount_root) ||
 *    g_file_equal (volume_activation_root, mount_root))
 * ]|
 * will always be %TRUE.
 *
 * Activation roots are typically used in #GVolumeMonitor
 * implementations to find the underlying mount to shadow, see
 * g_mount_is_shadowed() for more details.
 *
 * Returns: (nullable) (transfer full): the activation root of @volume
 *     or %NULL. Use g_object_unref() to free.
 *
 * Since: 2.18
 */
GFile *
g_volume_get_activation_root (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);
  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->get_activation_root == NULL)
    return NULL;

  return (* iface->get_activation_root) (volume);
}

/**
 * g_volume_get_sort_key:
 * @volume: a #GVolume
 *
 * Gets the sort key for @volume, if any.
 *
 * Returns: Sorting key for @volume or %NULL if no such key is available
 *
 * Since: 2.32
 */
const gchar *
g_volume_get_sort_key (GVolume *volume)
{
  const gchar *ret = NULL;
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);
  if (iface->get_sort_key != NULL)
    ret = iface->get_sort_key (volume);

  return ret;
}
