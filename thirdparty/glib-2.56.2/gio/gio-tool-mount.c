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

#include "config.h"

#include <gio/gio.h>
#include <gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "gio-tool.h"

#define STDIN_FILENO 0

typedef enum {
  MOUNT_OP_NONE,
  MOUNT_OP_ASKED,
  MOUNT_OP_ABORTED
} MountOpState;

static int outstanding_mounts = 0;
static GMainLoop *main_loop;
static GVolumeMonitor *volume_monitor;

static gboolean mount_mountable = FALSE;
static gboolean mount_unmount = FALSE;
static gboolean mount_eject = FALSE;
static gboolean force = FALSE;
static gboolean anonymous = FALSE;
static gboolean mount_list = FALSE;
static gboolean extra_detail = FALSE;
static gboolean mount_monitor = FALSE;
static const char *unmount_scheme = NULL;
static const char *mount_device_file = NULL;
static gboolean success = TRUE;


static const GOptionEntry entries[] =
{
  { "mountable", 'm', 0, G_OPTION_ARG_NONE, &mount_mountable, N_("Mount as mountable"), NULL },
  { "device", 'd', 0, G_OPTION_ARG_STRING, &mount_device_file, N_("Mount volume with device file"), N_("DEVICE") },
  { "unmount", 'u', 0, G_OPTION_ARG_NONE, &mount_unmount, N_("Unmount"), NULL},
  { "eject", 'e', 0, G_OPTION_ARG_NONE, &mount_eject, N_("Eject"), NULL},
  { "unmount-scheme", 's', 0, G_OPTION_ARG_STRING, &unmount_scheme, N_("Unmount all mounts with the given scheme"), N_("SCHEME") },
  { "force", 'f', 0, G_OPTION_ARG_NONE, &force, N_("Ignore outstanding file operations when unmounting or ejecting"), NULL },
  { "anonymous", 'a', 0, G_OPTION_ARG_NONE, &anonymous, N_("Use an anonymous user when authenticating"), NULL },
  /* Translator: List here is a verb as in 'List all mounts' */
  { "list", 'l', 0, G_OPTION_ARG_NONE, &mount_list, N_("List"), NULL},
  { "monitor", 'o', 0, G_OPTION_ARG_NONE, &mount_monitor, N_("Monitor events"), NULL},
  { "detail", 'i', 0, G_OPTION_ARG_NONE, &extra_detail, N_("Show extra information"), NULL},
  { NULL }
};

static char *
prompt_for (const char *prompt, const char *default_value, gboolean echo)
{
#ifdef HAVE_TERMIOS_H
  struct termios term_attr;
  int old_flags;
  gboolean restore_flags;
#endif
  char data[256];
  int len;

  if (default_value && *default_value != 0)
    g_print ("%s [%s]: ", prompt, default_value);
  else
    g_print ("%s: ", prompt);

  data[0] = 0;

#ifdef HAVE_TERMIOS_H
  restore_flags = FALSE;
  if (!echo && tcgetattr (STDIN_FILENO, &term_attr) == 0)
    {
      old_flags = term_attr.c_lflag;
      term_attr.c_lflag &= ~ECHO;
      restore_flags = TRUE;

      if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &term_attr) != 0)
        g_print ("Warning! Password will be echoed");
    }

#endif

  fgets(data, sizeof (data), stdin);

#ifdef HAVE_TERMIOS_H
  if (restore_flags)
    {
      term_attr.c_lflag = old_flags;
      tcsetattr (STDIN_FILENO, TCSAFLUSH, &term_attr);
    }
#endif

  len = strlen (data);
  if (len == 0)
    {
      g_print ("\n");
      return NULL;
    }
  if (data[len-1] == '\n')
    data[len-1] = 0;

  if (!echo)
    g_print ("\n");

  if (*data == 0 && default_value)
    return g_strdup (default_value);
  return g_strdup (data);
}

