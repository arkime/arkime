/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2008 Red Hat, Inc.
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

#include <string.h>

#include "gmount.h"
#include "gmountprivate.h"
#include "gthemedicon.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gmount
 * @short_description: Mount management
 * @include: gio/gio.h
 * @see_also: GVolume, GUnixMountEntry, GUnixMountPoint
 *
 * The #GMount interface represents user-visible mounts. Note, when 
 * porting from GnomeVFS, #GMount is the moral equivalent of #GnomeVFSVolume.
 *
 * #GMount is a "mounted" filesystem that you can access. Mounted is in
 * quotes because it's not the same as a unix mount, it might be a gvfs
 * mount, but you can still access the files on it if you use GIO. Might or
 * might not be related to a volume object.
 * 
 * Unmounting a #GMount instance is an asynchronous operation. For
 * more information about asynchronous operations, see #GAsyncResult
 * and #GTask. To unmount a #GMount instance, first call
 * g_mount_unmount_with_operation() with (at least) the #GMount instance and a
 * #GAsyncReadyCallback.  The callback will be fired when the
 * operation has resolved (either with success or failure), and a
 * #GAsyncResult structure will be passed to the callback.  That
 * callback should then call g_mount_unmount_with_operation_finish() with the #GMount
 * and the #GAsyncResult data to see if the operation was completed
 * successfully.  If an @error is present when g_mount_unmount_with_operation_finish() 
 * is called, then it will be filled with any error information.
 **/

typedef GMountIface GMountInterface;
G_DEFINE_INTERFACE (GMount, g_mount, G_TYPE_OBJECT)

static void
g_mount_default_init (GMountInterface *iface)
{
  /**
   * GMount::changed:
   * @mount: the object on which the signal is emitted
   *
   * Emitted when the mount has been changed.
   **/
  g_signal_new (I_("changed"),
                G_TYPE_MOUNT,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GMountIface, changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /**
   * GMount::unmounted:
   * @mount: the object on which the signal is emitted
   *
   * This signal is emitted when the #GMount have been
   * unmounted. If the recipient is holding references to the
   * object they should release them so the object can be
   * finalized.
   **/
  g_signal_new (I_("unmounted"),
                G_TYPE_MOUNT,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GMountIface, unmounted),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
  /**
   * GMount::pre-unmount:
   * @mount: the object on which the signal is emitted
   *
   * This signal may be emitted when the #GMount is about to be
   * unmounted.
   *
   * This signal depends on the backend and is only emitted if
   * GIO was used to unmount.
   *
   * Since: 2.22
   **/
  g_signal_new (I_("pre-unmount"),
                G_TYPE_MOUNT,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GMountIface, pre_unmount),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}

/**
 * g_mount_get_root:
 * @mount: a #GMount.
 * 
 * Gets the root directory on @mount.
 * 
 * Returns: (transfer full): a #GFile. 
 *      The returned object should be unreffed with 
 *      g_object_unref() when no longer needed.
 **/
GFile *
g_mount_get_root (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_root) (mount);
}

/**
 * g_mount_get_default_location:
 * @mount: a #GMount.
 *
 * Gets the default location of @mount. The default location of the given
 * @mount is a path that reflects the main entry point for the user (e.g.
 * the home directory, or the root of the volume).
 *
 * Returns: (transfer full): a #GFile.
 *      The returned object should be unreffed with
 *      g_object_unref() when no longer needed.
 **/
GFile *
g_mount_get_default_location (GMount *mount)
{
  GMountIface *iface;
  GFile       *file;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);
  
  /* Fallback to get_root when default_location () is not available */
  if (iface->get_default_location)
    file = (* iface->get_default_location) (mount);
  else
    file = (* iface->get_root) (mount);

  return file;
}

/**
 * g_mount_get_name:
 * @mount: a #GMount.
 * 
 * Gets the name of @mount.
 * 
 * Returns: the name for the given @mount. 
 *     The returned string should be freed with g_free()
 *     when no longer needed.
 **/
