/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2014 Руслан Ижбулатов
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
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Руслан Ижбулатов  <lrn1986@gmail.com>
 */

#include "config.h"

#include <string.h>

#include "gcontenttype.h"
#include "gwin32appinfo.h"
#include "gappinfo.h"
#include "gioerror.h"
#include "gfile.h"
#include <glib/gstdio.h>
#include "glibintl.h"
#include <gio/gwin32registrykey.h>

#include <windows.h>

/* We need to watch 8 places:
 * 0) HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations
 *    (anything below that key)
 *    On change: re-enumerate subkeys, read their values.
 * 1) HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts
 *    (anything below that key)
 *    On change: re-enumerate subkeys
 * 2) HKEY_CURRENT_USER\\Software\\Clients (anything below that key)
 *    On change: re-read the whole hierarchy of handlers
 * 3) HKEY_LOCAL_MACHINE\\Software\\Clients (anything below that key)
 *    On change: re-read the whole hierarchy of handlers
 * 4) HKEY_LOCAL_MACHINE\\Software\\RegisteredApplications (values of that key)
 *    On change: re-read the value list of registered applications
 * 5) HKEY_CURRENT_USER\\Software\\RegisteredApplications (values of that key)
 *    On change: re-read the value list of registered applications
 * 6) HKEY_CLASSES_ROOT\\Applications (anything below that key)
 *    On change: re-read the whole hierarchy of apps
 * 7) HKEY_CLASSES_ROOT (only its subkeys)
 *    On change: re-enumerate subkeys, try to filter out wrong names.
 *
 */

typedef struct _GWin32AppInfoURLSchema GWin32AppInfoURLSchema;
typedef struct _GWin32AppInfoFileExtension GWin32AppInfoFileExtension;
typedef struct _GWin32AppInfoHandler GWin32AppInfoHandler;
typedef struct _GWin32AppInfoApplication GWin32AppInfoApplication;

typedef struct _GWin32AppInfoURLSchemaClass GWin32AppInfoURLSchemaClass;
typedef struct _GWin32AppInfoFileExtensionClass GWin32AppInfoFileExtensionClass;
typedef struct _GWin32AppInfoHandlerClass GWin32AppInfoHandlerClass;
typedef struct _GWin32AppInfoApplicationClass GWin32AppInfoApplicationClass;

struct _GWin32AppInfoURLSchemaClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoFileExtensionClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoHandlerClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoApplicationClass
{
  GObjectClass parent_class;
};

struct _GWin32AppInfoURLSchema {
  GObject parent_instance;

  /* url schema (stuff before ':') */
  gunichar2 *schema;

  /* url schema (stuff before ':'), in UTF-8 */
  gchar *schema_u8;

  /* url schema (stuff before ':'), in UTF-8, folded */
  gchar *schema_folded;

  /* Handler currently selected for this schema */
  GWin32AppInfoHandler *chosen_handler;

  /* Maps folded handler IDs -> to other handlers for this schema */
  GHashTable *handlers;
};

struct _GWin32AppInfoHandler {
  GObject parent_instance;

  /* Class name in HKCR */
  gunichar2 *handler_id;

  /* Handler registry key (HKCR\\handler_id). Can be used to watch this handler. */
  GWin32RegistryKey *key;

  /* Class name in HKCR, UTF-8, folded */
  gchar *handler_id_folded;

  /* shell/open/command default value for the class named by class_id */
  gunichar2 *handler_command;

  /* If handler_id class has no command, it might point to another class */
  gunichar2 *proxy_id;

  /* Proxy registry key (HKCR\\proxy_id). Can be used to watch handler's proxy. */
  GWin32RegistryKey *proxy_key;

  /* shell/open/command default value for the class named by proxy_id */
  gunichar2 *proxy_command;

  /* Executable of the program (for matching, in folded form; UTF-8) */
  gchar *executable_folded;

  /* Executable of the program (UTF-8) */
  gchar *executable;

  /* Pointer to a location within @executable */
  gchar *executable_basename;

  /* Icon of the application for this handler */
  GIcon *icon;

  /* The application that is linked to this handler. */
  GWin32AppInfoApplication *app;
};

struct _GWin32AppInfoFileExtension {
  GObject parent_instance;

  /* File extension (with leading '.') */
  gunichar2 *extension;

  /* File extension (with leading '.'), in UTF-8 */
  gchar *extension_u8;

  /* handler currently selected for this extension */
  GWin32AppInfoHandler *chosen_handler;

  /* Maps folded handler IDs -> to other handlers for this extension */
  GHashTable *handlers;

  /* Maps folded app exename -> to apps that support this extension.
   * ONLY for apps that are not reachable via handlers (i.e. apps from
   * the HKCR/Applications, which have no handlers). */
  GHashTable *other_apps;
};

struct _GWin32AppInfoApplication {
  GObject parent_instance;

  /* Canonical name (used for key names). Can be NULL. */
  gunichar2 *canonical_name;

  /* Canonical name (used for key names), in UTF-8. Can be NULL. */
  gchar *canonical_name_u8;

  /* Canonical name (used for key names), in UTF-8, folded. Can be NULL. */
  gchar *canonical_name_folded;

  /* Human-readable name in English. Can be NULL */
  gunichar2 *pretty_name;

  /* Human-readable name in English, UTF-8. Can be NULL */
  gchar *pretty_name_u8;

  /* Human-readable name in user's language. Can be NULL  */
  gunichar2 *localized_pretty_name;

  /* Human-readable name in user's language, UTF-8. Can be NULL  */
  gchar *localized_pretty_name_u8;

  /* Description, could be in user's language. Can be NULL */
  gunichar2 *description;

  /* Description, could be in user's language, UTF-8. Can be NULL */
  gchar *description_u8;

  /* shell/open/command for the application. Can be NULL (see executable). */
  gunichar2 *command;

  /* shell/open/command for the application. Can be NULL (see executable). */
  gchar *command_u8;

  /* Executable of the program (for matching, in folded form;
   * UTF-8). Never NULL. */
  gchar *executable_folded;

  /* Executable of the program (UTF-8). Never NULL. */
  gchar *executable;

  /* Pointer to a location within @executable */
  gchar *executable_basename;

  /* Explicitly supported URLs, hashmap from map-owned gchar ptr (schema,
   * UTF-8, folded) -> a GWin32AppInfoHandler
   * Schema can be used as a key in the urls hashmap.
   */
  GHashTable *supported_urls;

  /* Explicitly supported extensions, hashmap from map-owned gchar ptr
   * (.extension, UTF-8, folded) -> a GWin32AppInfoHandler
   * Extension can be used as a key in the extensions hashmap.
   */
  GHashTable *supported_exts;

  /* Icon of the application (remember, handler can have its own icon too) */
  GIcon *icon;

  /* Set to TRUE to prevent this app from appearing in lists of apps for
   * opening files. This will not prevent it from appearing in lists of apps
   * just for running, or lists of apps for opening exts/urls for which this
   * app reports explicit support.
   */
  gboolean no_open_with;

  /* Set to TRUE for applications from HKEY_CURRENT_USER.
   * Give them priority over applications from HKEY_LOCAL_MACHINE, when all
   * other things are equal.
   */
  gboolean user_specific;

  /* Set to TRUE for applications that are machine-wide defaults (i.e. default
   * browser) */
  gboolean default_app;
};

#define G_TYPE_WIN32_APPINFO_URL_SCHEMA           (g_win32_appinfo_url_schema_get_type ())
#define G_WIN32_APPINFO_URL_SCHEMA(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_URL_SCHEMA, GWin32AppInfoURLSchema))

#define G_TYPE_WIN32_APPINFO_FILE_EXTENSION       (g_win32_appinfo_file_extension_get_type ())
#define G_WIN32_APPINFO_FILE_EXTENSION(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_FILE_EXTENSION, GWin32AppInfoFileExtension))

#define G_TYPE_WIN32_APPINFO_HANDLER              (g_win32_appinfo_handler_get_type ())
#define G_WIN32_APPINFO_HANDLER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_HANDLER, GWin32AppInfoHandler))

#define G_TYPE_WIN32_APPINFO_APPLICATION          (g_win32_appinfo_application_get_type ())
#define G_WIN32_APPINFO_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_APPINFO_APPLICATION, GWin32AppInfoApplication))

GType g_win32_appinfo_url_schema_get_type (void) G_GNUC_CONST;
GType g_win32_appinfo_file_extension_get_type (void) G_GNUC_CONST;
GType g_win32_appinfo_handler_get_type (void) G_GNUC_CONST;
GType g_win32_appinfo_application_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GWin32AppInfoURLSchema, g_win32_appinfo_url_schema, G_TYPE_OBJECT)
G_DEFINE_TYPE (GWin32AppInfoFileExtension, g_win32_appinfo_file_extension, G_TYPE_OBJECT)
G_DEFINE_TYPE (GWin32AppInfoHandler, g_win32_appinfo_handler, G_TYPE_OBJECT)
G_DEFINE_TYPE (GWin32AppInfoApplication, g_win32_appinfo_application, G_TYPE_OBJECT)

static void
g_win32_appinfo_url_schema_dispose (GObject *object)
{
  GWin32AppInfoURLSchema *url = G_WIN32_APPINFO_URL_SCHEMA (object);

  g_clear_pointer (&url->schema, g_free);
  g_clear_pointer (&url->schema_u8, g_free);
  g_clear_pointer (&url->schema_folded, g_free);
  g_clear_object (&url->chosen_handler);
  g_clear_pointer (&url->handlers, g_hash_table_destroy);
  G_OBJECT_CLASS (g_win32_appinfo_url_schema_parent_class)->dispose (object);
}


static void
g_win32_appinfo_handler_dispose (GObject *object)
{
  GWin32AppInfoHandler *handler = G_WIN32_APPINFO_HANDLER (object);

  g_clear_pointer (&handler->handler_id, g_free);
  g_clear_pointer (&handler->handler_id_folded, g_free);
  g_clear_pointer (&handler->handler_command, g_free);
  g_clear_pointer (&handler->proxy_id, g_free);
  g_clear_pointer (&handler->proxy_command, g_free);
  g_clear_pointer (&handler->executable_folded, g_free);
  g_clear_pointer (&handler->executable, g_free);
  g_clear_object (&handler->key);
  g_clear_object (&handler->proxy_key);
  g_clear_object (&handler->icon);
  g_clear_object (&handler->app);
  G_OBJECT_CLASS (g_win32_appinfo_handler_parent_class)->dispose (object);
}

static void
g_win32_appinfo_file_extension_dispose (GObject *object)
{
  GWin32AppInfoFileExtension *ext = G_WIN32_APPINFO_FILE_EXTENSION (object);

  g_clear_pointer (&ext->extension, g_free);
  g_clear_pointer (&ext->extension_u8, g_free);
  g_clear_object (&ext->chosen_handler);
  g_clear_pointer (&ext->handlers, g_hash_table_destroy);
  g_clear_pointer (&ext->other_apps, g_hash_table_destroy);
  G_OBJECT_CLASS (g_win32_appinfo_file_extension_parent_class)->dispose (object);
}

static void
g_win32_appinfo_application_dispose (GObject *object)
{
  GWin32AppInfoApplication *app = G_WIN32_APPINFO_APPLICATION (object);

  g_clear_pointer (&app->canonical_name_u8, g_free);
  g_clear_pointer (&app->canonical_name_folded, g_free);
  g_clear_pointer (&app->canonical_name, g_free);
  g_clear_pointer (&app->pretty_name, g_free);
  g_clear_pointer (&app->localized_pretty_name, g_free);
  g_clear_pointer (&app->description, g_free);
  g_clear_pointer (&app->command, g_free);
  g_clear_pointer (&app->pretty_name_u8, g_free);
  g_clear_pointer (&app->localized_pretty_name_u8, g_free);
  g_clear_pointer (&app->description_u8, g_free);
  g_clear_pointer (&app->command_u8, g_free);
  g_clear_pointer (&app->executable_folded, g_free);
  g_clear_pointer (&app->executable, g_free);
  g_clear_pointer (&app->supported_urls, g_hash_table_destroy);
  g_clear_pointer (&app->supported_exts, g_hash_table_destroy);
  g_clear_object (&app->icon);
  G_OBJECT_CLASS (g_win32_appinfo_application_parent_class)->dispose (object);
}

static void
g_win32_appinfo_url_schema_class_init (GWin32AppInfoURLSchemaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_url_schema_dispose;
}

static void
g_win32_appinfo_file_extension_class_init (GWin32AppInfoFileExtensionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_file_extension_dispose;
}

static void
g_win32_appinfo_handler_class_init (GWin32AppInfoHandlerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_handler_dispose;
}

static void
g_win32_appinfo_application_class_init (GWin32AppInfoApplicationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_appinfo_application_dispose;
}

static void
g_win32_appinfo_url_schema_init (GWin32AppInfoURLSchema *self)
{
  self->handlers = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);
}

static void
g_win32_appinfo_file_extension_init (GWin32AppInfoFileExtension *self)
{
  self->handlers = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);
  self->other_apps = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            g_object_unref);
}

static void
g_win32_appinfo_handler_init (GWin32AppInfoHandler *self)
{
}

static void
g_win32_appinfo_application_init (GWin32AppInfoApplication *self)
{
  self->supported_urls = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_object_unref);
  self->supported_exts = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_object_unref);
}

G_LOCK_DEFINE_STATIC (gio_win32_appinfo);

/* Map of owned ".ext" (with '.', UTF-8, folded)
 * to GWin32AppInfoFileExtension ptr
 */
static GHashTable *extensions = NULL;

/* Map of owned "schema" (without ':', UTF-8, folded)
 * to GWin32AppInfoURLSchema ptr
 */
static GHashTable *urls = NULL;

/* Map of owned "appID" (UTF-8, folded) to
 * GWin32AppInfoApplication ptr
 */
static GHashTable *apps_by_id = NULL;

/* Map of owned "app.exe" (UTF-8, folded) to
 * GWin32AppInfoApplication ptr.
 * This map and its values are separate from apps_by_id. The fact that an app
 * with known ID has the same executable [base]name as an app in this map does
 * not mean that they are the same application.
 */
static GHashTable *apps_by_exe = NULL;

/* Map of owned "handler id" (UTF-8, folded)
 * to GWin32AppInfoHandler ptr
 */
static GHashTable *handlers = NULL;

/* Watch this whole subtree */
static GWin32RegistryKey *url_associations_key;

/* Watch this whole subtree */
static GWin32RegistryKey *file_exts_key;

/* Watch this whole subtree */
static GWin32RegistryKey *user_clients_key;

/* Watch this whole subtree */
static GWin32RegistryKey *system_clients_key;

/* Watch this key */
static GWin32RegistryKey *user_registered_apps_key;

/* Watch this key */
static GWin32RegistryKey *system_registered_apps_key;

/* Watch this whole subtree */
static GWin32RegistryKey *applications_key;

/* Watch this key */
static GWin32RegistryKey *classes_root_key;

static gunichar2 *
g_wcsdup (const gunichar2 *str, gssize str_size)
{
  if (str_size == -1)
    {
      str_size = wcslen (str) + 1;
      str_size *= sizeof (gunichar2);
    }
  return g_memdup (str, str_size);
}

#define URL_ASSOCIATIONS L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"
#define USER_CHOICE L"\\UserChoice"
#define OPEN_WITH_PROGIDS L"\\OpenWithProgids"
#define FILE_EXTS L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\"
#define HKCR L"HKEY_CLASSES_ROOT\\"
#define HKCU L"HKEY_CURRENT_USER\\"
#define HKLM L"HKEY_LOCAL_MACHINE\\"
#define SHELL_OPEN_COMMAND L"\\shell\\open\\command"
#define REG_PATH_MAX 256
#define REG_PATH_MAX_SIZE (REG_PATH_MAX * sizeof (gunichar2))

