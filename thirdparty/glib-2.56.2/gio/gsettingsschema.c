/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2011 Canonical Limited
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
 */

#include "config.h"

#include "gsettingsschema-internal.h"
#include "gsettings.h"

#include "gvdb/gvdb-reader.h"
#include "strinfo.c"

#include <glibintl.h>
#include <locale.h>
#include <string.h>

/**
 * SECTION:gsettingsschema
 * @short_description: Introspecting and controlling the loading
 *     of GSettings schemas
 * @include: gio/gio.h
 *
 * The #GSettingsSchemaSource and #GSettingsSchema APIs provide a
 * mechanism for advanced control over the loading of schemas and a
 * mechanism for introspecting their content.
 *
 * Plugin loading systems that wish to provide plugins a way to access
 * settings face the problem of how to make the schemas for these
 * settings visible to GSettings.  Typically, a plugin will want to ship
 * the schema along with itself and it won't be installed into the
 * standard system directories for schemas.
 *
 * #GSettingsSchemaSource provides a mechanism for dealing with this by
 * allowing the creation of a new 'schema source' from which schemas can
 * be acquired.  This schema source can then become part of the metadata
 * associated with the plugin and queried whenever the plugin requires
 * access to some settings.
 *
 * Consider the following example:
 *
 * |[<!-- language="C" -->
 * typedef struct
 * {
 *    ...
 *    GSettingsSchemaSource *schema_source;
 *    ...
 * } Plugin;
 *
 * Plugin *
 * initialise_plugin (const gchar *dir)
 * {
 *   Plugin *plugin;
 *
 *   ...
 *
 *   plugin->schema_source =
 *     g_settings_schema_source_new_from_directory (dir,
 *       g_settings_schema_source_get_default (), FALSE, NULL);
 *
 *   ...
 *
 *   return plugin;
 * }
 *
 * ...
 *
 * GSettings *
 * plugin_get_settings (Plugin      *plugin,
 *                      const gchar *schema_id)
 * {
 *   GSettingsSchema *schema;
 *
 *   if (schema_id == NULL)
 *     schema_id = plugin->identifier;
 *
 *   schema = g_settings_schema_source_lookup (plugin->schema_source,
 *                                             schema_id, FALSE);
 *
 *   if (schema == NULL)
 *     {
 *       ... disable the plugin or abort, etc ...
 *     }
 *
 *   return g_settings_new_full (schema, NULL, NULL);
 * }
 * ]|
 *
 * The code above shows how hooks should be added to the code that
 * initialises (or enables) the plugin to create the schema source and
 * how an API can be added to the plugin system to provide a convenient
 * way for the plugin to access its settings, using the schemas that it
 * ships.
 *
 * From the standpoint of the plugin, it would need to ensure that it
 * ships a gschemas.compiled file as part of itself, and then simply do
 * the following:
 *
 * |[<!-- language="C" -->
 * {
 *   GSettings *settings;
 *   gint some_value;
 *
 *   settings = plugin_get_settings (self, NULL);
 *   some_value = g_settings_get_int (settings, "some-value");
 *   ...
 * }
 * ]|
 *
 * It's also possible that the plugin system expects the schema source
 * files (ie: .gschema.xml files) instead of a gschemas.compiled file.
 * In that case, the plugin loading system must compile the schemas for
 * itself before attempting to create the settings source.
 *
 * Since: 2.32
 **/

/**
 * GSettingsSchemaKey:
 *
 * #GSettingsSchemaKey is an opaque data structure and can only be accessed
 * using the following functions.
 **/

/**
 * GSettingsSchema:
 *
 * This is an opaque structure type.  You may not access it directly.
 *
 * Since: 2.32
 **/
struct _GSettingsSchema
{
  GSettingsSchemaSource *source;
  const gchar *gettext_domain;
  const gchar *path;
  GQuark *items;
  gint n_items;
  GvdbTable *table;
  gchar *id;

  GSettingsSchema *extends;

  gint ref_count;
};

/**
 * G_TYPE_SETTINGS_SCHEMA_SOURCE:
 *
 * A boxed #GType corresponding to #GSettingsSchemaSource.
 *
 * Since: 2.32
 **/
G_DEFINE_BOXED_TYPE (GSettingsSchemaSource, g_settings_schema_source, g_settings_schema_source_ref, g_settings_schema_source_unref)

/**
 * G_TYPE_SETTINGS_SCHEMA:
 *
 * A boxed #GType corresponding to #GSettingsSchema.
 *
 * Since: 2.32
 **/
G_DEFINE_BOXED_TYPE (GSettingsSchema, g_settings_schema, g_settings_schema_ref, g_settings_schema_unref)

/**
 * GSettingsSchemaSource:
 *
 * This is an opaque structure type.  You may not access it directly.
 *
 * Since: 2.32
 **/
struct _GSettingsSchemaSource
{
  GSettingsSchemaSource *parent;
  gchar *directory;
  GvdbTable *table;
  GHashTable **text_tables;

  gint ref_count;
};

static GSettingsSchemaSource *schema_sources;

/**
 * g_settings_schema_source_ref:
 * @source: a #GSettingsSchemaSource
 *
 * Increase the reference count of @source, returning a new reference.
 *
 * Returns: a new reference to @source
 *
 * Since: 2.32
 **/
GSettingsSchemaSource *
g_settings_schema_source_ref (GSettingsSchemaSource *source)
{
  g_atomic_int_inc (&source->ref_count);

  return source;
}

/**
 * g_settings_schema_source_unref:
 * @source: a #GSettingsSchemaSource
 *
 * Decrease the reference count of @source, possibly freeing it.
 *
 * Since: 2.32
 **/
void
g_settings_schema_source_unref (GSettingsSchemaSource *source)
{
  if (g_atomic_int_dec_and_test (&source->ref_count))
    {
      if (source == schema_sources)
        g_error ("g_settings_schema_source_unref() called too many times on the default schema source");

      if (source->parent)
        g_settings_schema_source_unref (source->parent);
      gvdb_table_unref (source->table);
      g_free (source->directory);

      if (source->text_tables)
        {
          g_hash_table_unref (source->text_tables[0]);
          g_hash_table_unref (source->text_tables[1]);
          g_free (source->text_tables);
        }

      g_slice_free (GSettingsSchemaSource, source);
    }
}

