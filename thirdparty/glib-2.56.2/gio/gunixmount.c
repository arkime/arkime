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
#include "gsubprocess.h"
#include "gioenums.h"
#include "gunixvolumemonitor.h"
#include "gunixmount.h"
#include "gunixmounts.h"
#include "gunixvolume.h"
#include "gmountprivate.h"
#include "gmount.h"
#include "gfile.h"
#include "gvolumemonitor.h"
#include "gthemedicon.h"
#include "gioerror.h"
#include "glibintl.h"
/* for BUFSIZ */
#include <stdio.h>


struct _GUnixMount {
  GObject parent;

  GVolumeMonitor   *volume_monitor;

  GUnixVolume      *volume; /* owned by volume monitor */

  char *name;
  GIcon *icon;
  GIcon *symbolic_icon;
  char *device_path;
  char *mount_path;

  gboolean can_eject;
};

static void g_unix_mount_mount_iface_init (GMountIface *iface);

#define g_unix_mount_get_type _g_unix_mount_get_type
G_DEFINE_TYPE_WITH_CODE (GUnixMount, g_unix_mount, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_MOUNT,
						g_unix_mount_mount_iface_init))


static void
g_unix_mount_finalize (GObject *object)
{
  GUnixMount *mount;
  
  mount = G_UNIX_MOUNT (object);

  if (mount->volume_monitor != NULL)
    g_object_unref (mount->volume_monitor);

  if (mount->volume)
    _g_unix_volume_unset_mount (mount->volume, mount);
    
  /* TODO: g_warn_if_fail (volume->volume == NULL); */
  g_object_unref (mount->icon);
  g_object_unref (mount->symbolic_icon);
  g_free (mount->name);
  g_free (mount->device_path);
  g_free (mount->mount_path);

  G_OBJECT_CLASS (g_unix_mount_parent_class)->finalize (object);
}

static void
g_unix_mount_class_init (GUnixMountClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_unix_mount_finalize;
}

static void
g_unix_mount_init (GUnixMount *unix_mount)
{
}

GUnixMount *
_g_unix_mount_new (GVolumeMonitor  *volume_monitor,
                   GUnixMountEntry *mount_entry,
                   GUnixVolume     *volume)
{
  GUnixMount *mount;
  
  /* No volume for mount: Ignore internal things */
  if (volume == NULL && !g_unix_mount_guess_should_display (mount_entry))
    return NULL;

  mount = g_object_new (G_TYPE_UNIX_MOUNT, NULL);
  mount->volume_monitor = volume_monitor != NULL ? g_object_ref (volume_monitor) : NULL;
  mount->device_path = g_strdup (g_unix_mount_get_device_path (mount_entry));
  mount->mount_path = g_strdup (g_unix_mount_get_mount_path (mount_entry));
  mount->can_eject = g_unix_mount_guess_can_eject (mount_entry);

  mount->name = g_unix_mount_guess_name (mount_entry);
  mount->icon = g_unix_mount_guess_icon (mount_entry);
  mount->symbolic_icon = g_unix_mount_guess_symbolic_icon (mount_entry);

  /* need to do this last */
  mount->volume = volume;
  if (volume != NULL)
    _g_unix_volume_set_mount (volume, mount);

  return mount;
}

void
_g_unix_mount_unmounted (GUnixMount *mount)
{
  if (mount->volume != NULL)
    {
      _g_unix_volume_unset_mount (mount->volume, mount);
      mount->volume = NULL;
      g_signal_emit_by_name (mount, "changed");
      /* there's really no need to emit mount_changed on the volume monitor 
       * as we're going to be deleted.. */
    }
}

void
_g_unix_mount_unset_volume (GUnixMount *mount,
                            GUnixVolume  *volume)
{
  if (mount->volume == volume)
    {
      mount->volume = NULL;
      /* TODO: Emit changed in idle to avoid locking issues */
      g_signal_emit_by_name (mount, "changed");
      if (mount->volume_monitor != NULL)
        g_signal_emit_by_name (mount->volume_monitor, "mount-changed", mount);
    }
}

static GFile *
g_unix_mount_get_root (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  return g_file_new_for_path (unix_mount->mount_path);
}

static GIcon *
g_unix_mount_get_icon (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  return g_object_ref (unix_mount->icon);
}

static GIcon *
g_unix_mount_get_symbolic_icon (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  return g_object_ref (unix_mount->symbolic_icon);
}

static char *
g_unix_mount_get_uuid (GMount *mount)
{
  return NULL;
}