static gunichar2 *
read_resource_string (gunichar2 *res)
{
  gunichar2 *id_str;
  gunichar2 *id_str_end;
  gunichar2 *filename_str;
  unsigned long id;
  HMODULE resource_module;
  gunichar2 *buffer;
  int string_length;
  int buffer_length;

  if (res == NULL || res[0] != L'@')
    return res;

  id_str = wcsrchr (res, L'-');

  if (id_str == NULL || id_str[-1] != L',')
    return res;

  id_str += 1;

  id = wcstoul (id_str, &id_str_end, 10);

  if (id_str_end == id_str || id_str_end[0] != L'\0' || id == ULONG_MAX)
    return res;

  filename_str = &res[1];
  id_str[-2] = L'\0';

  resource_module = LoadLibraryExW (filename_str,
                                    0,
                                    LOAD_LIBRARY_AS_DATAFILE |
                                    LOAD_LIBRARY_AS_IMAGE_RESOURCE);

  g_free (res);

  if (resource_module == NULL)
    return NULL;

  buffer_length = 256;
  string_length = buffer_length - 1;

  while (TRUE)
    {
      buffer = g_malloc (buffer_length * sizeof (gunichar2));
      string_length = LoadStringW (resource_module, id, buffer, buffer_length);

      if (string_length != 0 && string_length == buffer_length - 1)
        {
          g_free (buffer);
          buffer_length *= 2;
        }
      else
        {
          if (string_length == 0)
            g_clear_pointer (&buffer, g_free);

          break;
        }
    }

  FreeLibrary (resource_module);

  if (buffer)
    {
      gunichar2 *result = g_wcsdup (buffer, -1);
      g_free (buffer);
      return result;
    }

  return NULL;
}

static void
read_handler_icon (GWin32RegistryKey  *proxy_key,
                   GWin32RegistryKey  *program_key,
                   GIcon             **icon_out)
{
  gint counter;
  GWin32RegistryKey *key;

  *icon_out = NULL;

  for (counter = 0; counter < 2; counter++)
    {
      GWin32RegistryKey *icon_key;

      if (counter == 0)
        key = proxy_key;
      else
        key = program_key;

      if (!key)
        continue;

      icon_key = g_win32_registry_key_get_child_w (key, L"DefaultIcon", NULL);

      if (icon_key != NULL)
        {
          GWin32RegistryValueType default_type;
          gchar *default_value;

          if (g_win32_registry_key_get_value (icon_key,
                                              TRUE,
                                              "",
                                              &default_type,
                                              (gpointer *) &default_value,
                                              NULL,
                                              NULL))
            {
              if (default_type == G_WIN32_REGISTRY_VALUE_STR ||
                  default_value[0] != '\0')
                *icon_out = g_themed_icon_new (default_value);

              g_clear_pointer (&default_value, g_free);
            }

          g_object_unref (icon_key);
        }

      if (*icon_out)
        break;
    }
}

static gboolean build_registry_path (gunichar2 *output, gsize output_size, ...) G_GNUC_NULL_TERMINATED;
static gboolean build_registry_pathv (gunichar2 *output, gsize output_size, va_list components);

static GWin32RegistryKey *_g_win32_registry_key_build_and_new_w (GError **error, ...) G_GNUC_NULL_TERMINATED;

/* output_size is in *bytes*, not gunichar2s! */
static gboolean
build_registry_path (gunichar2 *output, gsize output_size, ...)
{
  va_list ap;
  gboolean result;

  va_start (ap, output_size);

  result = build_registry_pathv (output, output_size, ap);

  va_end (ap);

  return result;
}

/* output_size is in *bytes*, not gunichar2s! */
static gboolean
build_registry_pathv (gunichar2 *output, gsize output_size, va_list components)
{
  va_list lentest;
  gunichar2 *p;
  gunichar2 *component;
  gsize length;

  if (output == NULL)
    return FALSE;

  G_VA_COPY (lentest, components);

  for (length = 0, component = va_arg (lentest, gunichar2 *);
       component != NULL;
       component = va_arg (lentest, gunichar2 *))
    {
      length += wcslen (component);
    }

  va_end (lentest);

  if ((length >= REG_PATH_MAX_SIZE) ||
      (length * sizeof (gunichar2) >= output_size))
    return FALSE;

  output[0] = L'\0';

  for (p = output, component = va_arg (components, gunichar2 *);
       component != NULL;
       component = va_arg (components, gunichar2 *))
    {
      length = wcslen (component);
      wcscat (p, component);
      p += length;
    }

  return TRUE;
}


static GWin32RegistryKey *
_g_win32_registry_key_build_and_new_w (GError **error, ...)
{
  va_list ap;
  gunichar2 key_path[REG_PATH_MAX_SIZE + 1];
  GWin32RegistryKey *key;

  va_start (ap, error);

  key = NULL;

  if (build_registry_pathv (key_path, sizeof (key_path), ap))
    key = g_win32_registry_key_new_w (key_path, error);

  va_end (ap);

  return key;
}


static gboolean
utf8_and_fold (const gunichar2  *str,
               gchar           **str_u8,
               gchar           **str_u8_folded)
{
  gchar *u8;
  gchar *folded;
  u8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);

  if (u8 == NULL)
    return FALSE;

  folded = g_utf8_casefold (u8, -1);

  if (folded == NULL)
    {
      g_free (u8);
      return FALSE;
    }

  if (str_u8)
    *str_u8 = u8;
  else
    g_free (u8);

  if (str_u8_folded)
    *str_u8_folded = folded;
  else
    g_free (folded);

  return TRUE;
}


static gboolean
follow_class_chain_to_handler (const gunichar2    *program_id,
                               gsize               program_id_size,
                               gunichar2         **program_command,
                               GWin32RegistryKey **program_key,
                               gunichar2         **proxy_id,
                               gunichar2         **proxy_command,
                               GWin32RegistryKey **proxy_key,
                               gchar             **program_id_u8,
                               gchar             **program_id_folded)
{
  GWin32RegistryKey *key;
  GWin32RegistryValueType val_type;
  gsize proxy_id_size;
  gboolean got_value;

  g_assert (program_id && program_command && proxy_id && proxy_command);

  *program_command = NULL;
  *proxy_id = NULL;
  *proxy_command = NULL;

  if (program_key)
    *program_key = NULL;

  if (proxy_key)
    *proxy_key = NULL;


  key = _g_win32_registry_key_build_and_new_w (NULL, HKCR, program_id,
                                               SHELL_OPEN_COMMAND, NULL);

  if (key != NULL)
    {
      got_value = g_win32_registry_key_get_value_w (key,
                                                    TRUE,
                                                    L"",
                                                    &val_type,
                                                    (void **) program_command,
                                                    NULL,
                                                    NULL);
      if (got_value && val_type == G_WIN32_REGISTRY_VALUE_STR)
        {
          if ((program_id_u8 != NULL || program_id_folded != NULL) &&
              !utf8_and_fold (program_id, program_id_u8, program_id_folded))
            {
              g_object_unref (key);
              g_free (program_command);

              return FALSE;
            }
          if (program_key == NULL)
            g_object_unref (key);
          else
            *program_key = key;

          return TRUE;
        }
      else if (got_value)
        g_clear_pointer (program_command, g_free);

      g_object_unref (key);
    }

  key = _g_win32_registry_key_build_and_new_w (NULL, HKCR, program_id, NULL);

  if (key == NULL)
    return FALSE;

  got_value = g_win32_registry_key_get_value_w (key,
                                                TRUE,
                                                L"",
                                                &val_type,
                                                (void **) proxy_id,
                                                &proxy_id_size,
                                                NULL);
  if (!got_value ||
      (val_type != G_WIN32_REGISTRY_VALUE_STR))
    {
      g_object_unref (key);
      g_clear_pointer (proxy_id, g_free);
      return FALSE;
    }

  if (proxy_key)
    *proxy_key = key;
  else
    g_object_unref (key);

  key = _g_win32_registry_key_build_and_new_w (NULL, HKCR, *proxy_id,
                                               SHELL_OPEN_COMMAND, NULL);

  if (key == NULL)
    {
      g_clear_pointer (proxy_id, g_free);
      if (proxy_key)
        g_clear_object (proxy_key);
      return FALSE;
    }

  got_value = g_win32_registry_key_get_value_w (key,
                                                TRUE,
                                                L"",
                                                &val_type,
                                                (void **) proxy_command,
                                                NULL,
                                                NULL);
  g_object_unref (key);

  if (!got_value ||
      val_type != G_WIN32_REGISTRY_VALUE_STR ||
      ((program_id_u8 != NULL || program_id_folded != NULL) &&
       !utf8_and_fold (program_id, program_id_u8, program_id_folded)))
    {
      g_clear_pointer (proxy_id, g_free);
      g_clear_pointer (proxy_command, g_free);
      if (proxy_key)
        g_clear_object (proxy_key);
      return FALSE;
    }

  return TRUE;
}


static void
extract_executable (gunichar2  *commandline,
                    gchar     **ex_out,
                    gchar     **ex_basename_out,
                    gchar     **ex_folded_out,
                    gchar     **ex_folded_basename_out)
{
  gchar *ex;
  gchar *ex_folded;
  gunichar2 *p;
  gboolean quoted;
  size_t len;
  size_t execlen;
  gunichar2 *exepart;
  gboolean found;

  quoted = FALSE;
  execlen = 0;
  found = FALSE;
  len = wcslen (commandline);
  p = commandline;

  while (p < &commandline[len])
    {
      switch (p[0])
        {
        case L'"':
          quoted = !quoted;
          break;
        case L' ':
          if (!quoted)
            {
              execlen = p - commandline;
              p = &commandline[len];
              found = TRUE;
            }
          break;
        default:
          break;
        }
      p += 1;
    }

  if (!found)
    execlen = len;

  exepart = g_wcsdup (commandline, (execlen + 1) * sizeof (gunichar2));
  exepart[execlen] = L'\0';

  p = &exepart[0];

  while (execlen > 0 && exepart[0] == L'"' && exepart[execlen - 1] == L'"')
    {
      p = &exepart[1];
      exepart[execlen - 1] = L'\0';
      execlen -= 2;
    }

  if (!utf8_and_fold (p, &ex, &ex_folded))
    /* Currently no code to handle this case. It shouldn't happen though... */
    g_assert_not_reached ();

  g_free (exepart);

  if (ex_out)
    {
      *ex_out = ex;

      if (ex_basename_out)
        {
          *ex_basename_out = &ex[strlen (ex) - 1];

          while (*ex_basename_out > ex)
            {
              if ((*ex_basename_out)[0] == '/' ||
                  (*ex_basename_out)[0] == '\\')
                {
                  *ex_basename_out += 1;
                  break;
                }

              *ex_basename_out -= 1;
            }
        }
    }
  else
    {
      g_free (ex);
    }

  if (ex_folded_out)
    {
      *ex_folded_out = ex_folded;

      if (ex_folded_basename_out)
        {
          *ex_folded_basename_out = &ex_folded[strlen (ex_folded) - 1];

          while (*ex_folded_basename_out > ex_folded)
            {
              if ((*ex_folded_basename_out)[0] == '/' ||
                  (*ex_folded_basename_out)[0] == '\\')
                {
                  *ex_folded_basename_out += 1;
                  break;
                }

              *ex_folded_basename_out -= 1;
            }
        }
    }
  else
    {
      g_free (ex_folded);
    }
}

static void
get_url_association (const gunichar2 *schema)
{
  GWin32AppInfoURLSchema *schema_rec;
  GWin32AppInfoHandler *handler_rec;
  GWin32AppInfoHandler *handler_rec_in_url;
  gchar *schema_u8;
  gchar *schema_folded;
  GWin32RegistryKey *user_choice;
  GWin32RegistryValueType val_type;
  gunichar2 *program_id;
  gsize program_id_size;
  gunichar2 *program_command;
  gunichar2 *proxy_id;
  gunichar2 *proxy_command;
  gchar *program_id_u8;
  gchar *program_id_folded;
  GWin32RegistryKey *program_key;
  GWin32RegistryKey *proxy_key;

  user_choice = _g_win32_registry_key_build_and_new_w (NULL, URL_ASSOCIATIONS,
                                                       schema, USER_CHOICE,
                                                       NULL);

  if (user_choice == NULL)
    return;

  if (!utf8_and_fold (schema, &schema_u8, &schema_folded))
    {
      g_object_unref (user_choice);
      return;
    }

  schema_rec = g_hash_table_lookup (urls, schema_folded);

  if (!g_win32_registry_key_get_value_w (user_choice,
                                         TRUE,
                                         L"Progid",
                                         &val_type,
                                         (void **) &program_id,
                                         &program_id_size,
                                         NULL))
    {
      g_free (schema_u8);
      g_free (schema_folded);
      g_object_unref (user_choice);
      return;
    }

  if (val_type != G_WIN32_REGISTRY_VALUE_STR)
    {
      g_free (schema_u8);
      g_free (schema_folded);
      g_free (program_id);
      g_object_unref (user_choice);
      return;
    }

  program_key = proxy_key = NULL;
  program_command = proxy_id = proxy_command = NULL;

  if (!follow_class_chain_to_handler (program_id,
                                      program_id_size,
                                      &program_command,
                                      &program_key,
                                      &proxy_id,
                                      &proxy_command,
                                      &proxy_key,
                                      &program_id_u8,
                                      &program_id_folded))
    {
      g_free (schema_u8);
      g_free (schema_folded);
      g_free (program_id);
      g_object_unref (user_choice);
      return;
    }

  handler_rec = g_hash_table_lookup (handlers, program_id_folded);

  if (handler_rec == NULL)
    {
      handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER, NULL);

      handler_rec->proxy_key = proxy_key;
      handler_rec->key = program_key;
      handler_rec->handler_id = g_wcsdup (program_id, program_id_size);
      handler_rec->handler_id_folded =
          g_strdup (program_id_folded);
      handler_rec->handler_command =
          program_command ? g_wcsdup (program_command, -1) : NULL;
      handler_rec->proxy_id = proxy_id ? g_wcsdup (proxy_id, -1) : NULL;
      handler_rec->proxy_command =
          proxy_command ? g_wcsdup (proxy_command, -1) : NULL;
      extract_executable (proxy_command ? proxy_command : program_command,
                          &handler_rec->executable,
                          &handler_rec->executable_basename,
                          &handler_rec->executable_folded,
                          NULL);
      read_handler_icon (proxy_key, program_key, &handler_rec->icon);
      g_hash_table_insert (handlers,
                           g_strdup (program_id_folded),
                           handler_rec);
    }
  else
    {
      g_clear_object (&program_key);
      g_clear_object (&proxy_key);
    }

  if (schema_rec == NULL)
    {
      schema_rec = g_object_new (G_TYPE_WIN32_APPINFO_URL_SCHEMA, NULL);
      schema_rec->schema = g_wcsdup (schema, -1);
      schema_rec->schema_u8 = g_strdup (schema_u8);
      schema_rec->schema_folded = g_strdup (schema_folded);
      schema_rec->chosen_handler = g_object_ref (handler_rec);
      g_hash_table_insert (urls, g_strdup (schema_folded), schema_rec);
    }

  if (schema_rec->chosen_handler == NULL)
    schema_rec->chosen_handler = g_object_ref (handler_rec);

  handler_rec_in_url = g_hash_table_lookup (schema_rec->handlers,
                                            program_id_folded);

  if (handler_rec_in_url == NULL && schema_rec->chosen_handler != handler_rec)
    g_hash_table_insert (schema_rec->handlers,
                         g_strdup (program_id_folded),
                         g_object_ref (handler_rec));

  g_free (schema_u8);
  g_free (schema_folded);
  g_free (program_id);
  g_free (program_id_u8);
  g_free (program_id_folded);
  g_free (program_command);
  g_free (proxy_id);
  g_free (proxy_command);
  g_object_unref (user_choice);
}