static void
ask_password_cb (GMountOperation *op,
                 const char      *message,
                 const char      *default_user,
                 const char      *default_domain,
                 GAskPasswordFlags flags)
{
  if ((flags & G_ASK_PASSWORD_ANONYMOUS_SUPPORTED) && anonymous)
    {
      g_mount_operation_set_anonymous (op, TRUE);
    }
  else
    {
      char *s;
      g_print ("%s\n", message);

      if (flags & G_ASK_PASSWORD_NEED_USERNAME)
        {
          s = prompt_for ("User", default_user, TRUE);
          if (!s)
            goto error;
          g_mount_operation_set_username (op, s);
          g_free (s);
        }

      if (flags & G_ASK_PASSWORD_NEED_DOMAIN)
        {
          s = prompt_for ("Domain", default_domain, TRUE);
          if (!s)
            goto error;
          g_mount_operation_set_domain (op, s);
          g_free (s);
        }

      if (flags & G_ASK_PASSWORD_NEED_PASSWORD)
        {
          s = prompt_for ("Password", NULL, FALSE);
          if (!s)
            goto error;
          g_mount_operation_set_password (op, s);
          g_free (s);
        }
    }

  /* Only try anonymous access once. */
  if (anonymous &&
      GPOINTER_TO_INT (g_object_get_data (G_OBJECT (op), "state")) == MOUNT_OP_ASKED)
    {
      g_object_set_data (G_OBJECT (op), "state", GINT_TO_POINTER (MOUNT_OP_ABORTED));
      g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
    }
  else
    {
      g_object_set_data (G_OBJECT (op), "state", GINT_TO_POINTER (MOUNT_OP_ASKED));
      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }

  return;

error:
  g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
}

static void
ask_question_cb (GMountOperation *op,
                 char *message,
                 char **choices,
                 gpointer user_data)
{
  char **ptr = choices;
  char *s;
  int i, choice;

  g_print ("%s\n", message);

  i = 1;
  while (*ptr)
    {
      g_print ("[%d] %s\n", i, *ptr++);
      i++;
    }

  s = prompt_for ("Choice", NULL, TRUE);
  if (!s)
    goto error;

  choice = atoi (s);
  if (choice > 0 && choice < i)
    {
      g_mount_operation_set_choice (op, choice - 1);
      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }
  g_free (s);

  return;

error:
  g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
}

static void
mount_mountable_done_cb (GObject *object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  GFile *target;
  GError *error = NULL;
  GMountOperation *op = user_data;

  target = g_file_mount_mountable_finish (G_FILE (object), res, &error);

  if (target == NULL)
    {
      success = FALSE;
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (op), "state")) == MOUNT_OP_ABORTED)
        print_file_error (G_FILE (object), _("Anonymous access denied"));
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        print_file_error (G_FILE (object), error->message);

      g_error_free (error);
    }
  else
    g_object_unref (target);

  outstanding_mounts--;

  if (outstanding_mounts == 0)
    g_main_loop_quit (main_loop);
}

static void
mount_done_cb (GObject *object,
               GAsyncResult *res,
               gpointer user_data)
{
  gboolean succeeded;
  GError *error = NULL;
  GMountOperation *op = user_data;

  succeeded = g_file_mount_enclosing_volume_finish (G_FILE (object), res, &error);

  if (!succeeded)
    {
      success = FALSE;
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (op), "state")) == MOUNT_OP_ABORTED)
        print_file_error (G_FILE (object), _("Anonymous access denied"));
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        print_file_error (G_FILE (object), error->message);

      g_error_free (error);
    }

  outstanding_mounts--;

  if (outstanding_mounts == 0)
    g_main_loop_quit (main_loop);
}

static GMountOperation *
new_mount_op (void)
{
  GMountOperation *op;

  op = g_mount_operation_new ();

  g_object_set_data (G_OBJECT (op), "state", GINT_TO_POINTER (MOUNT_OP_NONE));

  g_signal_connect (op, "ask_password", G_CALLBACK (ask_password_cb), NULL);
  g_signal_connect (op, "ask_question", G_CALLBACK (ask_question_cb), NULL);

  /* TODO: we *should* also connect to the "aborted" signal but since the
   * main thread is blocked handling input we won't get that signal anyway...
   */

  return op;
}


