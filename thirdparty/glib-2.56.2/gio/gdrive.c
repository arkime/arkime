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
#include "gdrive.h"
#include "gtask.h"
#include "gthemedicon.h"
#include "gasyncresult.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gdrive
 * @short_description: Drive management
 * @include: gio/gio.h
 *
 * #GDrive - this represent a piece of hardware connected to the machine.
 * It's generally only created for removable hardware or hardware with
 * removable media.
 *
 * #GDrive is a container class for #GVolume objects that stem from
 * the same piece of media. As such, #GDrive abstracts a drive with
 * (or without) removable media and provides operations for querying
 * whether media is available, determining whether media change is
 * automatically detected and ejecting the media.
 *
 * If the #GDrive reports that media isn't automatically detected, one
 * can poll for media; typically one should not do this periodically
 * as a poll for media operation is potententially expensive and may
 * spin up the drive creating noise.
 *
 * #GDrive supports starting and stopping drives with authentication
 * support for the former. This can be used to support a diverse set
 * of use cases including connecting/disconnecting iSCSI devices,
 * powering down external disk enclosures and starting/stopping
 * multi-disk devices such as RAID devices. Note that the actual
 * semantics and side-effects of starting/stopping a #GDrive may vary
 * according to implementation. To choose the correct verbs in e.g. a
 * file manager, use g_drive_get_start_stop_type().
 *
 * For porting from GnomeVFS note that there is no equivalent of
 * #GDrive in that API.
 **/

typedef GDriveIface GDriveInterface;
G_DEFINE_INTERFACE(GDrive, g_drive, G_TYPE_OBJECT)

static void
g_drive_default_init (GDriveInterface *iface)
{
  /**
   * GDrive::changed:
   * @drive: a #GDrive.
   *
   * Emitted when the drive's state has changed.
   **/
  g_signal_new (I_("changed"),
		G_TYPE_DRIVE,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GDriveIface, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /**
   * GDrive::disconnected:
   * @drive: a #GDrive.
   *
   * This signal is emitted when the #GDrive have been
   * disconnected. If the recipient is holding references to the
   * object they should release them so the object can be
   * finalized.
   **/
  g_signal_new (I_("disconnected"),
		G_TYPE_DRIVE,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GDriveIface, disconnected),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /**
   * GDrive::eject-button:
   * @drive: a #GDrive.
   *
   * Emitted when the physical eject button (if any) of a drive has
   * been pressed.
   **/
  g_signal_new (I_("eject-button"),
		G_TYPE_DRIVE,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GDriveIface, eject_button),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

  /**
   * GDrive::stop-button:
   * @drive: a #GDrive.
   *
   * Emitted when the physical stop button (if any) of a drive has
   * been pressed.
   *
   * Since: 2.22
   **/
  g_signal_new (I_("stop-button"),
		G_TYPE_DRIVE,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GDriveIface, stop_button),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

/**
 * g_drive_get_name:
 * @drive: a #GDrive.
 * 
 * Gets the name of @drive.
 *
 * Returns: a string containing @drive's name. The returned 
 *     string should be freed when no longer needed.
 **/
char *
g_drive_get_name (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->get_name) (drive);
}

/**
 * g_drive_get_icon:
 * @drive: a #GDrive.
 * 
 * Gets the icon for @drive.
 * 
 * Returns: (transfer full): #GIcon for the @drive.
 *    Free the returned object with g_object_unref().
 **/
GIcon *
g_drive_get_icon (GDrive *drive)
{
  GDriveIface *iface;
  
  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->get_icon) (drive);
}

/**
 * g_drive_get_symbolic_icon:
 * @drive: a #GDrive.
 * 
 * Gets the icon for @drive.
 * 
 * Returns: (transfer full): symbolic #GIcon for the @drive.
 *    Free the returned object with g_object_unref().
 *
 * Since: 2.34
 **/
GIcon *
g_drive_get_symbolic_icon (GDrive *drive)
{
  GDriveIface *iface;
  GIcon *ret;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->get_symbolic_icon != NULL)
    ret = iface->get_symbolic_icon (drive);
  else
    ret = g_themed_icon_new_with_default_fallbacks ("drive-removable-media-symbolic");

  return ret;
}

/**
 * g_drive_has_volumes:
 * @drive: a #GDrive.
 * 
 * Check if @drive has any mountable volumes.
 * 
 * Returns: %TRUE if the @drive contains volumes, %FALSE otherwise.
 **/