static char *
g_unix_mount_get_name (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  
  return g_strdup (unix_mount->name);
}

gboolean
_g_unix_mount_has_mount_path (GUnixMount *mount,
                              const char  *mount_path)
{
  return strcmp (mount->mount_path, mount_path) == 0;
}

static GDrive *
g_unix_mount_get_drive (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  if (unix_mount->volume != NULL)
    return g_volume_get_drive (G_VOLUME (unix_mount->volume));

  return NULL;
}

static GVolume *
g_unix_mount_get_volume (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);

  if (unix_mount->volume)
    return G_VOLUME (g_object_ref (unix_mount->volume));
  
  return NULL;
}

static gboolean
g_unix_mount_can_unmount (GMount *mount)
{
  return TRUE;
}

static gboolean
g_unix_mount_can_eject (GMount *mount)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  return unix_mount->can_eject;
}

static void
eject_unmount_done (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GSubprocess *subprocess = G_SUBPROCESS (source);
  GTask *task = user_data;
  GError *error = NULL;
  gchar *stderr_str;

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
        /* ...and successful exit code */
        g_task_return_boolean (task, TRUE);

      g_free (stderr_str);
    }

  g_object_unref (task);
}

static gboolean
eject_unmount_do_cb (gpointer user_data)
{
  GTask *task = user_data;
  GError *error = NULL;
  GSubprocess *subprocess;
  const gchar **argv;

  argv = g_task_get_task_data (task);

  if (g_task_return_error_if_cancelled (task))
    {
      g_object_unref (task);
      return G_SOURCE_REMOVE;
    }

  subprocess = g_subprocess_newv (argv, G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
  g_assert_no_error (error);

  g_subprocess_communicate_utf8_async (subprocess, NULL,
                                       g_task_get_cancellable (task),
                                       eject_unmount_done, task);

  return G_SOURCE_REMOVE;
}

static void
eject_unmount_do (GMount              *mount,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data,
                  char               **argv)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  GTask *task;
  GSource *timeout;

  task = g_task_new (mount, cancellable, callback, user_data);
  g_task_set_source_tag (task, eject_unmount_do);
  g_task_set_task_data (task, g_strdupv (argv), (GDestroyNotify) g_strfreev);

  if (unix_mount->volume_monitor != NULL)
    g_signal_emit_by_name (unix_mount->volume_monitor, "mount-pre-unmount", mount);

  g_signal_emit_by_name (mount, "pre-unmount", 0);

  timeout = g_timeout_source_new (500);
  g_task_attach_source (task, timeout, (GSourceFunc) eject_unmount_do_cb);
  g_source_unref (timeout);
}

static void
g_unix_mount_unmount (GMount             *mount,
                      GMountUnmountFlags flags,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  char *argv[] = {"umount", NULL, NULL};

  if (unix_mount->mount_path != NULL)
    argv[1] = unix_mount->mount_path;
  else
    argv[1] = unix_mount->device_path;

  eject_unmount_do (mount, cancellable, callback, user_data, argv);
}

static gboolean
g_unix_mount_unmount_finish (GMount       *mount,
                             GAsyncResult  *result,
                             GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_unix_mount_eject (GMount             *mount,
                    GMountUnmountFlags flags,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  GUnixMount *unix_mount = G_UNIX_MOUNT (mount);
  char *argv[] = {"eject", NULL, NULL};

  if (unix_mount->mount_path != NULL)
    argv[1] = unix_mount->mount_path;
  else
    argv[1] = unix_mount->device_path;

  eject_unmount_do (mount, cancellable, callback, user_data, argv);
}

static gboolean
g_unix_mount_eject_finish (GMount       *mount,
                           GAsyncResult  *result,
                           GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_unix_mount_mount_iface_init (GMountIface *iface)
{
  iface->get_root = g_unix_mount_get_root;
  iface->get_name = g_unix_mount_get_name;
  iface->get_icon = g_unix_mount_get_icon;
  iface->get_symbolic_icon = g_unix_mount_get_symbolic_icon;
  iface->get_uuid = g_unix_mount_get_uuid;
  iface->get_drive = g_unix_mount_get_drive;
  iface->get_volume = g_unix_mount_get_volume;
  iface->can_unmount = g_unix_mount_can_unmount;
  iface->can_eject = g_unix_mount_can_eject;
  iface->unmount = g_unix_mount_unmount;
  iface->unmount_finish = g_unix_mount_unmount_finish;
  iface->eject = g_unix_mount_eject;
  iface->eject_finish = g_unix_mount_eject_finish;
}