/**
 * g_settings_schema_source_new_from_directory:
 * @directory: (type filename): the filename of a directory
 * @parent: (nullable): a #GSettingsSchemaSource, or %NULL
 * @trusted: %TRUE, if the directory is trusted
 * @error: a pointer to a #GError pointer set to %NULL, or %NULL
 *
 * Attempts to create a new schema source corresponding to the contents
 * of the given directory.
 *
 * This function is not required for normal uses of #GSettings but it
 * may be useful to authors of plugin management systems.
 *
 * The directory should contain a file called `gschemas.compiled` as
 * produced by the [glib-compile-schemas][glib-compile-schemas] tool.
 *
 * If @trusted is %TRUE then `gschemas.compiled` is trusted not to be
 * corrupted. This assumption has a performance advantage, but can result
 * in crashes or inconsistent behaviour in the case of a corrupted file.
 * Generally, you should set @trusted to %TRUE for files installed by the
 * system and to %FALSE for files in the home directory.
 *
 * If @parent is non-%NULL then there are two effects.
 *
 * First, if g_settings_schema_source_lookup() is called with the
 * @recursive flag set to %TRUE and the schema can not be found in the
 * source, the lookup will recurse to the parent.
 *
 * Second, any references to other schemas specified within this
 * source (ie: `child` or `extends`) references may be resolved
 * from the @parent.
 *
 * For this second reason, except in very unusual situations, the
 * @parent should probably be given as the default schema source, as
 * returned by g_settings_schema_source_get_default().
 *
 * Since: 2.32
 **/
GSettingsSchemaSource *
g_settings_schema_source_new_from_directory (const gchar            *directory,
                                             GSettingsSchemaSource  *parent,
                                             gboolean                trusted,
                                             GError                **error)
{
  GSettingsSchemaSource *source;
  GvdbTable *table;
  gchar *filename;

  filename = g_build_filename (directory, "gschemas.compiled", NULL);
  table = gvdb_table_new (filename, trusted, error);
  g_free (filename);

  if (table == NULL)
    return NULL;

  source = g_slice_new (GSettingsSchemaSource);
  source->directory = g_strdup (directory);
  source->parent = parent ? g_settings_schema_source_ref (parent) : NULL;
  source->text_tables = NULL;
  source->table = table;
  source->ref_count = 1;

  return source;
}

static void
try_prepend_dir (const gchar *directory)
{
  GSettingsSchemaSource *source;

  source = g_settings_schema_source_new_from_directory (directory, schema_sources, TRUE, NULL);

  /* If we successfully created it then prepend it to the global list */
  if (source != NULL)
    schema_sources = source;
}

static void
try_prepend_data_dir (const gchar *directory)
{
  gchar *dirname = g_build_filename (directory, "glib-2.0", "schemas", NULL);
  try_prepend_dir (dirname);
  g_free (dirname);
}

static void
initialise_schema_sources (void)
{
  static gsize initialised;

  /* need a separate variable because 'schema_sources' may legitimately
   * be null if we have zero valid schema sources
   */
  if G_UNLIKELY (g_once_init_enter (&initialised))
    {
      const gchar * const *dirs;
      const gchar *path;
      gint i;

      /* iterate in reverse: count up, then count down */
      dirs = g_get_system_data_dirs ();
      for (i = 0; dirs[i]; i++);

      while (i--)
        try_prepend_data_dir (dirs[i]);

      try_prepend_data_dir (g_get_user_data_dir ());

      if ((path = g_getenv ("GSETTINGS_SCHEMA_DIR")) != NULL)
        try_prepend_dir (path);

      g_once_init_leave (&initialised, TRUE);
    }
}

/**
 * g_settings_schema_source_get_default:
 *
 * Gets the default system schema source.
 *
 * This function is not required for normal uses of #GSettings but it
 * may be useful to authors of plugin management systems or to those who
 * want to introspect the content of schemas.
 *
 * If no schemas are installed, %NULL will be returned.
 *
 * The returned source may actually consist of multiple schema sources
 * from different directories, depending on which directories were given
 * in `XDG_DATA_DIRS` and `GSETTINGS_SCHEMA_DIR`. For this reason, all
 * lookups performed against the default source should probably be done
 * recursively.
 *
 * Returns: (transfer none) (nullable): the default schema source
 *
 * Since: 2.32
 **/
GSettingsSchemaSource *
g_settings_schema_source_get_default (void)
{
  initialise_schema_sources ();

  return schema_sources;
}

/**
 * g_settings_schema_source_lookup:
 * @source: a #GSettingsSchemaSource
 * @schema_id: a schema ID
 * @recursive: %TRUE if the lookup should be recursive
 *
 * Looks up a schema with the identifier @schema_id in @source.
 *
 * This function is not required for normal uses of #GSettings but it
 * may be useful to authors of plugin management systems or to those who
 * want to introspect the content of schemas.
 *
 * If the schema isn't found directly in @source and @recursive is %TRUE
 * then the parent sources will also be checked.
 *
 * If the schema isn't found, %NULL is returned.
 *
 * Returns: (nullable) (transfer full): a new #GSettingsSchema
 *
 * Since: 2.32
 **/
GSettingsSchema *
g_settings_schema_source_lookup (GSettingsSchemaSource *source,
                                 const gchar           *schema_id,
                                 gboolean               recursive)
{
  GSettingsSchema *schema;
  GvdbTable *table;
  const gchar *extends;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (schema_id != NULL, NULL);

  table = gvdb_table_get_table (source->table, schema_id);

  if (table == NULL && recursive)
    for (source = source->parent; source; source = source->parent)
      if ((table = gvdb_table_get_table (source->table, schema_id)))
        break;

  if (table == NULL)
    return NULL;

  schema = g_slice_new0 (GSettingsSchema);
  schema->source = g_settings_schema_source_ref (source);
  schema->ref_count = 1;
  schema->id = g_strdup (schema_id);
  schema->table = table;
  schema->path = g_settings_schema_get_string (schema, ".path");
  schema->gettext_domain = g_settings_schema_get_string (schema, ".gettext-domain");

  if (schema->gettext_domain)
    bind_textdomain_codeset (schema->gettext_domain, "UTF-8");

  extends = g_settings_schema_get_string (schema, ".extends");
  if (extends)
    {
      schema->extends = g_settings_schema_source_lookup (source, extends, TRUE);
      if (schema->extends == NULL)
        g_warning ("Schema '%s' extends schema '%s' but we could not find it", schema_id, extends);
    }

  return schema;
}

