/*
 * Copyright © 2011 William Hua
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
 * Author: William Hua <william@attente.ca>
 */

#include "config.h"

#include "gsettingsbackendinternal.h"
#include "gsimplepermission.h"
#include "giomodule.h"

#import <Foundation/Foundation.h>

GType g_nextstep_settings_backend_get_type (void);

#define G_NEXTSTEP_SETTINGS_BACKEND(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), g_nextstep_settings_backend_get_type (), GNextstepSettingsBackend))

typedef struct _GNextstepSettingsBackend GNextstepSettingsBackend;
typedef GSettingsBackendClass            GNextstepSettingsBackendClass;

struct _GNextstepSettingsBackend
{
  GSettingsBackend  parent_instance;

  /*< private >*/
  NSUserDefaults   *user_defaults;
  GMutex            mutex;
};

G_DEFINE_TYPE_WITH_CODE (GNextstepSettingsBackend,
                         g_nextstep_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "nextstep", 90));

static void          g_nextstep_settings_backend_finalize       (GObject            *backend);

static GVariant *    g_nextstep_settings_backend_read           (GSettingsBackend   *backend,
                                                                 const gchar        *key,
                                                                 const GVariantType *expected_type,
                                                                 gboolean            default_value);

static gboolean      g_nextstep_settings_backend_get_writable   (GSettingsBackend   *backend,
                                                                 const gchar        *key);

static gboolean      g_nextstep_settings_backend_write          (GSettingsBackend   *backend,
                                                                 const gchar        *key,
                                                                 GVariant           *value,
                                                                 gpointer            origin_tag);

static gboolean      g_nextstep_settings_backend_write_tree     (GSettingsBackend   *backend,
                                                                 GTree              *tree,
                                                                 gpointer            origin_tag);

static void          g_nextstep_settings_backend_reset          (GSettingsBackend   *backend,
                                                                 const gchar        *key,
                                                                 gpointer            origin_tag);

static void          g_nextstep_settings_backend_subscribe      (GSettingsBackend   *backend,
                                                                 const gchar        *name);

static void          g_nextstep_settings_backend_unsubscribe    (GSettingsBackend   *backend,
                                                                 const gchar        *name);

static void          g_nextstep_settings_backend_sync           (GSettingsBackend   *backend);

static GPermission * g_nextstep_settings_backend_get_permission (GSettingsBackend   *backend,
                                                                 const gchar        *path);

static gboolean      g_nextstep_settings_backend_write_pair     (gpointer            name,
                                                                 gpointer            value,
                                                                 gpointer            data);

static GVariant *    g_nextstep_settings_backend_get_g_variant  (id                  object,
                                                                 const GVariantType *type);

static id            g_nextstep_settings_backend_get_ns_object  (GVariant           *variant);

static void
g_nextstep_settings_backend_class_init (GNextstepSettingsBackendClass *class)
{
  G_OBJECT_CLASS (class)->finalize = g_nextstep_settings_backend_finalize;
  class->read = g_nextstep_settings_backend_read;
  class->get_writable = g_nextstep_settings_backend_get_writable;
  class->write = g_nextstep_settings_backend_write;
  class->write_tree = g_nextstep_settings_backend_write_tree;
  class->reset = g_nextstep_settings_backend_reset;
  class->subscribe = g_nextstep_settings_backend_subscribe;
  class->unsubscribe = g_nextstep_settings_backend_unsubscribe;
  class->sync = g_nextstep_settings_backend_sync;
  class->get_permission = g_nextstep_settings_backend_get_permission;
}

static void
g_nextstep_settings_backend_init (GNextstepSettingsBackend *self)
{
  NSAutoreleasePool *pool;

  pool = [[NSAutoreleasePool alloc] init];

  self->user_defaults = [[NSUserDefaults standardUserDefaults] retain];

  g_mutex_init (&self->mutex);

  [pool drain];
}

static void
g_nextstep_settings_backend_finalize (GObject *self)
{
  GNextstepSettingsBackend *backend = G_NEXTSTEP_SETTINGS_BACKEND (self);
  NSAutoreleasePool *pool;

  pool = [[NSAutoreleasePool alloc] init];

  g_mutex_clear (&backend->mutex);

  [backend->user_defaults release];

  [pool drain];

  G_OBJECT_CLASS (g_nextstep_settings_backend_parent_class)->finalize (self);
}

static GVariant *
g_nextstep_settings_backend_read (GSettingsBackend   *backend,
                                  const gchar        *key,
                                  const GVariantType *expected_type,
                                  gboolean            default_value)
{
  GNextstepSettingsBackend *self = G_NEXTSTEP_SETTINGS_BACKEND (backend);
  NSAutoreleasePool *pool;
  NSString *name;
  id value;
  GVariant *variant;

  if (default_value)
    return NULL;

  pool = [[NSAutoreleasePool alloc] init];
  name = [NSString stringWithUTF8String:key];

  g_mutex_lock (&self->mutex);
  value = [self->user_defaults objectForKey:name];
  g_mutex_unlock (&self->mutex);

  variant = g_nextstep_settings_backend_get_g_variant (value, expected_type);

  [pool drain];

  return variant;
}

