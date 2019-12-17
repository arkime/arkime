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

#include <string.h>

#include <glib.h>
#include "gunixvolumemonitor.h"
#include "gunixmounts.h"
#include "gunixmount.h"
#include "gunixvolume.h"
#include "gmount.h"
#include "gmountprivate.h"
#include "giomodule.h"
#include "glibintl.h"


struct _GUnixVolumeMonitor {
  GNativeVolumeMonitor parent;

  GUnixMountMonitor *mount_monitor;

  GList *last_mountpoints;
  GList *last_mounts;

  GList *volumes;
  GList *mounts;
};

static void mountpoints_changed      (GUnixMountMonitor  *mount_monitor,
                                      gpointer            user_data);
static void mounts_changed           (GUnixMountMonitor  *mount_monitor,
                                      gpointer            user_data);
static void update_volumes           (GUnixVolumeMonitor *monitor);
static void update_mounts            (GUnixVolumeMonitor *monitor);

#define g_unix_volume_monitor_get_type _g_unix_volume_monitor_get_type
G_DEFINE_TYPE_WITH_CODE (GUnixVolumeMonitor, g_unix_volume_monitor, G_TYPE_NATIVE_VOLUME_MONITOR,
                         g_io_extension_point_implement (G_NATIVE_VOLUME_MONITOR_EXTENSION_POINT_NAME,
							 g_define_type_id,
							 "unix",
							 0));

static void
g_unix_volume_monitor_finalize (GObject *object)
{
  GUnixVolumeMonitor *monitor;
  
  monitor = G_UNIX_VOLUME_MONITOR (object);

  g_signal_handlers_disconnect_by_func (monitor->mount_monitor, mountpoints_changed, monitor);
  g_signal_handlers_disconnect_by_func (monitor->mount_monitor, mounts_changed, monitor);
					
  g_object_unref (monitor->mount_monitor);

  g_list_free_full (monitor->last_mountpoints, (GDestroyNotify) g_unix_mount_point_free);
  g_list_free_full (monitor->last_mounts, (GDestroyNotify) g_unix_mount_free);

  g_list_free_full (monitor->volumes, g_object_unref);
  g_list_free_full (monitor->mounts, g_object_unref);

  G_OBJECT_CLASS (g_unix_volume_monitor_parent_class)->finalize (object);
}

static void
g_unix_volume_monitor_dispose (GObject *object)
{
  GUnixVolumeMonitor *monitor;

  monitor = G_UNIX_VOLUME_MONITOR (object);

  g_list_free_full (monitor->volumes, g_object_unref);
  monitor->volumes = NULL;
  
  g_list_free_full (monitor->mounts, g_object_unref);
  monitor->mounts = NULL;
  
  G_OBJECT_CLASS (g_unix_volume_monitor_parent_class)->dispose (object);
}

static GList *
get_mounts (GVolumeMonitor *volume_monitor)
{
  GUnixVolumeMonitor *monitor;
  
  monitor = G_UNIX_VOLUME_MONITOR (volume_monitor);

  return g_list_copy_deep (monitor->mounts, (GCopyFunc) g_object_ref, NULL);
}

static GList *
get_volumes (GVolumeMonitor *volume_monitor)
{
  GUnixVolumeMonitor *monitor;
  
  monitor = G_UNIX_VOLUME_MONITOR (volume_monitor);

  return g_list_copy_deep (monitor->volumes, (GCopyFunc) g_object_ref, NULL);
}

static GList *
get_connected_drives (GVolumeMonitor *volume_monitor)
{
  return NULL;
}

static GVolume *
get_volume_for_uuid (GVolumeMonitor *volume_monitor, const char *uuid)
{
  return NULL;
}

static GMount *
get_mount_for_uuid (GVolumeMonitor *volume_monitor, const char *uuid)
{
  return NULL;
}

static gboolean
is_supported (void)
{
  return TRUE;
}

