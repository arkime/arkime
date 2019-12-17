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
 *         David Zeuthen <davidz@redhat.com>
 */

#include "config.h"
#include "gvolumemonitor.h"
#include "gvolume.h"
#include "gmount.h"
#include "gdrive.h"
#include "glibintl.h"


/**
 * SECTION:gvolumemonitor
 * @short_description: Volume Monitor
 * @include: gio/gio.h
 * @see_also: #GFileMonitor
 * 
 * #GVolumeMonitor is for listing the user interesting devices and volumes
 * on the computer. In other words, what a file selector or file manager
 * would show in a sidebar. 
 *
 * #GVolumeMonitor is not
 * [thread-default-context aware][g-main-context-push-thread-default],
 * and so should not be used other than from the main thread, with no
 * thread-default-context active.
 **/

G_DEFINE_TYPE (GVolumeMonitor, g_volume_monitor, G_TYPE_OBJECT)

enum {
  VOLUME_ADDED,
  VOLUME_REMOVED,
  VOLUME_CHANGED,
  MOUNT_ADDED,
  MOUNT_REMOVED,
  MOUNT_PRE_UNMOUNT,
  MOUNT_CHANGED,
  DRIVE_CONNECTED,
  DRIVE_DISCONNECTED,
  DRIVE_CHANGED,
  DRIVE_EJECT_BUTTON,
  DRIVE_STOP_BUTTON,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void
g_volume_monitor_finalize (GObject *object)
{
  G_OBJECT_CLASS (g_volume_monitor_parent_class)->finalize (object);
}

static void
g_volume_monitor_class_init (GVolumeMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_volume_monitor_finalize;

  /**
   * GVolumeMonitor::volume-added:
   * @volume_monitor: The volume monitor emitting the signal.
   * @volume: a #GVolume that was added.
   * 
   * Emitted when a mountable volume is added to the system.
   **/
  signals[VOLUME_ADDED] = g_signal_new (I_("volume-added"),
                                        G_TYPE_VOLUME_MONITOR,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GVolumeMonitorClass, volume_added),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE, 1, G_TYPE_VOLUME);
  
  /**
   * GVolumeMonitor::volume-removed:
   * @volume_monitor: The volume monitor emitting the signal.
   * @volume: a #GVolume that was removed.
   * 
   * Emitted when a mountable volume is removed from the system.
   **/  
  signals[VOLUME_REMOVED] = g_signal_new (I_("volume-removed"),
                                          G_TYPE_VOLUME_MONITOR,
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (GVolumeMonitorClass, volume_removed),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE, 1, G_TYPE_VOLUME);
  
  /**
   * GVolumeMonitor::volume-changed:
   * @volume_monitor: The volume monitor emitting the signal.
   * @volume: a #GVolume that changed.
   * 
   * Emitted when mountable volume is changed.
   **/  
  signals[VOLUME_CHANGED] = g_signal_new (I_("volume-changed"),
                                          G_TYPE_VOLUME_MONITOR,
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (GVolumeMonitorClass, volume_changed),
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE, 1, G_TYPE_VOLUME);

  /**
   * GVolumeMonitor::mount-added:
   * @volume_monitor: The volume monitor emitting the signal.
   * @mount: a #GMount that was added.
   * 
   * Emitted when a mount is added.
   **/
  signals[MOUNT_ADDED] = g_signal_new (I_("mount-added"),
                                       G_TYPE_VOLUME_MONITOR,
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET (GVolumeMonitorClass, mount_added),
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__OBJECT,
                                       G_TYPE_NONE, 1, G_TYPE_MOUNT);