typedef struct
{
  GHashTable *summaries;
  GHashTable *descriptions;
  GSList     *gettext_domain;
  GSList     *schema_id;
  GSList     *key_name;
  GString    *string;
} TextTableParseInfo;

static const gchar *
get_attribute_value (GSList *list)
{
  GSList *node;

  for (node = list; node; node = node->next)
    if (node->data)
      return node->data;

  return NULL;
}

static void
pop_attribute_value (GSList **list)
{
  gchar *top;

  top = (*list)->data;
  *list = g_slist_remove (*list, top);

  g_free (top);
}

static void
push_attribute_value (GSList      **list,
                      const gchar  *value)
{
  *list = g_slist_prepend (*list, g_strdup (value));
}

static void
start_element (GMarkupParseContext  *context,
               const gchar          *element_name,
               const gchar         **attribute_names,
               const gchar         **attribute_values,
               gpointer              user_data,
               GError              **error)
{
  TextTableParseInfo *info = user_data;
  const gchar *gettext_domain = NULL;
  const gchar *schema_id = NULL;
  const gchar *key_name = NULL;
  gint i;

  for (i = 0; attribute_names[i]; i++)
    {
      if (g_str_equal (attribute_names[i], "gettext-domain"))
        gettext_domain = attribute_values[i];
      else if (g_str_equal (attribute_names[i], "id"))
        schema_id = attribute_values[i];
      else if (g_str_equal (attribute_names[i], "name"))
        key_name = attribute_values[i];
    }

  push_attribute_value (&info->gettext_domain, gettext_domain);
  push_attribute_value (&info->schema_id, schema_id);
  push_attribute_value (&info->key_name, key_name);

  if (info->string)
    {
      g_string_free (info->string, TRUE);
      info->string = NULL;
    }

  if (g_str_equal (element_name, "summary") || g_str_equal (element_name, "description"))
    info->string = g_string_new (NULL);
}

static gchar *
normalise_whitespace (const gchar *orig)
{
  /* We normalise by the same rules as in intltool:
   *
   *   sub cleanup {
   *       s/^\s+//;
   *       s/\s+$//;
   *       s/\s+/ /g;
   *       return $_;
   *   }
   *
   *   $message = join "\n\n", map &cleanup, split/\n\s*\n+/, $message;
   *
   * Where \s is an ascii space character.
   *
   * We aim for ease of implementation over efficiency -- this code is
   * not run in normal applications.
   */
  static GRegex *cleanup[3];
  static GRegex *splitter;
  gchar **lines;
  gchar *result;
  gint i;

  if (g_once_init_enter (&splitter))
    {
      GRegex *s;

      cleanup[0] = g_regex_new ("^\\s+", 0, 0, 0);
      cleanup[1] = g_regex_new ("\\s+$", 0, 0, 0);
      cleanup[2] = g_regex_new ("\\s+", 0, 0, 0);
      s = g_regex_new ("\\n\\s*\\n+", 0, 0, 0);

      g_once_init_leave (&splitter, s);
    }

  lines = g_regex_split (splitter, orig, 0);
  for (i = 0; lines[i]; i++)
    {
      gchar *a, *b, *c;

      a = g_regex_replace_literal (cleanup[0], lines[i], -1, 0, "", 0, 0);
      b = g_regex_replace_literal (cleanup[1], a, -1, 0, "", 0, 0);
      c = g_regex_replace_literal (cleanup[2], b, -1, 0, " ", 0, 0);
      g_free (lines[i]);
      g_free (a);
      g_free (b);
      lines[i] = c;
    }

  result = g_strjoinv ("\n\n", lines);
  g_strfreev (lines);

  return result;
}

static void
end_element (GMarkupParseContext *context,
             const gchar *element_name,
             gpointer user_data,
             GError **error)
{
  TextTableParseInfo *info = user_data;

  pop_attribute_value (&info->gettext_domain);
  pop_attribute_value (&info->schema_id);
  pop_attribute_value (&info->key_name);

  if (info->string)
    {
      GHashTable *source_table = NULL;
      const gchar *gettext_domain;
      const gchar *schema_id;
      const gchar *key_name;

      gettext_domain = get_attribute_value (info->gettext_domain);
      schema_id = get_attribute_value (info->schema_id);
      key_name = get_attribute_value (info->key_name);

      if (g_str_equal (element_name, "summary"))
        source_table = info->summaries;
      else if (g_str_equal (element_name, "description"))
        source_table = info->descriptions;

      if (source_table && schema_id && key_name)
        {
          GHashTable *schema_table;
          gchar *normalised;

          schema_table = g_hash_table_lookup (source_table, schema_id);

          if (schema_table == NULL)
            {
              schema_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
              g_hash_table_insert (source_table, g_strdup (schema_id), schema_table);
            }

          normalised = normalise_whitespace (info->string->str);

          if (gettext_domain && normalised[0])
            {
              gchar *translated;

              translated = g_strdup (g_dgettext (gettext_domain, normalised));
              g_free (normalised);
              normalised = translated;
            }

          g_hash_table_insert (schema_table, g_strdup (key_name), normalised);
        }

      g_string_free (info->string, TRUE);
      info->string = NULL;
    }
}

static void
text (GMarkupParseContext  *context,
      const gchar          *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
  TextTableParseInfo *info = user_data;

  if (info->string)
    g_string_append_len (info->string, text, text_len);
}