static gboolean
g_nextstep_settings_backend_get_writable (GSettingsBackend *backend,
                                          const gchar      *key)
{
  return TRUE;
}

static gboolean
g_nextstep_settings_backend_write (GSettingsBackend *backend,
                                   const gchar      *key,
                                   GVariant         *value,
                                   gpointer          origin_tag)
{
  GNextstepSettingsBackend *self = G_NEXTSTEP_SETTINGS_BACKEND (backend);
  NSAutoreleasePool *pool;

  pool = [[NSAutoreleasePool alloc] init];

  g_mutex_lock (&self->mutex);
  g_nextstep_settings_backend_write_pair ((gpointer) key, value, self);
  g_mutex_unlock (&self->mutex);

  g_settings_backend_changed (backend, key, origin_tag);

  [pool drain];

  return TRUE;
}

static gboolean
g_nextstep_settings_backend_write_tree (GSettingsBackend *backend,
                                        GTree            *tree,
                                        gpointer          origin_tag)
{
  GNextstepSettingsBackend *self = G_NEXTSTEP_SETTINGS_BACKEND (backend);
  NSAutoreleasePool *pool;

  pool = [[NSAutoreleasePool alloc] init];

  g_mutex_lock (&self->mutex);
  g_tree_foreach (tree, g_nextstep_settings_backend_write_pair, self);
  g_mutex_unlock (&self->mutex);
  g_settings_backend_changed_tree (backend, tree, origin_tag);

  [pool drain];

  return TRUE;
}

static void
g_nextstep_settings_backend_reset (GSettingsBackend *backend,
                                   const gchar      *key,
                                   gpointer          origin_tag)
{
  GNextstepSettingsBackend *self = G_NEXTSTEP_SETTINGS_BACKEND (backend);
  NSAutoreleasePool *pool;
  NSString *name;

  pool = [[NSAutoreleasePool alloc] init];
  name = [NSString stringWithUTF8String:key];

  g_mutex_lock (&self->mutex);
  [self->user_defaults removeObjectForKey:name];
  g_mutex_unlock (&self->mutex);

  g_settings_backend_changed (backend, key, origin_tag);

  [pool drain];
}

static void
g_nextstep_settings_backend_subscribe (GSettingsBackend *backend,
                                       const gchar      *name)
{
}

static void
g_nextstep_settings_backend_unsubscribe (GSettingsBackend *backend,
                                         const gchar      *name)
{
}

static void
g_nextstep_settings_backend_sync (GSettingsBackend *backend)
{
  GNextstepSettingsBackend *self = G_NEXTSTEP_SETTINGS_BACKEND (backend);
  NSAutoreleasePool *pool;

  pool = [[NSAutoreleasePool alloc] init];

  g_mutex_lock (&self->mutex);
  [self->user_defaults synchronize];
  g_mutex_unlock (&self->mutex);

  [pool drain];
}

static GPermission *
g_nextstep_settings_backend_get_permission (GSettingsBackend *backend,
                                            const gchar      *path)
{
  return g_simple_permission_new (TRUE);
}

static gboolean
g_nextstep_settings_backend_write_pair (gpointer name,
                                        gpointer value,
                                        gpointer data)
{
  GNextstepSettingsBackend *backend = G_NEXTSTEP_SETTINGS_BACKEND (data);
  NSString *key;
  id object;

  key = [NSString stringWithUTF8String:name];
  object = g_nextstep_settings_backend_get_ns_object (value);

  [backend->user_defaults setObject:object forKey:key];

  return FALSE;
}