gboolean
g_drive_has_volumes (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->has_volumes) (drive);
}

/**
 * g_drive_get_volumes:
 * @drive: a #GDrive.
 * 
 * Get a list of mountable volumes for @drive.
 *
 * The returned list should be freed with g_list_free(), after
 * its elements have been unreffed with g_object_unref().
 * 
 * Returns: (element-type GVolume) (transfer full): #GList containing any #GVolume objects on the given @drive.
 **/
GList *
g_drive_get_volumes (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->get_volumes) (drive);
}

/**
 * g_drive_is_media_check_automatic:
 * @drive: a #GDrive.
 * 
 * Checks if @drive is capabable of automatically detecting media changes.
 * 
 * Returns: %TRUE if the @drive is capabable of automatically detecting 
 *     media changes, %FALSE otherwise.
 **/
gboolean
g_drive_is_media_check_automatic (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->is_media_check_automatic) (drive);
}

/**
 * g_drive_is_removable:
 * @drive: a #GDrive.
 *
 * Checks if the #GDrive and/or its media is considered removable by the user.
 * See g_drive_is_media_removable().
 *
 * Returns: %TRUE if @drive and/or its media is considered removable, %FALSE otherwise.
 *
 * Since: 2.50
 **/
gboolean
g_drive_is_removable (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);
  if (iface->is_removable != NULL)
    return iface->is_removable (drive);

  return FALSE;
}

/**
 * g_drive_is_media_removable:
 * @drive: a #GDrive.
 * 
 * Checks if the @drive supports removable media.
 * 
 * Returns: %TRUE if @drive supports removable media, %FALSE otherwise.
 **/
gboolean
g_drive_is_media_removable (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->is_media_removable) (drive);
}

/**
 * g_drive_has_media:
 * @drive: a #GDrive.
 * 
 * Checks if the @drive has media. Note that the OS may not be polling
 * the drive for media changes; see g_drive_is_media_check_automatic()
 * for more details.
 * 
 * Returns: %TRUE if @drive has media, %FALSE otherwise.
 **/
gboolean
g_drive_has_media (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->has_media) (drive);
}

/**
 * g_drive_can_eject:
 * @drive: a #GDrive.
 * 
 * Checks if a drive can be ejected.
 * 
 * Returns: %TRUE if the @drive can be ejected, %FALSE otherwise.
 **/
gboolean
g_drive_can_eject (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->can_eject == NULL)
    return FALSE;

  return (* iface->can_eject) (drive);
}

/**
 * g_drive_can_poll_for_media:
 * @drive: a #GDrive.
 * 
 * Checks if a drive can be polled for media changes.
 * 
 * Returns: %TRUE if the @drive can be polled for media changes,
 *     %FALSE otherwise.
 **/
gboolean
g_drive_can_poll_for_media (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->poll_for_media == NULL)
    return FALSE;

  return (* iface->can_poll_for_media) (drive);
}

/**
 * g_drive_eject:
 * @drive: a #GDrive.
 * @flags: flags affecting the unmount if required for eject
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data to pass to @callback
 * 
 * Asynchronously ejects a drive.
 *
 * When the operation is finished, @callback will be called.
 * You can then call g_drive_eject_finish() to obtain the
 * result of the operation.
 *
 * Deprecated: 2.22: Use g_drive_eject_with_operation() instead.
 **/
void
g_drive_eject (GDrive              *drive,
	       GMountUnmountFlags   flags,
	       GCancellable        *cancellable,
	       GAsyncReadyCallback  callback,
	       gpointer             user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->eject == NULL)
    {
      g_task_report_new_error (drive, callback, user_data,
                               g_drive_eject_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("drive doesn’t implement eject"));
      return;
    }
  
  (* iface->eject) (drive, flags, cancellable, callback, user_data);
}

/**
 * g_drive_eject_finish:
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes ejecting a drive.
 * 
 * Returns: %TRUE if the drive has been ejected successfully,
 *     %FALSE otherwise.
 *
 * Deprecated: 2.22: Use g_drive_eject_with_operation_finish() instead.
 **/
gboolean
g_drive_eject_finish (GDrive        *drive,
		      GAsyncResult  *result,
		      GError       **error)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_drive_eject_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_DRIVE_GET_IFACE (drive);
  
  return (* iface->eject_finish) (drive, result, error);
}

