/*
 * Copyright 2015 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GIO_TOOL_H__
#define __GIO_TOOL_H__

void print_error      (const gchar    *format,
                       ...) G_GNUC_PRINTF (1, 2);
void print_file_error (GFile          *file,
                       const gchar    *message);
void show_help        (GOptionContext *context,
                       const char     *message);

const char         *file_type_to_string        (GFileType                type);
const char         *attribute_type_to_string   (GFileAttributeType       type);
GFileAttributeType  attribute_type_from_string (const char              *str);
char               *attribute_flags_to_string  (GFileAttributeInfoFlags  flags);

gboolean file_is_dir (GFile *file);

int handle_cat     (int argc, char *argv[], gboolean do_help);
int handle_copy    (int argc, char *argv[], gboolean do_help);
int handle_info    (int argc, char *argv[], gboolean do_help);
int handle_list    (int argc, char *argv[], gboolean do_help);
int handle_mime    (int argc, char *argv[], gboolean do_help);
int handle_mkdir   (int argc, char *argv[], gboolean do_help);
int handle_monitor (int argc, char *argv[], gboolean do_help);
int handle_mount   (int argc, char *argv[], gboolean do_help);
int handle_move    (int argc, char *argv[], gboolean do_help);
int handle_open    (int argc, char *argv[], gboolean do_help);
int handle_rename  (int argc, char *argv[], gboolean do_help);
int handle_remove  (int argc, char *argv[], gboolean do_help);
int handle_save    (int argc, char *argv[], gboolean do_help);
int handle_set     (int argc, char *argv[], gboolean do_help);
int handle_trash   (int argc, char *argv[], gboolean do_help);
int handle_tree    (int argc, char *argv[], gboolean do_help);

#endif  /* __GIO_TOOL_H__ */
