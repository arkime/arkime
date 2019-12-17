/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *               2009 Benjamin Otte
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
 * Author: Benjamin Otte <otte@gnome.org>
 */

#ifndef __G_FILE_INFO_PRIV_H__
#define __G_FILE_INFO_PRIV_H__

#include "gfileinfo.h"

#define G_FILE_ATTRIBUTE_ID_STANDARD_TYPE (1048576 + 1)
#define G_FILE_ATTRIBUTE_ID_STANDARD_IS_HIDDEN (1048576 + 2)
#define G_FILE_ATTRIBUTE_ID_STANDARD_IS_BACKUP (1048576 + 3)
#define G_FILE_ATTRIBUTE_ID_STANDARD_IS_SYMLINK (1048576 + 4)
#define G_FILE_ATTRIBUTE_ID_STANDARD_IS_VIRTUAL (1048576 + 5)
#define G_FILE_ATTRIBUTE_ID_STANDARD_NAME (1048576 + 6)
#define G_FILE_ATTRIBUTE_ID_STANDARD_DISPLAY_NAME (1048576 + 7)
#define G_FILE_ATTRIBUTE_ID_STANDARD_EDIT_NAME (1048576 + 8)
#define G_FILE_ATTRIBUTE_ID_STANDARD_COPY_NAME (1048576 + 9)
#define G_FILE_ATTRIBUTE_ID_STANDARD_DESCRIPTION (1048576 + 10)
#define G_FILE_ATTRIBUTE_ID_STANDARD_ICON (1048576 + 11)
#define G_FILE_ATTRIBUTE_ID_STANDARD_CONTENT_TYPE (1048576 + 12)
#define G_FILE_ATTRIBUTE_ID_STANDARD_FAST_CONTENT_TYPE (1048576 + 13)
#define G_FILE_ATTRIBUTE_ID_STANDARD_SIZE (1048576 + 14)
#define G_FILE_ATTRIBUTE_ID_STANDARD_ALLOCATED_SIZE (1048576 + 15)
#define G_FILE_ATTRIBUTE_ID_STANDARD_SYMLINK_TARGET (1048576 + 16)
#define G_FILE_ATTRIBUTE_ID_STANDARD_TARGET_URI (1048576 + 17)
#define G_FILE_ATTRIBUTE_ID_STANDARD_SORT_ORDER (1048576 + 18)
#define G_FILE_ATTRIBUTE_ID_STANDARD_SYMBOLIC_ICON (1048576 + 19)
#define G_FILE_ATTRIBUTE_ID_STANDARD_IS_VOLATILE (1048576 + 20)
#define G_FILE_ATTRIBUTE_ID_ETAG_VALUE (2097152 + 1)
#define G_FILE_ATTRIBUTE_ID_ID_FILE (3145728 + 1)
#define G_FILE_ATTRIBUTE_ID_ID_FILESYSTEM (3145728 + 2)
#define G_FILE_ATTRIBUTE_ID_ACCESS_CAN_READ (4194304 + 1)
#define G_FILE_ATTRIBUTE_ID_ACCESS_CAN_WRITE (4194304 + 2)
#define G_FILE_ATTRIBUTE_ID_ACCESS_CAN_EXECUTE (4194304 + 3)
#define G_FILE_ATTRIBUTE_ID_ACCESS_CAN_DELETE (4194304 + 4)
#define G_FILE_ATTRIBUTE_ID_ACCESS_CAN_TRASH (4194304 + 5)
#define G_FILE_ATTRIBUTE_ID_ACCESS_CAN_RENAME (4194304 + 6)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_CAN_MOUNT (5242880 + 1)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_CAN_UNMOUNT (5242880 + 2)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_CAN_EJECT (5242880 + 3)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_UNIX_DEVICE (5242880 + 4)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_UNIX_DEVICE_FILE (5242880 + 5)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_HAL_UDI (5242880 + 6)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_CAN_START (5242880 + 7)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_CAN_START_DEGRADED (5242880 + 8)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_CAN_STOP (5242880 + 9)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_START_STOP_TYPE (5242880 + 10)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_CAN_POLL (5242880 + 11)
#define G_FILE_ATTRIBUTE_ID_MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC (5242880 + 12)
#define G_FILE_ATTRIBUTE_ID_TIME_MODIFIED (6291456 + 1)
#define G_FILE_ATTRIBUTE_ID_TIME_MODIFIED_USEC (6291456 + 2)
#define G_FILE_ATTRIBUTE_ID_TIME_ACCESS (6291456 + 3)
#define G_FILE_ATTRIBUTE_ID_TIME_ACCESS_USEC (6291456 + 4)
#define G_FILE_ATTRIBUTE_ID_TIME_CHANGED (6291456 + 5)
#define G_FILE_ATTRIBUTE_ID_TIME_CHANGED_USEC (6291456 + 6)
#define G_FILE_ATTRIBUTE_ID_TIME_CREATED (6291456 + 7)
#define G_FILE_ATTRIBUTE_ID_TIME_CREATED_USEC (6291456 + 8)
#define G_FILE_ATTRIBUTE_ID_UNIX_DEVICE (7340032 + 1)
#define G_FILE_ATTRIBUTE_ID_UNIX_INODE (7340032 + 2)
#define G_FILE_ATTRIBUTE_ID_UNIX_MODE (7340032 + 3)
#define G_FILE_ATTRIBUTE_ID_UNIX_NLINK (7340032 + 4)
#define G_FILE_ATTRIBUTE_ID_UNIX_UID (7340032 + 5)
#define G_FILE_ATTRIBUTE_ID_UNIX_GID (7340032 + 6)
#define G_FILE_ATTRIBUTE_ID_UNIX_RDEV (7340032 + 7)
#define G_FILE_ATTRIBUTE_ID_UNIX_BLOCK_SIZE (7340032 + 8)
#define G_FILE_ATTRIBUTE_ID_UNIX_BLOCKS (7340032 + 9)
#define G_FILE_ATTRIBUTE_ID_UNIX_IS_MOUNTPOINT (7340032 + 10)
#define G_FILE_ATTRIBUTE_ID_DOS_IS_ARCHIVE (8388608 + 1)
#define G_FILE_ATTRIBUTE_ID_DOS_IS_SYSTEM (8388608 + 2)
#define G_FILE_ATTRIBUTE_ID_OWNER_USER (9437184 + 1)
#define G_FILE_ATTRIBUTE_ID_OWNER_USER_REAL (9437184 + 2)
#define G_FILE_ATTRIBUTE_ID_OWNER_GROUP (9437184 + 3)
#define G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH (10485760 + 1)
#define G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED (10485760 + 2)
#define G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID (10485760 + 3)
#define G_FILE_ATTRIBUTE_ID_PREVIEW_ICON (11534336 + 1)
#define G_FILE_ATTRIBUTE_ID_FILESYSTEM_SIZE (12582912 + 1)
#define G_FILE_ATTRIBUTE_ID_FILESYSTEM_FREE (12582912 + 2)
#define G_FILE_ATTRIBUTE_ID_FILESYSTEM_TYPE (12582912 + 3)
#define G_FILE_ATTRIBUTE_ID_FILESYSTEM_READONLY (12582912 + 4)
#define G_FILE_ATTRIBUTE_ID_FILESYSTEM_USE_PREVIEW (12582912 + 5)
#define G_FILE_ATTRIBUTE_ID_GVFS_BACKEND (13631488 + 1)
#define G_FILE_ATTRIBUTE_ID_SELINUX_CONTEXT (14680064 + 1)
#define G_FILE_ATTRIBUTE_ID_TRASH_ITEM_COUNT (15728640 + 1)
#define G_FILE_ATTRIBUTE_ID_TRASH_ORIG_PATH (15728640 + 2)
#define G_FILE_ATTRIBUTE_ID_TRASH_DELETION_DATE (15728640 + 3)