char *
g_mount_get_name (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_name) (mount);
}

/**
 * g_mount_get_icon:
 * @mount: a #GMount.
 * 
 * Gets the icon for @mount.
 * 
 * Returns: (transfer full): a #GIcon.
 *      The returned object should be unreffed with 
 *      g_object_unref() when no longer needed.
 **/
GIcon *
g_mount_get_icon (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_icon) (mount);
}


/**
 * g_mount_get_symbolic_icon:
 * @mount: a #GMount.
 * 
 * Gets the symbolic icon for @mount.
 * 
 * Returns: (transfer full): a #GIcon.
 *      The returned object should be unreffed with 
 *      g_object_unref() when no longer needed.
 *
 * Since: 2.34
 **/
GIcon *
g_mount_get_symbolic_icon (GMount *mount)
{
  GMountIface *iface;
  GIcon *ret;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->get_symbolic_icon != NULL)
    ret = iface->get_symbolic_icon (mount);
  else
    ret = g_themed_icon_new_with_default_fallbacks ("folder-remote-symbolic");

  return ret;
}

/**
 * g_mount_get_uuid:
 * @mount: a #GMount.
 * 
 * Gets the UUID for the @mount. The reference is typically based on
 * the file system UUID for the mount in question and should be
 * considered an opaque string. Returns %NULL if there is no UUID
 * available.
 * 
 * Returns: the UUID for @mount or %NULL if no UUID can be computed.
 *     The returned string should be freed with g_free()
 *     when no longer needed.
 **/
char *
g_mount_get_uuid (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_uuid) (mount);
}

/**
 * g_mount_get_volume:
 * @mount: a #GMount.
 * 
 * Gets the volume for the @mount.
 * 
 * Returns: (transfer full): a #GVolume or %NULL if @mount is not associated with a volume.
 *      The returned object should be unreffed with 
 *      g_object_unref() when no longer needed.
 **/
GVolume *
g_mount_get_volume (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_volume) (mount);
}

/**
 * g_mount_get_drive:
 * @mount: a #GMount.
 * 
 * Gets the drive for the @mount.
 *
 * This is a convenience method for getting the #GVolume and then
 * using that object to get the #GDrive.
 * 
 * Returns: (transfer full): a #GDrive or %NULL if @mount is not associated with a volume or a drive.
 *      The returned object should be unreffed with 
 *      g_object_unref() when no longer needed.
 **/
GDrive *
g_mount_get_drive (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_drive) (mount);
}

/**
 * g_mount_can_unmount: 
 * @mount: a #GMount.
 * 
 * Checks if @mount can be unmounted.
 * 
 * Returns: %TRUE if the @mount can be unmounted.
 **/
gboolean
g_mount_can_unmount (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->can_unmount) (mount);
}

/**
 * g_mount_can_eject: 
 * @mount: a #GMount.
 * 
 * Checks if @mount can be ejected.
 * 
 * Returns: %TRUE if the @mount can be ejected.
 **/
gboolean
g_mount_can_eject (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->can_eject) (mount);
}

/**
 * g_mount_unmount:
 * @mount: a #GMount.
 * @flags: flags affecting the operation
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 * 
 * Unmounts a mount. This is an asynchronous operation, and is 
 * finished by calling g_mount_unmount_finish() with the @mount 
 * and #GAsyncResult data returned in the @callback.
 *
 * Deprecated: 2.22: Use g_mount_unmount_with_operation() instead.
 **/
void
g_mount_unmount (GMount              *mount,
                 GMountUnmountFlags   flags,
                 GCancellable        *cancellable,
                 GAsyncReadyCallback  callback,
                 gpointer             user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));
  
  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->unmount == NULL)
    {
      g_task_report_new_error (mount, callback, user_data,
                               g_mount_unmount_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for mount objects that
                                * don't implement unmount. */
                               _("mount doesn’t implement “unmount”"));
      return;
    }
  
  (* iface->unmount) (mount, flags, cancellable, callback, user_data);
}