static void
parse_into_text_tables (const gchar *directory,
                        GHashTable  *summaries,
                        GHashTable  *descriptions)
{
  GMarkupParser parser = { start_element, end_element, text };
  TextTableParseInfo info = { summaries, descriptions };
  const gchar *basename;
  GDir *dir;

  dir = g_dir_open (directory, 0, NULL);
  while ((basename = g_dir_read_name (dir)))
    {
      gchar *filename;
      gchar *contents;
      gsize size;

      filename = g_build_filename (directory, basename, NULL);
      if (g_file_get_contents (filename, &contents, &size, NULL))
        {
          GMarkupParseContext *context;

          context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &info, NULL);
          /* Ignore errors here, this is best effort only. */
          if (g_markup_parse_context_parse (context, contents, size, NULL))
            (void) g_markup_parse_context_end_parse (context, NULL);
          g_markup_parse_context_free (context);

          /* Clean up dangling stuff in case there was an error. */
          g_slist_free_full (info.gettext_domain, g_free);
          g_slist_free_full (info.schema_id, g_free);
          g_slist_free_full (info.key_name, g_free);

          info.gettext_domain = NULL;
          info.schema_id = NULL;
          info.key_name = NULL;

          if (info.string)
            {
              g_string_free (info.string, TRUE);
              info.string = NULL;
            }

          g_free (contents);
        }

      g_free (filename);
    }
  
  g_dir_close (dir);
}

static GHashTable **
g_settings_schema_source_get_text_tables (GSettingsSchemaSource *source)
{
  if (g_once_init_enter (&source->text_tables))
    {
      GHashTable **text_tables;

      text_tables = g_new (GHashTable *, 2);
      text_tables[0] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_hash_table_unref);
      text_tables[1] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_hash_table_unref);

      if (source->directory)
        parse_into_text_tables (source->directory, text_tables[0], text_tables[1]);

      g_once_init_leave (&source->text_tables, text_tables);
    }

  return source->text_tables;
}

/**
 * g_settings_schema_source_list_schemas:
 * @source: a #GSettingsSchemaSource
 * @recursive: if we should recurse
 * @non_relocatable: (out) (transfer full) (array zero-terminated=1): the
 *   list of non-relocatable schemas
 * @relocatable: (out) (transfer full) (array zero-terminated=1): the list
 *   of relocatable schemas
 *
 * Lists the schemas in a given source.
 *
 * If @recursive is %TRUE then include parent sources.  If %FALSE then
 * only include the schemas from one source (ie: one directory).  You
 * probably want %TRUE.
 *
 * Non-relocatable schemas are those for which you can call
 * g_settings_new().  Relocatable schemas are those for which you must
 * use g_settings_new_with_path().
 *
 * Do not call this function from normal programs.  This is designed for
 * use by database editors, commandline tools, etc.
 *
 * Since: 2.40
 **/
void
g_settings_schema_source_list_schemas (GSettingsSchemaSource   *source,
                                       gboolean                 recursive,
                                       gchar                 ***non_relocatable,
                                       gchar                 ***relocatable)
{
  GHashTable *single, *reloc;
  GSettingsSchemaSource *s;

  /* We use hash tables to avoid duplicate listings for schemas that
   * appear in more than one file.
   */
  single = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  reloc = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (s = source; s; s = s->parent)
    {
      gchar **list;
      gint i;

      list = gvdb_table_list (s->table, "");

      /* empty schema cache file? */
      if (list == NULL)
        continue;

      for (i = 0; list[i]; i++)
        {
          if (!g_hash_table_contains (single, list[i]) &&
              !g_hash_table_contains (reloc, list[i]))
            {
              gchar *schema;
              GvdbTable *table;

              schema = g_strdup (list[i]);

              table = gvdb_table_get_table (s->table, list[i]);
              g_assert (table != NULL);

              if (gvdb_table_has_value (table, ".path"))
                g_hash_table_add (single, schema);
              else
                g_hash_table_add (reloc, schema);

              gvdb_table_unref (table);
            }
        }

      g_strfreev (list);

      /* Only the first source if recursive not requested */
      if (!recursive)
        break;
    }

  if (non_relocatable)
    {
      *non_relocatable = (gchar **) g_hash_table_get_keys_as_array (single, NULL);
      g_hash_table_steal_all (single);
    }

  if (relocatable)
    {
      *relocatable = (gchar **) g_hash_table_get_keys_as_array (reloc, NULL);
      g_hash_table_steal_all (reloc);
    }

  g_hash_table_unref (single);
  g_hash_table_unref (reloc);
}

static gchar **non_relocatable_schema_list;
static gchar **relocatable_schema_list;
static gsize schema_lists_initialised;

static void
ensure_schema_lists (void)
{
  if (g_once_init_enter (&schema_lists_initialised))
    {
      initialise_schema_sources ();

      g_settings_schema_source_list_schemas (schema_sources, TRUE,
                                             &non_relocatable_schema_list,
                                             &relocatable_schema_list);

      g_once_init_leave (&schema_lists_initialised, TRUE);
    }
}

/**
 * g_settings_list_schemas:
 *
 * Deprecated.
 *
 * Returns: (element-type utf8) (transfer none):  a list of #GSettings
 *   schemas that are available.  The list must not be modified or
 *   freed.
 *
 * Since: 2.26
 *
 * Deprecated: 2.40: Use g_settings_schema_source_list_schemas() instead.
 * If you used g_settings_list_schemas() to check for the presence of
 * a particular schema, use g_settings_schema_source_lookup() instead
 * of your whole loop.
 **/
const gchar * const *
g_settings_list_schemas (void)
{
  ensure_schema_lists ();

  return (const gchar **) non_relocatable_schema_list;
}

/**
 * g_settings_list_relocatable_schemas:
 *
 * Deprecated.
 *
 * Returns: (element-type utf8) (transfer none): a list of relocatable
 *   #GSettings schemas that are available.  The list must not be
 *   modified or freed.
 *
 * Since: 2.28
 *
 * Deprecated: 2.40: Use g_settings_schema_source_list_schemas() instead
 **/