static void
mount (GFile *file)
{
  GMountOperation *op;

  if (file == NULL)
    return;

  op = new_mount_op ();

  if (mount_mountable)
    g_file_mount_mountable (file, 0, op, NULL, mount_mountable_done_cb, op);
  else
    g_file_mount_enclosing_volume (file, 0, op, NULL, mount_done_cb, op);

  outstanding_mounts++;
}

static void
unmount_done_cb (GObject *object,
                 GAsyncResult *res,
                 gpointer user_data)
{
  gboolean succeeded;
  GError *error = NULL;
  GFile *file = G_FILE (user_data);

  succeeded = g_mount_unmount_with_operation_finish (G_MOUNT (object), res, &error);

  g_object_unref (G_MOUNT (object));

  if (!succeeded)
    {
      print_file_error (file, error->message);
      success = FALSE;
      g_error_free (error);
    }

  g_object_unref (file);

  outstanding_mounts--;

  if (outstanding_mounts == 0)
    g_main_loop_quit (main_loop);
}

static void
unmount (GFile *file)
{
  GMount *mount;
  GError *error = NULL;
  GMountOperation *mount_op;
  GMountUnmountFlags flags;

  if (file == NULL)
    return;

  mount = g_file_find_enclosing_mount (file, NULL, &error);
  if (mount == NULL)
    {
      print_file_error (file, error->message);
      success = FALSE;
      g_error_free (error);
      return;
    }

  mount_op = new_mount_op ();
  flags = force ? G_MOUNT_UNMOUNT_FORCE : G_MOUNT_UNMOUNT_NONE;
  g_mount_unmount_with_operation (mount, flags, mount_op, NULL, unmount_done_cb, g_object_ref (file));
  g_object_unref (mount_op);

  outstanding_mounts++;
}

static void
eject_done_cb (GObject *object,
               GAsyncResult *res,
               gpointer user_data)
{
  gboolean succeeded;
  GError *error = NULL;
  GFile *file = G_FILE (user_data);

  succeeded = g_mount_eject_with_operation_finish (G_MOUNT (object), res, &error);

  g_object_unref (G_MOUNT (object));

  if (!succeeded)
    {
      print_file_error (file, error->message);
      success = FALSE;
      g_error_free (error);
    }

  g_object_unref (file);

  outstanding_mounts--;

  if (outstanding_mounts == 0)
    g_main_loop_quit (main_loop);
}

static void
eject (GFile *file)
{
  GMount *mount;
  GError *error = NULL;
  GMountOperation *mount_op;
  GMountUnmountFlags flags;

  if (file == NULL)
    return;

  mount = g_file_find_enclosing_mount (file, NULL, &error);
  if (mount == NULL)
    {
      print_file_error (file, error->message);
      success = FALSE;
      g_error_free (error);
      return;
    }

  mount_op = new_mount_op ();
  flags = force ? G_MOUNT_UNMOUNT_FORCE : G_MOUNT_UNMOUNT_NONE;
  g_mount_eject_with_operation (mount, flags, mount_op, NULL, eject_done_cb, g_object_ref (file));
  g_object_unref (mount_op);

  outstanding_mounts++;
}

static gboolean
iterate_gmain_timeout_function (gpointer data)
{
  g_main_loop_quit (main_loop);
  return FALSE;
}

static void
iterate_gmain(void)
{
  g_timeout_add (500, iterate_gmain_timeout_function, NULL);
  g_main_loop_run (main_loop);
}

static void
show_themed_icon_names (GThemedIcon *icon, gboolean symbolic, int indent)
{
  char **names;
  char **iter;

  g_print ("%*s%sthemed icons:", indent, " ", symbolic ? "symbolic " : "");

  names = NULL;

  g_object_get (icon, "names", &names, NULL);

  for (iter = names; *iter; iter++)
    g_print ("  [%s]", *iter);

  g_print ("\n");
  g_strfreev (names);
}