static void
get_file_ext (const gunichar2 *ext)
{
  GWin32AppInfoFileExtension *file_extn;
  gboolean file_ext_known;
  GWin32AppInfoHandler *handler_rec;
  GWin32AppInfoHandler *handler_rec_in_ext;
  gchar *ext_u8;
  gchar *ext_folded;
  GWin32RegistryKey *user_choice;
  GWin32RegistryKey *open_with_progids;
  GWin32RegistryValueType val_type;
  gsize program_id_size;
  gboolean found_handler;
  gunichar2 *program_id;
  gunichar2 *proxy_id;
  gchar *program_id_u8;
  gchar *program_id_folded;
  GWin32RegistryKey *program_key;
  GWin32RegistryKey *proxy_key;
  gunichar2 *program_command;
  gunichar2 *proxy_command;

  open_with_progids = _g_win32_registry_key_build_and_new_w (NULL, FILE_EXTS,
                                                             ext,
                                                             OPEN_WITH_PROGIDS,
                                                             NULL);

  user_choice = _g_win32_registry_key_build_and_new_w (NULL, FILE_EXTS, ext,
                                                       USER_CHOICE, NULL);

  if (user_choice == NULL && open_with_progids == NULL)
    return;

  if (!utf8_and_fold (ext, &ext_u8, &ext_folded))
    {
      g_clear_object (&user_choice);
      g_clear_object (&open_with_progids);
      return;
    }

  file_extn = NULL;
  file_ext_known = g_hash_table_lookup_extended (extensions,
                                                 ext_folded,
                                                 NULL,
                                                 (void **) &file_extn);

  if (!file_ext_known)
    file_extn = g_object_new (G_TYPE_WIN32_APPINFO_FILE_EXTENSION, NULL);

  found_handler = FALSE;

  if (user_choice != NULL)
    {
      if (g_win32_registry_key_get_value_w (user_choice,
                                            TRUE,
                                            L"Progid",
                                            &val_type,
                                            (void **) &program_id,
                                            &program_id_size,
                                            NULL))
        {
          program_key = proxy_key = NULL;

          if (val_type == G_WIN32_REGISTRY_VALUE_STR &&
              follow_class_chain_to_handler (program_id,
                                             program_id_size,
                                             &program_command,
                                             &program_key,
                                             &proxy_id,
                                             &proxy_command,
                                             &proxy_key,
                                             &program_id_u8,
                                             &program_id_folded))
            {
              handler_rec = g_hash_table_lookup (handlers,
                                                 program_id_folded);

              if (handler_rec == NULL)
                {
                  handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER,
                                              NULL);
                  handler_rec->proxy_key = proxy_key;
                  handler_rec->key = program_key;
                  handler_rec->handler_id =
                      g_wcsdup (program_id, program_id_size);
                  handler_rec->handler_id_folded =
                      g_strdup (program_id_folded);
                  handler_rec->handler_command =
                      program_command ? g_wcsdup (program_command, -1) : NULL;
                  handler_rec->proxy_id =
                      proxy_id ? g_wcsdup (proxy_id, -1) : NULL;
                  handler_rec->proxy_command =
                      proxy_command ? g_wcsdup (proxy_command, -1) : NULL;
                  extract_executable (proxy_command ? proxy_command : program_command,
                                      &handler_rec->executable,
                                      &handler_rec->executable_basename,
                                      &handler_rec->executable_folded,
                                      NULL);
                  read_handler_icon (proxy_key,
                                     program_key,
                                     &handler_rec->icon);
                  g_hash_table_insert (handlers,
                                       g_strdup (program_id_folded),
                                       handler_rec);
                }
              else
                {
                  g_clear_object (&program_key);
                  g_clear_object (&proxy_key);
                }

              found_handler = TRUE;

              handler_rec_in_ext = g_hash_table_lookup (file_extn->handlers,
                                                        program_id_folded);

              if (file_extn->chosen_handler == NULL)
                {
                  g_hash_table_insert (file_extn->handlers,
                                       g_strdup (program_id_folded),
                                       g_object_ref (handler_rec));
                }
              else if (handler_rec_in_ext == NULL)
                {
                  if (file_extn->chosen_handler->handler_id_folded &&
                      strcmp (file_extn->chosen_handler->handler_id_folded,
                              program_id_folded) != 0)
                    g_hash_table_insert (file_extn->handlers,
                                         g_strdup (program_id_folded),
                                         g_object_ref (handler_rec));
                }

              g_free (program_id_u8);
              g_free (program_id_folded);
              g_free (program_command);
              g_free (proxy_id);
              g_free (proxy_command);
            }

          g_free (program_id);
        }

      g_object_unref (user_choice);
    }

  if (open_with_progids != NULL)
    {
      GWin32RegistryValueIter iter;

      if (g_win32_registry_value_iter_init (&iter, open_with_progids, NULL))
        {
          gunichar2 *value_name;
          gunichar2 *value_data;
          gsize      value_name_len;
          gsize      value_data_size;
          GWin32RegistryValueType value_type;

          while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
            {
              gsize value_name_size;
              program_key = proxy_key = NULL;

              if ((!g_win32_registry_value_iter_get_value_type (&iter,
                                                                &value_type,
                                                                NULL)) ||
                  ((val_type != G_WIN32_REGISTRY_VALUE_STR) &&
                   (val_type != G_WIN32_REGISTRY_VALUE_EXPAND_STR)) ||
                  (!g_win32_registry_value_iter_get_name_w (&iter, &value_name,
                                                            &value_name_len,
                                                            NULL)) ||
                  (value_name_len <= 0) ||
                  (!g_win32_registry_value_iter_get_data_w (&iter, TRUE,
                                                            (void **) &value_data,
                                                            &value_data_size,
                                                            NULL)) ||
                  (value_data_size < sizeof (gunichar2)) ||
                  (value_data[0] == L'\0'))
                continue;

              value_name_size = sizeof (gunichar2) * (value_name_len + 1);

              if (!follow_class_chain_to_handler (value_name,
                                                  value_name_size,
                                                  &program_command,
                                                  &program_key,
                                                  &proxy_id,
                                                  &proxy_command,
                                                  &proxy_key,
                                                  &program_id_u8,
                                                  &program_id_folded))
                continue;

              handler_rec = g_hash_table_lookup (handlers,
                                                 program_id_folded);

              if (handler_rec == NULL)
                {
                  handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER, NULL);

                  handler_rec->proxy_key = proxy_key;
                  handler_rec->key = program_key;
                  handler_rec->handler_id =
                      g_wcsdup (value_name, value_name_size);
                  handler_rec->handler_id_folded =
                      g_strdup (program_id_folded);
                  handler_rec->handler_command =
                      program_command ? g_wcsdup (program_command, -1) : NULL;
                  handler_rec->proxy_id =
                      proxy_id ? g_wcsdup (proxy_id, -1) : NULL;
                  handler_rec->proxy_command =
                      proxy_command ? g_wcsdup (proxy_command, -1) : NULL;
                  extract_executable (proxy_command ? proxy_command : program_command,
                                      &handler_rec->executable,
                                      &handler_rec->executable_basename,
                                      &handler_rec->executable_folded,
                                      NULL);
                  read_handler_icon (proxy_key,
                                     program_key,
                                     &handler_rec->icon);
                  g_hash_table_insert (handlers,
                                       g_strdup (program_id_folded),
                                       handler_rec);
                }
              else
                {
                  g_clear_object (&program_key);
                  g_clear_object (&proxy_key);
                }

              found_handler = TRUE;

              handler_rec_in_ext = g_hash_table_lookup (file_extn->handlers,
                                                        program_id_folded);

              if (handler_rec_in_ext == NULL)
                {
                  if (file_extn->chosen_handler == NULL)
                    g_hash_table_insert (file_extn->handlers,
                                         g_strdup (program_id_folded),
                                         g_object_ref (handler_rec));
                  else if (file_extn->chosen_handler->handler_id_folded &&
                           strcmp (file_extn->chosen_handler->handler_id_folded,
                                   program_id_folded) != 0)
                    g_hash_table_insert (file_extn->handlers,
                                         g_strdup (program_id_folded),
                                         g_object_ref (handler_rec));
                }

              g_free (program_id_u8);
              g_free (program_id_folded);
              g_free (program_command);
              g_free (proxy_id);
              g_free (proxy_command);
            }

          g_win32_registry_value_iter_clear (&iter);
        }

      g_object_unref (open_with_progids);
    }

  if (!found_handler)
    {
      if (!file_ext_known)
        g_object_unref (file_extn);
    }
  else if (!file_ext_known)
    {
      file_extn->extension = g_wcsdup (ext, -1);
      file_extn->extension_u8 = g_strdup (ext_u8);
      g_hash_table_insert (extensions, g_strdup (ext_folded), file_extn);
    }

  g_free (ext_u8);
  g_free (ext_folded);
}

static void
collect_capable_apps_from_clients (GPtrArray *capable_apps,
                                   GPtrArray *priority_capable_apps,
                                   gboolean   user_registry)
{
  GWin32RegistryKey *clients;
  GWin32RegistrySubkeyIter clients_iter;

  gunichar2 *client_type_name;
  gsize client_type_name_len;


  if (user_registry)
    clients =
        g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Clients",
                                     NULL);
  else
    clients =
        g_win32_registry_key_new_w (L"HKEY_LOCAL_MACHINE\\Software\\Clients",
                                     NULL);

  if (clients == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&clients_iter, clients, NULL))
    {
      g_object_unref (clients);
      return;
    }

  while (g_win32_registry_subkey_iter_next (&clients_iter, TRUE, NULL))
    {
      GWin32RegistrySubkeyIter subkey_iter;
      GWin32RegistryKey *system_client_type;
      GWin32RegistryValueType default_type;
      gunichar2 *default_value;
      gunichar2 *client_name;
      gsize client_name_len;

      if (!g_win32_registry_subkey_iter_get_name_w (&clients_iter,
                                                    &client_type_name,
                                                    &client_type_name_len,
                                                    NULL))
        continue;

      system_client_type = g_win32_registry_key_get_child_w (clients,
                                                             client_type_name,
                                                             NULL);

      if (system_client_type == NULL)
        continue;

      if (g_win32_registry_key_get_value_w (system_client_type,
                                            TRUE,
                                            L"",
                                            &default_type,
                                            (gpointer *) &default_value,
                                            NULL,
                                            NULL))
        {
          if (default_type != G_WIN32_REGISTRY_VALUE_STR ||
              default_value[0] == L'\0')
            g_clear_pointer (&default_value, g_free);
        }

      if (!g_win32_registry_subkey_iter_init (&subkey_iter,
                                              system_client_type,
                                              NULL))
        {
          g_clear_pointer (&default_value, g_free);
          g_object_unref (system_client_type);
          continue;
        }

      while (g_win32_registry_subkey_iter_next (&subkey_iter, TRUE, NULL))
        {
          GWin32RegistryKey *system_client;
          GWin32RegistryKey *system_client_assoc;
          gboolean add;
          gunichar2 *keyname;

          if (!g_win32_registry_subkey_iter_get_name_w (&subkey_iter,
                                                        &client_name,
                                                        &client_name_len,
                                                        NULL))
            continue;

          system_client = g_win32_registry_key_get_child_w (system_client_type,
                                                            client_name,
                                                            NULL);

          if (system_client == NULL)
            continue;

          add = FALSE;

          system_client_assoc = g_win32_registry_key_get_child_w (system_client,
                                                                  L"Capabilities\\FileAssociations",
                                                                  NULL);

          if (system_client_assoc != NULL)
            {
              add = TRUE;
              g_object_unref (system_client_assoc);
            }
          else
            {
              system_client_assoc = g_win32_registry_key_get_child_w (system_client,
                                                                      L"Capabilities\\UrlAssociations",
                                                                      NULL);

              if (system_client_assoc != NULL)
                {
                  add = TRUE;
                  g_object_unref (system_client_assoc);
                }
            }

          if (add)
            {
              keyname = g_wcsdup (g_win32_registry_key_get_path_w (system_client), -1);

              if (default_value && wcscmp (default_value, client_name) == 0)
                g_ptr_array_add (priority_capable_apps, keyname);
              else
                g_ptr_array_add (capable_apps, keyname);
            }

          g_object_unref (system_client);
        }

      g_win32_registry_subkey_iter_clear (&subkey_iter);
      g_clear_pointer (&default_value, g_free);
      g_object_unref (system_client_type);
    }

  g_win32_registry_subkey_iter_clear (&clients_iter);
  g_object_unref (clients);
}

static void
collect_capable_apps_from_registered_apps (GPtrArray *capable_apps,
                                           gboolean   user_registry)
{
  GWin32RegistryValueIter iter;

  gunichar2 *value_data;
  gsize      value_data_size;
  GWin32RegistryValueType value_type;
  GWin32RegistryKey *registered_apps;

  if (user_registry)
    registered_apps =
        g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\RegisteredApplications",
                                    NULL);
  else
    registered_apps =
        g_win32_registry_key_new_w (L"HKEY_LOCAL_MACHINE\\Software\\RegisteredApplications",
                                    NULL);

  if (!registered_apps)
    return;

  if (!g_win32_registry_value_iter_init (&iter, registered_apps, NULL))
    {
      g_object_unref (registered_apps);
      return;
    }

  while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
    {
      gunichar2 possible_location[REG_PATH_MAX_SIZE + 1];
      GWin32RegistryKey *location = NULL;

      if ((!g_win32_registry_value_iter_get_value_type (&iter,
                                                        &value_type,
                                                        NULL)) ||
          (value_type != G_WIN32_REGISTRY_VALUE_STR) ||
          (!g_win32_registry_value_iter_get_data_w (&iter, TRUE,
                                                    (void **) &value_data,
                                                    &value_data_size,
                                                    NULL)) ||
          (value_data_size < sizeof (gunichar2)) ||
          (value_data[0] == L'\0'))
        continue;

      if (build_registry_path (possible_location, sizeof (possible_location),
                               HKCU, value_data, NULL))
        location = g_win32_registry_key_new_w (possible_location, NULL);

      if (location)
        {
          gunichar2 *p = wcsrchr (possible_location, L'\\');

          if (p)
            *p = L'\0';

          g_ptr_array_add (capable_apps, g_wcsdup (possible_location, -1));
          g_object_unref (location);
          continue;
        }

      if (!build_registry_path (possible_location, sizeof (possible_location),
                                user_registry ? HKCU : HKLM, value_data, NULL))
        continue;

      location = g_win32_registry_key_new_w (possible_location, NULL);

      if (location)
        {
          gunichar2 *p = wcsrchr (possible_location, L'\\');
          if (p)
            *p = L'\0';
          g_ptr_array_add (capable_apps, g_wcsdup (possible_location, -1));
          g_object_unref (location);
        }
    }

  g_win32_registry_value_iter_clear (&iter);
  g_object_unref (registered_apps);
}

