/*
 * Copyright © 2009, 2010 Codethink Limited
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gdelayedsettingsbackend.h"
#include "gsettingsbackendinternal.h"

#include <string.h>


struct _GDelayedSettingsBackendPrivate
{
  GSettingsBackend *backend;
  GMutex lock;
  GTree *delayed;

  GMainContext *owner_context;
  gpointer owner;
};

G_DEFINE_TYPE_WITH_PRIVATE (GDelayedSettingsBackend,
                            g_delayed_settings_backend,
                            G_TYPE_SETTINGS_BACKEND)

static gboolean
invoke_notify_unapplied (gpointer data)
{
  g_object_notify (data, "has-unapplied");
  g_object_unref (data);

  return FALSE;
}

static void
g_delayed_settings_backend_notify_unapplied (GDelayedSettingsBackend *delayed)
{
  GMainContext *target_context;
  GObject *target;

  g_mutex_lock (&delayed->priv->lock);
  if (delayed->priv->owner)
    {
      target_context = delayed->priv->owner_context;
      target = g_object_ref (delayed->priv->owner);
    }
  else
    {
      target_context = NULL;
      target = NULL;
    }
  g_mutex_unlock (&delayed->priv->lock);

  if (target != NULL)
    g_main_context_invoke (target_context, invoke_notify_unapplied, target);
}


static GVariant *
g_delayed_settings_backend_read (GSettingsBackend   *backend,
                                 const gchar        *key,
                                 const GVariantType *expected_type,
                                 gboolean            default_value)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gpointer result = NULL;

  if (!default_value)
    {
      g_mutex_lock (&delayed->priv->lock);
      if (g_tree_lookup_extended (delayed->priv->delayed, key, NULL, &result))
        {
          /* NULL in the tree means we should consult the default value */
          if (result != NULL)
            g_variant_ref (result);
          else
            default_value = TRUE;
        }
      g_mutex_unlock (&delayed->priv->lock);
    }

  if (result == NULL)
    result = g_settings_backend_read (delayed->priv->backend, key,
                                      expected_type, default_value);

  return result;
}

static GVariant *
g_delayed_settings_backend_read_user_value (GSettingsBackend   *backend,
                                            const gchar        *key,
                                            const GVariantType *expected_type)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean value_found = FALSE;
  gpointer result = NULL;

  /* If we find an explicit NULL in our changeset then we want to return
   * NULL (because the user value has been reset).
   *
   * Otherwise, chain up.
   */
  g_mutex_lock (&delayed->priv->lock);
  value_found = g_tree_lookup_extended (delayed->priv->delayed, key, NULL, &result);
  if (result)
    g_variant_ref (result);
  g_mutex_unlock (&delayed->priv->lock);

  if (value_found)
    return result;

  return g_settings_backend_read_user_value (delayed->priv->backend, key, expected_type);
}

static gboolean
g_delayed_settings_backend_write (GSettingsBackend *backend,
                                  const gchar      *key,
                                  GVariant         *value,
                                  gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean was_empty;

  g_mutex_lock (&delayed->priv->lock);
  was_empty = g_tree_nnodes (delayed->priv->delayed) == 0;
  g_tree_insert (delayed->priv->delayed, g_strdup (key),
                 g_variant_ref_sink (value));
  g_mutex_unlock (&delayed->priv->lock);

  g_settings_backend_changed (backend, key, origin_tag);

  if (was_empty)
    g_delayed_settings_backend_notify_unapplied (delayed);

  return TRUE;
}

static gboolean
add_to_tree (gpointer key,
             gpointer value,
             gpointer user_data)
{
  g_tree_insert (user_data, g_strdup (key), g_variant_ref (value));
  return FALSE;
}

static gboolean
g_delayed_settings_backend_write_tree (GSettingsBackend *backend,
                                       GTree            *tree,
                                       gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean was_empty;

  g_mutex_lock (&delayed->priv->lock);
  was_empty = g_tree_nnodes (delayed->priv->delayed) == 0;

  g_tree_foreach (tree, add_to_tree, delayed->priv->delayed);
  g_mutex_unlock (&delayed->priv->lock);

  g_settings_backend_changed_tree (backend, tree, origin_tag);

  if (was_empty)
    g_delayed_settings_backend_notify_unapplied (delayed);

  return TRUE;
}

static gboolean
g_delayed_settings_backend_get_writable (GSettingsBackend *backend,
                                         const gchar      *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  return g_settings_backend_get_writable (delayed->priv->backend, name);
}