/* don't copy-paste this code */
static char *
get_type_name (gpointer object)
{
  const char *type_name;
  char *ret;

  type_name = g_type_name (G_TYPE_FROM_INSTANCE (object));
  if (strcmp ("GProxyDrive", type_name) == 0)
    {
      ret = g_strdup_printf ("%s (%s)",
                             type_name,
                             (const char *) g_object_get_data (G_OBJECT (object),
                                                               "g-proxy-drive-volume-monitor-name"));
    }
  else if (strcmp ("GProxyVolume", type_name) == 0)
    {
      ret = g_strdup_printf ("%s (%s)",
                             type_name,
                             (const char *) g_object_get_data (G_OBJECT (object),
                                                               "g-proxy-volume-volume-monitor-name"));
    }
  else if (strcmp ("GProxyMount", type_name) == 0)
    {
      ret = g_strdup_printf ("%s (%s)",
                             type_name,
                             (const char *) g_object_get_data (G_OBJECT (object),
                                                               "g-proxy-mount-volume-monitor-name"));
    }
  else if (strcmp ("GProxyShadowMount", type_name) == 0)
    {
      ret = g_strdup_printf ("%s (%s)",
                             type_name,
                             (const char *) g_object_get_data (G_OBJECT (object),
                                                               "g-proxy-shadow-mount-volume-monitor-name"));
    }
  else
    {
      ret = g_strdup (type_name);
    }

  return ret;
}

static void
list_mounts (GList *mounts,
             int indent,
             gboolean only_with_no_volume)
{
  GList *l;
  int c;
  GMount *mount;
  GVolume *volume;
  char *name, *uuid, *uri;
  GFile *root, *default_location;
  GIcon *icon;
  char **x_content_types;
  char *type_name;
  const gchar *sort_key;

  for (c = 0, l = mounts; l != NULL; l = l->next, c++)
    {
      mount = (GMount *) l->data;

      if (only_with_no_volume)
        {
          volume = g_mount_get_volume (mount);
          if (volume != NULL)
            {
              g_object_unref (volume);
              continue;
            }
        }

      name = g_mount_get_name (mount);
      root = g_mount_get_root (mount);
      uri = g_file_get_uri (root);

      g_print ("%*sMount(%d): %s -> %s\n", indent, "", c, name, uri);

      type_name = get_type_name (mount);
      g_print ("%*sType: %s\n", indent+2, "", type_name);
      g_free (type_name);

      if (extra_detail)
        {
          uuid = g_mount_get_uuid (mount);
          if (uuid)
            g_print ("%*suuid=%s\n", indent + 2, "", uuid);

          default_location = g_mount_get_default_location (mount);
          if (default_location)
            {
              char *loc_uri = g_file_get_uri (default_location);
              g_print ("%*sdefault_location=%s\n", indent + 2, "", loc_uri);
              g_free (loc_uri);
              g_object_unref (default_location);
            }

          icon = g_mount_get_icon (mount);
          if (icon)
            {
              if (G_IS_THEMED_ICON (icon))
                show_themed_icon_names (G_THEMED_ICON (icon), FALSE, indent + 2);

              g_object_unref (icon);
            }

          icon = g_mount_get_symbolic_icon (mount);
          if (icon)
            {
              if (G_IS_THEMED_ICON (icon))
                show_themed_icon_names (G_THEMED_ICON (icon), TRUE, indent + 2);

              g_object_unref (icon);
            }

          x_content_types = g_mount_guess_content_type_sync (mount, FALSE, NULL, NULL);
          if (x_content_types != NULL && g_strv_length (x_content_types) > 0)
            {
              int n;
              g_print ("%*sx_content_types:", indent + 2, "");
              for (n = 0; x_content_types[n] != NULL; n++)
                  g_print (" %s", x_content_types[n]);
              g_print ("\n");
            }
          g_strfreev (x_content_types);

          g_print ("%*scan_unmount=%d\n", indent + 2, "", g_mount_can_unmount (mount));
          g_print ("%*scan_eject=%d\n", indent + 2, "", g_mount_can_eject (mount));
          g_print ("%*sis_shadowed=%d\n", indent + 2, "", g_mount_is_shadowed (mount));
          sort_key = g_mount_get_sort_key (mount);
          if (sort_key != NULL)
            g_print ("%*ssort_key=%s\n", indent + 2, "", sort_key);
          g_free (uuid);
        }

      g_object_unref (root);
      g_free (name);
      g_free (uri);
    }
}

