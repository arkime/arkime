/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2010 Novell, Inc.
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
 * Authors: Vincent Untz <vuntz@gnome.org>
 *          Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "gfile.h"
#include "gfileinfo.h"
#include "gfilemonitor.h"
#include "gsimplepermission.h"
#include "gsettingsbackend.h"


#define G_TYPE_KEYFILE_SETTINGS_BACKEND      (g_keyfile_settings_backend_get_type ())
#define G_KEYFILE_SETTINGS_BACKEND(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst),      \
                                              G_TYPE_KEYFILE_SETTINGS_BACKEND,         \
                                              GKeyfileSettingsBackend))
#define G_IS_KEYFILE_SETTINGS_BACKEND(inst)  (G_TYPE_CHECK_INSTANCE_TYPE ((inst),      \
                                              G_TYPE_KEYFILE_SETTINGS_BACKEND))


typedef GSettingsBackendClass GKeyfileSettingsBackendClass;

typedef struct
{
  GSettingsBackend   parent_instance;

  GKeyFile          *keyfile;
  GPermission       *permission;
  gboolean           writable;

  gchar             *prefix;
  gint               prefix_len;
  gchar             *root_group;
  gint               root_group_len;

  GFile             *file;
  GFileMonitor      *file_monitor;
  guint8             digest[32];
  GFile             *dir;
  GFileMonitor      *dir_monitor;
} GKeyfileSettingsBackend;

static GType g_keyfile_settings_backend_get_type (void);
G_DEFINE_TYPE (GKeyfileSettingsBackend,
               g_keyfile_settings_backend,
               G_TYPE_SETTINGS_BACKEND)

static void
compute_checksum (guint8        *digest,
                  gconstpointer  contents,
                  gsize          length)
{
  GChecksum *checksum;
  gsize len = 32;

  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (checksum, contents, length);
  g_checksum_get_digest (checksum, digest, &len);
  g_checksum_free (checksum);
  g_assert (len == 32);
}

static void
g_keyfile_settings_backend_keyfile_write (GKeyfileSettingsBackend *kfsb)
{
  gchar *contents;
  gsize length;

  contents = g_key_file_to_data (kfsb->keyfile, &length, NULL);
  g_file_replace_contents (kfsb->file, contents, length, NULL, FALSE,
                           G_FILE_CREATE_REPLACE_DESTINATION,
                           NULL, NULL, NULL);

  compute_checksum (kfsb->digest, contents, length);
  g_free (contents);
}

static gboolean
group_name_matches (const gchar *group_name,
                    const gchar *prefix)
{
  /* sort of like g_str_has_prefix() except that it must be an exact
   * match or the prefix followed by '/'.
   *
   * for example 'a' is a prefix of 'a' and 'a/b' but not 'ab'.
   */
  gint i;

  for (i = 0; prefix[i]; i++)
    if (prefix[i] != group_name[i])
      return FALSE;

  return group_name[i] == '\0' || group_name[i] == '/';
}

static gboolean
convert_path (GKeyfileSettingsBackend  *kfsb,
              const gchar              *key,
              gchar                   **group,
              gchar                   **basename)
{
  gint key_len = strlen (key);
  gint i;

  if (key_len < kfsb->prefix_len ||
      memcmp (key, kfsb->prefix, kfsb->prefix_len) != 0)
    return FALSE;

  key_len -= kfsb->prefix_len;
  key += kfsb->prefix_len;

  for (i = key_len; i >= 0; i--)
    if (key[i] == '/')
      break;

  if (kfsb->root_group)
    {
      /* if a root_group was specified, make sure the user hasn't given
       * a path that ghosts that group name
       */
      if (i == kfsb->root_group_len && memcmp (key, kfsb->root_group, i) == 0)
        return FALSE;
    }
  else
    {
      /* if no root_group was given, ensure that the user gave a path */
      if (i == -1)
        return FALSE;
    }

  if (group)
    {
      if (i >= 0)
        {
          *group = g_memdup (key, i + 1);
          (*group)[i] = '\0';
        }
      else
        *group = g_strdup (kfsb->root_group);
    }

  if (basename)
    *basename = g_memdup (key + i + 1, key_len - i);

  return TRUE;
}

