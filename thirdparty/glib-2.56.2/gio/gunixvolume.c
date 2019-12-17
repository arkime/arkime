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
#include <sys/wait.h>
#include <unistd.h>

#include <glib.h>
#include "gunixvolume.h"
#include "gunixmount.h"
#include "gunixmounts.h"
#include "gthemedicon.h"
#include "gvolume.h"
#include "gvolumemonitor.h"
#include "gtask.h"
#include "gioerror.h"
#include "glibintl.h"
/* for BUFSIZ */
#include <stdio.h>


struct _GUnixVolume {
  GObject parent;

  GVolumeMonitor *volume_monitor;
  GUnixMount     *mount; /* owned by volume monitor */
  
  char *device_path;
  char *mount_path;
  gboolean can_eject;

  char *identifier;
  char *identifier_type;
  
  char *name;
  GIcon *icon;
  GIcon *symbolic_icon;
};

static void g_unix_volume_volume_iface_init (GVolumeIface *iface);

#define g_unix_volume_get_type _g_unix_volume_get_type
G_DEFINE_TYPE_WITH_CODE (GUnixVolume, g_unix_volume, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_VOLUME,
						g_unix_volume_volume_iface_init))

static void
g_unix_volume_finalize (GObject *object)
{
  GUnixVolume *volume;
  
  volume = G_UNIX_VOLUME (object);

  if (volume->volume_monitor != NULL)
    g_object_unref (volume->volume_monitor);

  if (volume->mount)
    _g_unix_mount_unset_volume (volume->mount, volume);
  
  g_object_unref (volume->icon);
  g_object_unref (volume->symbolic_icon);
  g_free (volume->name);
  g_free (volume->mount_path);
  g_free (volume->device_path);
  g_free (volume->identifier);
  g_free (volume->identifier_type);

  G_OBJECT_CLASS (g_unix_volume_parent_class)->finalize (object);
}

static void
g_unix_volume_class_init (GUnixVolumeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_unix_volume_finalize;
}

static void
g_unix_volume_init (GUnixVolume *unix_volume)
{
}

GUnixVolume *
_g_unix_volume_new (GVolumeMonitor  *volume_monitor,
                    GUnixMountPoint *mountpoint)
{
  GUnixVolume *volume;
  
  if (!(g_unix_mount_point_is_user_mountable (mountpoint) ||
	g_str_has_prefix (g_unix_mount_point_get_device_path (mountpoint), "/vol/")) ||
      g_unix_mount_point_is_loopback (mountpoint))
    return NULL;
  
  volume = g_object_new (G_TYPE_UNIX_VOLUME, NULL);
  volume->volume_monitor = volume_monitor != NULL ? g_object_ref (volume_monitor) : NULL;
  volume->mount_path = g_strdup (g_unix_mount_point_get_mount_path (mountpoint));
  volume->device_path = g_strdup (g_unix_mount_point_get_device_path (mountpoint));
  volume->can_eject = g_unix_mount_point_guess_can_eject (mountpoint);

  volume->name = g_unix_mount_point_guess_name (mountpoint);
  volume->icon = g_unix_mount_point_guess_icon (mountpoint);
  volume->symbolic_icon = g_unix_mount_point_guess_symbolic_icon (mountpoint);


  if (strcmp (g_unix_mount_point_get_fs_type (mountpoint), "nfs") == 0)
    {
      volume->identifier_type = g_strdup (G_VOLUME_IDENTIFIER_KIND_NFS_MOUNT);
      volume->identifier = g_strdup (volume->device_path);
    }
  else if (g_str_has_prefix (volume->device_path, "LABEL="))
    {
      volume->identifier_type = g_strdup (G_VOLUME_IDENTIFIER_KIND_LABEL);
      volume->identifier = g_strdup (volume->device_path + 6);
    }
  else if (g_str_has_prefix (volume->device_path, "UUID="))
    {
      volume->identifier_type = g_strdup (G_VOLUME_IDENTIFIER_KIND_UUID);
      volume->identifier = g_strdup (volume->device_path + 5);
    }
  else if (g_path_is_absolute (volume->device_path))
    {
      volume->identifier_type = g_strdup (G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
      volume->identifier = g_strdup (volume->device_path);
    }
  
  return volume;
}

void
_g_unix_volume_disconnected (GUnixVolume *volume)
{
  if (volume->mount)
    {
      _g_unix_mount_unset_volume (volume->mount, volume);
      volume->mount = NULL;
    }
}

void
_g_unix_volume_set_mount (GUnixVolume *volume,
                          GUnixMount  *mount)
{
  if (volume->mount == mount)
    return;
  
  if (volume->mount)
    _g_unix_mount_unset_volume (volume->mount, volume);
  
  volume->mount = mount;
  
  /* TODO: Emit changed in idle to avoid locking issues */
  g_signal_emit_by_name (volume, "changed");
  if (volume->volume_monitor != NULL)
    g_signal_emit_by_name (volume->volume_monitor, "volume-changed", volume);
}

void
_g_unix_volume_unset_mount (GUnixVolume  *volume,
                            GUnixMount *mount)
{
  if (volume->mount == mount)
    {
      volume->mount = NULL;
      /* TODO: Emit changed in idle to avoid locking issues */
      g_signal_emit_by_name (volume, "changed");
      if (volume->volume_monitor != NULL)
        g_signal_emit_by_name (volume->volume_monitor, "volume-changed", volume);
    }
}

static GIcon *
g_unix_volume_get_icon (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  return g_object_ref (unix_volume->icon);
}

static GIcon *
g_unix_volume_get_symbolic_icon (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  return g_object_ref (unix_volume->symbolic_icon);
}

static char *
g_unix_volume_get_name (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  return g_strdup (unix_volume->name);
}

static char *
g_unix_volume_get_uuid (GVolume *volume)
{
  return NULL;
}

static gboolean
g_unix_volume_can_mount (GVolume *volume)
{
  return TRUE;
}

static gboolean
g_unix_volume_can_eject (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  return unix_volume->can_eject;
}

static gboolean
g_unix_volume_should_automount (GVolume *volume)
{
  /* We automount all local volumes because we don't even
   * make the internal stuff visible
   */
  return TRUE;
}

static GDrive *
g_unix_volume_get_drive (GVolume *volume)
{
  return NULL;
}

static GMount *
g_unix_volume_get_mount (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);

  if (unix_volume->mount != NULL)
    return g_object_ref (G_MOUNT (unix_volume->mount));

  return NULL;
}