/**
 * g_drive_eject_with_operation:
 * @drive: a #GDrive.
 * @flags: flags affecting the unmount if required for eject
 * @mount_operation: (nullable): a #GMountOperation or %NULL to avoid
 *     user interaction.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 *
 * Ejects a drive. This is an asynchronous operation, and is
 * finished by calling g_drive_eject_with_operation_finish() with the @drive
 * and #GAsyncResult data returned in the @callback.
 *
 * Since: 2.22
 **/
void
g_drive_eject_with_operation (GDrive              *drive,
                              GMountUnmountFlags   flags,
                              GMountOperation     *mount_operation,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->eject == NULL && iface->eject_with_operation == NULL)
    {
      g_task_report_new_error (drive, callback, user_data,
                               g_drive_eject_with_operation,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               /* Translators: This is an error
                                * message for drive objects that
                                * don't implement any of eject or eject_with_operation. */
                               _("drive doesn’t implement eject or eject_with_operation"));
      return;
    }

  if (iface->eject_with_operation != NULL)
    (* iface->eject_with_operation) (drive, flags, mount_operation, cancellable, callback, user_data);
  else
    (* iface->eject) (drive, flags, cancellable, callback, user_data);
}

/**
 * g_drive_eject_with_operation_finish:
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 *     ignore.
 *
 * Finishes ejecting a drive. If any errors occurred during the operation,
 * @error will be set to contain the errors and %FALSE will be returned.
 *
 * Returns: %TRUE if the drive was successfully ejected. %FALSE otherwise.
 *
 * Since: 2.22
 **/
gboolean
g_drive_eject_with_operation_finish (GDrive        *drive,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_drive_eject_with_operation))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_DRIVE_GET_IFACE (drive);
  if (iface->eject_with_operation_finish != NULL)
    return (* iface->eject_with_operation_finish) (drive, result, error);
  else
    return (* iface->eject_finish) (drive, result, error);
}

/**
 * g_drive_poll_for_media:
 * @drive: a #GDrive.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data to pass to @callback
 * 
 * Asynchronously polls @drive to see if media has been inserted or removed.
 * 
 * When the operation is finished, @callback will be called.
 * You can then call g_drive_poll_for_media_finish() to obtain the
 * result of the operation.
 **/
void
g_drive_poll_for_media (GDrive              *drive,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->poll_for_media == NULL)
    {
      g_task_report_new_error (drive, callback, user_data,
                               g_drive_poll_for_media,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("drive doesn’t implement polling for media"));
      return;
    }
  
  (* iface->poll_for_media) (drive, cancellable, callback, user_data);
}

/**
 * g_drive_poll_for_media_finish:
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes an operation started with g_drive_poll_for_media() on a drive.
 * 
 * Returns: %TRUE if the drive has been poll_for_mediaed successfully,
 *     %FALSE otherwise.
 **/
gboolean
g_drive_poll_for_media_finish (GDrive        *drive,
                               GAsyncResult  *result,
                               GError       **error)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_drive_poll_for_media))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_DRIVE_GET_IFACE (drive);
  
  return (* iface->poll_for_media_finish) (drive, result, error);
}

/**
 * g_drive_get_identifier:
 * @drive: a #GDrive
 * @kind: the kind of identifier to return
 *
 * Gets the identifier of the given kind for @drive.
 *
 * Returns: a newly allocated string containing the
 *     requested identfier, or %NULL if the #GDrive
 *     doesn't have this kind of identifier.
 */
char *
g_drive_get_identifier (GDrive     *drive,
			const char *kind)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);
  g_return_val_if_fail (kind != NULL, NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->get_identifier == NULL)
    return NULL;
  
  return (* iface->get_identifier) (drive, kind);
}

/**
 * g_drive_enumerate_identifiers:
 * @drive: a #GDrive
 *
 * Gets the kinds of identifiers that @drive has. 
 * Use g_drive_get_identifier() to obtain the identifiers
 * themselves.
 *
 * Returns: (transfer full) (array zero-terminated=1): a %NULL-terminated
 *     array of strings containing kinds of identifiers. Use g_strfreev()
 *     to free.
 */
char **
g_drive_enumerate_identifiers (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);
  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->enumerate_identifiers == NULL)
    return NULL;
  
  return (* iface->enumerate_identifiers) (drive);
}