/**
 * g_mount_unmount_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 *     ignore.
 * 
 * Finishes unmounting a mount. If any errors occurred during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the mount was successfully unmounted. %FALSE otherwise.
 *
 * Deprecated: 2.22: Use g_mount_unmount_with_operation_finish() instead.
 **/
gboolean
g_mount_unmount_finish (GMount        *mount,
                        GAsyncResult  *result,
                        GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_mount_unmount_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);
  
  iface = G_MOUNT_GET_IFACE (mount);
  return (* iface->unmount_finish) (mount, result, error);
}


/**
 * g_mount_eject:
 * @mount: a #GMount.
 * @flags: flags affecting the unmount if required for eject
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 * 
 * Ejects a mount. This is an asynchronous operation, and is 
 * finished by calling g_mount_eject_finish() with the @mount 
 * and #GAsyncResult data returned in the @callback.
 *
 * Deprecated: 2.22: Use g_mount_eject_with_operation() instead.
 **/
void
g_mount_eject (GMount              *mount,
               GMountUnmountFlags   flags,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));
  
  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->eject == NULL)
    {
      g_task_report_new_error (mount, callback, user_data,
                               g_mount_eject_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for mount objects that
                                * don't implement eject. */
                               _("mount doesn’t implement “eject”"));
      return;
    }
  
  (* iface->eject) (mount, flags, cancellable, callback, user_data);
}

/**
 * g_mount_eject_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 *     ignore.
 * 
 * Finishes ejecting a mount. If any errors occurred during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the mount was successfully ejected. %FALSE otherwise.
 *
 * Deprecated: 2.22: Use g_mount_eject_with_operation_finish() instead.
 **/
gboolean
g_mount_eject_finish (GMount        *mount,
                      GAsyncResult  *result,
                      GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_mount_eject_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);
  
  iface = G_MOUNT_GET_IFACE (mount);
  return (* iface->eject_finish) (mount, result, error);
}

/**
 * g_mount_unmount_with_operation:
 * @mount: a #GMount.
 * @flags: flags affecting the operation
 * @mount_operation: (nullable): a #GMountOperation or %NULL to avoid
 *     user interaction.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 *
 * Unmounts a mount. This is an asynchronous operation, and is
 * finished by calling g_mount_unmount_with_operation_finish() with the @mount 
 * and #GAsyncResult data returned in the @callback.
 *
 * Since: 2.22
 **/
void
g_mount_unmount_with_operation (GMount              *mount,
                                GMountUnmountFlags   flags,
                                GMountOperation     *mount_operation,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));

  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->unmount == NULL && iface->unmount_with_operation == NULL)
    {
      g_task_report_new_error (mount, callback, user_data,
                               g_mount_unmount_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for mount objects that
                                * don't implement any of unmount or unmount_with_operation. */
                               _("mount doesn’t implement “unmount” or “unmount_with_operation”"));
      return;
    }

  if (iface->unmount_with_operation != NULL)
    (* iface->unmount_with_operation) (mount, flags, mount_operation, cancellable, callback, user_data);
  else
    (* iface->unmount) (mount, flags, cancellable, callback, user_data);
}

/**
 * g_mount_unmount_with_operation_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 *     ignore.
 *
 * Finishes unmounting a mount. If any errors occurred during the operation,
 * @error will be set to contain the errors and %FALSE will be returned.
 *
 * Returns: %TRUE if the mount was successfully unmounted. %FALSE otherwise.
 *
 * Since: 2.22
 **/
gboolean
g_mount_unmount_with_operation_finish (GMount        *mount,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_mount_unmount_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_MOUNT_GET_IFACE (mount);
  if (iface->unmount_with_operation_finish != NULL)
    return (* iface->unmount_with_operation_finish) (mount, result, error);
  else
    return (* iface->unmount_finish) (mount, result, error);
}