static GVariant *
g_nextstep_settings_backend_get_g_variant (id                  object,
                                           const GVariantType *type)
{
  if ([object isKindOfClass:[NSData class]])
    return g_variant_parse (type, [[[[NSString alloc] initWithData:object encoding:NSUTF8StringEncoding] autorelease] UTF8String], NULL, NULL, NULL);
  else if ([object isKindOfClass:[NSNumber class]])
    {
      if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
        return g_variant_new_boolean ([object boolValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
        return g_variant_new_byte ([object unsignedCharValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
        return g_variant_new_int16 ([object shortValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
        return g_variant_new_uint16 ([object unsignedShortValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
        return g_variant_new_int32 ([object longValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
        return g_variant_new_uint32 ([object unsignedLongValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
        return g_variant_new_int64 ([object longLongValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
        return g_variant_new_uint64 ([object unsignedLongLongValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_HANDLE))
        return g_variant_new_handle ([object longValue]);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
        return g_variant_new_double ([object doubleValue]);
    }
  else if ([object isKindOfClass:[NSString class]])
    {
      const char *string;

      string = [object UTF8String];

      if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
        return g_variant_new_string (string);
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
        return g_variant_is_object_path (string) ?
               g_variant_new_object_path (string) : NULL;
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
        return g_variant_is_signature (string) ?
               g_variant_new_signature (string) : NULL;
    }
  else if ([object isKindOfClass:[NSDictionary class]])
    {
      if (g_variant_type_is_subtype_of (type, G_VARIANT_TYPE ("a{s*}")))
        {
          const GVariantType *value_type;
          GVariantBuilder builder;
          NSString *key;

          value_type = g_variant_type_value (g_variant_type_element (type));

          g_variant_builder_init (&builder, type);

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
          for(key in object)
#else
          NSEnumerator *enumerator = [object objectEnumerator];
          while((key = [enumerator nextObject]))
#endif
            {
              GVariant *name;
              id value;
              GVariant *variant;
              GVariant *entry;

              name = g_variant_new_string ([key UTF8String]);
              value = [object objectForKey:key];
              variant = g_nextstep_settings_backend_get_g_variant (value, value_type);

              if (variant == NULL)
                {
                  g_variant_builder_clear (&builder);

                  return NULL;
                }

              entry = g_variant_new_dict_entry (name, variant);
              g_variant_builder_add_value (&builder, entry);
            }

          return g_variant_builder_end (&builder);
        }
    }
  else if ([object isKindOfClass:[NSArray class]])
    {
      if (g_variant_type_is_subtype_of (type, G_VARIANT_TYPE_ARRAY))
        {
          const GVariantType *value_type;
          GVariantBuilder builder;
          id value;

          value_type = g_variant_type_element (type);
          g_variant_builder_init (&builder, type);

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
          for(value in object)
#else
          NSEnumerator *enumerator = [object objectEnumerator];
          while((value = [enumerator nextObject]))
#endif
            {
              GVariant *variant = g_nextstep_settings_backend_get_g_variant (value, value_type);

              if (variant == NULL)
                {
                  g_variant_builder_clear (&builder);

                  return NULL;
                }

              g_variant_builder_add_value (&builder, variant);
            }

          return g_variant_builder_end (&builder);
        }
    }

  return NULL;
}

static id
g_nextstep_settings_backend_get_ns_object (GVariant *variant)
{
  if (variant == NULL)
    return nil;
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN))
    return [NSNumber numberWithBool:g_variant_get_boolean (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BYTE))
    return [NSNumber numberWithUnsignedChar:g_variant_get_byte (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT16))
    return [NSNumber numberWithShort:g_variant_get_int16 (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT16))
    return [NSNumber numberWithUnsignedShort:g_variant_get_uint16 (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32))
    return [NSNumber numberWithLong:g_variant_get_int32 (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT32))
    return [NSNumber numberWithUnsignedLong:g_variant_get_uint32 (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT64))
    return [NSNumber numberWithLongLong:g_variant_get_int64 (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT64))
    return [NSNumber numberWithUnsignedLongLong:g_variant_get_uint64 (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_HANDLE))
    return [NSNumber numberWithLong:g_variant_get_handle (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_DOUBLE))
    return [NSNumber numberWithDouble:g_variant_get_double (variant)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    return [NSString stringWithUTF8String:g_variant_get_string (variant, NULL)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_OBJECT_PATH))
    return [NSString stringWithUTF8String:g_variant_get_string (variant, NULL)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_SIGNATURE))
    return [NSString stringWithUTF8String:g_variant_get_string (variant, NULL)];
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE ("a{s*}")))
    {
      NSMutableDictionary *dictionary;
      GVariantIter iter;
      GVariant *name;
      GVariant *value;

      dictionary = [NSMutableDictionary dictionaryWithCapacity:g_variant_iter_init (&iter, variant)];

      while (g_variant_iter_loop (&iter, "{s*}", &name, &value))
        {
          NSString *key;
          id object;

          key = [NSString stringWithUTF8String:g_variant_get_string (name, NULL)];
          object = g_nextstep_settings_backend_get_ns_object (value);

          [dictionary setObject:object forKey:key];
        }

      return dictionary;
    }
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_ARRAY))
    {
      NSMutableArray *array;
      GVariantIter iter;
      GVariant *value;

      array = [NSMutableArray arrayWithCapacity:g_variant_iter_init (&iter, variant)];

      while ((value = g_variant_iter_next_value (&iter)) != NULL)
        [array addObject:g_nextstep_settings_backend_get_ns_object (value)];

      return array;
    }
  else
    return [[NSString stringWithUTF8String:g_variant_print (variant, TRUE)] dataUsingEncoding:NSUTF8StringEncoding];
}