static void
read_capable_app (gunichar2 *input_app_key_path, gboolean user_specific, gboolean default_app)
{
  GWin32AppInfoApplication *app;
  gunichar2 *app_key_path;
  gunichar2 *canonical_name;
  gchar *canonical_name_u8;
  gchar *canonical_name_folded;
  GWin32RegistryKey *appkey;
  gunichar2 *fallback_friendly_name;
  GWin32RegistryValueType vtype;
  gboolean success;
  gunichar2 *friendly_name;
  gunichar2 *description;
  gunichar2 *narrow_application_name;
  gunichar2 *icon_source;
  GWin32RegistryKey *capabilities;
  GWin32RegistryKey *default_icon_key;
  GWin32RegistryKey *shell_open_command_key;
  gunichar2 *shell_open_command;
  gchar *app_executable;
  gchar *app_executable_basename;
  gchar *app_executable_folded;
  gchar *app_executable_folded_basename;
  GWin32RegistryKey *associations;

  app_key_path = g_wcsdup (input_app_key_path, -1);

  canonical_name = wcsrchr (app_key_path, L'\\');

  if (canonical_name == NULL)
    {
      /* The key must have at least one '\\' */
      g_free (app_key_path);
      return;
    }

  canonical_name += 1;

  if (!utf8_and_fold (canonical_name, &canonical_name_u8, &canonical_name_folded))
    {
      g_free (app_key_path);
      return;
    }

  appkey = g_win32_registry_key_new_w (app_key_path, NULL);

  if (appkey == NULL)
    {
      g_free (canonical_name_u8);
      g_free (canonical_name_folded);
      g_free (app_key_path);
      return;
    }

  capabilities =
      g_win32_registry_key_get_child_w (appkey, L"Capabilities", NULL);

  if (capabilities == NULL)
    {
      /* Must have capabilities */
      g_free (canonical_name_u8);
      g_free (canonical_name_folded);
      g_free (app_key_path);
      return;
    }

  shell_open_command_key =
      g_win32_registry_key_get_child_w (appkey,
                                        L"shell\\open\\command",
                                        NULL);

  if (shell_open_command_key == NULL)
    {
      g_object_unref (capabilities);
      g_free (canonical_name_u8);
      g_free (canonical_name_folded);
      g_free (app_key_path);
      g_object_unref (appkey);
      return ;
    }

  shell_open_command = NULL;

  success = g_win32_registry_key_get_value_w (shell_open_command_key,
                                              TRUE,
                                              L"",
                                              &vtype,
                                              (gpointer *) &shell_open_command,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    {
      /* Must have a command */
      g_clear_pointer (&shell_open_command, g_free);
      g_object_unref (capabilities);
      g_free (canonical_name_u8);
      g_free (canonical_name_folded);
      g_free (app_key_path);
      g_object_unref (appkey);
      return;
    }

  extract_executable (shell_open_command,
                      &app_executable,
                      &app_executable_basename,
                      &app_executable_folded,
                      &app_executable_folded_basename);

  app = g_hash_table_lookup (apps_by_id, canonical_name_folded);

  if (app == NULL)
    {
      app = g_object_new (G_TYPE_WIN32_APPINFO_APPLICATION, NULL);

      app->canonical_name = g_wcsdup (canonical_name, -1);
      app->canonical_name_u8 = g_strdup (canonical_name_u8);
      app->canonical_name_folded =
          g_strdup (canonical_name_folded);

      app->command = g_wcsdup (shell_open_command, -1);
      app->command_u8 =
          g_utf16_to_utf8 (shell_open_command, -1, NULL, NULL, NULL);
      app->executable = g_strdup (app_executable);
      app->executable_basename =
          &app->executable[app_executable_basename - app_executable];
      app->executable_folded =
          g_strdup (app_executable_folded);

      app->no_open_with = FALSE;

      app->user_specific = user_specific;
      app->default_app = default_app;

      g_hash_table_insert (apps_by_id,
                           g_strdup (canonical_name_folded),
                           app);
    }

  fallback_friendly_name = NULL;
  success = g_win32_registry_key_get_value_w (appkey,
                                              TRUE,
                                              L"",
                                              &vtype,
                                              (void **) &fallback_friendly_name,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&fallback_friendly_name, g_free);

  if (fallback_friendly_name && app->pretty_name == NULL)
    {
      app->pretty_name = g_wcsdup (fallback_friendly_name, -1);
      g_clear_pointer (&app->pretty_name_u8, g_free);
      app->pretty_name_u8 = g_utf16_to_utf8 (fallback_friendly_name,
                                             -1,
                                             NULL,
                                             NULL,
                                             NULL);
    }

  friendly_name = NULL;
  success = g_win32_registry_key_get_value_w (capabilities,
                                              TRUE,
                                              L"LocalizedString",
                                              &vtype,
                                              (void **) &friendly_name,
                                              NULL,
                                              NULL);

  if (success && (vtype != G_WIN32_REGISTRY_VALUE_STR || friendly_name[0] != L'@'))
    g_clear_pointer (&friendly_name, g_free);

  friendly_name = read_resource_string (friendly_name);

  if (friendly_name && app->localized_pretty_name == NULL)
    {
      app->localized_pretty_name = g_wcsdup (friendly_name, -1);
      g_clear_pointer (&app->localized_pretty_name_u8, g_free);
      app->localized_pretty_name_u8 = g_utf16_to_utf8 (friendly_name,
                                                       -1,
                                                       NULL,
                                                       NULL,
                                                       NULL);
    }

  description = NULL;
  success = g_win32_registry_key_get_value_w (capabilities,
                                              TRUE,
                                              L"ApplicationDescription",
                                              &vtype,
                                              (void **) &description,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&description, g_free);

  description = read_resource_string (description);

  if (description && app->description == NULL)
    {
      app->description = g_wcsdup (description, -1);
      g_clear_pointer (&app->description_u8, g_free);
      app->description_u8 = g_utf16_to_utf8 (description, -1, NULL, NULL, NULL);
    }

  default_icon_key = g_win32_registry_key_get_child_w (appkey,
                                                       L"DefaultIcon",
                                                       NULL);

  icon_source = NULL;

  if (default_icon_key != NULL)
    {
      success = g_win32_registry_key_get_value_w (default_icon_key,
                                                  TRUE,
                                                  L"",
                                                  &vtype,
                                                  (void **) &icon_source,
                                                  NULL,
                                                  NULL);

      if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
        g_clear_pointer (&icon_source, g_free);

      g_object_unref (default_icon_key);
    }

  if (icon_source == NULL)
    {
      success = g_win32_registry_key_get_value_w (capabilities,
                                                  TRUE,
                                                  L"ApplicationIcon",
                                                  &vtype,
                                                  (void **) &icon_source,
                                                  NULL,
                                                  NULL);

      if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
        g_clear_pointer (&icon_source, g_free);
    }

  if (icon_source && app->icon == NULL)
    {
      gchar *name = g_utf16_to_utf8 (icon_source, -1, NULL, NULL, NULL);
      app->icon = g_themed_icon_new (name);
      g_free (name);
    }

  narrow_application_name = NULL;
  success = g_win32_registry_key_get_value_w (capabilities,
                                              TRUE,
                                              L"ApplicationName",
                                              &vtype,
                                              (void **) &narrow_application_name,
                                              NULL,
                                              NULL);

  if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
    g_clear_pointer (&narrow_application_name, g_free);

  narrow_application_name = read_resource_string (narrow_application_name);

  /* TODO: do something with the narrow name. Maybe make a kind of sub-app?
   * Narrow name is a more precise name of the application in given context.
   * I.e. Thunderbird's name is "Thunderbird", whereas its narrow name is
   * "Thunderbird (news)" when registering it as a news client.
   * Maybe we should consider applications with different narrow names as
   * different applications altogether?
   */

  associations = g_win32_registry_key_get_child_w (capabilities,
                                                   L"FileAssociations",
                                                   NULL);

  if (associations != NULL)
    {
      GWin32RegistryValueIter iter;

      if (g_win32_registry_value_iter_init (&iter, associations, NULL))
        {
          gunichar2 *file_extension;
          gunichar2 *extension_handler;
          gsize      file_extension_len;
          gsize      extension_handler_size;
          GWin32RegistryValueType value_type;

          while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
            {
              GWin32AppInfoHandler *handler_rec;
              GWin32AppInfoHandler *handler_rec_in_ext;
              GWin32AppInfoFileExtension *ext;
              gunichar2 *program_command;
              gunichar2 *proxy_id;
              gunichar2 *proxy_command;
              GWin32RegistryKey *program_key;
              GWin32RegistryKey *proxy_key;
              gchar *program_id_u8;
              gchar *program_id_folded;
              gchar *file_extension_u8;
              gchar *file_extension_folded;

              if ((!g_win32_registry_value_iter_get_value_type (&iter,
                                                                &value_type,
                                                                NULL)) ||
                  (value_type != G_WIN32_REGISTRY_VALUE_STR) ||
                  (!g_win32_registry_value_iter_get_name_w (&iter,
                                                            &file_extension,
                                                            &file_extension_len,
                                                            NULL)) ||
                  (file_extension_len <= 0) ||
                  (file_extension[0] != L'.') ||
                  (!g_win32_registry_value_iter_get_data_w (&iter, TRUE,
                                                            (void **) &extension_handler,
                                                            &extension_handler_size,
                                                            NULL)) ||
                  (extension_handler_size < sizeof (gunichar2)) ||
                  (extension_handler[0] == L'\0'))
                continue;

              if (!follow_class_chain_to_handler (extension_handler,
                                                  extension_handler_size,
                                                  &program_command,
                                                  &program_key,
                                                  &proxy_id,
                                                  &proxy_command,
                                                  &proxy_key,
                                                  &program_id_u8,
                                                  &program_id_folded))
                continue;

              handler_rec = g_hash_table_lookup (handlers,
                                                 program_id_folded);

              if (handler_rec == NULL)
                {
                  handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER, NULL);

                  handler_rec->proxy_key = proxy_key;
                  handler_rec->key = program_key;
                  handler_rec->handler_id =
                      g_wcsdup (extension_handler,extension_handler_size);
                  handler_rec->handler_id_folded =
                      g_strdup (program_id_folded);
                  handler_rec->handler_command =
                      program_command ? g_wcsdup (program_command, -1) : NULL;
                  handler_rec->proxy_id =
                      proxy_id ? g_wcsdup (proxy_id, -1) : NULL;
                  handler_rec->proxy_command =
                      proxy_command ? g_wcsdup (proxy_command, -1) : NULL;
                  extract_executable (proxy_command ? proxy_command : program_command,
                                      &handler_rec->executable,
                                      &handler_rec->executable_basename,
                                      &handler_rec->executable_folded,
                                      NULL);
                  read_handler_icon (proxy_key,
                                     program_key,
                                     &handler_rec->icon);
                  g_hash_table_insert (handlers,
                                       g_strdup (program_id_folded),
                                       handler_rec);
                }
              else
                {
                  g_clear_object (&program_key);
                  g_clear_object (&proxy_key);
                }

                if (utf8_and_fold (file_extension,
                                   &file_extension_u8,
                                   &file_extension_folded))
                  {
                    ext = g_hash_table_lookup (extensions,
                                               file_extension_folded);

                    if (ext == NULL)
                      {
                        ext = g_object_new (G_TYPE_WIN32_APPINFO_FILE_EXTENSION, NULL);

                        ext->extension = g_wcsdup (file_extension, -1);
                        ext->extension_u8 = g_strdup (file_extension_u8);
                        g_hash_table_insert (extensions, g_strdup (file_extension_folded), ext);
                      }

                    handler_rec_in_ext =
                        g_hash_table_lookup (ext->handlers,
                                             program_id_folded);

                    if (handler_rec_in_ext == NULL)
                      {
                        if (ext->chosen_handler == NULL)
                          g_hash_table_insert (ext->handlers,
                                               g_strdup (program_id_folded),
                                               g_object_ref (handler_rec));
                        else if (ext->chosen_handler->handler_id_folded &&
                                 strcmp (ext->chosen_handler->handler_id_folded,
                                         program_id_folded) != 0)
                          g_hash_table_insert (ext->handlers,
                                               g_strdup (program_id_folded),
                                               g_object_ref (handler_rec));
                      }

                    handler_rec_in_ext =
                        g_hash_table_lookup (app->supported_exts,
                                             file_extension_folded);

                    if (handler_rec_in_ext == NULL)
                      g_hash_table_insert (app->supported_exts,
                                           g_strdup (file_extension_folded),
                                           g_object_ref (handler_rec));

                    g_free (file_extension_u8);
                    g_free (file_extension_folded);
                  }

              g_free (program_id_u8);
              g_free (program_id_folded);
              g_free (program_command);
              g_free (proxy_id);
              g_free (proxy_command);
            }

          g_win32_registry_value_iter_clear (&iter);
        }

      g_object_unref (associations);
    }

  associations = g_win32_registry_key_get_child_w (capabilities, L"URLAssociations", NULL);

  if (associations != NULL)
    {
      GWin32RegistryValueIter iter;

      if (g_win32_registry_value_iter_init (&iter, associations, NULL))
        {
          gunichar2 *url_schema;
          gunichar2 *schema_handler;
          gsize      url_schema_len;
          gsize      schema_handler_size;
          GWin32RegistryValueType value_type;

          while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
            {
              GWin32AppInfoHandler *handler_rec;
              GWin32AppInfoHandler *handler_rec_in_url;
              GWin32AppInfoURLSchema *schema;
              gunichar2 *program_command;
              gunichar2 *proxy_id;
              gunichar2 *proxy_command;
              GWin32RegistryKey *program_key;
              GWin32RegistryKey *proxy_key;
              gchar *program_id_u8;
              gchar *program_id_folded;
              gchar *schema_u8;
              gchar *schema_folded;

              if ((!g_win32_registry_value_iter_get_value_type (&iter,
                                                                &value_type,
                                                                NULL)) ||
                  ((value_type != G_WIN32_REGISTRY_VALUE_STR) &&
                   (value_type != G_WIN32_REGISTRY_VALUE_EXPAND_STR)) ||
                  (!g_win32_registry_value_iter_get_name_w (&iter,
                                                            &url_schema,
                                                            &url_schema_len,
                                                            NULL)) ||
                  (url_schema_len <= 0) ||
                  (url_schema[0] == L'\0') ||
                  (!g_win32_registry_value_iter_get_data_w (&iter, TRUE,
                                                            (void **) &schema_handler,
                                                            &schema_handler_size,
                                                            NULL)) ||
                  (schema_handler_size < sizeof (gunichar2)) ||
                  (schema_handler[0] == L'\0'))
                continue;

              if (!follow_class_chain_to_handler (schema_handler,
                                                  schema_handler_size,
                                                  &program_command,
                                                  &program_key,
                                                  &proxy_id,
                                                  &proxy_command,
                                                  &proxy_key,
                                                  &program_id_u8,
                                                  &program_id_folded))
                continue;

              
              handler_rec = g_hash_table_lookup (handlers, program_id_folded);

              if (handler_rec == NULL)
                {
                  handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER, NULL);

                  handler_rec->proxy_key = proxy_key;
                  handler_rec->key = program_key;
                  handler_rec->handler_id =
                      g_wcsdup (schema_handler, schema_handler_size);
                  handler_rec->handler_id_folded =
                      g_strdup (program_id_folded);
                  handler_rec->handler_command = program_command ?
                      g_wcsdup (program_command, -1) : NULL;
                  handler_rec->proxy_id =
                      proxy_id ? g_wcsdup (proxy_id, -1) : NULL;
                  handler_rec->proxy_command =
                      proxy_command ? g_wcsdup (proxy_command, -1) : NULL;
                  extract_executable (proxy_command ? proxy_command : program_command,
                                      &handler_rec->executable,
                                      &handler_rec->executable_basename,
                                      &handler_rec->executable_folded,
                                      NULL);
                  read_handler_icon (proxy_key,
                                     program_key,
                                     &handler_rec->icon);
                  g_hash_table_insert (handlers,
                                       g_strdup (program_id_folded),
                                       handler_rec);
                }
              else
                {
                  g_clear_object (&program_key);
                  g_clear_object (&proxy_key);
                }

                if (utf8_and_fold (url_schema,
                                   &schema_u8,
                                   &schema_folded))
                  {
                    schema = g_hash_table_lookup (urls,
                                                  schema_folded);

                    if (schema == NULL)
                      {
                        schema = g_object_new (G_TYPE_WIN32_APPINFO_URL_SCHEMA, NULL);

                        schema->schema = g_wcsdup (url_schema, -1);
                        schema->schema_u8 = g_strdup (schema_u8);
                        schema->schema_folded =
                            g_strdup (schema_folded);
                        g_hash_table_insert (urls,
                                             g_strdup (schema_folded),
                                             schema);
                      }

                    handler_rec_in_url =
                        g_hash_table_lookup (schema->handlers,
                                             program_id_folded);

                    if (handler_rec_in_url == NULL)
                      g_hash_table_insert (schema->handlers,
                                           g_strdup (program_id_folded),
                                           g_object_ref (handler_rec));

                    handler_rec_in_url =
                        g_hash_table_lookup (app->supported_urls,
                                             schema_folded);

                    if (handler_rec_in_url == NULL)
                      g_hash_table_insert (app->supported_urls,
                                           g_strdup (schema_folded),
                                           g_object_ref (handler_rec));

                    g_free (schema_u8);
                    g_free (schema_folded);
                  }

              g_free (program_id_u8);
              g_free (program_id_folded);
              g_free (program_command);
              g_free (proxy_id);
              g_free (proxy_command);
            }

          g_win32_registry_value_iter_clear (&iter);
        }

      g_object_unref (associations);
    }

  g_clear_pointer (&app_executable, g_free);
  g_clear_pointer (&app_executable_folded, g_free);
  g_clear_pointer (&fallback_friendly_name, g_free);
  g_clear_pointer (&description, g_free);
  g_clear_pointer (&icon_source, g_free);
  g_clear_pointer (&narrow_application_name, g_free);
  g_clear_pointer (&shell_open_command, g_free);

  g_object_unref (appkey);
  g_object_unref (shell_open_command_key);
  g_object_unref (capabilities);
  g_free (canonical_name_u8);
  g_free (canonical_name_folded);
  g_free (app_key_path);
}