static gboolean
path_is_valid (GKeyfileSettingsBackend *kfsb,
               const gchar             *path)
{
  return convert_path (kfsb, path, NULL, NULL);
}

static GVariant *
get_from_keyfile (GKeyfileSettingsBackend *kfsb,
                  const GVariantType      *type,
                  const gchar             *key)
{
  GVariant *return_value = NULL;
  gchar *group, *name;

  if (convert_path (kfsb, key, &group, &name))
    {
      gchar *str;

      g_assert (*name);

      str = g_key_file_get_value (kfsb->keyfile, group, name, NULL);

      if (str)
        {
          return_value = g_variant_parse (type, str, NULL, NULL, NULL);
          g_free (str);
        }

      g_free (group);
      g_free (name);
    }

  return return_value;
}

static gboolean
set_to_keyfile (GKeyfileSettingsBackend *kfsb,
                const gchar             *key,
                GVariant                *value)
{
  gchar *group, *name;

  if (convert_path (kfsb, key, &group, &name))
    {
      if (value)
        {
          gchar *str = g_variant_print (value, FALSE);
          g_key_file_set_value (kfsb->keyfile, group, name, str);
          g_variant_unref (g_variant_ref_sink (value));
          g_free (str);
        }
      else
        {
          if (*name == '\0')
            {
              gchar **groups;
              gint i;

              groups = g_key_file_get_groups (kfsb->keyfile, NULL);

              for (i = 0; groups[i]; i++)
                if (group_name_matches (groups[i], group))
                  g_key_file_remove_group (kfsb->keyfile, groups[i], NULL);

              g_strfreev (groups);
            }
          else
            g_key_file_remove_key (kfsb->keyfile, group, name, NULL);
        }

      g_free (group);
      g_free (name);

      return TRUE;
    }

  return FALSE;
}

static GVariant *
g_keyfile_settings_backend_read (GSettingsBackend   *backend,
                                 const gchar        *key,
                                 const GVariantType *expected_type,
                                 gboolean            default_value)
{
  GKeyfileSettingsBackend *kfsb = G_KEYFILE_SETTINGS_BACKEND (backend);

  if (default_value)
    return NULL;

  return get_from_keyfile (kfsb, expected_type, key);
}

typedef struct
{
  GKeyfileSettingsBackend *kfsb;
  gboolean failed;
} WriteManyData;

static gboolean
g_keyfile_settings_backend_write_one (gpointer key,
                                      gpointer value,
                                      gpointer user_data)
{
  WriteManyData *data = user_data;
  gboolean success;

  success = set_to_keyfile (data->kfsb, key, value);
  g_assert (success);

  return FALSE;
}

static gboolean
g_keyfile_settings_backend_check_one (gpointer key,
                                      gpointer value,
                                      gpointer user_data)
{
  WriteManyData *data = user_data;

  return data->failed = !path_is_valid (data->kfsb, key);
}

static gboolean
g_keyfile_settings_backend_write_tree (GSettingsBackend *backend,
                                       GTree            *tree,
                                       gpointer          origin_tag)
{
  WriteManyData data = { G_KEYFILE_SETTINGS_BACKEND (backend) };

  if (!data.kfsb->writable)
    return FALSE;

  g_tree_foreach (tree, g_keyfile_settings_backend_check_one, &data);

  if (data.failed)
    return FALSE;

  g_tree_foreach (tree, g_keyfile_settings_backend_write_one, &data);
  g_keyfile_settings_backend_keyfile_write (data.kfsb);

  g_settings_backend_changed_tree (backend, tree, origin_tag);

  return TRUE;
}