  /**
   * GVolumeMonitor::mount-removed:
   * @volume_monitor: The volume monitor emitting the signal.
   * @mount: a #GMount that was removed.
   * 
   * Emitted when a mount is removed.
   **/
  signals[MOUNT_REMOVED] = g_signal_new (I_("mount-removed"),
                                         G_TYPE_VOLUME_MONITOR,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GVolumeMonitorClass, mount_removed),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__OBJECT,
                                         G_TYPE_NONE, 1, G_TYPE_MOUNT);

  /**
   * GVolumeMonitor::mount-pre-unmount:
   * @volume_monitor: The volume monitor emitting the signal.
   * @mount: a #GMount that is being unmounted.
   *
   * May be emitted when a mount is about to be removed.
   *
   * This signal depends on the backend and is only emitted if
   * GIO was used to unmount.
   **/ 
  signals[MOUNT_PRE_UNMOUNT] = g_signal_new (I_("mount-pre-unmount"),
                                             G_TYPE_VOLUME_MONITOR,
                                             G_SIGNAL_RUN_LAST,
                                             G_STRUCT_OFFSET (GVolumeMonitorClass, mount_pre_unmount),
                                             NULL, NULL,
                                             g_cclosure_marshal_VOID__OBJECT,
                                             G_TYPE_NONE, 1, G_TYPE_MOUNT);

  /**
   * GVolumeMonitor::mount-changed:
   * @volume_monitor: The volume monitor emitting the signal.
   * @mount: a #GMount that changed.
   *
   * Emitted when a mount changes.
   **/ 
  signals[MOUNT_CHANGED] = g_signal_new (I_("mount-changed"),
                                         G_TYPE_VOLUME_MONITOR,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GVolumeMonitorClass, mount_changed),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__OBJECT,
                                         G_TYPE_NONE, 1, G_TYPE_MOUNT);

  /**
   * GVolumeMonitor::drive-connected:
   * @volume_monitor: The volume monitor emitting the signal.
   * @drive: a #GDrive that was connected.
   * 
   * Emitted when a drive is connected to the system.
   **/
  signals[DRIVE_CONNECTED] = g_signal_new (I_("drive-connected"),
					   G_TYPE_VOLUME_MONITOR,
					   G_SIGNAL_RUN_LAST,
					   G_STRUCT_OFFSET (GVolumeMonitorClass, drive_connected),
					   NULL, NULL,
					   g_cclosure_marshal_VOID__OBJECT,
					   G_TYPE_NONE, 1, G_TYPE_DRIVE);
  
  /**
   * GVolumeMonitor::drive-disconnected:
   * @volume_monitor: The volume monitor emitting the signal.
   * @drive: a #GDrive that was disconnected.
   * 
   * Emitted when a drive is disconnected from the system.
   **/  
  signals[DRIVE_DISCONNECTED] = g_signal_new (I_("drive-disconnected"),
					      G_TYPE_VOLUME_MONITOR,
					      G_SIGNAL_RUN_LAST,
					      G_STRUCT_OFFSET (GVolumeMonitorClass, drive_disconnected),
					      NULL, NULL,
					      g_cclosure_marshal_VOID__OBJECT,
					      G_TYPE_NONE, 1, G_TYPE_DRIVE);

  /**
   * GVolumeMonitor::drive-changed:
   * @volume_monitor: The volume monitor emitting the signal.
   * @drive: the drive that changed
   *
   * Emitted when a drive changes.
   **/ 
  signals[DRIVE_CHANGED] = g_signal_new (I_("drive-changed"),
                                         G_TYPE_VOLUME_MONITOR,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GVolumeMonitorClass, drive_changed),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__OBJECT,
                                         G_TYPE_NONE, 1, G_TYPE_DRIVE);

  /**
   * GVolumeMonitor::drive-eject-button:
   * @volume_monitor: The volume monitor emitting the signal.
   * @drive: the drive where the eject button was pressed
   *
   * Emitted when the eject button is pressed on @drive.
   *
   * Since: 2.18
   **/
  signals[DRIVE_EJECT_BUTTON] = g_signal_new (I_("drive-eject-button"),
                                              G_TYPE_VOLUME_MONITOR,
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (GVolumeMonitorClass, drive_eject_button),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__OBJECT,
                                              G_TYPE_NONE, 1, G_TYPE_DRIVE);

  /**
   * GVolumeMonitor::drive-stop-button:
   * @volume_monitor: The volume monitor emitting the signal.
   * @drive: the drive where the stop button was pressed
   *
   * Emitted when the stop button is pressed on @drive.
   *
   * Since: 2.22
   **/
  signals[DRIVE_STOP_BUTTON] = g_signal_new (I_("drive-stop-button"),
                                             G_TYPE_VOLUME_MONITOR,
                                             G_SIGNAL_RUN_LAST,
                                             G_STRUCT_OFFSET (GVolumeMonitorClass, drive_stop_button),
                                             NULL, NULL,
                                             g_cclosure_marshal_VOID__OBJECT,
                                             G_TYPE_NONE, 1, G_TYPE_DRIVE);

}