gboolean           _g_file_attribute_matcher_matches_id         (GFileAttributeMatcher *matcher,
                                                                 guint32                id);

void               _g_file_info_set_attribute_by_id             (GFileInfo             *info,
                                                                 guint32                attribute,
                                                                 GFileAttributeType     type,
                                                                 gpointer               value_p);
void               _g_file_info_set_attribute_string_by_id      (GFileInfo             *info,
                                                                 guint32                attribute,
							         const char            *attr_value);
void               _g_file_info_set_attribute_byte_string_by_id (GFileInfo             *info,
                                                                 guint32                attribute,
							         const char            *attr_value);
void               _g_file_info_set_attribute_boolean_by_id     (GFileInfo             *info,
                                                                 guint32                attribute,
							         gboolean               attr_value);
void               _g_file_info_set_attribute_uint32_by_id      (GFileInfo             *info,
                                                                 guint32                attribute,
							         guint32                attr_value);
void               _g_file_info_set_attribute_int32_by_id       (GFileInfo             *info,
                                                                 guint32                attribute,
							         gint32                 attr_value);
void               _g_file_info_set_attribute_uint64_by_id      (GFileInfo             *info,
                                                                 guint32                attribute,
							         guint64                attr_value);
void               _g_file_info_set_attribute_int64_by_id       (GFileInfo             *info,
                                                                 guint32                attribute,
							         gint64                 attr_value);
void               _g_file_info_set_attribute_object_by_id      (GFileInfo             *info,
                                                                 guint32                attribute,
							         GObject               *attr_value);
void               _g_file_info_set_attribute_stringv_by_id     (GFileInfo             *info,
                                                                 guint32                attribute,
							         char                 **attr_value);


#endif /* __G_FILE_INFO_PRIV_H__ */