const gchar * const *
g_settings_list_relocatable_schemas (void)
{
  ensure_schema_lists ();

  return (const gchar **) relocatable_schema_list;
}

/**
 * g_settings_schema_ref:
 * @schema: a #GSettingsSchema
 *
 * Increase the reference count of @schema, returning a new reference.
 *
 * Returns: a new reference to @schema
 *
 * Since: 2.32
 **/
GSettingsSchema *
g_settings_schema_ref (GSettingsSchema *schema)
{
  g_atomic_int_inc (&schema->ref_count);

  return schema;
}

/**
 * g_settings_schema_unref:
 * @schema: a #GSettingsSchema
 *
 * Decrease the reference count of @schema, possibly freeing it.
 *
 * Since: 2.32
 **/
void
g_settings_schema_unref (GSettingsSchema *schema)
{
  if (g_atomic_int_dec_and_test (&schema->ref_count))
    {
      if (schema->extends)
        g_settings_schema_unref (schema->extends);

      g_settings_schema_source_unref (schema->source);
      gvdb_table_unref (schema->table);
      g_free (schema->items);
      g_free (schema->id);

      g_slice_free (GSettingsSchema, schema);
    }
}

const gchar *
g_settings_schema_get_string (GSettingsSchema *schema,
                              const gchar     *key)
{
  const gchar *result = NULL;
  GVariant *value;

  if ((value = gvdb_table_get_raw_value (schema->table, key)))
    {
      result = g_variant_get_string (value, NULL);
      g_variant_unref (value);
    }

  return result;
}

GVariantIter *
g_settings_schema_get_value (GSettingsSchema *schema,
                             const gchar     *key)
{
  GSettingsSchema *s = schema;
  GVariantIter *iter;
  GVariant *value = NULL;

  g_return_val_if_fail (schema != NULL, NULL);

  for (s = schema; s; s = s->extends)
    if ((value = gvdb_table_get_raw_value (s->table, key)))
      break;

  if G_UNLIKELY (value == NULL || !g_variant_is_of_type (value, G_VARIANT_TYPE_TUPLE))
    g_error ("Settings schema '%s' does not contain a key named '%s'", schema->id, key);

  iter = g_variant_iter_new (value);
  g_variant_unref (value);

  return iter;
}

/**
 * g_settings_schema_get_path:
 * @schema: a #GSettingsSchema
 *
 * Gets the path associated with @schema, or %NULL.
 *
 * Schemas may be single-instance or relocatable.  Single-instance
 * schemas correspond to exactly one set of keys in the backend
 * database: those located at the path returned by this function.
 *
 * Relocatable schemas can be referenced by other schemas and can
 * threfore describe multiple sets of keys at different locations.  For
 * relocatable schemas, this function will return %NULL.
 *
 * Returns: (transfer none): the path of the schema, or %NULL
 *
 * Since: 2.32
 **/
const gchar *
g_settings_schema_get_path (GSettingsSchema *schema)
{
  return schema->path;
}

const gchar *
g_settings_schema_get_gettext_domain (GSettingsSchema *schema)
{
  return schema->gettext_domain;
}

/**
 * g_settings_schema_has_key:
 * @schema: a #GSettingsSchema
 * @name: the name of a key
 *
 * Checks if @schema has a key named @name.
 *
 * Returns: %TRUE if such a key exists
 *
 * Since: 2.40
 **/
gboolean
g_settings_schema_has_key (GSettingsSchema *schema,
                           const gchar     *key)
{
  return gvdb_table_has_value (schema->table, key);
}

/**
 * g_settings_schema_list_children:
 * @schema: a #GSettingsSchema
 *
 * Gets the list of children in @schema.
 *
 * You should free the return value with g_strfreev() when you are done
 * with it.
 *
 * Returns: (transfer full) (element-type utf8): a list of the children on @settings
 *
 * Since: 2.44
 */
gchar **
g_settings_schema_list_children (GSettingsSchema *schema)
{
  const GQuark *keys;
  gchar **strv;
  gint n_keys;
  gint i, j;

  g_return_val_if_fail (schema != NULL, NULL);

  keys = g_settings_schema_list (schema, &n_keys);
  strv = g_new (gchar *, n_keys + 1);
  for (i = j = 0; i < n_keys; i++)
    {
      const gchar *key = g_quark_to_string (keys[i]);

      if (g_str_has_suffix (key, "/"))
        {
          gint length = strlen (key);

          strv[j] = g_memdup (key, length);
          strv[j][length - 1] = '\0';
          j++;
        }
    }
  strv[j] = NULL;

  return strv;
}

/**
 * g_settings_schema_list_keys:
 * @schema: a #GSettingsSchema
 *
 * Introspects the list of keys on @schema.
 *
 * You should probably not be calling this function from "normal" code
 * (since you should already know what keys are in your schema).  This
 * function is intended for introspection reasons.
 *
 * Returns: (transfer full) (element-type utf8): a list of the keys on
 *   @schema
 *
 * Since: 2.46
 */
gchar **
g_settings_schema_list_keys (GSettingsSchema *schema)
{
  const GQuark *keys;
  gchar **strv;
  gint n_keys;
  gint i, j;

  g_return_val_if_fail (schema != NULL, NULL);

  keys = g_settings_schema_list (schema, &n_keys);
  strv = g_new (gchar *, n_keys + 1);
  for (i = j = 0; i < n_keys; i++)
    {
      const gchar *key = g_quark_to_string (keys[i]);

      if (!g_str_has_suffix (key, "/"))
        strv[j++] = g_strdup (key);
    }
  strv[j] = NULL;

  return strv;
}

