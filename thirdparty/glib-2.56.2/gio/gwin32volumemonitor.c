/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2008 Hans Breuer
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
 *         Hans Breuer <hans@breuer.org>
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include "glibintl.h"

#include "gwin32volumemonitor.h"
#include "gwin32mount.h"
#include "gmount.h"
#include "giomodule.h"

#include <windows.h>

struct _GWin32VolumeMonitor {
  GNativeVolumeMonitor parent;
};

#define g_win32_volume_monitor_get_type _g_win32_volume_monitor_get_type
G_DEFINE_TYPE_WITH_CODE (GWin32VolumeMonitor, g_win32_volume_monitor, G_TYPE_NATIVE_VOLUME_MONITOR,
                         g_io_extension_point_implement (G_NATIVE_VOLUME_MONITOR_EXTENSION_POINT_NAME,
							 g_define_type_id,
							 "win32",
							 0));
							 
/**
 * get_viewable_logical_drives:
 *
 * Returns the list of logical and viewable drives as defined by
 * GetLogicalDrives() and the registry keys
 * Software\Microsoft\Windows\CurrentVersion\Policies\Explorer under
 * HKLM or HKCU. If neither key exists the result of
 * GetLogicalDrives() is returned.
 *
 * Returns: bitmask with same meaning as returned by GetLogicalDrives()
 */
static guint32 
get_viewable_logical_drives (void)
{
  guint viewable_drives = GetLogicalDrives ();
  HKEY key;

  DWORD var_type = REG_DWORD; //the value's a REG_DWORD type
  DWORD no_drives_size = 4;
  DWORD no_drives;
  gboolean hklm_present = FALSE;

  if (RegOpenKeyEx (HKEY_LOCAL_MACHINE,
		    "Software\\Microsoft\\Windows\\"
		    "CurrentVersion\\Policies\\Explorer",
		    0, KEY_READ, &key) == ERROR_SUCCESS)
    {
      if (RegQueryValueEx (key, "NoDrives", NULL, &var_type,
			   (LPBYTE) &no_drives, &no_drives_size) == ERROR_SUCCESS)
	{
	  /* We need the bits that are set in viewable_drives, and
	   * unset in no_drives.
	   */
	  viewable_drives = viewable_drives & ~no_drives;
	  hklm_present = TRUE;
	}
      RegCloseKey (key);
    }

  /* If the key is present in HKLM then the one in HKCU should be ignored */
  if (!hklm_present)
    {
      if (RegOpenKeyEx (HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\"
			"CurrentVersion\\Policies\\Explorer",
			0, KEY_READ, &key) == ERROR_SUCCESS)
	{
	  if (RegQueryValueEx (key, "NoDrives", NULL, &var_type,
			       (LPBYTE) &no_drives, &no_drives_size) == ERROR_SUCCESS)
	    {
	      viewable_drives = viewable_drives & ~no_drives;
	    }
	  RegCloseKey (key);
	}
    }

  return viewable_drives; 
}

/* deliver accesible (aka 'mounted') volumes */
static GList *
get_mounts (GVolumeMonitor *volume_monitor)
{
  DWORD   drives;
  gchar   drive[4] = "A:\\";
  GList *list = NULL;
  
  drives = get_viewable_logical_drives ();

  if (!drives)
    g_warning ("get_viewable_logical_drives failed.");

  while (drives && drive[0] <= 'Z')
    {
      if (drives & 1)
      {
	list = g_list_prepend (list, _g_win32_mount_new (volume_monitor, drive, NULL));
      }
      drives >>= 1;
      drive[0]++;
    }
  return list;
}

/* actually 'mounting' volumes is out of GIOs business on win32, so no volumes are delivered either */
static GList *
get_volumes (GVolumeMonitor *volume_monitor)
{
  return NULL;
}

/* real hardware */
static GList *
get_connected_drives (GVolumeMonitor *volume_monitor)
{
  GList *list = NULL;

#if 0
  HANDLE  find_handle;
  BOOL    found;
  wchar_t wc_name[MAX_PATH+1];
  
  find_handle = FindFirstVolumeW (wc_name, MAX_PATH);
  found = (find_handle != INVALID_HANDLE_VALUE);
  while (found)
    {
      /* I don't know what this code is supposed to do; clearly it now
       * does nothing, the returned GList is always NULL. But what was
       * this code supposed to be a start of? The volume names that
       * the FindFirstVolume/FindNextVolume loop iterates over returns
       * device names like
       *
       *   \Device\HarddiskVolume1
       *   \Device\HarddiskVolume2
       *   \Device\CdRom0
       *
       * No DOS devices there, so I don't see the point with the
       * QueryDosDevice call below. Probably this code is confusing volumes
       * with something else that does contain the mapping from DOS devices
       * to volumes.
       */
      wchar_t wc_dev_name[MAX_PATH+1];
      guint trailing = wcslen (wc_name) - 1;

      /* remove trailing backslash and leading \\?\\ */
      wc_name[trailing] = L'\0';
      if (QueryDosDeviceW (&wc_name[4], wc_dev_name, MAX_PATH))
        {
          gchar *name = g_utf16_to_utf8 (wc_dev_name, -1, NULL, NULL, NULL);
          g_print ("%s\n", name);
	  g_free (name);
	}

      found = FindNextVolumeW (find_handle, wc_name, MAX_PATH);
    }
  if (find_handle != INVALID_HANDLE_VALUE)
    FindVolumeClose (find_handle);
#endif

  return list;
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
  GWin32Mount *mount;

  /* TODO: Set mountable volume? */
  mount = _g_win32_mount_new (NULL, mount_path, NULL);

  return G_MOUNT (mount);
}

static void
g_win32_volume_monitor_class_init (GWin32VolumeMonitorClass *klass)
{
  GVolumeMonitorClass *monitor_class = G_VOLUME_MONITOR_CLASS (klass);
  GNativeVolumeMonitorClass *native_class = G_NATIVE_VOLUME_MONITOR_CLASS (klass);
  
  monitor_class->get_mounts = get_mounts;
  monitor_class->get_volumes = get_volumes;
  monitor_class->get_connected_drives = get_connected_drives;
  monitor_class->get_volume_for_uuid = get_volume_for_uuid;
  monitor_class->get_mount_for_uuid = get_mount_for_uuid;
  monitor_class->is_supported = is_supported;

  native_class->get_mount_for_mount_path = get_mount_for_mount_path;
}

static void
g_win32_volume_monitor_init (GWin32VolumeMonitor *win32_monitor)
{
  /* maybe we shoud setup a callback window to listern for WM_DEVICECHANGE ? */
#if 0
  unix_monitor->mount_monitor = g_win32_mount_monitor_new ();

  g_signal_connect (win32_monitor->mount_monitor,
		    "mounts-changed", G_CALLBACK (mounts_changed),
		    win32_monitor);
  
  g_signal_connect (win32_monitor->mount_monitor,
		    "mountpoints-changed", G_CALLBACK (mountpoints_changed),
		    win32_monitor);
		    
  update_volumes (win32_monitor);
  update_mounts (win32_monitor);
#endif
}