static void
read_urls (GWin32RegistryKey *url_associations)
{
  GWin32RegistrySubkeyIter url_iter;
  gunichar2 *url_schema;
  gsize url_schema_len;

  if (url_associations == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&url_iter, url_associations, NULL))
    return;

  while (g_win32_registry_subkey_iter_next (&url_iter, TRUE, NULL))
    {
      if (!g_win32_registry_subkey_iter_get_name_w (&url_iter,
                                                    &url_schema,
                                                    &url_schema_len,
                                                    NULL))
        continue;

      get_url_association (url_schema);
    }

  g_win32_registry_subkey_iter_clear (&url_iter);
}

static void
read_exeapps (void)
{
  GWin32RegistryKey *applications_key;
  GWin32RegistrySubkeyIter app_iter;
  gunichar2 *app_exe_basename;
  gsize app_exe_basename_len;

  applications_key =
      g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT\\Applications", NULL);

  if (applications_key == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&app_iter, applications_key, NULL))
    {
      g_object_unref (applications_key);
      return;
    }

  while (g_win32_registry_subkey_iter_next (&app_iter, TRUE, NULL))
    {
      GWin32RegistryKey *incapable_app;
      gunichar2 *friendly_app_name;
      gboolean success;
      gboolean no_open_with;
      GWin32RegistryValueType vtype;
      GWin32RegistryKey *default_icon_key;
      gunichar2 *icon_source;
      GIcon *icon = NULL;
      gchar *appexe;
      gchar *appexe_basename;
      gchar *appexe_folded;
      gchar *appexe_folded_basename;
      GWin32AppInfoApplication *app;
      GWin32RegistryKey *shell_open_command_key;
      gunichar2 *shell_open_command;
      GWin32RegistryKey *supported_key;

      if (!g_win32_registry_subkey_iter_get_name_w (&app_iter,
                                                    &app_exe_basename,
                                                    &app_exe_basename_len,
                                                    NULL))
        continue;

      incapable_app =
          g_win32_registry_key_get_child_w (applications_key,
                                            app_exe_basename,
                                            NULL);

      if (incapable_app == NULL)
        continue;

      extract_executable (app_exe_basename,
                          &appexe,
                          &appexe_basename,
                          &appexe_folded,
                          &appexe_folded_basename);

      shell_open_command_key =
          g_win32_registry_key_get_child_w (incapable_app,
                                            L"shell\\open\\command",
                                            NULL);

      shell_open_command = NULL;

      if (shell_open_command_key != NULL)
        {
          success = g_win32_registry_key_get_value_w (shell_open_command_key,
                                                      TRUE,
                                                      L"",
                                                      &vtype,
                                                      (gpointer *) &shell_open_command,
                                                      NULL,
                                                      NULL);

          if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
            {
              g_clear_pointer (&shell_open_command, g_free);
            }

          g_object_unref (shell_open_command_key);
        }

      friendly_app_name = NULL;
      success = g_win32_registry_key_get_value_w (incapable_app,
                                                  TRUE,
                                                  L"FriendlyAppName",
                                                  &vtype,
                                                  (void **) &friendly_app_name,
                                                  NULL,
                                                  NULL);

      if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
        g_clear_pointer (&friendly_app_name, g_free);

      friendly_app_name = read_resource_string (friendly_app_name);

      no_open_with = FALSE;
      success = g_win32_registry_key_get_value_w (incapable_app,
                                                  TRUE,
                                                  L"NoOpenWith",
                                                  &vtype,
                                                  NULL,
                                                  NULL,
                                                  NULL);

      if (success)
        no_open_with = TRUE;

      default_icon_key =
          g_win32_registry_key_get_child_w (incapable_app,
                                            L"DefaultIcon",
                                            NULL);

      icon_source = NULL;

      if (default_icon_key != NULL)
      {
        success =
            g_win32_registry_key_get_value_w (default_icon_key,
                                              TRUE,
                                              L"",
                                              &vtype,
                                              (void **) &icon_source,
                                              NULL,
                                              NULL);

        if (success && vtype != G_WIN32_REGISTRY_VALUE_STR)
          g_clear_pointer (&icon_source, g_free);

        g_object_unref (default_icon_key);
      }

      if (icon_source)
        {
          gchar *name = g_utf16_to_utf8 (icon_source, -1, NULL, NULL, NULL);
          icon = g_themed_icon_new (name);
          g_free (name);
        }

      app = g_hash_table_lookup (apps_by_exe, appexe_folded_basename);

      if (app == NULL)
        {
          app = g_object_new (G_TYPE_WIN32_APPINFO_APPLICATION, NULL);

          app->command =
              shell_open_command ? g_wcsdup (shell_open_command, -1) : NULL;

          if (shell_open_command)
            app->command_u8 = g_utf16_to_utf8 (shell_open_command, -1, NULL, NULL, NULL);

          app->executable = g_strdup (appexe);
          app->executable_basename = &app->executable[appexe_basename - appexe];
          app->executable_folded = g_strdup (appexe_folded);

          app->no_open_with = no_open_with;

          if (friendly_app_name)
            {
              app->localized_pretty_name = g_wcsdup (friendly_app_name, -1);
              g_clear_pointer (&app->localized_pretty_name_u8, g_free);
              app->localized_pretty_name_u8 =
                  g_utf16_to_utf8 (friendly_app_name, -1, NULL, NULL, NULL);
            }

          if (icon)
            app->icon = g_object_ref (icon);

          app->user_specific = FALSE;
          app->default_app = FALSE;

          g_hash_table_insert (apps_by_exe,
                               g_strdup (appexe_folded_basename),
                               app);
        }

      supported_key =
          g_win32_registry_key_get_child_w (incapable_app,
                                            L"SupportedTypes",
                                            NULL);

      if (supported_key)
        {
          GWin32RegistryValueIter sup_iter;
          if (g_win32_registry_value_iter_init (&sup_iter, supported_key, NULL))
            {
              gunichar2 *ext_name;
              gsize      ext_name_len;

              while (g_win32_registry_value_iter_next (&sup_iter, TRUE, NULL))
                {
                  gchar *ext_u8;
                  gchar *ext_folded;
                  GWin32AppInfoFileExtension *file_extn;
                  gboolean file_ext_known;

                  if ((!g_win32_registry_value_iter_get_name_w (&sup_iter,
                                                                &ext_name,
                                                                &ext_name_len,
                                                                NULL)) ||
                      (ext_name_len <= 0) ||
                      (ext_name[0] != L'.') ||
                      (!utf8_and_fold (ext_name,
                                       &ext_u8,
                                       &ext_folded)))
                    continue;

                  file_extn = NULL;
                  file_ext_known =
                      g_hash_table_lookup_extended (extensions,
                                                    ext_folded,
                                                    NULL,
                                                    (void **) &file_extn);

                  if (!file_ext_known)
                    {
                      file_extn =
                          g_object_new (G_TYPE_WIN32_APPINFO_FILE_EXTENSION, NULL);
                      file_extn->extension = g_wcsdup (ext_name, -1);
                      file_extn->extension_u8 = g_strdup (ext_u8);
                      g_hash_table_insert (extensions,
                                           g_strdup (ext_folded),
                                           file_extn);
                    }

                  g_hash_table_insert (file_extn->other_apps,
                                       g_strdup (appexe_folded),
                                       g_object_ref (app));

                  g_free (ext_u8);
                  g_free (ext_folded);
                }

              g_win32_registry_value_iter_clear (&sup_iter);
            }

          g_object_unref (supported_key);
        }


      g_free (appexe);
      g_free (appexe_folded);
      g_free (shell_open_command);
      g_free (friendly_app_name);
      g_free (icon_source);

      g_clear_object (&icon);
      g_clear_object (&incapable_app);
    }

  g_win32_registry_subkey_iter_clear (&app_iter);
  g_object_unref (applications_key);
}


static void
read_exts (GWin32RegistryKey *file_exts)
{
  GWin32RegistrySubkeyIter ext_iter;
  gunichar2 *file_extension;
  gsize file_extension_len;

  if (file_exts == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&ext_iter, file_exts, NULL))
    return;

  while (g_win32_registry_subkey_iter_next (&ext_iter, TRUE, NULL))
    {
      if (!g_win32_registry_subkey_iter_get_name_w (&ext_iter,
                                                    &file_extension,
                                                    &file_extension_len,
                                                    NULL))
        continue;

      get_file_ext (file_extension);
    }

  g_win32_registry_subkey_iter_clear (&ext_iter);
}

static void
read_class_extension (GWin32RegistryKey *classes_root,
                      gunichar2         *class_name,
                      gsize              class_name_len)
{
  gchar *ext_u8;
  gchar *ext_folded;
  GWin32AppInfoFileExtension *file_extn;
  GWin32AppInfoHandler *handler_rec;
  GWin32AppInfoHandler *handler_rec_in_ext;
  GWin32RegistryKey *class_key;
  gsize program_id_size;
  gunichar2 *program_id;
  gunichar2 *proxy_id;
  GWin32RegistryKey *program_key;
  GWin32RegistryKey *proxy_key;
  gunichar2 *program_command;
  gunichar2 *proxy_command;

  class_key = g_win32_registry_key_get_child_w (classes_root, class_name, NULL);

  if (class_key == NULL)
    return;

  program_id = class_name;
  program_id_size = (class_name_len + 1) * sizeof (gunichar2);
  program_key = proxy_key = NULL;
  program_command = proxy_command = NULL;

  if (!follow_class_chain_to_handler (program_id,
                                      program_id_size,
                                      &program_command,
                                      &program_key,
                                      &proxy_id,
                                      &proxy_command,
                                      &proxy_key,
                                      &ext_u8,
                                      &ext_folded))
    {
      g_object_unref (class_key);
      return;
    }


  file_extn = g_hash_table_lookup (extensions, ext_folded);
  handler_rec = g_hash_table_lookup (handlers, ext_folded);

  if (file_extn == NULL)
    {
      file_extn = g_object_new (G_TYPE_WIN32_APPINFO_FILE_EXTENSION, NULL);
      file_extn->extension = g_wcsdup (class_name, -1);
      file_extn->extension_u8 = g_strdup (ext_u8);
      g_hash_table_insert (extensions, g_strdup (ext_folded), file_extn);
    }

  if (handler_rec == NULL)
    {
      handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER, NULL);

      handler_rec->proxy_key = proxy_key;
      handler_rec->key = program_key;
      handler_rec->handler_id = g_wcsdup (program_id, program_id_size);
      handler_rec->handler_id_folded = g_strdup (ext_folded);
      handler_rec->handler_command =
          program_command ? g_wcsdup (program_command, -1) : NULL;
      handler_rec->proxy_id = proxy_id ? g_wcsdup (proxy_id, -1) : NULL;
      handler_rec->proxy_command =
          proxy_command ? g_wcsdup (proxy_command, -1) : NULL;
      extract_executable (proxy_command ? proxy_command : program_command,
                          &handler_rec->executable,
                          &handler_rec->executable_basename,
                          &handler_rec->executable_folded,
                          NULL);
      read_handler_icon (proxy_key, program_key, &handler_rec->icon);
      g_hash_table_insert (handlers,
                           g_strdup (ext_folded),
                           handler_rec);
    }
  else
    {
      g_clear_object (&program_key);
      g_clear_object (&proxy_key);
    }

  handler_rec_in_ext = g_hash_table_lookup (file_extn->handlers,
                                            ext_folded);

  if (file_extn->chosen_handler == NULL)
    g_hash_table_insert (file_extn->handlers,
                         g_strdup (ext_folded),
                         g_object_ref (handler_rec));
  else if (handler_rec_in_ext == NULL)
    {
      if (file_extn->chosen_handler->handler_id_folded &&
          strcmp (file_extn->chosen_handler->handler_id_folded,
                  ext_folded) != 0)
        g_hash_table_insert (file_extn->handlers,
                             g_strdup (ext_folded),
                             g_object_ref (handler_rec));
    }

  g_free (program_command);
  g_free (proxy_id);
  g_free (proxy_command);
  g_free (ext_u8);
  g_free (ext_folded);
  g_object_unref (class_key);
}

static void
read_class_url (GWin32RegistryKey *classes_root,
                gunichar2         *class_name,
                gsize              class_name_len)
{
  GWin32RegistryKey *class_key;
  gboolean success;
  GWin32RegistryValueType vtype;
  GWin32AppInfoURLSchema *schema_rec;
  GWin32AppInfoHandler *handler_rec;
  GWin32AppInfoHandler *handler_rec_in_url;
  gunichar2 *program_id;
  gsize program_id_size;
  gunichar2 *program_command;
  gunichar2 *proxy_id;
  gunichar2 *proxy_command;
  gchar *program_id_u8;
  gchar *program_id_folded;
  GWin32RegistryKey *program_key;
  GWin32RegistryKey *proxy_key;

  class_key = g_win32_registry_key_get_child_w (classes_root, class_name, NULL);

  if (class_key == NULL)
    return;

  success = g_win32_registry_key_get_value_w (class_key,
                                              TRUE,
                                              L"URL Protocol",
                                              &vtype,
                                              NULL,
                                              NULL,
                                              NULL);

  if (!success ||
      vtype != G_WIN32_REGISTRY_VALUE_STR)
    {
      g_object_unref (class_key);
      return;
    }

  program_id = class_name;
  program_id_size = (class_name_len + 1) * sizeof (gunichar2);
  proxy_key = program_key = NULL;
  program_command = proxy_id = proxy_command = NULL;

  if (!follow_class_chain_to_handler (program_id,
                                      program_id_size,
                                      &program_command,
                                      &program_key,
                                      &proxy_id,
                                      &proxy_command,
                                      &proxy_key,
                                      &program_id_u8,
                                      &program_id_folded))
    {
      g_object_unref (class_key);
      return;
    }

  schema_rec = g_hash_table_lookup (urls, program_id_folded);
  handler_rec = g_hash_table_lookup (handlers, program_id_folded);

  if (handler_rec == NULL)
    {
      handler_rec = g_object_new (G_TYPE_WIN32_APPINFO_HANDLER, NULL);

      handler_rec->proxy_key = proxy_key;
      handler_rec->key = program_key;
      handler_rec->handler_id = g_wcsdup (program_id, program_id_size);
      handler_rec->handler_id_folded =
          g_strdup (program_id_folded);
      handler_rec->handler_command =
          program_command ? g_wcsdup (program_command, -1) : NULL;
      handler_rec->proxy_id = proxy_id ? g_wcsdup (proxy_id, -1) : NULL;
      handler_rec->proxy_command =
          proxy_command ? g_wcsdup (proxy_command, -1) : NULL;
      extract_executable (proxy_command ? proxy_command : program_command,
                          &handler_rec->executable,
                          &handler_rec->executable_basename,
                          &handler_rec->executable_folded,
                          NULL);
      read_handler_icon (proxy_key, program_key, &handler_rec->icon);
      g_hash_table_insert (handlers,
                           g_strdup (program_id_folded),
                           handler_rec);
    }
  else
    {
      g_clear_object (&program_key);
      g_clear_object (&proxy_key);
    }

  if (schema_rec == NULL)
    {
      schema_rec = g_object_new (G_TYPE_WIN32_APPINFO_URL_SCHEMA, NULL);
      schema_rec->schema = g_wcsdup (class_name, -1);
      schema_rec->schema_u8 = g_strdup (program_id_u8);
      schema_rec->schema_folded = g_strdup (program_id_folded);
      schema_rec->chosen_handler = g_object_ref (handler_rec);
      g_hash_table_insert (urls,
                           g_strdup (program_id_folded),
                           schema_rec);
    }

  if (schema_rec->chosen_handler == NULL)
    schema_rec->chosen_handler = g_object_ref (handler_rec);

  handler_rec_in_url = g_hash_table_lookup (schema_rec->handlers,
                                            program_id_folded);

  if (handler_rec_in_url == NULL && schema_rec->chosen_handler != handler_rec)
    g_hash_table_insert (schema_rec->handlers,
                         g_strdup (program_id_folded),
                         g_object_ref (handler_rec));

  g_free (program_id_u8);
  g_free (program_id_folded);
  g_free (program_command);
  g_free (proxy_id);
  g_free (proxy_command);
  g_object_unref (class_key);
}