static void
list_volumes (GList *volumes,
              int indent,
              gboolean only_with_no_drive)
{
  GList *l, *mounts;
  int c, i;
  GMount *mount;
  GVolume *volume;
  GDrive *drive;
  char *name;
  char *uuid;
  GFile *activation_root;
  char **ids;
  GIcon *icon;
  char *type_name;
  const gchar *sort_key;

  for (c = 0, l = volumes; l != NULL; l = l->next, c++)
    {
      volume = (GVolume *) l->data;

      if (only_with_no_drive)
        {
          drive = g_volume_get_drive (volume);
          if (drive != NULL)
            {
              g_object_unref (drive);
              continue;
            }
        }

      name = g_volume_get_name (volume);

      g_print ("%*sVolume(%d): %s\n", indent, "", c, name);
      g_free (name);

      type_name = get_type_name (volume);
      g_print ("%*sType: %s\n", indent+2, "", type_name);
      g_free (type_name);

      if (extra_detail)
        {
          ids = g_volume_enumerate_identifiers (volume);
          if (ids && ids[0] != NULL)
            {
              g_print ("%*sids:\n", indent+2, "");
              for (i = 0; ids[i] != NULL; i++)
                {
                  char *id = g_volume_get_identifier (volume,
                                                      ids[i]);
                  g_print ("%*s %s: '%s'\n", indent+2, "", ids[i], id);
                  g_free (id);
                }
            }
          g_strfreev (ids);

          uuid = g_volume_get_uuid (volume);
          if (uuid)
            g_print ("%*suuid=%s\n", indent + 2, "", uuid);
          activation_root = g_volume_get_activation_root (volume);
          if (activation_root)
            {
              char *uri;
              uri = g_file_get_uri (activation_root);
              g_print ("%*sactivation_root=%s\n", indent + 2, "", uri);
              g_free (uri);
              g_object_unref (activation_root);
            }
          icon = g_volume_get_icon (volume);
          if (icon)
            {
              if (G_IS_THEMED_ICON (icon))
                show_themed_icon_names (G_THEMED_ICON (icon), FALSE, indent + 2);

              g_object_unref (icon);
            }

          icon = g_volume_get_symbolic_icon (volume);
          if (icon)
            {
              if (G_IS_THEMED_ICON (icon))
                show_themed_icon_names (G_THEMED_ICON (icon), TRUE, indent + 2);

              g_object_unref (icon);
            }

          g_print ("%*scan_mount=%d\n", indent + 2, "", g_volume_can_mount (volume));
          g_print ("%*scan_eject=%d\n", indent + 2, "", g_volume_can_eject (volume));
          g_print ("%*sshould_automount=%d\n", indent + 2, "", g_volume_should_automount (volume));
          sort_key = g_volume_get_sort_key (volume);
          if (sort_key != NULL)
            g_print ("%*ssort_key=%s\n", indent + 2, "", sort_key);
          g_free (uuid);
        }

      mount = g_volume_get_mount (volume);
      if (mount)
        {
          mounts = g_list_prepend (NULL, mount);
          list_mounts (mounts, indent + 2, FALSE);
          g_list_free (mounts);
          g_object_unref (mount);
        }
    }
}