static gboolean
g_keyfile_settings_backend_write (GSettingsBackend *backend,
                                  const gchar      *key,
                                  GVariant         *value,
                                  gpointer          origin_tag)
{
  GKeyfileSettingsBackend *kfsb = G_KEYFILE_SETTINGS_BACKEND (backend);
  gboolean success;

  if (!kfsb->writable)
    return FALSE;

  success = set_to_keyfile (kfsb, key, value);

  if (success)
    {
      g_settings_backend_changed (backend, key, origin_tag);
      g_keyfile_settings_backend_keyfile_write (kfsb);
    }

  return success;
}

static void
g_keyfile_settings_backend_reset (GSettingsBackend *backend,
                                  const gchar      *key,
                                  gpointer          origin_tag)
{
  GKeyfileSettingsBackend *kfsb = G_KEYFILE_SETTINGS_BACKEND (backend);

  if (set_to_keyfile (kfsb, key, NULL))
    g_keyfile_settings_backend_keyfile_write (kfsb);

  g_settings_backend_changed (backend, key, origin_tag);
}

static gboolean
g_keyfile_settings_backend_get_writable (GSettingsBackend *backend,
                                         const gchar      *name)
{
  GKeyfileSettingsBackend *kfsb = G_KEYFILE_SETTINGS_BACKEND (backend);

  return kfsb->writable && path_is_valid (kfsb, name);
}

static GPermission *
g_keyfile_settings_backend_get_permission (GSettingsBackend *backend,
                                           const gchar      *path)
{
  GKeyfileSettingsBackend *kfsb = G_KEYFILE_SETTINGS_BACKEND (backend);

  return g_object_ref (kfsb->permission);
}

static void
keyfile_to_tree (GKeyfileSettingsBackend *kfsb,
                 GTree                   *tree,
                 GKeyFile                *keyfile,
                 gboolean                 dup_check)
{
  gchar **groups;
  gint i;

  groups = g_key_file_get_groups (keyfile, NULL);
  for (i = 0; groups[i]; i++)
    {
      gboolean is_root_group;
      gchar **keys;
      gint j;

      is_root_group = g_strcmp0 (kfsb->root_group, groups[i]) == 0;

      /* reject group names that will form invalid key names */
      if (!is_root_group &&
          (g_str_has_prefix (groups[i], "/") ||
           g_str_has_suffix (groups[i], "/") || strstr (groups[i], "//")))
        continue;

      keys = g_key_file_get_keys (keyfile, groups[i], NULL, NULL);
      g_assert (keys != NULL);

      for (j = 0; keys[j]; j++)
        {
          gchar *path, *value;

          /* reject key names with slashes in them */
          if (strchr (keys[j], '/'))
            continue;

          if (is_root_group)
            path = g_strdup_printf ("%s%s", kfsb->prefix, keys[j]);
          else
            path = g_strdup_printf ("%s%s/%s", kfsb->prefix, groups[i], keys[j]);

          value = g_key_file_get_value (keyfile, groups[i], keys[j], NULL);

          if (dup_check && g_strcmp0 (g_tree_lookup (tree, path), value) == 0)
            {
              g_tree_remove (tree, path);
              g_free (value);
              g_free (path);
            }
          else
            g_tree_insert (tree, path, value);
        }

      g_strfreev (keys);
    }
  g_strfreev (groups);
}