static void
read_classes (GWin32RegistryKey *classes_root)
{
  GWin32RegistrySubkeyIter class_iter;
  gunichar2 *class_name;
  gsize class_name_len;

  if (classes_root == NULL)
    return;

  if (!g_win32_registry_subkey_iter_init (&class_iter, classes_root, NULL))
    return;

  while (g_win32_registry_subkey_iter_next (&class_iter, TRUE, NULL))
    {
      if ((!g_win32_registry_subkey_iter_get_name_w (&class_iter,
                                                     &class_name,
                                                     &class_name_len,
                                                     NULL)) ||
          (class_name_len <= 1))
        continue;

      if (class_name[0] == L'.')
        read_class_extension (classes_root, class_name, class_name_len);
      else
        {
          gsize i;

          for (i = 0; i < class_name_len; i++)
            if (!iswalpha (class_name[i]))
              break;

          if (i == class_name_len)
            read_class_url (classes_root, class_name, class_name_len);
        }
    }

  g_win32_registry_subkey_iter_clear (&class_iter);
}

static void
link_chosen_handlers (void)
{
  GHashTableIter iter;
  GHashTableIter handler_iter;
  gchar *schema_folded;
  GWin32AppInfoURLSchema *schema;
  gchar *handler_id_folded;
  GWin32AppInfoHandler *handler;
  gchar *ext_folded;
  GWin32AppInfoFileExtension *ext;

  g_hash_table_iter_init (&iter, urls);

  while (g_hash_table_iter_next (&iter,
                                (gpointer *) &schema_folded,
                                (gpointer *) &schema))
    {
      if (schema->chosen_handler != NULL)
        continue;

      g_hash_table_iter_init (&handler_iter, schema->handlers);

      while (g_hash_table_iter_next (&handler_iter,
                                     (gpointer *) &handler_id_folded,
                                     (gpointer *) &handler))
        {
          gchar *proxy_id_folded;

          if (schema->chosen_handler != NULL)
            break;

          if (strcmp (handler_id_folded, schema_folded) != 0)
            continue;

          if (handler->proxy_command &&
              handler->proxy_id &&
              utf8_and_fold (handler->proxy_id,
                             NULL,
                             &proxy_id_folded))
            {
              GWin32AppInfoHandler *proxy;

              proxy = g_hash_table_lookup (handlers, proxy_id_folded);

              if (proxy)
                {
                  schema->chosen_handler = g_object_ref (proxy);
                  g_debug ("Linking schema %s to proxy handler %c ? \"%S\" : %S\n",
                           schema->schema_u8,
                           schema->chosen_handler->proxy_id ? 'P' : 'T',
                           schema->chosen_handler->proxy_id ? schema->chosen_handler->proxy_id : schema->chosen_handler->handler_id,
                           schema->chosen_handler->proxy_command ? schema->chosen_handler->proxy_command : schema->chosen_handler->handler_command);
                }

              g_free (proxy_id_folded);
            }

          if (schema->chosen_handler == NULL)
            {
              schema->chosen_handler = g_object_ref (handler);
              g_debug ("Linking schema %s to handler %c ? \"%S\" : %S\n",
                       schema->schema_u8,
                       schema->chosen_handler->proxy_id ? 'P' : 'T',
                       schema->chosen_handler->proxy_id ? schema->chosen_handler->proxy_id : schema->chosen_handler->handler_id,
                       schema->chosen_handler->proxy_command ? schema->chosen_handler->proxy_command : schema->chosen_handler->handler_command);
            }
        }
    }

  g_hash_table_iter_init (&iter, extensions);

  while (g_hash_table_iter_next (&iter,
                                 (gpointer *) &ext_folded,
                                 (gpointer *) &ext))
    {
      if (ext->chosen_handler != NULL)
        continue;

      g_hash_table_iter_init (&handler_iter, ext->handlers);

      while (g_hash_table_iter_next (&handler_iter,
                                     (gpointer *) &handler_id_folded,
                                     (gpointer *) &handler))
        {
          gchar *proxy_id_folded;

          if (ext->chosen_handler != NULL)
            break;

          if (strcmp (handler_id_folded, ext_folded) != 0)
            continue;

          if (handler->proxy_command &&
              handler->proxy_id &&
              utf8_and_fold (handler->proxy_id,
                             NULL,
                             &proxy_id_folded))
            {
              GWin32AppInfoHandler *proxy;

              proxy = g_hash_table_lookup (handlers, proxy_id_folded);

              if (proxy)
                {
                  ext->chosen_handler = g_object_ref (proxy);
                  g_debug ("Linking ext %s to proxy handler %c ? \"%S\" : %S\n",
                           ext->extension_u8,
                           ext->chosen_handler->proxy_id ? 'P' : 'T',
                           ext->chosen_handler->proxy_id ? ext->chosen_handler->proxy_id : ext->chosen_handler->handler_id,
                           ext->chosen_handler->proxy_command ? ext->chosen_handler->proxy_command : ext->chosen_handler->handler_command);
                }

              g_free (proxy_id_folded);
            }

          if (ext->chosen_handler == NULL)
            {
              ext->chosen_handler = g_object_ref (handler);
              g_debug ("Linking ext %s to handler %c ? \"%S\" : %S\n",
                       ext->extension_u8,
                       ext->chosen_handler->proxy_id ? 'P' : 'T',
                       ext->chosen_handler->proxy_id ? ext->chosen_handler->proxy_id : ext->chosen_handler->handler_id,
                       ext->chosen_handler->proxy_command ? ext->chosen_handler->proxy_command : ext->chosen_handler->handler_command);
            }
        }
    }
}

static void
link_handlers_to_registered_apps (void)
{
  GHashTableIter iter;
  GHashTableIter sup_iter;
  gchar *app_id_folded;
  GWin32AppInfoApplication *app;
  gchar *schema_folded;
  GWin32AppInfoURLSchema *schema;
  gchar *ext_folded;
  GWin32AppInfoFileExtension *ext;
  gsize unhandled_exts;

  g_hash_table_iter_init (&sup_iter, urls);
  while (g_hash_table_iter_next (&sup_iter,
                                 (gpointer *) &schema_folded,
                                 (gpointer *) &schema))
    {
      if (schema->chosen_handler == NULL)
        g_debug ("WARNING: schema %s has no chosen handler\n", schema->schema_u8);
    }
  unhandled_exts= 0;
  g_hash_table_iter_init (&sup_iter, extensions);
  while (g_hash_table_iter_next (&sup_iter,
                                 (gpointer *) &ext_folded,
                                 (gpointer *) &ext))
    {
      if (ext->chosen_handler == NULL)
        {
          g_debug ("WARNING: extension %s has no chosen handler\n",
                   ext->extension_u8);
          unhandled_exts += 1;
        }
    }

  g_hash_table_iter_init (&iter, apps_by_id);
  while (g_hash_table_iter_next (&iter,
                                 (gpointer *) &app_id_folded,
                                 (gpointer *) &app))
    {
      if (app->supported_urls)
        {
          GWin32AppInfoHandler *handler;

          g_hash_table_iter_init (&sup_iter, app->supported_urls);
          while (g_hash_table_iter_next (&sup_iter,
                                         (gpointer *) &schema_folded,
                                         (gpointer *) &handler))
            {
              schema = g_hash_table_lookup (urls, schema_folded);

              g_assert (schema != NULL);

              if (schema->chosen_handler != NULL &&
                  schema->chosen_handler->app == NULL)
                {
                  schema->chosen_handler->app = g_object_ref (app);
                  g_debug ("Linking %S", app->canonical_name);

                  if (app->localized_pretty_name)
                    g_debug (" '%S'", app->localized_pretty_name);
                  else if (app->pretty_name)
                    g_debug (" '%S'", app->pretty_name);
                  else
                    g_debug (" '%s'", app->executable);

                  if (app->command)
                    g_debug (" %S", app->command);

                  g_debug ("\n to schema %s handler %c ? \"%S\" : %S\n",
                           schema->schema_u8,
                           schema->chosen_handler->proxy_id ? 'P' : 'T',
                           schema->chosen_handler->proxy_id ? schema->chosen_handler->proxy_id : schema->chosen_handler->handler_id,
                           schema->chosen_handler->proxy_command ? schema->chosen_handler->proxy_command : schema->chosen_handler->handler_command);
                }
            }

          g_hash_table_iter_init (&sup_iter, app->supported_urls);
          while (g_hash_table_iter_next (&sup_iter,
                                         (gpointer *) &schema_folded,
                                         (gpointer *) &handler))
            {
              if (handler->app == NULL)
                {
                  handler->app = g_object_ref (app);
                  g_debug ("Linking %S", app->canonical_name);

                  if (app->localized_pretty_name)
                    g_debug (" '%S'", app->localized_pretty_name);
                  else if (app->pretty_name)
                    g_debug (" '%S'", app->pretty_name);
                  else
                    g_debug (" '%s'", app->executable);

                  if (app->command)
                    g_debug (" %S", app->command);

                  g_debug ("\n directly to schema handler to %c ? \"%S\" : %S\n",
                           handler->proxy_id ? 'P' : 'T',
                           handler->proxy_id ? handler->proxy_id : handler->handler_id,
                           handler->proxy_command ? handler->proxy_command : handler->handler_command);
                }
            }
        }

      if (app->supported_exts)
        {
          GWin32AppInfoHandler *handler;

          g_hash_table_iter_init (&sup_iter, app->supported_exts);
          while (g_hash_table_iter_next (&sup_iter,
                                         (gpointer *) &ext_folded,
                                         (gpointer *) &handler))
            {
              ext = g_hash_table_lookup (extensions, ext_folded);

              g_assert (ext != NULL);

              if (ext->chosen_handler != NULL &&
                  ext->chosen_handler->app == NULL)
                {
                  ext->chosen_handler->app = g_object_ref (app);
                  g_debug ("Linking %S", app->canonical_name);

                  if (app->localized_pretty_name)
                    g_debug (" '%S'", app->localized_pretty_name);
                  else if (app->pretty_name)
                    g_debug (" '%S'", app->pretty_name);
                  else
                    g_debug (" '%s'", app->executable);

                  if (app->command)
                    g_debug (" %S", app->command);

                  g_debug ("\n to ext %s handler %c ? \"%S\" : %S\n",
                           ext->extension_u8,
                           ext->chosen_handler->proxy_id ? 'P' : 'T',
                           ext->chosen_handler->proxy_id ? ext->chosen_handler->proxy_id : ext->chosen_handler->handler_id,
                           ext->chosen_handler->proxy_command ? ext->chosen_handler->proxy_command : ext->chosen_handler->handler_command);
                }
            }

          g_hash_table_iter_init (&sup_iter, app->supported_exts);
          while (g_hash_table_iter_next (&sup_iter,
                                         (gpointer *) &ext_folded,
                                         (gpointer *) &handler))
            {
              if (handler->app == NULL)
                {
                  handler->app = g_object_ref (app);
                  g_debug ("Linking %S", app->canonical_name);

                  if (app->localized_pretty_name)
                    g_debug (" '%S'", app->localized_pretty_name);
                  else if (app->pretty_name)
                    g_debug (" '%S'", app->pretty_name);
                  else
                    g_debug (" '%s'", app->executable);

                  if (app->command)
                    g_debug (" %S", app->command);

                  g_debug ("\n directly to ext handler %c ? \"%S\" : %S\n",
                           handler->proxy_id ? 'P' : 'T',
                           handler->proxy_id ? handler->proxy_id : handler->handler_id,
                           handler->proxy_command ? handler->proxy_command : handler->handler_command);
                }
            }
        }
    }

  g_debug ("%" G_GSIZE_FORMAT "undefhandled extensions\n", unhandled_exts);
  unhandled_exts= 0;
  g_hash_table_iter_init (&sup_iter, extensions);
  while (g_hash_table_iter_next (&sup_iter,
                                 (gpointer *) &ext_folded,
                                 (gpointer *) &ext))
    {
      if (ext->chosen_handler == NULL)
        {
          g_debug ("WARNING: extension %s has no chosen handler\n",
                   ext->extension_u8);
          unhandled_exts += 1;
        }
    }
  g_debug ("%" G_GSIZE_FORMAT "undefhandled extensions\n", unhandled_exts);
}

static void
link_handlers_to_unregistered_apps (void)
{
  GHashTableIter iter;
  GHashTableIter app_iter;
  GWin32AppInfoHandler *handler;
  gchar *handler_id_fc;
  GWin32AppInfoApplication *app;
  gchar *canonical_name_fc;
  gchar *appexe_fc_basename;

  g_hash_table_iter_init (&iter, handlers);
  while (g_hash_table_iter_next (&iter,
                                 (gpointer *) &handler_id_fc,
                                 (gpointer *) &handler))
    {
      gchar *hndexe_fc_basename;

      if ((handler->app != NULL) ||
          (handler->executable_folded == NULL))
        continue;

      hndexe_fc_basename = g_utf8_casefold (handler->executable_basename, -1);

      if (hndexe_fc_basename == NULL)
        continue;

      g_hash_table_iter_init (&app_iter, apps_by_id);

      while (g_hash_table_iter_next (&app_iter,
                                     (gpointer *) &canonical_name_fc,
                                     (gpointer *) &app))
        {
          if (app->executable_folded == NULL)
            continue;

          if (strcmp (app->executable_folded,
                      handler->executable_folded) != 0)
            continue;

          handler->app = app;
          break;
        }

      if (handler->app != NULL)
        continue;

      g_hash_table_iter_init (&app_iter, apps_by_exe);

      while ((hndexe_fc_basename != NULL) &&
             (g_hash_table_iter_next (&app_iter,
                                      (gpointer *) &appexe_fc_basename,
                                      (gpointer *) &app)))
        {
          /* Use basename because apps_by_exe only has basenames */
          if (strcmp (hndexe_fc_basename, appexe_fc_basename) != 0)
            continue;

          handler->app = app;
          break;
        }

      g_free (hndexe_fc_basename);

      if (handler->app == NULL)
        g_debug ("WARNING: handler that runs %s has no corresponding app\n",
                 handler->executable);
    }
}


static void
update_registry_data (void)
{
  guint i;
  GPtrArray *capable_apps_keys;
  GPtrArray *user_capable_apps_keys;
  GPtrArray *priority_capable_apps_keys;
  GWin32RegistryKey *url_associations;
  GWin32RegistryKey *file_exts;
  GWin32RegistryKey *classes_root;
  DWORD collect_start, collect_end, alloc_end, capable_end, url_end, ext_end, exeapp_end, classes_end, postproc_end;

  url_associations =
      g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations",
                                   NULL);
  file_exts =
      g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts",
                                   NULL);
  classes_root = g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT", NULL);

  capable_apps_keys = g_ptr_array_new_with_free_func (g_free);
  user_capable_apps_keys = g_ptr_array_new_with_free_func (g_free);
  priority_capable_apps_keys = g_ptr_array_new_with_free_func (g_free);

  g_clear_pointer (&apps_by_id, g_hash_table_destroy);
  g_clear_pointer (&apps_by_exe, g_hash_table_destroy);
  g_clear_pointer (&urls, g_hash_table_destroy);
  g_clear_pointer (&extensions, g_hash_table_destroy);
  g_clear_pointer (&handlers, g_hash_table_destroy);

  collect_start = GetTickCount ();
  collect_capable_apps_from_clients (capable_apps_keys,
                                     priority_capable_apps_keys,
                                     FALSE);
  collect_capable_apps_from_clients (user_capable_apps_keys,
                                     priority_capable_apps_keys,
                                     TRUE);
  collect_capable_apps_from_registered_apps (user_capable_apps_keys,
                                             TRUE);
  collect_capable_apps_from_registered_apps (capable_apps_keys,
                                             FALSE);
  collect_end = GetTickCount ();

  apps_by_id =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  apps_by_exe =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  urls =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  extensions =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  handlers =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  alloc_end = GetTickCount ();

  for (i = 0; i < priority_capable_apps_keys->len; i++)
    read_capable_app (g_ptr_array_index (priority_capable_apps_keys, i),
                      TRUE,
                      TRUE);
  for (i = 0; i < user_capable_apps_keys->len; i++)
    read_capable_app (g_ptr_array_index (user_capable_apps_keys, i),
                      TRUE,
                      FALSE);
  for (i = 0; i < capable_apps_keys->len; i++)
    read_capable_app (g_ptr_array_index (capable_apps_keys, i),
                      FALSE,
                      FALSE);
  capable_end = GetTickCount ();

  read_urls (url_associations);
  url_end = GetTickCount ();
  read_exts (file_exts);
  ext_end = GetTickCount ();
  read_exeapps ();
  exeapp_end = GetTickCount ();
  read_classes (classes_root);
  classes_end = GetTickCount ();
  link_chosen_handlers ();
  link_handlers_to_registered_apps ();
  link_handlers_to_unregistered_apps ();
  postproc_end = GetTickCount ();

  g_debug ("Collecting capable appnames: %lums\n"
           "Allocating hashtables:...... %lums\n"
           "Reading capable apps:        %lums\n"
           "Reading URL associations:... %lums\n"
           "Reading extension assocs:    %lums\n"
           "Reading exe-only apps:...... %lums\n"
           "Reading classes:             %lums\n"
           "Postprocessing:..............%lums\n"
           "TOTAL:                       %lums\n",
           collect_end - collect_start,
           alloc_end - collect_end,
           capable_end - alloc_end,
           url_end - capable_end,
           ext_end - url_end,
           exeapp_end - ext_end,
           classes_end - exeapp_end,
           postproc_end - classes_end,
           postproc_end - collect_start);

  g_clear_object (&classes_root);
  g_clear_object (&url_associations);
  g_clear_object (&file_exts);
  g_ptr_array_free (capable_apps_keys, TRUE);
  g_ptr_array_free (user_capable_apps_keys, TRUE);
  g_ptr_array_free (priority_capable_apps_keys, TRUE);

  return;
}