const GQuark *
g_settings_schema_list (GSettingsSchema *schema,
                        gint            *n_items)
{
  if (schema->items == NULL)
    {
      GSettingsSchema *s;
      GHashTableIter iter;
      GHashTable *items;
      gpointer name;
      gint len;
      gint i;

      items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

      for (s = schema; s; s = s->extends)
        {
          gchar **list;

          list = gvdb_table_list (s->table, "");

          if (list)
            {
              for (i = 0; list[i]; i++)
                g_hash_table_add (items, list[i]); /* transfer ownership */

              g_free (list); /* free container only */
            }
        }

      /* Do a first pass to eliminate child items that do not map to
       * valid schemas (ie: ones that would crash us if we actually
       * tried to create them).
       */
      g_hash_table_iter_init (&iter, items);
      while (g_hash_table_iter_next (&iter, &name, NULL))
        if (g_str_has_suffix (name, "/"))
          {
            GSettingsSchemaSource *source;
            GVariant *child_schema;
            GvdbTable *child_table;

            child_schema = gvdb_table_get_raw_value (schema->table, name);
            if (!child_schema)
              continue;

            child_table = NULL;

            for (source = schema->source; source; source = source->parent)
              if ((child_table = gvdb_table_get_table (source->table, g_variant_get_string (child_schema, NULL))))
                break;

            g_variant_unref (child_schema);

            /* Schema is not found -> remove it from the list */
            if (child_table == NULL)
              {
                g_hash_table_iter_remove (&iter);
                continue;
              }

            /* Make sure the schema is relocatable or at the
             * expected path
             */
            if (gvdb_table_has_value (child_table, ".path"))
              {
                GVariant *path;
                gchar *expected;
                gboolean same;

                path = gvdb_table_get_raw_value (child_table, ".path");
                expected = g_strconcat (schema->path, name, NULL);
                same = g_str_equal (expected, g_variant_get_string (path, NULL));
                g_variant_unref (path);
                g_free (expected);

                /* Schema is non-relocatable and did not have the
                 * expected path -> remove it from the list
                 */
                if (!same)
                  g_hash_table_iter_remove (&iter);
              }

            gvdb_table_unref (child_table);
          }

      /* Now create the list */
      len = g_hash_table_size (items);
      schema->items = g_new (GQuark, len);
      i = 0;
      g_hash_table_iter_init (&iter, items);

      while (g_hash_table_iter_next (&iter, &name, NULL))
        schema->items[i++] = g_quark_from_string (name);
      schema->n_items = i;
      g_assert (i == len);

      g_hash_table_unref (items);
    }

  *n_items = schema->n_items;
  return schema->items;
}

/**
 * g_settings_schema_get_id:
 * @schema: a #GSettingsSchema
 *
 * Get the ID of @schema.
 *
 * Returns: (transfer none): the ID
 **/
const gchar *
g_settings_schema_get_id (GSettingsSchema *schema)
{
  return schema->id;
}

static inline void
endian_fixup (GVariant **value)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
  GVariant *tmp;

  tmp = g_variant_byteswap (*value);
  g_variant_unref (*value);
  *value = tmp;
#endif
}

void
g_settings_schema_key_init (GSettingsSchemaKey *key,
                            GSettingsSchema    *schema,
                            const gchar        *name)
{
  GVariantIter *iter;
  GVariant *data;
  guchar code;

  memset (key, 0, sizeof *key);

  iter = g_settings_schema_get_value (schema, name);

  key->schema = g_settings_schema_ref (schema);
  key->default_value = g_variant_iter_next_value (iter);
  endian_fixup (&key->default_value);
  key->type = g_variant_get_type (key->default_value);
  key->name = g_intern_string (name);

  while (g_variant_iter_next (iter, "(y*)", &code, &data))
    {
      switch (code)
        {
        case 'l':
          /* translation requested */
          g_variant_get (data, "(y&s)", &key->lc_char, &key->unparsed);
          break;

        case 'e':
          /* enumerated types... */
          key->is_enum = TRUE;
          goto choice;

        case 'f':
          /* flags... */
          key->is_flags = TRUE;
          goto choice;

        choice: case 'c':
          /* ..., choices, aliases */
          key->strinfo = g_variant_get_fixed_array (data, &key->strinfo_length, sizeof (guint32));
          break;

        case 'r':
          g_variant_get (data, "(**)", &key->minimum, &key->maximum);
          endian_fixup (&key->minimum);
          endian_fixup (&key->maximum);
          break;

        default:
          g_warning ("unknown schema extension '%c'", code);
          break;
        }

      g_variant_unref (data);
    }

  g_variant_iter_free (iter);
}

void
g_settings_schema_key_clear (GSettingsSchemaKey *key)
{
  if (key->minimum)
    g_variant_unref (key->minimum);

  if (key->maximum)
    g_variant_unref (key->maximum);

  g_variant_unref (key->default_value);

  g_settings_schema_unref (key->schema);
}

gboolean
g_settings_schema_key_type_check (GSettingsSchemaKey *key,
                                  GVariant           *value)
{
  g_return_val_if_fail (value != NULL, FALSE);

  return g_variant_is_of_type (value, key->type);
}

GVariant *
g_settings_schema_key_range_fixup (GSettingsSchemaKey *key,
                                   GVariant           *value)
{
  const gchar *target;

  if (g_settings_schema_key_range_check (key, value))
    return g_variant_ref (value);

  if (key->strinfo == NULL)
    return NULL;

  if (g_variant_is_container (value))
    {
      GVariantBuilder builder;
      GVariantIter iter;
      GVariant *child;

      g_variant_iter_init (&iter, value);
      g_variant_builder_init (&builder, g_variant_get_type (value));

      while ((child = g_variant_iter_next_value (&iter)))
        {
          GVariant *fixed;

          fixed = g_settings_schema_key_range_fixup (key, child);
          g_variant_unref (child);

          if (fixed == NULL)
            {
              g_variant_builder_clear (&builder);
              return NULL;
            }

          g_variant_builder_add_value (&builder, fixed);
          g_variant_unref (fixed);
        }

      return g_variant_ref_sink (g_variant_builder_end (&builder));
    }

  target = strinfo_string_from_alias (key->strinfo, key->strinfo_length,
                                      g_variant_get_string (value, NULL));
  return target ? g_variant_ref_sink (g_variant_new_string (target)) : NULL;
}