/**
 * g_drive_get_start_stop_type:
 * @drive: a #GDrive.
 *
 * Gets a hint about how a drive can be started/stopped.
 *
 * Returns: A value from the #GDriveStartStopType enumeration.
 *
 * Since: 2.22
 */
GDriveStartStopType
g_drive_get_start_stop_type (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->get_start_stop_type == NULL)
    return G_DRIVE_START_STOP_TYPE_UNKNOWN;

  return (* iface->get_start_stop_type) (drive);
}


/**
 * g_drive_can_start:
 * @drive: a #GDrive.
 *
 * Checks if a drive can be started.
 *
 * Returns: %TRUE if the @drive can be started, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_drive_can_start (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->can_start == NULL)
    return FALSE;

  return (* iface->can_start) (drive);
}

/**
 * g_drive_can_start_degraded:
 * @drive: a #GDrive.
 *
 * Checks if a drive can be started degraded.
 *
 * Returns: %TRUE if the @drive can be started degraded, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_drive_can_start_degraded (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->can_start_degraded == NULL)
    return FALSE;

  return (* iface->can_start_degraded) (drive);
}

/**
 * g_drive_start:
 * @drive: a #GDrive.
 * @flags: flags affecting the start operation.
 * @mount_operation: (nullable): a #GMountOperation or %NULL to avoid
 *     user interaction.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data to pass to @callback
 *
 * Asynchronously starts a drive.
 *
 * When the operation is finished, @callback will be called.
 * You can then call g_drive_start_finish() to obtain the
 * result of the operation.
 *
 * Since: 2.22
 */
void
g_drive_start (GDrive              *drive,
               GDriveStartFlags     flags,
               GMountOperation     *mount_operation,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->start == NULL)
    {
      g_task_report_new_error (drive, callback, user_data,
                               g_drive_start,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("drive doesn’t implement start"));
      return;
    }

  (* iface->start) (drive, flags, mount_operation, cancellable, callback, user_data);
}

/**
 * g_drive_start_finish:
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes starting a drive.
 *
 * Returns: %TRUE if the drive has been started successfully,
 *     %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_drive_start_finish (GDrive         *drive,
                      GAsyncResult   *result,
                      GError        **error)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_drive_start))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->start_finish) (drive, result, error);
}

/**
 * g_drive_can_stop:
 * @drive: a #GDrive.
 *
 * Checks if a drive can be stopped.
 *
 * Returns: %TRUE if the @drive can be stopped, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_drive_can_stop (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->can_stop == NULL)
    return FALSE;

  return (* iface->can_stop) (drive);
}

/**
 * g_drive_stop:
 * @drive: a #GDrive.
 * @flags: flags affecting the unmount if required for stopping.
 * @mount_operation: (nullable): a #GMountOperation or %NULL to avoid
 *     user interaction.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (nullable): a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data to pass to @callback
 *
 * Asynchronously stops a drive.
 *
 * When the operation is finished, @callback will be called.
 * You can then call g_drive_stop_finish() to obtain the
 * result of the operation.
 *
 * Since: 2.22
 */
void
g_drive_stop (GDrive               *drive,
              GMountUnmountFlags    flags,
              GMountOperation      *mount_operation,
              GCancellable         *cancellable,
              GAsyncReadyCallback   callback,
              gpointer              user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->stop == NULL)
    {
      g_task_report_new_error (drive, callback, user_data,
                               g_drive_start,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("drive doesn’t implement stop"));
      return;
    }

  (* iface->stop) (drive, flags, mount_operation, cancellable, callback, user_data);
}

/**
 * g_drive_stop_finish:
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes stopping a drive.
 *
 * Returns: %TRUE if the drive has been stopped successfully,
 *     %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_drive_stop_finish (GDrive        *drive,
                     GAsyncResult  *result,
                     GError       **error)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_drive_start))
    return g_task_propagate_boolean (G_TASK (result), error);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->stop_finish) (drive, result, error);
}

/**
 * g_drive_get_sort_key:
 * @drive: A #GDrive.
 *
 * Gets the sort key for @drive, if any.
 *
 * Returns: Sorting key for @drive or %NULL if no such key is available.
 *
 * Since: 2.32
 */
const gchar *
g_drive_get_sort_key (GDrive  *drive)
{
  const gchar *ret = NULL;
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);
  if (iface->get_sort_key != NULL)
    ret = iface->get_sort_key (drive);

  return ret;
}