static void
watch_keys (void)
{
  if (url_associations_key)
    g_win32_registry_key_watch (url_associations_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (file_exts_key)
    g_win32_registry_key_watch (file_exts_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (user_clients_key)
    g_win32_registry_key_watch (user_clients_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (system_clients_key)
    g_win32_registry_key_watch (system_clients_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (applications_key)
    g_win32_registry_key_watch (applications_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (user_registered_apps_key)
    g_win32_registry_key_watch (user_registered_apps_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (system_registered_apps_key)
    g_win32_registry_key_watch (system_registered_apps_key,
                                TRUE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);

  if (classes_root_key)
    g_win32_registry_key_watch (classes_root_key,
                                FALSE,
                                G_WIN32_REGISTRY_WATCH_NAME |
                                G_WIN32_REGISTRY_WATCH_ATTRIBUTES |
                                G_WIN32_REGISTRY_WATCH_VALUES,
                                NULL,
                                NULL,
                                NULL);
}


static void
g_win32_appinfo_init (void)
{
  static gsize initialized;

  if (g_once_init_enter (&initialized))
    {
      url_associations_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations",
                                       NULL);
      file_exts_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts",
                                       NULL);
      user_clients_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\Clients",
                                       NULL);
      system_clients_key =
          g_win32_registry_key_new_w (L"HKEY_LOCAL_MACHINE\\Software\\Clients",
                                       NULL);
      applications_key =
          g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT\\Applications",
                                       NULL);
      user_registered_apps_key =
          g_win32_registry_key_new_w (L"HKEY_CURRENT_USER\\Software\\RegisteredApplications",
                                       NULL);
      system_registered_apps_key =
          g_win32_registry_key_new_w (L"HKEY_LOCAL_MACHINE\\Software\\RegisteredApplications",
                                       NULL);
      classes_root_key =
          g_win32_registry_key_new_w (L"HKEY_CLASSES_ROOT",
                                       NULL);

      watch_keys ();

      update_registry_data ();

      g_once_init_leave (&initialized, TRUE);
    }

  if ((url_associations_key       && g_win32_registry_key_has_changed (url_associations_key))       ||
      (file_exts_key              && g_win32_registry_key_has_changed (file_exts_key))              ||
      (user_clients_key           && g_win32_registry_key_has_changed (user_clients_key))           ||
      (system_clients_key         && g_win32_registry_key_has_changed (system_clients_key))         ||
      (applications_key           && g_win32_registry_key_has_changed (applications_key))           ||
      (user_registered_apps_key   && g_win32_registry_key_has_changed (user_registered_apps_key))   ||
      (system_registered_apps_key && g_win32_registry_key_has_changed (system_registered_apps_key)) ||
      (classes_root_key           && g_win32_registry_key_has_changed (classes_root_key)))
    {
      G_LOCK (gio_win32_appinfo);
      update_registry_data ();
      watch_keys ();
      G_UNLOCK (gio_win32_appinfo);
    }
}


static void g_win32_app_info_iface_init (GAppInfoIface *iface);

struct _GWin32AppInfo
{
  GObject parent_instance;

  /*<private>*/
  gchar **supported_types;

  GWin32AppInfoApplication *app;

  GWin32AppInfoHandler *handler;

  guint startup_notify : 1;
};

G_DEFINE_TYPE_WITH_CODE (GWin32AppInfo, g_win32_app_info, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_APP_INFO,
                                                g_win32_app_info_iface_init))


static void
g_win32_app_info_finalize (GObject *object)
{
  GWin32AppInfo *info;

  info = G_WIN32_APP_INFO (object);

  g_clear_pointer (&info->supported_types, g_free);
  g_clear_object (&info->app);
  g_clear_object (&info->handler);

  G_OBJECT_CLASS (g_win32_app_info_parent_class)->finalize (object);
}

static void
g_win32_app_info_class_init (GWin32AppInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_win32_app_info_finalize;
}

static void
g_win32_app_info_init (GWin32AppInfo *local)
{
}

static GAppInfo *
g_win32_app_info_new_from_app (GWin32AppInfoApplication *app,
                               GWin32AppInfoHandler     *handler)
{
  GWin32AppInfo *new_info;
  GHashTableIter iter;
  gpointer ext;
  int i;

  new_info = g_object_new (G_TYPE_WIN32_APP_INFO, NULL);

  new_info->app = g_object_ref (app);

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  i = 0;
  g_hash_table_iter_init (&iter, new_info->app->supported_exts);

  while (g_hash_table_iter_next (&iter, &ext, NULL))
    {
      if (ext)
        i += 1;
    }

  new_info->supported_types = g_malloc (sizeof (gchar *) * (i + 1));

  i = 0;
  g_hash_table_iter_init (&iter, new_info->app->supported_exts);

  while (g_hash_table_iter_next (&iter, &ext, NULL))
    {
      if (!ext)
        continue;

      new_info->supported_types[i] = (gchar *) ext;
      i += 1;
    }

  G_UNLOCK (gio_win32_appinfo);

  new_info->supported_types[i] = NULL;

  new_info->handler = handler ? g_object_ref (handler) : NULL;

  return G_APP_INFO (new_info);
}


static GAppInfo *
g_win32_app_info_dup (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);
  GWin32AppInfo *new_info;

  new_info = g_object_new (G_TYPE_WIN32_APP_INFO, NULL);

  if (info->app)
    new_info->app = g_object_ref (info->app);

  if (info->handler)
    new_info->handler = g_object_ref (info->handler);

  new_info->startup_notify = info->startup_notify;

  if (info->supported_types)
    {
      int i;

      for (i = 0; info->supported_types[i]; i++)
        break;

      new_info->supported_types = g_malloc (sizeof (gchar *) * (i + 1));

      for (i = 0; info->supported_types[i]; i++)
        new_info->supported_types[i] = g_strdup (info->supported_types[i]);

      new_info->supported_types[i] = NULL;
    }

  return G_APP_INFO (new_info);
}

static gboolean
g_win32_app_info_equal (GAppInfo *appinfo1,
                        GAppInfo *appinfo2)
{
  GWin32AppInfo *info1 = G_WIN32_APP_INFO (appinfo1);
  GWin32AppInfo *info2 = G_WIN32_APP_INFO (appinfo2);

  if (info1->app == NULL ||
      info2->app == NULL)
    return info1 == info2;

  if (info1->app->canonical_name_folded != NULL &&
      info2->app->canonical_name_folded != NULL)
    return (strcmp (info1->app->canonical_name_folded,
                    info2->app->canonical_name_folded)) == 0;

  if (info1->app->executable_folded != NULL &&
      info2->app->executable_folded != NULL)
    return (strcmp (info1->app->executable_folded,
                    info2->app->executable_folded)) == 0;

  return info1->app == info2->app;
}

static const char *
g_win32_app_info_get_id (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  if (info->app->canonical_name_u8)
    return info->app->canonical_name_u8;

  if (info->app->executable_basename)
    return info->app->executable_basename;

  return NULL;
}

static const char *
g_win32_app_info_get_name (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app && info->app->canonical_name_u8)
    return info->app->canonical_name_u8;
  else
    return P_("Unnamed");
}

static const char *
g_win32_app_info_get_display_name (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app)
    {
      if (info->app->localized_pretty_name_u8)
        return info->app->localized_pretty_name_u8;
      else if (info->app->pretty_name_u8)
        return info->app->pretty_name_u8;
    }

  return g_win32_app_info_get_name (appinfo);
}

static const char *
g_win32_app_info_get_description (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  return info->app->description_u8;
}

static const char *
g_win32_app_info_get_executable (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  return info->app->executable;
}

static const char *
g_win32_app_info_get_commandline (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  return info->app->command_u8;
}

static GIcon *
g_win32_app_info_get_icon (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return NULL;

  return info->app->icon;
}

typedef struct _file_or_uri {
  gchar *uri;
  gchar *file;
} file_or_uri;

static char *
expand_macro_single (char macro, file_or_uri *obj)
{
  char *result = NULL;

  switch (macro)
    {
    case '*':
    case '0':
    case '1':
    case 'l':
    case 'd':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      /* TODO: handle 'l' and 'd' differently (longname and desktop name) */
      if (obj->uri)
        result = g_strdup (obj->uri);
      else if (obj->file)
        result = g_strdup (obj->file);
      break;
    case 'u':
    case 'U':
      if (obj->uri)
        result = g_shell_quote (obj->uri);
      break;
    case 'f':
    case 'F':
      if (obj->file)
        result = g_shell_quote (obj->file);
      break;
    }

  return result;
}

static gboolean
expand_macro (char               macro,
              GString           *exec,
              GWin32AppInfo     *info,
              GList            **stat_obj_list,
              GList            **obj_list)
{
  GList *objs = *obj_list;
  char *expanded;
  gboolean result = FALSE;

  g_return_val_if_fail (exec != NULL, FALSE);

/*
Legend: (from http://msdn.microsoft.com/en-us/library/windows/desktop/cc144101%28v=vs.85%29.aspx)
%* - replace with all parameters
%~ - replace with all parameters starting with and following the second parameter
%0 or %1 the first file parameter. For example "C:\\Users\\Eric\\Destop\\New Text Document.txt". Generally this should be in quotes and the applications command line parsing should accept quotes to disambiguate files with spaces in the name and different command line parameters (this is a security best practice and I believe mentioned in MSDN).
%<n> (where N is 2 - 9), replace with the nth parameter
%s - show command
%h - hotkey value
%i - IDList stored in a shared memory handle is passed here.
%l - long file name form of the first parameter. Note win32 applications will be passed the long file name, win16 applications get the short file name. Specifying %L is preferred as it avoids the need to probe for the application type.
%d - desktop absolute parsing name of the first parameter (for items that don't have file system paths)
%v - for verbs that are none implies all, if there is no parameter passed this is the working directory
%w - the working directory
*/

  switch (macro)
    {
    case '*':
    case '~':
      if (*stat_obj_list)
        {
          gint i;
          GList *o;

          for (o = *stat_obj_list, i = 0;
               macro == '~' && o && i < 2;
               o = o->next, i++);

          for (; o; o = o->next)
            {
              expanded = expand_macro_single (macro, o->data);

              if (expanded)
                {
                  if (o != *stat_obj_list)
                    g_string_append (exec, " ");

                  g_string_append (exec, expanded);
                  g_free (expanded);
                }
            }

          objs = NULL;
          result = TRUE;
        }
      break;
    case '0':
    case '1':
    case 'l':
    case 'd':
      if (*stat_obj_list)
        {
          GList *o;

          o = *stat_obj_list;

          if (o)
            {
              expanded = expand_macro_single (macro, o->data);

              if (expanded)
                {
                  if (o != *stat_obj_list)
                    g_string_append (exec, " ");

                  g_string_append (exec, expanded);
                  g_free (expanded);
                }
            }

          if (objs)
            objs = objs->next;

          result = TRUE;
        }
      break;
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (*stat_obj_list)
        {
          gint i;
          GList *o;
          gint n;

          switch (macro)
            {
            case '2':
              n = 2;
              break;
            case '3':
              n = 3;
              break;
            case '4':
              n = 4;
              break;
            case '5':
              n = 5;
              break;
            case '6':
              n = 6;
              break;
            case '7':
              n = 7;
              break;
            case '8':
              n = 8;
              break;
            case '9':
              n = 9;
              break;
            }

          for (o = *stat_obj_list, i = 0; o && i < n; o = o->next, i++);

          if (o)
            {
              expanded = expand_macro_single (macro, o->data);

              if (expanded)
                {
                  if (o != *stat_obj_list)
                    g_string_append (exec, " ");

                  g_string_append (exec, expanded);
                  g_free (expanded);
                }
            }
          result = TRUE;

          if (objs)
            objs = NULL;
        }
      break;
    case 's':
      break;
    case 'h':
      break;
    case 'i':
      break;
    case 'v':
      break;
    case 'w':
      expanded = g_get_current_dir ();
      g_string_append (exec, expanded);
      g_free (expanded);
      break;
    case 'u':
    case 'f':
      if (objs)
        {
          expanded = expand_macro_single (macro, objs->data);

          if (expanded)
            {
              g_string_append (exec, expanded);
              g_free (expanded);
            }
          objs = objs->next;
          result = TRUE;
        }

      break;

    case 'U':
    case 'F':
      while (objs)
        {
          expanded = expand_macro_single (macro, objs->data);

          if (expanded)
            {
              g_string_append (exec, expanded);
              g_free (expanded);
            }

          objs = objs->next;
          result = TRUE;

          if (objs != NULL && expanded)
            g_string_append_c (exec, ' ');
        }

      break;

    case 'c':
      if (info->app && info->app->localized_pretty_name_u8)
        {
          expanded = g_shell_quote (info->app->localized_pretty_name_u8);
          g_string_append (exec, expanded);
          g_free (expanded);
        }
      break;

    case 'm': /* deprecated */
    case 'n': /* deprecated */
    case 'N': /* deprecated */
    /*case 'd': *//* deprecated */
    case 'D': /* deprecated */
      break;

    case '%':
      g_string_append_c (exec, '%');
      break;
    }

  *obj_list = objs;

  return result;
}

static gboolean
expand_application_parameters (GWin32AppInfo   *info,
                               const gchar     *exec_line,
                               GList          **objs,
                               int             *argc,
                               char          ***argv,
                               GError         **error)
{
  GList *obj_list = *objs;
  GList **stat_obj_list = objs;
  const char *p = exec_line;
  GString *expanded_exec;
  gboolean res;
  gchar *a_char;

  if (exec_line == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           P_("Application registry did not specify"
                              " a shell\\open\\command"));
      return FALSE;
    }

  expanded_exec = g_string_new (NULL);
  res = FALSE;

  while (*p)
    {
      if (p[0] == '%' && p[1] != '\0')
        {
          if (expand_macro (p[1],
                            expanded_exec,
                            info, stat_obj_list,
                            objs))
            res = TRUE;

          p++;
        }
      else
        g_string_append_c (expanded_exec, *p);

      p++;
    }

  /* No file substitutions */
  if (obj_list == *objs && obj_list != NULL && !res)
    {
      /* If there is no macro default to %f. This is also what KDE does */
      g_string_append_c (expanded_exec, ' ');
      expand_macro ('f', expanded_exec, info, stat_obj_list, objs);
    }

  /* Replace '\\' with '/', because g_shell_parse_argv considers them
   * to be escape sequences.
   */
  for (a_char = expanded_exec->str;
       a_char <= &expanded_exec->str[expanded_exec->len];
       a_char++)
    {
      if (*a_char == '\\')
        *a_char = '/';
    }

  res = g_shell_parse_argv (expanded_exec->str, argc, argv, error);
  g_string_free (expanded_exec, TRUE);
  return res;
}