GVariant *
g_settings_schema_key_get_translated_default (GSettingsSchemaKey *key)
{
  const gchar *translated;
  GError *error = NULL;
  const gchar *domain;
  GVariant *value;

  domain = g_settings_schema_get_gettext_domain (key->schema);

  if (key->lc_char == '\0')
    /* translation not requested for this key */
    return NULL;

  if (key->lc_char == 't')
    translated = g_dcgettext (domain, key->unparsed, LC_TIME);
  else
    translated = g_dgettext (domain, key->unparsed);

  if (translated == key->unparsed)
    /* the default value was not translated */
    return NULL;

  /* try to parse the translation of the unparsed default */
  value = g_variant_parse (key->type, translated, NULL, NULL, &error);

  if (value == NULL)
    {
      g_warning ("Failed to parse translated string '%s' for "
                 "key '%s' in schema '%s': %s", translated, key->name,
                 g_settings_schema_get_id (key->schema), error->message);
      g_warning ("Using untranslated default instead.");
      g_error_free (error);
    }

  else if (!g_settings_schema_key_range_check (key, value))
    {
      g_warning ("Translated default '%s' for key '%s' in schema '%s' "
                 "is outside of valid range", key->unparsed, key->name,
                 g_settings_schema_get_id (key->schema));
      g_variant_unref (value);
      value = NULL;
    }

  return value;
}

gint
g_settings_schema_key_to_enum (GSettingsSchemaKey *key,
                               GVariant           *value)
{
  gboolean it_worked;
  guint result;

  it_worked = strinfo_enum_from_string (key->strinfo, key->strinfo_length,
                                        g_variant_get_string (value, NULL),
                                        &result);

  /* 'value' can only come from the backend after being filtered for validity,
   * from the translation after being filtered for validity, or from the schema
   * itself (which the schema compiler checks for validity).  If this assertion
   * fails then it's really a bug in GSettings or the schema compiler...
   */
  g_assert (it_worked);

  return result;
}

GVariant *
g_settings_schema_key_from_enum (GSettingsSchemaKey *key,
                                 gint                value)
{
  const gchar *string;

  string = strinfo_string_from_enum (key->strinfo, key->strinfo_length, value);

  if (string == NULL)
    return NULL;

  return g_variant_new_string (string);
}

guint
g_settings_schema_key_to_flags (GSettingsSchemaKey *key,
                                GVariant           *value)
{
  GVariantIter iter;
  const gchar *flag;
  guint result;

  result = 0;
  g_variant_iter_init (&iter, value);
  while (g_variant_iter_next (&iter, "&s", &flag))
    {
      gboolean it_worked;
      guint flag_value;

      it_worked = strinfo_enum_from_string (key->strinfo, key->strinfo_length, flag, &flag_value);
      /* as in g_settings_to_enum() */
      g_assert (it_worked);

      result |= flag_value;
    }

  return result;
}

GVariant *
g_settings_schema_key_from_flags (GSettingsSchemaKey *key,
                                  guint               value)
{
  GVariantBuilder builder;
  gint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  for (i = 0; i < 32; i++)
    if (value & (1u << i))
      {
        const gchar *string;

        string = strinfo_string_from_enum (key->strinfo, key->strinfo_length, 1u << i);

        if (string == NULL)
          {
            g_variant_builder_clear (&builder);
            return NULL;
          }

        g_variant_builder_add (&builder, "s", string);
      }

  return g_variant_builder_end (&builder);
}

G_DEFINE_BOXED_TYPE (GSettingsSchemaKey, g_settings_schema_key, g_settings_schema_key_ref, g_settings_schema_key_unref)

/**
 * g_settings_schema_key_ref:
 * @key: a #GSettingsSchemaKey
 *
 * Increase the reference count of @key, returning a new reference.
 *
 * Returns: a new reference to @key
 *
 * Since: 2.40
 **/
GSettingsSchemaKey *
g_settings_schema_key_ref (GSettingsSchemaKey *key)
{
  g_return_val_if_fail (key != NULL, NULL);

  g_atomic_int_inc (&key->ref_count);

  return key;
}

/**
 * g_settings_schema_key_unref:
 * @key: a #GSettingsSchemaKey
 *
 * Decrease the reference count of @key, possibly freeing it.
 *
 * Since: 2.40
 **/
void
g_settings_schema_key_unref (GSettingsSchemaKey *key)
{
  g_return_if_fail (key != NULL);

  if (g_atomic_int_dec_and_test (&key->ref_count))
    {
      g_settings_schema_key_clear (key);

      g_slice_free (GSettingsSchemaKey, key);
    }
}

/**
 * g_settings_schema_get_key:
 * @schema: a #GSettingsSchema
 * @name: the name of a key
 *
 * Gets the key named @name from @schema.
 *
 * It is a programmer error to request a key that does not exist.  See
 * g_settings_schema_list_keys().
 *
 * Returns: (transfer full): the #GSettingsSchemaKey for @name
 *
 * Since: 2.40
 **/