static void
list_drives (GList *drives,
             int indent)
{
  GList *volumes, *l;
  int c, i;
  GDrive *drive;
  char *name;
  char **ids;
  GIcon *icon;
  char *type_name;
  const gchar *sort_key;

  for (c = 0, l = drives; l != NULL; l = l->next, c++)
    {
      drive = (GDrive *) l->data;
      name = g_drive_get_name (drive);

      g_print ("%*sDrive(%d): %s\n", indent, "", c, name);
      g_free (name);

      type_name = get_type_name (drive);
      g_print ("%*sType: %s\n", indent+2, "", type_name);
      g_free (type_name);

      if (extra_detail)
        {
          GEnumValue *enum_value;
          gpointer klass;

          ids = g_drive_enumerate_identifiers (drive);
          if (ids && ids[0] != NULL)
            {
              g_print ("%*sids:\n", indent+2, "");
              for (i = 0; ids[i] != NULL; i++)
                {
                  char *id = g_drive_get_identifier (drive,
                                                     ids[i]);
                  g_print ("%*s %s: '%s'\n", indent+2, "", ids[i], id);
                  g_free (id);
                }
            }
          g_strfreev (ids);

          icon = g_drive_get_icon (drive);
          if (icon)
          {
                  if (G_IS_THEMED_ICON (icon))
                          show_themed_icon_names (G_THEMED_ICON (icon), FALSE, indent + 2);
                  g_object_unref (icon);
          }

          icon = g_drive_get_symbolic_icon (drive);
          if (icon)
            {
              if (G_IS_THEMED_ICON (icon))
                show_themed_icon_names (G_THEMED_ICON (icon), TRUE, indent + 2);

              g_object_unref (icon);
            }

          g_print ("%*sis_removable=%d\n", indent + 2, "", g_drive_is_removable (drive));
          g_print ("%*sis_media_removable=%d\n", indent + 2, "", g_drive_is_media_removable (drive));
          g_print ("%*shas_media=%d\n", indent + 2, "", g_drive_has_media (drive));
          g_print ("%*sis_media_check_automatic=%d\n", indent + 2, "", g_drive_is_media_check_automatic (drive));
          g_print ("%*scan_poll_for_media=%d\n", indent + 2, "", g_drive_can_poll_for_media (drive));
          g_print ("%*scan_eject=%d\n", indent + 2, "", g_drive_can_eject (drive));
          g_print ("%*scan_start=%d\n", indent + 2, "", g_drive_can_start (drive));
          g_print ("%*scan_stop=%d\n", indent + 2, "", g_drive_can_stop (drive));

          enum_value = NULL;
          klass = g_type_class_ref (G_TYPE_DRIVE_START_STOP_TYPE);
          if (klass != NULL)
            {
              enum_value = g_enum_get_value (klass, g_drive_get_start_stop_type (drive));
              g_print ("%*sstart_stop_type=%s\n", indent + 2, "",
                       enum_value != NULL ? enum_value->value_nick : "UNKNOWN");
              g_type_class_unref (klass);
            }

          sort_key = g_drive_get_sort_key (drive);
          if (sort_key != NULL)
            g_print ("%*ssort_key=%s\n", indent + 2, "", sort_key);
        }
      volumes = g_drive_get_volumes (drive);
      list_volumes (volumes, indent + 2, FALSE);
      g_list_free_full (volumes, g_object_unref);
    }
}


static void
list_monitor_items (void)
{
  GList *drives, *volumes, *mounts;

  /* populate gvfs network mounts */
  iterate_gmain();

  drives = g_volume_monitor_get_connected_drives (volume_monitor);
  list_drives (drives, 0);
  g_list_free_full (drives, g_object_unref);

  volumes = g_volume_monitor_get_volumes (volume_monitor);
  list_volumes (volumes, 0, TRUE);
  g_list_free_full (volumes, g_object_unref);

  mounts = g_volume_monitor_get_mounts (volume_monitor);
  list_mounts (mounts, 0, TRUE);
  g_list_free_full (mounts, g_object_unref);
}

static void
unmount_all_with_scheme (const char *scheme)
{
  GList *mounts;
  GList *l;

  /* populate gvfs network mounts */
  iterate_gmain();

  mounts = g_volume_monitor_get_mounts (volume_monitor);
  for (l = mounts; l != NULL; l = l->next) {
    GMount *mount = G_MOUNT (l->data);
    GFile *root;

    root = g_mount_get_root (mount);
    if (g_file_has_uri_scheme (root, scheme)) {
            unmount (root);
    }
    g_object_unref (root);
  }
  g_list_free_full (mounts, g_object_unref);
}