static gchar *
get_appath_for_exe (gunichar2 *exe_basename)
{
  GWin32RegistryKey *apppath_key = NULL;
  GWin32RegistryValueType val_type;
  gunichar2 *appath = NULL;
  gboolean got_value;
  gchar *result = NULL;

  apppath_key = _g_win32_registry_key_build_and_new_w (NULL, L"HKEY_LOCAL_MACHINE\\"
                                                       L"\\SOFTWARE"
                                                       L"\\Microsoft"
                                                       L"\\Windows"
                                                       L"\\CurrentVersion"
                                                       L"\\App Paths\\",
                                                       exe_basename, NULL);

  if (apppath_key == NULL)
    return NULL;

  got_value = g_win32_registry_key_get_value_w (apppath_key,
                                                TRUE,
                                                L"Path",
                                                &val_type,
                                                (void **) &appath,
                                                NULL,
                                                NULL);

  g_object_unref (apppath_key);

  if (got_value && val_type == G_WIN32_REGISTRY_VALUE_STR)
    result = g_utf16_to_utf8 (appath, -1, NULL,NULL, NULL);

  g_clear_pointer (&appath, g_free);

  return result;
}


static gboolean
g_win32_app_info_launch_internal (GWin32AppInfo      *info,
                                  GList              *objs,
                                  GAppLaunchContext  *launch_context,
                                  GSpawnFlags         spawn_flags,
                                  GError            **error)
{
  gboolean completed = FALSE;
  char **argv, **envp;
  int argc;
  gchar *command;
  gchar *apppath;
  gunichar2 *exe_basename;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->app != NULL, FALSE);

  argv = NULL;

  if (launch_context)
    envp = g_app_launch_context_get_environment (launch_context);
  else
    envp = g_get_environ ();

  command = NULL;
  exe_basename = NULL;

  if (info->handler)
    {
      if (info->handler->handler_command)
        {
          command = g_utf16_to_utf8 (info->handler->handler_command,
                                     -1,
                                     NULL,
                                     NULL,
                                     NULL);
          exe_basename = g_utf8_to_utf16 (info->handler->executable_basename,
                                          -1,
                                          NULL,
                                          NULL,
                                          NULL);
        }
      else if (info->handler->proxy_command)
        {
          command = g_utf16_to_utf8 (info->handler->proxy_command,
                                     -1,
                                     NULL,
                                     NULL,
                                     NULL);
          exe_basename = g_utf8_to_utf16 (info->handler->executable_basename,
                                          -1,
                                          NULL,
                                          NULL,
                                          NULL);
        }
    }

  if (command == NULL)
    {
      command = g_strdup (info->app->command_u8);
      exe_basename = g_utf8_to_utf16 (info->app->executable_basename,
                                      -1,
                                      NULL,
                                      NULL,
                                      NULL);
    }

  apppath = get_appath_for_exe (exe_basename);

  g_free (exe_basename);

  if (apppath)
    {
      gchar **p;
      gint p_index;

      for (p = envp, p_index = 0; p[0]; p++, p_index++)
        if ((p[0][0] == 'p' || p[0][0] == 'P') &&
            (p[0][1] == 'a' || p[0][1] == 'A') &&
            (p[0][2] == 't' || p[0][2] == 'T') &&
            (p[0][3] == 'h' || p[0][3] == 'H') &&
            (p[0][4] == '='))
          break;

      if (p[0] == NULL)
        {
          gchar **new_envp;
          new_envp = g_new (char *, g_strv_length (envp) + 2);
          new_envp[0] = g_strdup_printf ("PATH=%s", apppath);

          for (p_index = 0; p_index <= g_strv_length (envp); p_index++)
            new_envp[1 + p_index] = envp[p_index];

          g_free (envp);
          envp = new_envp;
        }
      else
        {
          gchar *p_path;

          p_path = &p[0][5];

          if (p_path[0] != '\0')
            envp[p_index] = g_strdup_printf ("PATH=%s%c%s",
                                             apppath,
                                             G_SEARCHPATH_SEPARATOR,
                                             p_path);
          else
            envp[p_index] = g_strdup_printf ("PATH=%s", apppath);

          g_free (&p_path[-5]);
        }
    }

  do
    {
      GPid pid;

      if (!expand_application_parameters (info,
                                          command,
                                          &objs,
                                          &argc,
                                          &argv,
                                          error))
        goto out;

      if (!g_spawn_async (NULL,
                          argv,
                          envp,
                          spawn_flags,
                          NULL,
                          NULL,
                          &pid,
                          error))
          goto out;

      if (launch_context != NULL)
        {
          GVariantBuilder builder;
          GVariant *platform_data;

          g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
          g_variant_builder_add (&builder, "{sv}", "pid", g_variant_new_int32 ((gint32) pid));

          platform_data = g_variant_ref_sink (g_variant_builder_end (&builder));
          g_signal_emit_by_name (launch_context, "launched", info, platform_data);
          g_variant_unref (platform_data);
        }

      g_strfreev (argv);
      argv = NULL;
    }
  while (objs != NULL);

  completed = TRUE;

 out:
  g_strfreev (argv);
  g_strfreev (envp);
  g_free (command);

  return completed;
}

static void
free_file_or_uri (gpointer ptr)
{
  file_or_uri *obj = ptr;
  g_free (obj->file);
  g_free (obj->uri);
  g_free (obj);
}


static gboolean
g_win32_app_supports_uris (GWin32AppInfoApplication *app)
{
  gssize num_of_uris_supported;

  if (app == NULL)
    return FALSE;

  num_of_uris_supported = (gssize) g_hash_table_size (app->supported_urls);

  if (g_hash_table_lookup (app->supported_urls, "file"))
    num_of_uris_supported -= 1;

  return num_of_uris_supported > 0;
}


static gboolean
g_win32_app_info_supports_uris (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return FALSE;

  return g_win32_app_supports_uris (info->app);
}


static gboolean
g_win32_app_info_supports_files (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  if (info->app == NULL)
    return FALSE;

  return g_hash_table_size (info->app->supported_exts) > 0;
}


static gboolean
g_win32_app_info_launch_uris (GAppInfo           *appinfo,
                              GList              *uris,
                              GAppLaunchContext  *launch_context,
                              GError            **error)
{
  gboolean res = FALSE;
  gboolean do_files;
  GList *objs;

  do_files = g_win32_app_info_supports_files (appinfo);

  objs = NULL;
  while (uris)
    {
      file_or_uri *obj;
      obj = g_new0 (file_or_uri, 1);

      if (do_files)
        {
          GFile *file;
          gchar *path;

          file = g_file_new_for_uri (uris->data);
          path = g_file_get_path (file);
          obj->file = path;
          g_object_unref (file);
        }

      obj->uri = g_strdup (uris->data);

      objs = g_list_prepend (objs, obj);
      uris = uris->next;
    }

  objs = g_list_reverse (objs);

  res = g_win32_app_info_launch_internal (G_WIN32_APP_INFO (appinfo),
                                          objs,
                                          launch_context,
                                          G_SPAWN_SEARCH_PATH,
                                          error);

  g_list_free_full (objs, free_file_or_uri);

  return res;
}

static gboolean
g_win32_app_info_launch (GAppInfo           *appinfo,
                         GList              *files,
                         GAppLaunchContext  *launch_context,
                         GError            **error)
{
  gboolean res = FALSE;
  gboolean do_uris;
  GList *objs;

  do_uris = g_win32_app_info_supports_uris (appinfo);

  objs = NULL;
  while (files)
    {
      file_or_uri *obj;
      obj = g_new0 (file_or_uri, 1);
      obj->file = g_file_get_path (G_FILE (files->data));

      if (do_uris)
        obj->uri = g_file_get_uri (G_FILE (files->data));

      objs = g_list_prepend (objs, obj);
      files = files->next;
    }

  objs = g_list_reverse (objs);

  res = g_win32_app_info_launch_internal (G_WIN32_APP_INFO (appinfo),
                                          objs,
                                          launch_context,
                                          G_SPAWN_SEARCH_PATH,
                                          error);

  g_list_free_full (objs, free_file_or_uri);

  return res;
}

static const char **
g_win32_app_info_get_supported_types (GAppInfo *appinfo)
{
  GWin32AppInfo *info = G_WIN32_APP_INFO (appinfo);

  return (const char**) info->supported_types;
}

GAppInfo *
g_app_info_create_from_commandline (const char           *commandline,
                                    const char           *application_name,
                                    GAppInfoCreateFlags   flags,
                                    GError              **error)
{
  GWin32AppInfo *info;
  GWin32AppInfoApplication *app;

  g_return_val_if_fail (commandline, NULL);

  info = g_object_new (G_TYPE_WIN32_APP_INFO, NULL);

  app = g_object_new (G_TYPE_WIN32_APPINFO_APPLICATION, NULL);

  if (application_name)
    {
      app->canonical_name = g_utf8_to_utf16 (application_name,
                                             -1,
                                             NULL,
                                             NULL,
                                             NULL);
      app->canonical_name_u8 = g_strdup (application_name);
      app->canonical_name_folded = g_utf8_casefold (application_name, -1);
    }

  app->command = g_utf8_to_utf16 (commandline, -1, NULL, NULL, NULL);
  app->command_u8 = g_strdup (commandline);

  extract_executable (app->command,
                      &app->executable,
                      &app->executable_basename,
                      &app->executable_folded,
                      NULL);

  app->no_open_with = FALSE;
  app->user_specific = FALSE;
  app->default_app = FALSE;

  info->app = app;
  info->handler = NULL;

  return G_APP_INFO (info);
}

/* GAppInfo interface init */

static void
g_win32_app_info_iface_init (GAppInfoIface *iface)
{
  iface->dup = g_win32_app_info_dup;
  iface->equal = g_win32_app_info_equal;
  iface->get_id = g_win32_app_info_get_id;
  iface->get_name = g_win32_app_info_get_name;
  iface->get_description = g_win32_app_info_get_description;
  iface->get_executable = g_win32_app_info_get_executable;
  iface->get_icon = g_win32_app_info_get_icon;
  iface->launch = g_win32_app_info_launch;
  iface->supports_uris = g_win32_app_info_supports_uris;
  iface->supports_files = g_win32_app_info_supports_files;
  iface->launch_uris = g_win32_app_info_launch_uris;
/*  iface->should_show = g_win32_app_info_should_show;*/
/*  iface->set_as_default_for_type = g_win32_app_info_set_as_default_for_type;*/
/*  iface->set_as_default_for_extension = g_win32_app_info_set_as_default_for_extension;*/
/*  iface->add_supports_type = g_win32_app_info_add_supports_type;*/
/*  iface->can_remove_supports_type = g_win32_app_info_can_remove_supports_type;*/
/*  iface->remove_supports_type = g_win32_app_info_remove_supports_type;*/
/*  iface->can_delete = g_win32_app_info_can_delete;*/
/*  iface->do_delete = g_win32_app_info_delete;*/
  iface->get_commandline = g_win32_app_info_get_commandline;
  iface->get_display_name = g_win32_app_info_get_display_name;
/*  iface->set_as_last_used_for_type = g_win32_app_info_set_as_last_used_for_type;*/
  iface->get_supported_types = g_win32_app_info_get_supported_types;
}

GAppInfo *
g_app_info_get_default_for_uri_scheme (const char *uri_scheme)
{
  GWin32AppInfoURLSchema *scheme;
  char *scheme_down;
  GAppInfo *result;

  scheme_down = g_utf8_casefold (uri_scheme, -1);

  if (!scheme_down)
    return NULL;

  if (strcmp (scheme_down, "file") == 0)
    {
      g_free (scheme_down);
      return NULL;
    }

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  scheme = g_hash_table_lookup (urls, scheme_down);
  g_free (scheme_down);

  if (scheme)
    g_object_ref (scheme);

  G_UNLOCK (gio_win32_appinfo);

  result = NULL;

  if (scheme != NULL &&
      scheme->chosen_handler != NULL &&
      scheme->chosen_handler->app != NULL)
    result = g_win32_app_info_new_from_app (scheme->chosen_handler->app,
                                            scheme->chosen_handler);

  if (scheme)
    g_object_unref (scheme);

  return result;
}

GAppInfo *
g_app_info_get_default_for_type (const char *content_type,
                                 gboolean    must_support_uris)
{
  GWin32AppInfoFileExtension *ext;
  char *ext_down;
  GWin32AppInfoHandler *handler;
  GAppInfo *result;
  GWin32AppInfoApplication *app;
  GHashTableIter iter;

  ext_down = g_utf8_casefold (content_type, -1);

  if (!ext_down)
    return NULL;

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  /* Assuming that "content_type" is a file extension, not a MIME type */
  ext = g_hash_table_lookup (extensions, ext_down);
  g_free (ext_down);

  result = NULL;

  if (ext != NULL)
    g_object_ref (ext);

  G_UNLOCK (gio_win32_appinfo);

  if (ext != NULL)
    {
      if (ext->chosen_handler != NULL &&
          ext->chosen_handler->app != NULL &&
          (!must_support_uris ||
           g_win32_app_supports_uris (ext->chosen_handler->app)))
        result = g_win32_app_info_new_from_app (ext->chosen_handler->app,
                                                ext->chosen_handler);
      else
        {
          g_hash_table_iter_init (&iter, ext->handlers);

          while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &handler))
            {
              if (handler->app &&
                  (!must_support_uris ||
                   g_win32_app_supports_uris (ext->chosen_handler->app)))
                {
                  result = g_win32_app_info_new_from_app (handler->app, handler);
                  break;
                }
            }

          if (result == NULL)
            {
              g_hash_table_iter_init (&iter, ext->other_apps);
              while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &app))
                {
                  if (!must_support_uris ||
                       g_win32_app_supports_uris (ext->chosen_handler->app))
                    {
                      result = g_win32_app_info_new_from_app (app, NULL);
                      break;
                    }
                }
            }
        }
      g_object_unref (ext);
    }

  return result;
}

GList *
g_app_info_get_all (void)
{
  GHashTableIter iter;
  gpointer value;
  GList *infos;
  GList *apps;
  GList *apps_i;

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  apps = NULL;
  g_hash_table_iter_init (&iter, apps_by_id);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    apps = g_list_prepend (apps, g_object_ref (G_OBJECT (value)));

  G_UNLOCK (gio_win32_appinfo);

  infos = NULL;
  for (apps_i = apps; apps_i; apps_i = apps_i->next)
    infos = g_list_prepend (infos,
                            g_win32_app_info_new_from_app (apps_i->data, NULL));

  g_list_free_full (apps, g_object_unref);

  return infos;
}

GList *
g_app_info_get_all_for_type (const char *content_type)
{
  GWin32AppInfoFileExtension *ext;
  char *ext_down;
  GWin32AppInfoHandler *handler;
  GWin32AppInfoApplication *app;
  GHashTableIter iter;
  GList *result;

  ext_down = g_utf8_casefold (content_type, -1);

  if (!ext_down)
    return NULL;

  g_win32_appinfo_init ();
  G_LOCK (gio_win32_appinfo);

  /* Assuming that "content_type" is a file extension, not a MIME type */
  ext = g_hash_table_lookup (extensions, ext_down);
  g_free (ext_down);

  result = NULL;

  if (ext != NULL)
    g_object_ref (ext);

  G_UNLOCK (gio_win32_appinfo);

  if (ext == NULL)
    return NULL;

  if (ext->chosen_handler != NULL &&
      ext->chosen_handler->app != NULL)
    result = g_list_prepend (result,
                             g_win32_app_info_new_from_app (ext->chosen_handler->app,
                                                            ext->chosen_handler));

  g_hash_table_iter_init (&iter, ext->handlers);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &handler))
    {
      if (handler->app &&
          (ext->chosen_handler == NULL || ext->chosen_handler->app != handler->app))
          result = g_list_prepend (result,
                                   g_win32_app_info_new_from_app (handler->app,
                                                                  handler));
    }

  g_hash_table_iter_init (&iter, ext->other_apps);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &app))
    {
      result = g_list_prepend (result, g_win32_app_info_new_from_app (app, NULL));
    }

  g_object_unref (ext);

  result = g_list_reverse (result);

  return result;
}

GList *
g_app_info_get_fallback_for_type (const gchar *content_type)
{
  /* TODO: fix this once gcontenttype support is improved */
  return g_app_info_get_all_for_type (content_type);
}

GList *
g_app_info_get_recommended_for_type (const gchar *content_type)
{
  /* TODO: fix this once gcontenttype support is improved */
  return g_app_info_get_all_for_type (content_type);
}

void
g_app_info_reset_type_associations (const char *content_type)
{
  /* nothing to do */
}