static void
g_keyfile_settings_backend_keyfile_reload (GKeyfileSettingsBackend *kfsb)
{
  guint8 digest[32];
  gchar *contents;
  gsize length;

  contents = NULL;
  length = 0;

  g_file_load_contents (kfsb->file, NULL, &contents, &length, NULL, NULL);
  compute_checksum (digest, contents, length);

  if (memcmp (kfsb->digest, digest, sizeof digest) != 0)
    {
      GKeyFile *keyfiles[2];
      GTree *tree;

      tree = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
                              g_free, g_free);

      keyfiles[0] = kfsb->keyfile;
      keyfiles[1] = g_key_file_new ();

      if (length > 0)
        g_key_file_load_from_data (keyfiles[1], contents, length,
                                   G_KEY_FILE_KEEP_COMMENTS |
                                   G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

      keyfile_to_tree (kfsb, tree, keyfiles[0], FALSE);
      keyfile_to_tree (kfsb, tree, keyfiles[1], TRUE);
      g_key_file_free (keyfiles[0]);
      kfsb->keyfile = keyfiles[1];

      if (g_tree_nnodes (tree) > 0)
        g_settings_backend_changed_tree (&kfsb->parent_instance, tree, NULL);

      g_tree_unref (tree);

      memcpy (kfsb->digest, digest, sizeof digest);
    }

  g_free (contents);
}