static void
g_volume_monitor_init (GVolumeMonitor *monitor)
{
}


/**
 * g_volume_monitor_get_connected_drives:
 * @volume_monitor: a #GVolumeMonitor.
 * 
 * Gets a list of drives connected to the system.
 * 
 * The returned list should be freed with g_list_free(), after
 * its elements have been unreffed with g_object_unref().
 *
 * Returns: (element-type GDrive) (transfer full): a #GList of connected #GDrive objects.
 **/
GList *
g_volume_monitor_get_connected_drives (GVolumeMonitor *volume_monitor)
{
  GVolumeMonitorClass *class;

  g_return_val_if_fail (G_IS_VOLUME_MONITOR (volume_monitor), NULL);

  class = G_VOLUME_MONITOR_GET_CLASS (volume_monitor);

  return class->get_connected_drives (volume_monitor);
}

/**
 * g_volume_monitor_get_volumes:
 * @volume_monitor: a #GVolumeMonitor.
 * 
 * Gets a list of the volumes on the system.
 * 
 * The returned list should be freed with g_list_free(), after
 * its elements have been unreffed with g_object_unref().
 *
 * Returns: (element-type GVolume) (transfer full): a #GList of #GVolume objects.
 **/
GList *
g_volume_monitor_get_volumes (GVolumeMonitor *volume_monitor)
{
  GVolumeMonitorClass *class;

  g_return_val_if_fail (G_IS_VOLUME_MONITOR (volume_monitor), NULL);

  class = G_VOLUME_MONITOR_GET_CLASS (volume_monitor);

  return class->get_volumes (volume_monitor);
}

/**
 * g_volume_monitor_get_mounts:
 * @volume_monitor: a #GVolumeMonitor.
 * 
 * Gets a list of the mounts on the system.
 *
 * The returned list should be freed with g_list_free(), after
 * its elements have been unreffed with g_object_unref().
 * 
 * Returns: (element-type GMount) (transfer full): a #GList of #GMount objects.
 **/
GList *
g_volume_monitor_get_mounts (GVolumeMonitor *volume_monitor)
{
  GVolumeMonitorClass *class;

  g_return_val_if_fail (G_IS_VOLUME_MONITOR (volume_monitor), NULL);

  class = G_VOLUME_MONITOR_GET_CLASS (volume_monitor);

  return class->get_mounts (volume_monitor);
}

/**
 * g_volume_monitor_get_volume_for_uuid:
 * @volume_monitor: a #GVolumeMonitor.
 * @uuid: the UUID to look for
 * 
 * Finds a #GVolume object by its UUID (see g_volume_get_uuid())
 * 
 * Returns: (transfer full): a #GVolume or %NULL if no such volume is available.
 *     Free the returned object with g_object_unref().
 **/
GVolume *
g_volume_monitor_get_volume_for_uuid (GVolumeMonitor *volume_monitor, 
                                      const char     *uuid)
{
  GVolumeMonitorClass *class;

  g_return_val_if_fail (G_IS_VOLUME_MONITOR (volume_monitor), NULL);
  g_return_val_if_fail (uuid != NULL, NULL);

  class = G_VOLUME_MONITOR_GET_CLASS (volume_monitor);

  return class->get_volume_for_uuid (volume_monitor, uuid);
}

/**
 * g_volume_monitor_get_mount_for_uuid:
 * @volume_monitor: a #GVolumeMonitor.
 * @uuid: the UUID to look for
 * 
 * Finds a #GMount object by its UUID (see g_mount_get_uuid())
 * 
 * Returns: (transfer full): a #GMount or %NULL if no such mount is available.
 *     Free the returned object with g_object_unref().
 **/
GMount *
g_volume_monitor_get_mount_for_uuid (GVolumeMonitor *volume_monitor, 
                                     const char     *uuid)
{
  GVolumeMonitorClass *class;

  g_return_val_if_fail (G_IS_VOLUME_MONITOR (volume_monitor), NULL);
  g_return_val_if_fail (uuid != NULL, NULL);

  class = G_VOLUME_MONITOR_GET_CLASS (volume_monitor);

  return class->get_mount_for_uuid (volume_monitor, uuid);
}