static void
g_delayed_settings_backend_reset (GSettingsBackend *backend,
                                  const gchar      *key,
                                  gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);
  gboolean was_empty;

  g_mutex_lock (&delayed->priv->lock);
  was_empty = g_tree_nnodes (delayed->priv->delayed) == 0;
  g_tree_insert (delayed->priv->delayed, g_strdup (key), NULL);
  g_mutex_unlock (&delayed->priv->lock);

  if (was_empty)
    g_delayed_settings_backend_notify_unapplied (delayed);
}

static void
g_delayed_settings_backend_subscribe (GSettingsBackend *backend,
                                      const char       *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_settings_backend_subscribe (delayed->priv->backend, name);
}

static void
g_delayed_settings_backend_unsubscribe (GSettingsBackend *backend,
                                        const char       *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_settings_backend_unsubscribe (delayed->priv->backend, name);
}

static GPermission *
g_delayed_settings_backend_get_permission (GSettingsBackend *backend,
                                           const gchar      *path)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  return g_settings_backend_get_permission (delayed->priv->backend, path);
}


/* method calls */
gboolean
g_delayed_settings_backend_get_has_unapplied (GDelayedSettingsBackend *delayed)
{
  /* we don't need to lock for this... */

  return g_tree_nnodes (delayed->priv->delayed) > 0;
}

void
g_delayed_settings_backend_apply (GDelayedSettingsBackend *delayed)
{
  if (g_tree_nnodes (delayed->priv->delayed) > 0)
    {
      gboolean success;
      GTree *tmp;

      g_mutex_lock (&delayed->priv->lock);
      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      success = g_settings_backend_write_tree (delayed->priv->backend,
                                               tmp, delayed->priv);
      g_mutex_unlock (&delayed->priv->lock);

      if (!success)
        g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed),
                                         tmp, NULL);

      g_tree_unref (tmp);

      g_delayed_settings_backend_notify_unapplied (delayed);
    }
}

void
g_delayed_settings_backend_revert (GDelayedSettingsBackend *delayed)
{
  if (g_tree_nnodes (delayed->priv->delayed) > 0)
    {
      GTree *tmp;

      g_mutex_lock (&delayed->priv->lock);
      tmp = delayed->priv->delayed;
      delayed->priv->delayed = g_settings_backend_create_tree ();
      g_mutex_unlock (&delayed->priv->lock);
      g_settings_backend_changed_tree (G_SETTINGS_BACKEND (delayed), tmp, NULL);
      g_tree_unref (tmp);

      g_delayed_settings_backend_notify_unapplied (delayed);
    }
}

/* change notification */
static void
delayed_backend_changed (GObject          *target,
                         GSettingsBackend *backend,
                         const gchar      *key,
                         gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (target);

  if (origin_tag != delayed->priv)
    g_settings_backend_changed (G_SETTINGS_BACKEND (delayed),
                                key, origin_tag);
}

static void
delayed_backend_keys_changed (GObject             *target,
                              GSettingsBackend    *backend,
                              const gchar         *path,
                              gpointer             origin_tag,
                              const gchar * const *items)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (target);

  if (origin_tag != delayed->priv)
    g_settings_backend_keys_changed (G_SETTINGS_BACKEND (delayed),
                                     path, items, origin_tag);
}

static void
delayed_backend_path_changed (GObject          *target,
                              GSettingsBackend *backend,
                              const gchar      *path,
                              gpointer          origin_tag)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (target);

  if (origin_tag != delayed->priv)
    g_settings_backend_path_changed (G_SETTINGS_BACKEND (delayed),
                                     path, origin_tag);
}

static void
delayed_backend_writable_changed (GObject          *target,
                                  GSettingsBackend *backend,
                                  const gchar      *key)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (target);
  gboolean last_one = FALSE;

  g_mutex_lock (&delayed->priv->lock);

  if (g_tree_lookup (delayed->priv->delayed, key) != NULL &&
      !g_settings_backend_get_writable (delayed->priv->backend, key))
    {
      /* drop the key from our changeset if it just became read-only.
       * no need to signal since the writable change below implies it.
       *
       * note that the item in the tree may very well be set to NULL in
       * the case that the user stored a reset.  we intentionally don't
       * drop the key in this case since a reset will always succeed
       * (even against a non-writable key).
       */
      g_tree_remove (delayed->priv->delayed, key);

      /* if that was the only key... */
      last_one = g_tree_nnodes (delayed->priv->delayed) == 0;
    }

  g_mutex_unlock (&delayed->priv->lock);

  if (last_one)
    g_delayed_settings_backend_notify_unapplied (delayed);

  g_settings_backend_writable_changed (G_SETTINGS_BACKEND (delayed), key);
}