static void
mount_with_device_file_cb (GObject *object,
                           GAsyncResult *res,
                           gpointer user_data)
{
  GVolume *volume;
  gboolean succeeded;
  GError *error = NULL;
  gchar *device_path = (gchar *)user_data;

  volume = G_VOLUME (object);

  succeeded = g_volume_mount_finish (volume, res, &error);

  if (!succeeded)
    {
      print_error ("%s: %s", device_path, error->message);
      g_error_free (error);
      success = FALSE;
    }
  else
    {
      GMount *mount;
      GFile *root;
      char *mount_path;

      mount = g_volume_get_mount (volume);
      root = g_mount_get_root (mount);
      mount_path = g_file_get_path (root);

      g_print (_("Mounted %s at %s\n"), device_path, mount_path);

      g_object_unref (mount);
      g_object_unref (root);
      g_free (mount_path);
    }

  g_free (device_path);

  outstanding_mounts--;

  if (outstanding_mounts == 0)
    g_main_loop_quit (main_loop);
}

static void
mount_with_device_file (const char *device_file)
{
  GList *volumes;
  GList *l;

  volumes = g_volume_monitor_get_volumes (volume_monitor);
  for (l = volumes; l != NULL; l = l->next)
    {
      GVolume *volume = G_VOLUME (l->data);
      gchar *id;

      id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
      if (g_strcmp0 (id, device_file) == 0)
        {
          GMountOperation *op;

          op = new_mount_op ();

          g_volume_mount (volume,
                          G_MOUNT_MOUNT_NONE,
                          op,
                          NULL,
                          mount_with_device_file_cb,
                          id);

          outstanding_mounts++;
        }
      else
        g_free (id);
    }
  g_list_free_full (volumes, g_object_unref);

  if (outstanding_mounts == 0)
    {
      print_error ("%s: %s", device_file, _("No volume for device file"));
      success = FALSE;
    }
}

static void
monitor_print_mount (GMount *mount)
{
  if (extra_detail)
    {
      GList *l;
      l = g_list_prepend (NULL, mount);
      list_mounts (l, 2, FALSE);
      g_list_free (l);
      g_print ("\n");
    }
}

static void
monitor_print_volume (GVolume *volume)
{
  if (extra_detail)
    {
      GList *l;
      l = g_list_prepend (NULL, volume);
      list_volumes (l, 2, FALSE);
      g_list_free (l);
      g_print ("\n");
    }
}

static void
monitor_print_drive (GDrive *drive)
{
  if (extra_detail)
    {
      GList *l;
      l = g_list_prepend (NULL, drive);
      list_drives (l, 2);
      g_list_free (l);
      g_print ("\n");
    }
}

static void
monitor_mount_added (GVolumeMonitor *volume_monitor, GMount *mount)
{
  char *name;
  name = g_mount_get_name (mount);
  g_print ("Mount added:        '%s'\n", name);
  g_free (name);
  monitor_print_mount (mount);
}

static void
monitor_mount_removed (GVolumeMonitor *volume_monitor, GMount *mount)
{
  char *name;
  name = g_mount_get_name (mount);
  g_print ("Mount removed:      '%s'\n", name);
  g_free (name);
  monitor_print_mount (mount);
}

static void
monitor_mount_changed (GVolumeMonitor *volume_monitor, GMount *mount)
{
  char *name;
  name = g_mount_get_name (mount);
  g_print ("Mount changed:      '%s'\n", name);
  g_free (name);
  monitor_print_mount (mount);
}

static void
monitor_mount_pre_unmount (GVolumeMonitor *volume_monitor, GMount *mount)
{
  char *name;
  name = g_mount_get_name (mount);
  g_print ("Mount pre-unmount:  '%s'\n", name);
  g_free (name);
  monitor_print_mount (mount);
}

static void
monitor_volume_added (GVolumeMonitor *volume_monitor, GVolume *volume)
{
  char *name;
  name = g_volume_get_name (volume);
  g_print ("Volume added:       '%s'\n", name);
  g_free (name);
  monitor_print_volume (volume);
}