/**
 * g_mount_eject_with_operation:
 * @mount: a #GMount.
 * @flags: flags affecting the unmount if required for eject
 * @mount_operation: (nullable): a #GMountOperation or %NULL to avoid
 *     user interaction.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 *
 * Ejects a mount. This is an asynchronous operation, and is
 * finished by calling g_mount_eject_with_operation_finish() with the @mount
 * and #GAsyncResult data returned in the @callback.
 *
 * Since: 2.22
 **/
void
g_mount_eject_with_operation (GMount              *mount,
                              GMountUnmountFlags   flags,
                              GMountOperation     *mount_operation,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));

  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->eject == NULL && iface->eject_with_operation == NULL)
    {
      g_task_report_new_error (mount, callback, user_data,
                               g_mount_eject_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for mount objects that
                                * don't implement any of eject or eject_with_operation. */
                               _("mount doesn’t implement “eject” or “eject_with_operation”"));
      return;
    }

  if (iface->eject_with_operation != NULL)
    (* iface->eject_with_operation) (mount, flags, mount_operation, cancellable, callback, user_data);
  else
    (* iface->eject) (mount, flags, cancellable, callback, user_data);
}

/**
 * g_mount_eject_with_operation_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 *     ignore.
 *
 * Finishes ejecting a mount. If any errors occurred during the operation,
 * @error will be set to contain the errors and %FALSE will be returned.
 *
 * Returns: %TRUE if the mount was successfully ejected. %FALSE otherwise.
 *
 * Since: 2.22
 **/
gboolean
g_mount_eject_with_operation_finish (GMount        *mount,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_mount_eject_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_MOUNT_GET_IFACE (mount);
  if (iface->eject_with_operation_finish != NULL)
    return (* iface->eject_with_operation_finish) (mount, result, error);
  else
    return (* iface->eject_finish) (mount, result, error);
}

/**
 * g_mount_remount:
 * @mount: a #GMount.
 * @flags: flags affecting the operation
 * @mount_operation: (nullable): a #GMountOperation or %NULL to avoid
 *     user interaction.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 * 
 * Remounts a mount. This is an asynchronous operation, and is 
 * finished by calling g_mount_remount_finish() with the @mount 
 * and #GAsyncResults data returned in the @callback.
 *
 * Remounting is useful when some setting affecting the operation
 * of the volume has been changed, as these may need a remount to
 * take affect. While this is semantically equivalent with unmounting
 * and then remounting not all backends might need to actually be
 * unmounted.
 **/
void
g_mount_remount (GMount              *mount,
                 GMountMountFlags     flags,
                 GMountOperation     *mount_operation,
                 GCancellable        *cancellable,
                 GAsyncReadyCallback  callback,
                 gpointer             user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));
  
  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->remount == NULL)
    { 
      g_task_report_new_error (mount, callback, user_data,
                               g_mount_remount,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for mount objects that
                                * don't implement remount. */
                               _("mount doesn’t implement “remount”"));
      return;
    }
  
  (* iface->remount) (mount, flags, mount_operation, cancellable, callback, user_data);
}

/**
 * g_mount_remount_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 *     ignore.
 * 
 * Finishes remounting a mount. If any errors occurred during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the mount was successfully remounted. %FALSE otherwise.
 **/
gboolean
g_mount_remount_finish (GMount        *mount,
                        GAsyncResult  *result,
                        GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_mount_remount))
    return g_task_propagate_boolean (G_TASK (result), error);
  
  iface = G_MOUNT_GET_IFACE (mount);
  return (* iface->remount_finish) (mount, result, error);
}