static GMount *
get_mount_for_mount_path (const char *mount_path,
                          GCancellable *cancellable)
{
  GUnixMountEntry *mount_entry;
  GUnixMount *mount;

  mount_entry = g_unix_mount_at (mount_path, NULL);

  if (!mount_entry)
    return NULL;

  /* TODO: Set mountable volume? */
  mount = _g_unix_mount_new (NULL, mount_entry, NULL);

  g_unix_mount_free (mount_entry);

  return G_MOUNT (mount);
}

static void
g_unix_volume_monitor_class_init (GUnixVolumeMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GVolumeMonitorClass *monitor_class = G_VOLUME_MONITOR_CLASS (klass);
  GNativeVolumeMonitorClass *native_class = G_NATIVE_VOLUME_MONITOR_CLASS (klass);
  
  gobject_class->finalize = g_unix_volume_monitor_finalize;
  gobject_class->dispose = g_unix_volume_monitor_dispose;

  monitor_class->get_mounts = get_mounts;
  monitor_class->get_volumes = get_volumes;
  monitor_class->get_connected_drives = get_connected_drives;
  monitor_class->get_volume_for_uuid = get_volume_for_uuid;
  monitor_class->get_mount_for_uuid = get_mount_for_uuid;
  monitor_class->is_supported = is_supported;

  native_class->get_mount_for_mount_path = get_mount_for_mount_path;
}

void
_g_unix_volume_monitor_update (GUnixVolumeMonitor *unix_monitor)
{
  /* Update both to make sure volumes are created before mounts */
  update_volumes (unix_monitor);
  update_mounts (unix_monitor);
}

static void
mountpoints_changed (GUnixMountMonitor *mount_monitor,
		     gpointer           user_data)
{
  GUnixVolumeMonitor *unix_monitor = user_data;

  _g_unix_volume_monitor_update (unix_monitor);
}

static void
mounts_changed (GUnixMountMonitor *mount_monitor,
		gpointer           user_data)
{
  GUnixVolumeMonitor *unix_monitor = user_data;

  _g_unix_volume_monitor_update (unix_monitor);
}

static void
g_unix_volume_monitor_init (GUnixVolumeMonitor *unix_monitor)
{

  unix_monitor->mount_monitor = g_unix_mount_monitor_get ();

  g_signal_connect (unix_monitor->mount_monitor,
		    "mounts-changed", G_CALLBACK (mounts_changed),
		    unix_monitor);
  
  g_signal_connect (unix_monitor->mount_monitor,
		    "mountpoints-changed", G_CALLBACK (mountpoints_changed),
		    unix_monitor);
		    
  _g_unix_volume_monitor_update (unix_monitor);
}

GVolumeMonitor *
_g_unix_volume_monitor_new (void)
{
  GUnixVolumeMonitor *monitor;

  monitor = g_object_new (G_TYPE_UNIX_VOLUME_MONITOR, NULL);
  
  return G_VOLUME_MONITOR (monitor);
}

static void
diff_sorted_lists (GList         *list1, 
                   GList         *list2, 
                   GCompareFunc   compare,
		   GList        **added, 
                   GList        **removed)
{
  int order;
  
  *added = *removed = NULL;
  
  while (list1 != NULL &&
	 list2 != NULL)
    {
      order = (*compare) (list1->data, list2->data);
      if (order < 0)
	{
	  *removed = g_list_prepend (*removed, list1->data);
	  list1 = list1->next;
	}
      else if (order > 0)
	{
	  *added = g_list_prepend (*added, list2->data);
	  list2 = list2->next;
	}
      else
	{ /* same item */
	  list1 = list1->next;
	  list2 = list2->next;
	}
    }

  while (list1 != NULL)
    {
      *removed = g_list_prepend (*removed, list1->data);
      list1 = list1->next;
    }
  while (list2 != NULL)
    {
      *added = g_list_prepend (*added, list2->data);
      list2 = list2->next;
    }
}

GUnixVolume *
_g_unix_volume_monitor_lookup_volume_for_mount_path (GUnixVolumeMonitor *monitor,
                                                     const char         *mount_path)
{
  GList *l;

  for (l = monitor->volumes; l != NULL; l = l->next)
    {
      GUnixVolume *volume = l->data;

      if (_g_unix_volume_has_mount_path (volume, mount_path))
	return volume;
    }
  
  return NULL;
}