static void
g_keyfile_settings_backend_keyfile_writable (GKeyfileSettingsBackend *kfsb)
{
  GFileInfo *fileinfo;
  gboolean writable;

  fileinfo = g_file_query_info (kfsb->dir, "access::*", 0, NULL, NULL);

  if (fileinfo)
    {
      writable =
        g_file_info_get_attribute_boolean (fileinfo, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE) &&
        g_file_info_get_attribute_boolean (fileinfo, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
      g_object_unref (fileinfo);
    }
  else
    writable = FALSE;

  if (writable != kfsb->writable)
    {
      kfsb->writable = writable;
      g_settings_backend_path_writable_changed (&kfsb->parent_instance, "/");
    }
}

static void
g_keyfile_settings_backend_finalize (GObject *object)
{
  GKeyfileSettingsBackend *kfsb = G_KEYFILE_SETTINGS_BACKEND (object);

  g_key_file_free (kfsb->keyfile);
  g_object_unref (kfsb->permission);

  g_file_monitor_cancel (kfsb->file_monitor);
  g_object_unref (kfsb->file_monitor);
  g_object_unref (kfsb->file);

  g_file_monitor_cancel (kfsb->dir_monitor);
  g_object_unref (kfsb->dir_monitor);
  g_object_unref (kfsb->dir);

  g_free (kfsb->root_group);
  g_free (kfsb->prefix);

  G_OBJECT_CLASS (g_keyfile_settings_backend_parent_class)
    ->finalize (object);
}

static void
g_keyfile_settings_backend_init (GKeyfileSettingsBackend *kfsb)
{
}

static void
g_keyfile_settings_backend_class_init (GKeyfileSettingsBackendClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_keyfile_settings_backend_finalize;

  class->read = g_keyfile_settings_backend_read;
  class->write = g_keyfile_settings_backend_write;
  class->write_tree = g_keyfile_settings_backend_write_tree;
  class->reset = g_keyfile_settings_backend_reset;
  class->get_writable = g_keyfile_settings_backend_get_writable;
  class->get_permission = g_keyfile_settings_backend_get_permission;
  /* No need to implement subscribed/unsubscribe: the only point would be to
   * stop monitoring the file when there's no GSettings anymore, which is no
   * big win.
   */
}

static void
file_changed (GFileMonitor      *monitor,
              GFile             *file,
              GFile             *other_file,
              GFileMonitorEvent  event_type,
              gpointer           user_data)
{
  GKeyfileSettingsBackend *kfsb = user_data;

  /* Ignore file deletions, let the GKeyFile content remain in tact. */
  if (event_type != G_FILE_MONITOR_EVENT_DELETED)
    g_keyfile_settings_backend_keyfile_reload (kfsb);
}

static void
dir_changed (GFileMonitor       *monitor,
              GFile             *file,
              GFile             *other_file,
              GFileMonitorEvent  event_type,
              gpointer           user_data)
{
  GKeyfileSettingsBackend *kfsb = user_data;

  g_keyfile_settings_backend_keyfile_writable (kfsb);
}

/**
 * g_keyfile_settings_backend_new:
 * @filename: the filename of the keyfile
 * @root_path: the path under which all settings keys appear
 * @root_group: (nullable): the group name corresponding to
 *              @root_path, or %NULL
 *
 * Creates a keyfile-backed #GSettingsBackend.
 *
 * The filename of the keyfile to use is given by @filename.
 *
 * All settings read to or written from the backend must fall under the
 * path given in @root_path (which must start and end with a slash and
 * not contain two consecutive slashes).  @root_path may be "/".
 *
 * If @root_group is non-%NULL then it specifies the name of the keyfile
 * group used for keys that are written directly below @root_path.  For
 * example, if @root_path is "/apps/example/" and @root_group is
 * "toplevel", then settings the key "/apps/example/enabled" to a value
 * of %TRUE will cause the following to appear in the keyfile:
 *
 * |[
 *   [toplevel]
 *   enabled=true
 * ]|
 *
 * If @root_group is %NULL then it is not permitted to store keys
 * directly below the @root_path.
 *
 * For keys not stored directly below @root_path (ie: in a sub-path),
 * the name of the subpath (with the final slash stripped) is used as
 * the name of the keyfile group.  To continue the example, if
 * "/apps/example/profiles/default/font-size" were set to
 * 12 then the following would appear in the keyfile:
 *
 * |[
 *   [profiles/default]
 *   font-size=12
 * ]|
 *
 * The backend will refuse writes (and return writability as being
 * %FALSE) for keys outside of @root_path and, in the event that
 * @root_group is %NULL, also for keys directly under @root_path.
 * Writes will also be refused if the backend detects that it has the
 * inability to rewrite the keyfile (ie: the containing directory is not
 * writable).
 *
 * There is no checking done for your key namespace clashing with the
 * syntax of the key file format.  For example, if you have '[' or ']'
 * characters in your path names or '=' in your key names you may be in
 * trouble.
 *
 * Returns: (transfer full): a keyfile-backed #GSettingsBackend
 **/
GSettingsBackend *
g_keyfile_settings_backend_new (const gchar *filename,
                                const gchar *root_path,
                                const gchar *root_group)
{
  GKeyfileSettingsBackend *kfsb;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (root_path != NULL, NULL);
  g_return_val_if_fail (g_str_has_prefix (root_path, "/"), NULL);
  g_return_val_if_fail (g_str_has_suffix (root_path, "/"), NULL);
  g_return_val_if_fail (strstr (root_path, "//") == NULL, NULL);

  kfsb = g_object_new (G_TYPE_KEYFILE_SETTINGS_BACKEND, NULL);
  kfsb->keyfile = g_key_file_new ();
  kfsb->permission = g_simple_permission_new (TRUE);

  kfsb->file = g_file_new_for_path (filename);
  kfsb->dir = g_file_get_parent (kfsb->file);
  g_file_make_directory_with_parents (kfsb->dir, NULL, NULL);

  kfsb->file_monitor = g_file_monitor (kfsb->file, 0, NULL, NULL);
  kfsb->dir_monitor = g_file_monitor (kfsb->dir, 0, NULL, NULL);

  kfsb->prefix_len = strlen (root_path);
  kfsb->prefix = g_strdup (root_path);

  if (root_group)
    {
      kfsb->root_group_len = strlen (root_group);
      kfsb->root_group = g_strdup (root_group);
    }

  compute_checksum (kfsb->digest, NULL, 0);

  g_signal_connect (kfsb->file_monitor, "changed",
                    G_CALLBACK (file_changed), kfsb);
  g_signal_connect (kfsb->dir_monitor, "changed",
                    G_CALLBACK (dir_changed), kfsb);

  g_keyfile_settings_backend_keyfile_writable (kfsb);
  g_keyfile_settings_backend_keyfile_reload (kfsb);

  return G_SETTINGS_BACKEND (kfsb);
}