/**
 * g_mount_guess_content_type:
 * @mount: a #GMount
 * @force_rescan: Whether to force a rescan of the content. 
 *     Otherwise a cached result will be used if available
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @callback: a #GAsyncReadyCallback
 * @user_data: user data passed to @callback
 * 
 * Tries to guess the type of content stored on @mount. Returns one or
 * more textual identifiers of well-known content types (typically
 * prefixed with "x-content/"), e.g. x-content/image-dcf for camera 
 * memory cards. See the 
 * [shared-mime-info](http://www.freedesktop.org/wiki/Specifications/shared-mime-info-spec)
 * specification for more on x-content types.
 *
 * This is an asynchronous operation (see
 * g_mount_guess_content_type_sync() for the synchronous version), and
 * is finished by calling g_mount_guess_content_type_finish() with the
 * @mount and #GAsyncResult data returned in the @callback.
 *
 * Since: 2.18
 */
void
g_mount_guess_content_type (GMount              *mount,
                            gboolean             force_rescan,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));

  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->guess_content_type == NULL)
    {
      g_task_report_new_error (mount, callback, user_data,
                               g_mount_guess_content_type,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for mount objects that
                                * don't implement content type guessing. */
                               _("mount doesn’t implement content type guessing"));
      return;
    }
  
  (* iface->guess_content_type) (mount, force_rescan, cancellable, callback, user_data);
}

/**
 * g_mount_guess_content_type_finish:
 * @mount: a #GMount
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to 
 *     ignore
 * 
 * Finishes guessing content types of @mount. If any errors occurred
 * during the operation, @error will be set to contain the errors and
 * %FALSE will be returned. In particular, you may get an 
 * %G_IO_ERROR_NOT_SUPPORTED if the mount does not support content 
 * guessing.
 * 
 * Returns: (transfer full) (element-type utf8): a %NULL-terminated array of content types or %NULL on error. 
 *     Caller should free this array with g_strfreev() when done with it.
 *
 * Since: 2.18
 **/
gchar **
g_mount_guess_content_type_finish (GMount        *mount,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;
  else if (g_async_result_is_tagged (result, g_mount_guess_content_type))
    return g_task_propagate_pointer (G_TASK (result), error);
  
  iface = G_MOUNT_GET_IFACE (mount);
  return (* iface->guess_content_type_finish) (mount, result, error);
}

/**
 * g_mount_guess_content_type_sync:
 * @mount: a #GMount
 * @force_rescan: Whether to force a rescan of the content.
 *     Otherwise a cached result will be used if available
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @error: a #GError location to store the error occurring, or %NULL to
 *     ignore
 *
 * Tries to guess the type of content stored on @mount. Returns one or
 * more textual identifiers of well-known content types (typically
 * prefixed with "x-content/"), e.g. x-content/image-dcf for camera 
 * memory cards. See the 
 * [shared-mime-info](http://www.freedesktop.org/wiki/Specifications/shared-mime-info-spec)
 * specification for more on x-content types.
 *
 * This is an synchronous operation and as such may block doing IO;
 * see g_mount_guess_content_type() for the asynchronous version.
 *
 * Returns: (transfer full) (element-type utf8): a %NULL-terminated array of content types or %NULL on error.
 *     Caller should free this array with g_strfreev() when done with it.
 *
 * Since: 2.18
 */
char **
g_mount_guess_content_type_sync (GMount              *mount,
                                 gboolean             force_rescan,
                                 GCancellable        *cancellable,
                                 GError             **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->guess_content_type_sync == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           /* Translators: This is an error
                            * message for mount objects that
                            * don't implement content type guessing. */
                           _("mount doesn’t implement synchronous content type guessing"));

      return NULL;
    }

  return (* iface->guess_content_type_sync) (mount, force_rescan, cancellable, error);
}

G_LOCK_DEFINE_STATIC (priv_lock);

/* only access this structure when holding priv_lock */
typedef struct
{
  gint shadow_ref_count;
} GMountPrivate;

static void
free_private (GMountPrivate *private)
{
  G_LOCK (priv_lock);
  g_free (private);
  G_UNLOCK (priv_lock);
}