GSettingsSchemaKey *
g_settings_schema_get_key (GSettingsSchema *schema,
                           const gchar     *name)
{
  GSettingsSchemaKey *key;

  g_return_val_if_fail (schema != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  key = g_slice_new (GSettingsSchemaKey);
  g_settings_schema_key_init (key, schema, name);
  key->ref_count = 1;

  return key;
}

/**
 * g_settings_schema_key_get_name:
 * @key: a #GSettingsSchemaKey
 *
 * Gets the name of @key.
 *
 * Returns: the name of @key.
 *
 * Since: 2.44
 */
const gchar *
g_settings_schema_key_get_name (GSettingsSchemaKey *key)
{
  g_return_val_if_fail (key != NULL, NULL);

  return key->name;
}

/**
 * g_settings_schema_key_get_summary:
 * @key: a #GSettingsSchemaKey
 *
 * Gets the summary for @key.
 *
 * If no summary has been provided in the schema for @key, returns
 * %NULL.
 *
 * The summary is a short description of the purpose of the key; usually
 * one short sentence.  Summaries can be translated and the value
 * returned from this function is is the current locale.
 *
 * This function is slow.  The summary and description information for
 * the schemas is not stored in the compiled schema database so this
 * function has to parse all of the source XML files in the schema
 * directory.
 *
 * Returns: the summary for @key, or %NULL
 *
 * Since: 2.34
 **/
const gchar *
g_settings_schema_key_get_summary (GSettingsSchemaKey *key)
{
  GHashTable **text_tables;
  GHashTable *summaries;

  text_tables = g_settings_schema_source_get_text_tables (key->schema->source);
  summaries = g_hash_table_lookup (text_tables[0], key->schema->id);

  return summaries ? g_hash_table_lookup (summaries, key->name) : NULL;
}

/**
 * g_settings_schema_key_get_description:
 * @key: a #GSettingsSchemaKey
 *
 * Gets the description for @key.
 *
 * If no description has been provided in the schema for @key, returns
 * %NULL.
 *
 * The description can be one sentence to several paragraphs in length.
 * Paragraphs are delimited with a double newline.  Descriptions can be
 * translated and the value returned from this function is is the
 * current locale.
 *
 * This function is slow.  The summary and description information for
 * the schemas is not stored in the compiled schema database so this
 * function has to parse all of the source XML files in the schema
 * directory.
 *
 * Returns: the description for @key, or %NULL
 *
 * Since: 2.34
 **/
const gchar *
g_settings_schema_key_get_description (GSettingsSchemaKey *key)
{
  GHashTable **text_tables;
  GHashTable *descriptions;

  text_tables = g_settings_schema_source_get_text_tables (key->schema->source);
  descriptions = g_hash_table_lookup (text_tables[1], key->schema->id);

  return descriptions ? g_hash_table_lookup (descriptions, key->name) : NULL;
}

/**
 * g_settings_schema_key_get_value_type:
 * @key: a #GSettingsSchemaKey
 *
 * Gets the #GVariantType of @key.
 *
 * Returns: (transfer none): the type of @key
 *
 * Since: 2.40
 **/
const GVariantType *
g_settings_schema_key_get_value_type (GSettingsSchemaKey *key)
{
  g_return_val_if_fail (key, NULL);

  return key->type;
}

/**
 * g_settings_schema_key_get_default_value:
 * @key: a #GSettingsSchemaKey
 *
 * Gets the default value for @key.
 *
 * Note that this is the default value according to the schema.  System
 * administrator defaults and lockdown are not visible via this API.
 *
 * Returns: (transfer full): the default value for the key
 *
 * Since: 2.40
 **/
GVariant *
g_settings_schema_key_get_default_value (GSettingsSchemaKey *key)
{
  GVariant *value;

  g_return_val_if_fail (key, NULL);

  value = g_settings_schema_key_get_translated_default (key);

  if (!value)
    value = g_variant_ref (key->default_value);

  return value;
}

/**
 * g_settings_schema_key_get_range:
 * @key: a #GSettingsSchemaKey
 *
 * Queries the range of a key.
 *
 * This function will return a #GVariant that fully describes the range
 * of values that are valid for @key.
 *
 * The type of #GVariant returned is `(sv)`. The string describes
 * the type of range restriction in effect. The type and meaning of
 * the value contained in the variant depends on the string.
 *
 * If the string is `'type'` then the variant contains an empty array.
 * The element type of that empty array is the expected type of value
 * and all values of that type are valid.
 *
 * If the string is `'enum'` then the variant contains an array
 * enumerating the possible values. Each item in the array is
 * a possible valid value and no other values are valid.
 *
 * If the string is `'flags'` then the variant contains an array. Each
 * item in the array is a value that may appear zero or one times in an
 * array to be used as the value for this key. For example, if the
 * variant contained the array `['x', 'y']` then the valid values for
 * the key would be `[]`, `['x']`, `['y']`, `['x', 'y']` and
 * `['y', 'x']`.
 *
 * Finally, if the string is `'range'` then the variant contains a pair
 * of like-typed values -- the minimum and maximum permissible values
 * for this key.
 *
 * This information should not be used by normal programs.  It is
 * considered to be a hint for introspection purposes.  Normal programs
 * should already know what is permitted by their own schema.  The
 * format may change in any way in the future -- but particularly, new
 * forms may be added to the possibilities described above.
 *
 * You should free the returned value with g_variant_unref() when it is
 * no longer needed.
 *
 * Returns: (transfer full): a #GVariant describing the range
 *
 * Since: 2.40
 **/
GVariant *
g_settings_schema_key_get_range (GSettingsSchemaKey *key)
{
  const gchar *type;
  GVariant *range;

  if (key->minimum)
    {
      range = g_variant_new ("(**)", key->minimum, key->maximum);
      type = "range";
    }
  else if (key->strinfo)
    {
      range = strinfo_enumerate (key->strinfo, key->strinfo_length);
      type = key->is_flags ? "flags" : "enum";
    }
  else
    {
      range = g_variant_new_array (key->type, NULL, 0);
      type = "type";
    }

  return g_variant_ref_sink (g_variant_new ("(sv)", type, range));
}

/**
 * g_settings_schema_key_range_check:
 * @key: a #GSettingsSchemaKey
 * @value: the value to check
 *
 * Checks if the given @value is of the correct type and within the
 * permitted range for @key.
 *
 * It is a programmer error if @value is not of the correct type -- you
 * must check for this first.
 *
 * Returns: %TRUE if @value is valid for @key
 *
 * Since: 2.40
 **/
gboolean
g_settings_schema_key_range_check (GSettingsSchemaKey *key,
                                   GVariant           *value)
{
  if (key->minimum == NULL && key->strinfo == NULL)
    return TRUE;

  if (g_variant_is_container (value))
    {
      gboolean ok = TRUE;
      GVariantIter iter;
      GVariant *child;

      g_variant_iter_init (&iter, value);
      while (ok && (child = g_variant_iter_next_value (&iter)))
        {
          ok = g_settings_schema_key_range_check (key, child);
          g_variant_unref (child);
        }

      return ok;
    }

  if (key->minimum)
    {
      return g_variant_compare (key->minimum, value) <= 0 &&
             g_variant_compare (value, key->maximum) <= 0;
    }

  return strinfo_is_string_valid (key->strinfo, key->strinfo_length,
                                  g_variant_get_string (value, NULL));
}