static GUnixMount *
find_mount_by_mountpath (GUnixVolumeMonitor *monitor,
                         const char *mount_path)
{
  GList *l;

  for (l = monitor->mounts; l != NULL; l = l->next)
    {
      GUnixMount *mount = l->data;

      if (_g_unix_mount_has_mount_path (mount, mount_path))
	return mount;
    }
  
  return NULL;
}

static void
update_volumes (GUnixVolumeMonitor *monitor)
{
  GList *new_mountpoints;
  GList *removed, *added;
  GList *l;
  GUnixVolume *volume;
  
  new_mountpoints = g_unix_mount_points_get (NULL);
  
  new_mountpoints = g_list_sort (new_mountpoints, (GCompareFunc) g_unix_mount_point_compare);
  
  diff_sorted_lists (monitor->last_mountpoints,
		     new_mountpoints, (GCompareFunc) g_unix_mount_point_compare,
		     &added, &removed);
  
  for (l = removed; l != NULL; l = l->next)
    {
      GUnixMountPoint *mountpoint = l->data;
      
      volume = _g_unix_volume_monitor_lookup_volume_for_mount_path (monitor,
                                                                    g_unix_mount_point_get_mount_path (mountpoint));
      if (volume)
	{
	  _g_unix_volume_disconnected (volume);
	  monitor->volumes = g_list_remove (monitor->volumes, volume);
	  g_signal_emit_by_name (monitor, "volume-removed", volume);
	  g_signal_emit_by_name (volume, "removed");
	  g_object_unref (volume);
	}
    }
  
  for (l = added; l != NULL; l = l->next)
    {
      GUnixMountPoint *mountpoint = l->data;
      
      volume = _g_unix_volume_new (G_VOLUME_MONITOR (monitor), mountpoint);
      if (volume)
	{
	  monitor->volumes = g_list_prepend (monitor->volumes, volume);
	  g_signal_emit_by_name (monitor, "volume-added", volume);
	}
    }
  
  g_list_free (added);
  g_list_free (removed);
  g_list_free_full (monitor->last_mountpoints, (GDestroyNotify) g_unix_mount_point_free);
  monitor->last_mountpoints = new_mountpoints;
}

static void
update_mounts (GUnixVolumeMonitor *monitor)
{
  GList *new_mounts;
  GList *removed, *added;
  GList *l;
  GUnixMount *mount;
  GUnixVolume *volume;
  const char *mount_path;
  
  new_mounts = g_unix_mounts_get (NULL);
  
  new_mounts = g_list_sort (new_mounts, (GCompareFunc) g_unix_mount_compare);
  
  diff_sorted_lists (monitor->last_mounts,
		     new_mounts, (GCompareFunc) g_unix_mount_compare,
		     &added, &removed);
  
  for (l = removed; l != NULL; l = l->next)
    {
      GUnixMountEntry *mount_entry = l->data;
      
      mount = find_mount_by_mountpath (monitor, g_unix_mount_get_mount_path (mount_entry));
      if (mount)
	{
	  _g_unix_mount_unmounted (mount);
	  monitor->mounts = g_list_remove (monitor->mounts, mount);
	  g_signal_emit_by_name (monitor, "mount-removed", mount);
	  g_signal_emit_by_name (mount, "unmounted");
	  g_object_unref (mount);
	}
    }
  
  for (l = added; l != NULL; l = l->next)
    {
      GUnixMountEntry *mount_entry = l->data;

      mount_path = g_unix_mount_get_mount_path (mount_entry);
      
      volume = _g_unix_volume_monitor_lookup_volume_for_mount_path (monitor, mount_path);
      mount = _g_unix_mount_new (G_VOLUME_MONITOR (monitor), mount_entry, volume);
      if (mount)
	{
	  monitor->mounts = g_list_prepend (monitor->mounts, mount);
	  g_signal_emit_by_name (monitor, "mount-added", mount);
	}
    }
  
  g_list_free (added);
  g_list_free (removed);
  g_list_free_full (monitor->last_mounts, (GDestroyNotify) g_unix_mount_free);
  monitor->last_mounts = new_mounts;
}