static void
monitor_volume_removed (GVolumeMonitor *volume_monitor, GVolume *volume)
{
  char *name;
  name = g_volume_get_name (volume);
  g_print ("Volume removed:     '%s'\n", name);
  g_free (name);
  monitor_print_volume (volume);
}

static void
monitor_volume_changed (GVolumeMonitor *volume_monitor, GVolume *volume)
{
  char *name;
  name = g_volume_get_name (volume);
  g_print ("Volume changed:     '%s'\n", name);
  g_free (name);
  monitor_print_volume (volume);
}

static void
monitor_drive_connected (GVolumeMonitor *volume_monitor, GDrive *drive)
{
  char *name;
  name = g_drive_get_name (drive);
  g_print ("Drive connected:    '%s'\n", name);
  g_free (name);
  monitor_print_drive (drive);
}

static void
monitor_drive_disconnected (GVolumeMonitor *volume_monitor, GDrive *drive)
{
  char *name;
  name = g_drive_get_name (drive);
  g_print ("Drive disconnected: '%s'\n", name);
  g_free (name);
  monitor_print_drive (drive);
}

static void
monitor_drive_changed (GVolumeMonitor *volume_monitor, GDrive *drive)
{
  char *name;
  name = g_drive_get_name (drive);
  g_print ("Drive changed:      '%s'\n", name);
  g_free (name);
  monitor_print_drive (drive);
}

static void
monitor_drive_eject_button (GVolumeMonitor *volume_monitor, GDrive *drive)
{
  char *name;
  name = g_drive_get_name (drive);
  g_print ("Drive eject button: '%s'\n", name);
  g_free (name);
}

static void
monitor (void)
{
  g_signal_connect (volume_monitor, "mount-added", (GCallback) monitor_mount_added, NULL);
  g_signal_connect (volume_monitor, "mount-removed", (GCallback) monitor_mount_removed, NULL);
  g_signal_connect (volume_monitor, "mount-changed", (GCallback) monitor_mount_changed, NULL);
  g_signal_connect (volume_monitor, "mount-pre-unmount", (GCallback) monitor_mount_pre_unmount, NULL);
  g_signal_connect (volume_monitor, "volume-added", (GCallback) monitor_volume_added, NULL);
  g_signal_connect (volume_monitor, "volume-removed", (GCallback) monitor_volume_removed, NULL);
  g_signal_connect (volume_monitor, "volume-changed", (GCallback) monitor_volume_changed, NULL);
  g_signal_connect (volume_monitor, "drive-connected", (GCallback) monitor_drive_connected, NULL);
  g_signal_connect (volume_monitor, "drive-disconnected", (GCallback) monitor_drive_disconnected, NULL);
  g_signal_connect (volume_monitor, "drive-changed", (GCallback) monitor_drive_changed, NULL);
  g_signal_connect (volume_monitor, "drive-eject-button", (GCallback) monitor_drive_eject_button, NULL);

  g_print ("Monitoring events. Press Ctrl+C to quit.\n");

  g_main_loop_run (main_loop);
}

int
handle_mount (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  GFile *file;
  int i;

  g_set_prgname ("gio mount");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("[%s...]", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Mount or unmount the locations."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (do_help)
    {
      show_help (context, NULL);
      g_option_context_free (context);
      return 0;
    }

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      show_help (context, error->message);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  main_loop = g_main_loop_new (NULL, FALSE);
  volume_monitor = g_volume_monitor_get ();

  if (mount_list)
    list_monitor_items ();
  else if (mount_device_file != NULL)
    mount_with_device_file (mount_device_file);
  else if (unmount_scheme != NULL)
    unmount_all_with_scheme (unmount_scheme);
  else if (mount_monitor)
    monitor ();
  else if (argc > 1)
    {
      for (i = 1; i < argc; i++)
        {
          file = g_file_new_for_commandline_arg (argv[i]);
          if (mount_unmount)
            unmount (file);
          else if (mount_eject)
            eject (file);
          else
            mount (file);
          g_object_unref (file);
        }
    }

  if (outstanding_mounts > 0)
    g_main_loop_run (main_loop);

  g_object_unref (volume_monitor);

  return success ? 0 : 2;
}