/* slow method until we get foreach-with-remove in GTree
 */
typedef struct
{
  const gchar *path;
  const gchar **keys;
  gsize index;
} CheckPrefixState;

static gboolean
check_prefix (gpointer key,
              gpointer value,
              gpointer data)
{
  CheckPrefixState *state = data;

  if (g_str_has_prefix (key, state->path))
    state->keys[state->index++] = key;

  return FALSE;
}

static void
delayed_backend_path_writable_changed (GObject          *target,
                                       GSettingsBackend *backend,
                                       const gchar      *path)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (target);
  gboolean last_one = FALSE;
  gsize n_keys;

  g_mutex_lock (&delayed->priv->lock);

  n_keys = g_tree_nnodes (delayed->priv->delayed);

  if (n_keys > 0)
    {
      CheckPrefixState state = { path, g_new (const gchar *, n_keys) };
      gsize i;

      /* collect a list of possibly-affected keys (ie: matching the path) */
      g_tree_foreach (delayed->priv->delayed, check_prefix, &state);

      /* drop the keys that have been affected.
       *
       * don't drop 'reset' keys (see above) */
      for (i = 0; i < state.index; i++)
        if (g_tree_lookup (delayed->priv->delayed, state.keys[i]) != NULL &&
            !g_settings_backend_get_writable (delayed->priv->backend,
                                              state.keys[i]))
          g_tree_remove (delayed->priv->delayed, state.keys[i]);

      g_free (state.keys);

      last_one = g_tree_nnodes (delayed->priv->delayed) == 0;
    }

  g_mutex_unlock (&delayed->priv->lock);

  if (last_one)
    g_delayed_settings_backend_notify_unapplied (delayed);

  g_settings_backend_path_writable_changed (G_SETTINGS_BACKEND (delayed),
                                            path);
}

static void
g_delayed_settings_backend_finalize (GObject *object)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (object);

  g_mutex_clear (&delayed->priv->lock);
  g_object_unref (delayed->priv->backend);
  g_tree_unref (delayed->priv->delayed);

  /* if our owner is still alive, why are we finalizing? */
  g_assert (delayed->priv->owner == NULL);

  G_OBJECT_CLASS (g_delayed_settings_backend_parent_class)
    ->finalize (object);
}

static void
g_delayed_settings_backend_class_init (GDelayedSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  backend_class->read = g_delayed_settings_backend_read;
  backend_class->read_user_value = g_delayed_settings_backend_read_user_value;
  backend_class->write = g_delayed_settings_backend_write;
  backend_class->write_tree = g_delayed_settings_backend_write_tree;
  backend_class->reset = g_delayed_settings_backend_reset;
  backend_class->get_writable = g_delayed_settings_backend_get_writable;
  backend_class->subscribe = g_delayed_settings_backend_subscribe;
  backend_class->unsubscribe = g_delayed_settings_backend_unsubscribe;
  backend_class->get_permission = g_delayed_settings_backend_get_permission;

  object_class->finalize = g_delayed_settings_backend_finalize;
}

static void
g_delayed_settings_backend_init (GDelayedSettingsBackend *delayed)
{
  delayed->priv = g_delayed_settings_backend_get_instance_private (delayed);
  delayed->priv->delayed = g_settings_backend_create_tree ();
  g_mutex_init (&delayed->priv->lock);
}

static void
g_delayed_settings_backend_disown (gpointer  data,
                                   GObject  *where_the_object_was)
{
  GDelayedSettingsBackend *delayed = data;

  g_mutex_lock (&delayed->priv->lock);
  delayed->priv->owner_context = NULL;
  delayed->priv->owner = NULL;
  g_mutex_unlock (&delayed->priv->lock);
}

GDelayedSettingsBackend *
g_delayed_settings_backend_new (GSettingsBackend *backend,
                                gpointer          owner,
                                GMainContext     *owner_context)
{
  static GSettingsListenerVTable vtable = {
    delayed_backend_changed,
    delayed_backend_path_changed,
    delayed_backend_keys_changed,
    delayed_backend_writable_changed,
    delayed_backend_path_writable_changed
  };
  GDelayedSettingsBackend *delayed;

  delayed = g_object_new (G_TYPE_DELAYED_SETTINGS_BACKEND, NULL);
  delayed->priv->backend = g_object_ref (backend);
  delayed->priv->owner_context = owner_context;
  delayed->priv->owner = owner;

  g_object_weak_ref (owner, g_delayed_settings_backend_disown, delayed);

  g_settings_backend_watch (delayed->priv->backend,
                            &vtable, G_OBJECT (delayed), NULL);

  return delayed;
}