gboolean
_g_unix_volume_has_mount_path (GUnixVolume *volume,
                               const char  *mount_path)
{
  return strcmp (volume->mount_path, mount_path) == 0;
}

static void
eject_mount_done (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GSubprocess *subprocess = G_SUBPROCESS (source);
  GTask *task = user_data;
  GError *error = NULL;
  gchar *stderr_str;
  GUnixVolume *unix_volume;

  if (!g_subprocess_communicate_utf8_finish (subprocess, result, NULL, &stderr_str, &error))
    {
      g_task_return_error (task, error);
      g_error_free (error);
    }
  else /* successful communication */
    {
      if (!g_subprocess_get_successful (subprocess))
        /* ...but bad exit code */
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", stderr_str);
      else
        {
          /* ...and successful exit code */
          unix_volume = G_UNIX_VOLUME (g_task_get_source_object (task));
          _g_unix_volume_monitor_update (G_UNIX_VOLUME_MONITOR (unix_volume->volume_monitor));
          g_task_return_boolean (task, TRUE);
        }

      g_free (stderr_str);
    }

  g_object_unref (task);
}

static void
eject_mount_do (GVolume              *volume,
                GCancellable         *cancellable,
                GAsyncReadyCallback   callback,
                gpointer              user_data,
                const gchar * const  *argv)
{
  GSubprocess *subprocess;
  GError *error = NULL;
  GTask *task;

  task = g_task_new (volume, cancellable, callback, user_data);
  g_task_set_source_tag (task, eject_mount_do);

  if (g_task_return_error_if_cancelled (task))
    {
      g_object_unref (task);
      return;
    }

  subprocess = g_subprocess_newv (argv, G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
  g_assert_no_error (error);

  g_subprocess_communicate_utf8_async (subprocess, NULL,
                                       g_task_get_cancellable (task),
                                       eject_mount_done, task);
}

static void
g_unix_volume_mount (GVolume            *volume,
                     GMountMountFlags    flags,
                     GMountOperation     *mount_operation,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  const gchar *argv[] = { "mount", NULL, NULL };

  if (unix_volume->mount_path != NULL)
    argv[1] = unix_volume->mount_path;
  else
    argv[1] = unix_volume->device_path;

  eject_mount_do (volume, cancellable, callback, user_data, argv);
}

static gboolean
g_unix_volume_mount_finish (GVolume        *volume,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, volume), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_unix_volume_eject (GVolume             *volume,
                     GMountUnmountFlags   flags,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  const gchar *argv[] = { "eject", NULL, NULL };

  argv[1] = unix_volume->device_path;

  eject_mount_do (volume, cancellable, callback, user_data, argv);
}

static gboolean
g_unix_volume_eject_finish (GVolume       *volume,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, volume), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gchar *
g_unix_volume_get_identifier (GVolume     *volume,
                              const gchar *kind)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);

  if (unix_volume->identifier_type != NULL &&
      strcmp (kind, unix_volume->identifier_type) == 0)
    return g_strdup (unix_volume->identifier);

  return NULL;
}

static gchar **
g_unix_volume_enumerate_identifiers (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  gchar **res;

  if (unix_volume->identifier_type)
    {
      res = g_new (gchar *, 2);
      res[0] = g_strdup (unix_volume->identifier_type);
      res[1] = NULL;
    }
  else
    {
      res = g_new (gchar *, 1);
      res[0] = NULL;
    }

  return res;
}

static void
g_unix_volume_volume_iface_init (GVolumeIface *iface)
{
  iface->get_name = g_unix_volume_get_name;
  iface->get_icon = g_unix_volume_get_icon;
  iface->get_symbolic_icon = g_unix_volume_get_symbolic_icon;
  iface->get_uuid = g_unix_volume_get_uuid;
  iface->get_drive = g_unix_volume_get_drive;
  iface->get_mount = g_unix_volume_get_mount;
  iface->can_mount = g_unix_volume_can_mount;
  iface->can_eject = g_unix_volume_can_eject;
  iface->should_automount = g_unix_volume_should_automount;
  iface->mount_fn = g_unix_volume_mount;
  iface->mount_finish = g_unix_volume_mount_finish;
  iface->eject = g_unix_volume_eject;
  iface->eject_finish = g_unix_volume_eject_finish;
  iface->get_identifier = g_unix_volume_get_identifier;
  iface->enumerate_identifiers = g_unix_volume_enumerate_identifiers;
}