/* may only be called when holding priv_lock */
static GMountPrivate *
get_private (GMount *mount)
{
  GMountPrivate *private;

  private = g_object_get_data (G_OBJECT (mount), "g-mount-private");
  if (G_LIKELY (private != NULL))
    goto out;

  private = g_new0 (GMountPrivate, 1);
  g_object_set_data_full (G_OBJECT (mount),
                          "g-mount-private",
                          private,
                          (GDestroyNotify) free_private);

 out:
  return private;
}

/**
 * g_mount_is_shadowed:
 * @mount: A #GMount.
 *
 * Determines if @mount is shadowed. Applications or libraries should
 * avoid displaying @mount in the user interface if it is shadowed.
 *
 * A mount is said to be shadowed if there exists one or more user
 * visible objects (currently #GMount objects) with a root that is
 * inside the root of @mount.
 *
 * One application of shadow mounts is when exposing a single file
 * system that is used to address several logical volumes. In this
 * situation, a #GVolumeMonitor implementation would create two
 * #GVolume objects (for example, one for the camera functionality of
 * the device and one for a SD card reader on the device) with
 * activation URIs `gphoto2://[usb:001,002]/store1/`
 * and `gphoto2://[usb:001,002]/store2/`. When the
 * underlying mount (with root
 * `gphoto2://[usb:001,002]/`) is mounted, said
 * #GVolumeMonitor implementation would create two #GMount objects
 * (each with their root matching the corresponding volume activation
 * root) that would shadow the original mount.
 *
 * The proxy monitor in GVfs 2.26 and later, automatically creates and
 * manage shadow mounts (and shadows the underlying mount) if the
 * activation root on a #GVolume is set.
 *
 * Returns: %TRUE if @mount is shadowed.
 *
 * Since: 2.20
 **/
gboolean
g_mount_is_shadowed (GMount *mount)
{
  GMountPrivate *priv;
  gboolean ret;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);

  G_LOCK (priv_lock);
  priv = get_private (mount);
  ret = (priv->shadow_ref_count > 0);
  G_UNLOCK (priv_lock);

  return ret;
}

/**
 * g_mount_shadow:
 * @mount: A #GMount.
 *
 * Increments the shadow count on @mount. Usually used by
 * #GVolumeMonitor implementations when creating a shadow mount for
 * @mount, see g_mount_is_shadowed() for more information. The caller
 * will need to emit the #GMount::changed signal on @mount manually.
 *
 * Since: 2.20
 **/
void
g_mount_shadow (GMount *mount)
{
  GMountPrivate *priv;

  g_return_if_fail (G_IS_MOUNT (mount));

  G_LOCK (priv_lock);
  priv = get_private (mount);
  priv->shadow_ref_count += 1;
  G_UNLOCK (priv_lock);
}

/**
 * g_mount_unshadow:
 * @mount: A #GMount.
 *
 * Decrements the shadow count on @mount. Usually used by
 * #GVolumeMonitor implementations when destroying a shadow mount for
 * @mount, see g_mount_is_shadowed() for more information. The caller
 * will need to emit the #GMount::changed signal on @mount manually.
 *
 * Since: 2.20
 **/
void
g_mount_unshadow (GMount *mount)
{
  GMountPrivate *priv;

  g_return_if_fail (G_IS_MOUNT (mount));

  G_LOCK (priv_lock);
  priv = get_private (mount);
  priv->shadow_ref_count -= 1;
  if (priv->shadow_ref_count < 0)
    g_warning ("Shadow ref count on GMount is negative");
  G_UNLOCK (priv_lock);
}

/**
 * g_mount_get_sort_key:
 * @mount: A #GMount.
 *
 * Gets the sort key for @mount, if any.
 *
 * Returns: Sorting key for @mount or %NULL if no such key is available.
 *
 * Since: 2.32
 */
const gchar *
g_mount_get_sort_key (GMount  *mount)
{
  const gchar *ret = NULL;
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);
  if (iface->get_sort_key != NULL)
    ret = iface->get_sort_key (mount);

  return ret;
}
