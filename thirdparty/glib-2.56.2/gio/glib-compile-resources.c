/*
 * Copyright © 2011 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <glib.h>
#include <gstdio.h>
#include <gi18n.h>
#include <gioenums.h>

#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <errno.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif

#include <gio/gmemoryoutputstream.h>
#include <gio/gzlibcompressor.h>
#include <gio/gconverteroutputstream.h>

#include <glib.h>
#include "gvdb/gvdb-builder.h"

#include "gconstructor_as_data.h"

#ifdef G_OS_WIN32
#include "glib/glib-private.h"
#endif

typedef struct
{
  char *filename;
  char *content;
  gsize content_size;
  gsize size;
  guint32 flags;
} FileData;

typedef struct
{
  GHashTable *table; /* resource path -> FileData */

  gboolean collect_data;

  /* per gresource */
  char *prefix;

  /* per file */
  char *alias;
  gboolean compressed;
  char *preproc_options;

  GString *string;  /* non-NULL when accepting text */
} ParseState;

static gchar **sourcedirs = NULL;
static gchar *xmllint = NULL;
static gchar *gdk_pixbuf_pixdata = NULL;

static void
file_data_free (FileData *data)
{
  g_free (data->filename);
  g_free (data->content);
  g_free (data);
}

static void
start_element (GMarkupParseContext  *context,
	       const gchar          *element_name,
	       const gchar         **attribute_names,
	       const gchar         **attribute_values,
	       gpointer              user_data,
	       GError              **error)
{
  ParseState *state = user_data;
  const GSList *element_stack;
  const gchar *container;

  element_stack = g_markup_parse_context_get_element_stack (context);
  container = element_stack->next ? element_stack->next->data : NULL;

#define COLLECT(first, ...) \
  g_markup_collect_attributes (element_name,                                 \
			       attribute_names, attribute_values, error,     \
			       first, __VA_ARGS__, G_MARKUP_COLLECT_INVALID)
#define OPTIONAL   G_MARKUP_COLLECT_OPTIONAL
#define STRDUP     G_MARKUP_COLLECT_STRDUP
#define STRING     G_MARKUP_COLLECT_STRING
#define BOOL       G_MARKUP_COLLECT_BOOLEAN
#define NO_ATTRS()  COLLECT (G_MARKUP_COLLECT_INVALID, NULL)

  if (container == NULL)
    {
      if (strcmp (element_name, "gresources") == 0)
	return;
    }
  else if (strcmp (container, "gresources") == 0)
    {
      if (strcmp (element_name, "gresource") == 0)
	{
	  COLLECT (OPTIONAL | STRDUP,
		   "prefix", &state->prefix);
	  return;
	}
    }
  else if (strcmp (container, "gresource") == 0)
    {
      if (strcmp (element_name, "file") == 0)
	{
	  COLLECT (OPTIONAL | STRDUP, "alias", &state->alias,
		   OPTIONAL | BOOL, "compressed", &state->compressed,
                   OPTIONAL | STRDUP, "preprocess", &state->preproc_options);
	  state->string = g_string_new ("");
	  return;
	}
    }

  if (container)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		 _("Element <%s> not allowed inside <%s>"),
		 element_name, container);
  else
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		 _("Element <%s> not allowed at toplevel"), element_name);

}

static GvdbItem *
get_parent (GHashTable *table,
	    gchar      *key,
	    gint        length)
{
  GvdbItem *grandparent, *parent;

  if (length == 1)
    return NULL;

  while (key[--length - 1] != '/');
  key[length] = '\0';

  parent = g_hash_table_lookup (table, key);

  if (parent == NULL)
    {
      parent = gvdb_hash_table_insert (table, key);

      grandparent = get_parent (table, key, length);

      if (grandparent != NULL)
	gvdb_item_set_parent (parent, grandparent);
    }

  return parent;
}

static gchar *
find_file (const gchar *filename)
{
  guint i;
  gchar *real_file;
  gboolean exists;

  if (g_path_is_absolute (filename))
    return g_strdup (filename);

  /* search all the sourcedirs for the correct files in order */
  for (i = 0; sourcedirs[i] != NULL; i++)
    {
	real_file = g_build_path ("/", sourcedirs[i], filename, NULL);
	exists = g_file_test (real_file, G_FILE_TEST_EXISTS);
	if (exists)
	  return real_file;
	g_free (real_file);
    }
    return NULL;
}

static void
end_element (GMarkupParseContext  *context,
	     const gchar          *element_name,
	     gpointer              user_data,
	     GError              **error)
{
  ParseState *state = user_data;
  GError *my_error = NULL;

  if (strcmp (element_name, "gresource") == 0)
    {
      g_free (state->prefix);
      state->prefix = NULL;
    }

  else if (strcmp (element_name, "file") == 0)
    {
      gchar *file;
      gchar *real_file = NULL;
      gchar *key;
      FileData *data = NULL;
      char *tmp_file = NULL;
      char *tmp_file2 = NULL;

      file = state->string->str;
      key = file;
      if (state->alias)
	key = state->alias;

      if (state->prefix)
	key = g_build_path ("/", "/", state->prefix, key, NULL);
      else
	key = g_build_path ("/", "/", key, NULL);

      if (g_hash_table_lookup (state->table, key) != NULL)
	{
	  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
		       _("File %s appears multiple times in the resource"),
		       key);
	  return;
	}

      if (sourcedirs != NULL)
        {
	  real_file = find_file (file);
	  if (real_file == NULL && state->collect_data)
	    {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			     _("Failed to locate “%s” in any source directory"), file);
		return;
	    }
	}
      else
        {
	  gboolean exists;
	  exists = g_file_test (file, G_FILE_TEST_EXISTS);
	  if (!exists && state->collect_data)
	    {
	      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   _("Failed to locate “%s” in current directory"), file);
	      return;
	    }
	}

      if (real_file == NULL)
        real_file = g_strdup (file);

      data = g_new0 (FileData, 1);
      data->filename = g_strdup (real_file);
      if (!state->collect_data)
        goto done;

      if (state->preproc_options)
        {
          gchar **options;
          guint i;
          gboolean xml_stripblanks = FALSE;
          gboolean to_pixdata = FALSE;

          options = g_strsplit (state->preproc_options, ",", -1);

          for (i = 0; options[i]; i++)
            {
              if (!strcmp (options[i], "xml-stripblanks"))
                xml_stripblanks = TRUE;
              else if (!strcmp (options[i], "to-pixdata"))
                to_pixdata = TRUE;
              else
                {
                  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                               _("Unknown processing option “%s”"), options[i]);
                  g_strfreev (options);
                  goto cleanup;
                }
            }
          g_strfreev (options);

          if (xml_stripblanks && xmllint != NULL)
            {
              int fd;
	      GSubprocess *proc;

              tmp_file = g_strdup ("resource-XXXXXXXX");
              if ((fd = g_mkstemp (tmp_file)) == -1)
                {
                  int errsv = errno;

                  g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                               _("Failed to create temp file: %s"),
                              g_strerror (errsv));
                  g_free (tmp_file);
                  tmp_file = NULL;
                  goto cleanup;
                }
              close (fd);

              proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE, error,
                                       xmllint, "--nonet", "--noblanks", "--output", tmp_file, real_file, NULL);
              g_free (real_file);
	      real_file = NULL;

	      if (!proc)
		goto cleanup;

	      if (!g_subprocess_wait_check (proc, NULL, error))
		{
		  g_object_unref (proc);
                  goto cleanup;
                }

	      g_object_unref (proc);

              real_file = g_strdup (tmp_file);
            }

          if (to_pixdata)
            {
              int fd;
	      GSubprocess *proc;

              if (gdk_pixbuf_pixdata == NULL)
                {
                  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                       "to-pixbuf preprocessing requested but GDK_PIXBUF_PIXDATA "
                                       "not set and gdk-pixbuf-pixdata not found in path");
                  goto cleanup;
                }

              tmp_file2 = g_strdup ("resource-XXXXXXXX");
              if ((fd = g_mkstemp (tmp_file2)) == -1)
                {
                  int errsv = errno;

                  g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                               _("Failed to create temp file: %s"),
			       g_strerror (errsv));
                  g_free (tmp_file2);
                  tmp_file2 = NULL;
                  goto cleanup;
                }
              close (fd);

              proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE, error,
                                       gdk_pixbuf_pixdata, real_file, tmp_file2, NULL);
              g_free (real_file);
              real_file = NULL;

	      if (!g_subprocess_wait_check (proc, NULL, error))
		{
		  g_object_unref (proc);
                  goto cleanup;
		}

	      g_object_unref (proc);

              real_file = g_strdup (tmp_file2);
            }
	}

      if (!g_file_get_contents (real_file, &data->content, &data->size, &my_error))
	{
	  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
		       _("Error reading file %s: %s"),
		       real_file, my_error->message);
	  g_clear_error (&my_error);
	  goto cleanup;
	}
      /* Include zero termination in content_size for uncompressed files (but not in size) */
      data->content_size = data->size + 1;

      if (state->compressed)
	{
	  GOutputStream *out = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
	  GZlibCompressor *compressor =
	    g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_ZLIB, 9);
	  GOutputStream *out2 = g_converter_output_stream_new (out, G_CONVERTER (compressor));

	  if (!g_output_stream_write_all (out2, data->content, data->size,
					  NULL, NULL, NULL) ||
	      !g_output_stream_close (out2, NULL, NULL))
	    {
	      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			   _("Error compressing file %s"),
			   real_file);
	      goto cleanup;
	    }

	  g_free (data->content);
	  data->content_size = g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (out));
	  data->content = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (out));

	  g_object_unref (compressor);
	  g_object_unref (out);
	  g_object_unref (out2);

	  data->flags |= G_RESOURCE_FLAGS_COMPRESSED;
	}

done:
      g_hash_table_insert (state->table, key, data);
      data = NULL;

    cleanup:
      /* Cleanup */

      g_free (state->alias);
      state->alias = NULL;
      g_string_free (state->string, TRUE);
      state->string = NULL;
      g_free (state->preproc_options);
      state->preproc_options = NULL;

      g_free (real_file);

      if (tmp_file)
        {
          unlink (tmp_file);
          g_free (tmp_file);
        }

      if (tmp_file2)
        {
          unlink (tmp_file2);
          g_free (tmp_file2);
        }

      if (data != NULL)
        file_data_free (data);
    }
}

static void
text (GMarkupParseContext  *context,
      const gchar          *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
  ParseState *state = user_data;
  gsize i;

  for (i = 0; i < text_len; i++)
    if (!g_ascii_isspace (text[i]))
      {
	if (state->string)
	  g_string_append_len (state->string, text, text_len);

	else
	  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
		       _("text may not appear inside <%s>"),
		       g_markup_parse_context_get_element (context));

	break;
      }
}

static GHashTable *
parse_resource_file (const gchar *filename,
                     gboolean     collect_data,
                     GHashTable  *files)
{
  GMarkupParser parser = { start_element, end_element, text };
  ParseState state = { 0, };
  GMarkupParseContext *context;
  GError *error = NULL;
  gchar *contents;
  GHashTable *table = NULL;
  gsize size;

  if (!g_file_get_contents (filename, &contents, &size, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  state.collect_data = collect_data;
  state.table = g_hash_table_ref (files);

  context = g_markup_parse_context_new (&parser,
					G_MARKUP_TREAT_CDATA_AS_TEXT |
					G_MARKUP_PREFIX_ERROR_POSITION,
					&state, NULL);

  if (!g_markup_parse_context_parse (context, contents, size, &error) ||
      !g_markup_parse_context_end_parse (context, &error))
    {
      g_printerr ("%s: %s.\n", filename, error->message);
      g_clear_error (&error);
    }
  else
    {
      GHashTableIter iter;
      const char *key;
      char *mykey;
      gsize key_len;
      FileData *data;
      GVariant *v_data;
      GVariantBuilder builder;
      GvdbItem *item;

      table = gvdb_hash_table_new (NULL, NULL);

      g_hash_table_iter_init (&iter, state.table);
      while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&data))
	{
	  key_len = strlen (key);
	  mykey = g_strdup (key);

	  item = gvdb_hash_table_insert (table, key);
	  gvdb_item_set_parent (item,
				get_parent (table, mykey, key_len));

	  g_free (mykey);

	  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(uuay)"));

	  g_variant_builder_add (&builder, "u", data->size); /* Size */
	  g_variant_builder_add (&builder, "u", data->flags); /* Flags */

	  v_data = g_variant_new_from_data (G_VARIANT_TYPE("ay"),
					    data->content, data->content_size, TRUE,
					    g_free, data->content);
	  g_variant_builder_add_value (&builder, v_data);
	  data->content = NULL; /* Take ownership */

	  gvdb_item_set_value (item,
			       g_variant_builder_end (&builder));
	}
    }

  g_hash_table_unref (state.table);
  g_markup_parse_context_free (context);
  g_free (contents);

  return table;
}

static gboolean
write_to_file (GHashTable   *table,
	       const gchar  *filename,
	       GError      **error)
{
  gboolean success;

  success = gvdb_table_write_contents (table, filename,
				       G_BYTE_ORDER != G_LITTLE_ENDIAN,
				       error);

  return success;
}

static gboolean
extension_in_set (const char *str,
                  ...)
{
  va_list list;
  const char *ext, *value;
  gboolean rv = FALSE;

  ext = strrchr (str, '.');
  if (ext == NULL)
    return FALSE;

  ext++;
  va_start (list, str);
  while ((value = va_arg (list, const char *)) != NULL)
    {
      if (g_ascii_strcasecmp (ext, value) != 0)
        continue;

      rv = TRUE;
      break;
    }

  va_end (list);
  return rv;
}

/*
 * We must escape any characters that `make` finds significant.
 * This is largely a duplicate of the logic in gcc's `mkdeps.c:munge()`.
 */
static char *
escape_makefile_string (const char *string)
{
  GString *str;
  const char *p, *q;

  str = g_string_sized_new (strlen (string) + 1);
  for (p = string; *p != '\0'; ++p)
    {
      switch (*p)
        {
        case ' ':
        case '\t':
          /* GNU make uses a weird quoting scheme for white space.
             A space or tab preceded by 2N+1 backslashes represents
             N backslashes followed by space; a space or tab
             preceded by 2N backslashes represents N backslashes at
             the end of a file name; and backslashes in other
             contexts should not be doubled.  */
          for (q = p - 1; string <= q && *q == '\\';  q--)
            g_string_append_c (str, '\\');
          g_string_append_c (str, '\\');
          break;

        case '$':
          g_string_append_c (str, '$');
          break;

        case '#':
          g_string_append_c (str, '\\');
          break;
        }
      g_string_append_c (str, *p);
    }

  return g_string_free (str, FALSE);
}

int
main (int argc, char **argv)
{
  GError *error;
  GHashTable *table;
  GHashTable *files;
  gchar *srcfile;
  gboolean show_version_and_exit = FALSE;
  gchar *target = NULL;
  gchar *binary_target = NULL;
  gboolean generate_automatic = FALSE;
  gboolean generate_source = FALSE;
  gboolean generate_header = FALSE;
  gboolean manual_register = FALSE;
  gboolean internal = FALSE;
  gboolean generate_dependencies = FALSE;
  gboolean generate_phony_targets = FALSE;
  char *dependency_file = NULL;
  char *c_name = NULL;
  char *c_name_no_underscores;
  const char *linkage = "extern";
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "version", 0, 0, G_OPTION_ARG_NONE, &show_version_and_exit, N_("Show program version and exit"), NULL },
    { "target", 0, 0, G_OPTION_ARG_FILENAME, &target, N_("name of the output file"), N_("FILE") },
    { "sourcedir", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &sourcedirs, N_("The directories where files are to be read from (default to current directory)"), N_("DIRECTORY") },
    { "generate", 0, 0, G_OPTION_ARG_NONE, &generate_automatic, N_("Generate output in the format selected for by the target filename extension"), NULL },
    { "generate-header", 0, 0, G_OPTION_ARG_NONE, &generate_header, N_("Generate source header"), NULL },
    { "generate-source", 0, 0, G_OPTION_ARG_NONE, &generate_source, N_("Generate sourcecode used to link in the resource file into your code"), NULL },
    { "generate-dependencies", 0, 0, G_OPTION_ARG_NONE, &generate_dependencies, N_("Generate dependency list"), NULL },
    { "dependency-file", 0, 0, G_OPTION_ARG_FILENAME, &dependency_file, N_("name of the dependency file to generate"), N_("FILE") },
    { "generate-phony-targets", 0, 0, G_OPTION_ARG_NONE, &generate_phony_targets, N_("Include phony targets in the generated dependency file"), NULL },
    { "manual-register", 0, 0, G_OPTION_ARG_NONE, &manual_register, N_("Don’t automatically create and register resource"), NULL },
    { "internal", 0, 0, G_OPTION_ARG_NONE, &internal, N_("Don’t export functions; declare them G_GNUC_INTERNAL"), NULL },
    { "c-name", 0, 0, G_OPTION_ARG_STRING, &c_name, N_("C identifier name used for the generated source code"), NULL },
    { NULL }
  };

#ifdef G_OS_WIN32
  gchar *tmp;
#endif

  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);

#ifdef G_OS_WIN32
  tmp = _glib_get_locale_dir ();
  bindtextdomain (GETTEXT_PACKAGE, tmp);
  g_free (tmp);
#else
  bindtextdomain (GETTEXT_PACKAGE, GLIB_LOCALE_DIR);
#endif

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  context = g_option_context_new (N_("FILE"));
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_summary (context,
    N_("Compile a resource specification into a resource file.\n"
       "Resource specification files have the extension .gresource.xml,\n"
       "and the resource file have the extension called .gresource."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  error = NULL;
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }

  g_option_context_free (context);

  if (show_version_and_exit)
    {
      g_print (PACKAGE_VERSION "\n");
      return 0;
    }

  if (argc != 2)
    {
      g_printerr (_("You should give exactly one file name\n"));
      g_free (c_name);
      return 1;
    }

  if (internal)
    linkage = "G_GNUC_INTERNAL";

  srcfile = argv[1];

  xmllint = g_strdup (g_getenv ("XMLLINT"));
  if (xmllint == NULL)
    xmllint = g_find_program_in_path ("xmllint");
  if (xmllint == NULL)
    g_printerr ("XMLLINT not set and xmllint not found in path; skipping xml preprocessing.\n");

  gdk_pixbuf_pixdata = g_strdup (g_getenv ("GDK_PIXBUF_PIXDATA"));
  if (gdk_pixbuf_pixdata == NULL)
    gdk_pixbuf_pixdata = g_find_program_in_path ("gdk-pixbuf-pixdata");

  if (target == NULL)
    {
      char *dirname = g_path_get_dirname (srcfile);
      char *base = g_path_get_basename (srcfile);
      char *target_basename;
      if (g_str_has_suffix (base, ".xml"))
	base[strlen(base) - strlen (".xml")] = 0;

      if (generate_source)
	{
	  if (g_str_has_suffix (base, ".gresource"))
	    base[strlen(base) - strlen (".gresource")] = 0;
	  target_basename = g_strconcat (base, ".c", NULL);
	}
      else if (generate_header)
        {
          if (g_str_has_suffix (base, ".gresource"))
            base[strlen(base) - strlen (".gresource")] = 0;
          target_basename = g_strconcat (base, ".h", NULL);
        }
      else
	{
	  if (g_str_has_suffix (base, ".gresource"))
	    target_basename = g_strdup (base);
	  else
	    target_basename = g_strconcat (base, ".gresource", NULL);
	}

      target = g_build_filename (dirname, target_basename, NULL);
      g_free (target_basename);
      g_free (dirname);
      g_free (base);
    }
  else if (generate_automatic)
    {
      if (extension_in_set (target, "c", "cc", "cpp", "cxx", "c++", NULL))
        generate_source = TRUE;
      else if (extension_in_set (target, "h", "hh", "hpp", "hxx", "h++", NULL))
        generate_header = TRUE;
      else if (extension_in_set (target, "gresource", NULL))
        { }
    }

  files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)file_data_free);

  if ((table = parse_resource_file (srcfile, !generate_dependencies, files)) == NULL)
    {
      g_free (target);
      g_free (c_name);
      g_hash_table_unref (files);
      return 1;
    }

  /* This can be used in the same invocation
     as other generate commands */
  if (dependency_file != NULL)
    {
      /* Generate a .d file that describes the dependencies for
       * build tools, gcc -M -MF style */
      GString *dep_string;
      GHashTableIter iter;
      gpointer key, data;
      FileData *file_data;
      char *escaped;

      g_hash_table_iter_init (&iter, files);

      dep_string = g_string_new (NULL);
      escaped = escape_makefile_string (srcfile);
      g_string_printf (dep_string, "%s:", escaped);
      g_free (escaped);

      /* First rule: foo.xml: resource1 resource2.. */
      while (g_hash_table_iter_next (&iter, &key, &data))
        {
          file_data = data;
          if (!g_str_equal (file_data->filename, srcfile))
            {
              escaped = escape_makefile_string (file_data->filename);
              g_string_append_printf (dep_string, " %s", escaped);
              g_free (escaped);
            }
        }

      g_string_append (dep_string, "\n");

      /* Optionally include phony targets as it silences `make` but
       * isn't supported on `ninja` at the moment. See also: `gcc -MP`
       */
      if (generate_phony_targets)
        {
					g_string_append (dep_string, "\n");

          /* One rule for every resource: resourceN: */
          g_hash_table_iter_init (&iter, files);
          while (g_hash_table_iter_next (&iter, &key, &data))
            {
              file_data = data;
              if (!g_str_equal (file_data->filename, srcfile))
                {
                  escaped = escape_makefile_string (file_data->filename);
                  g_string_append_printf (dep_string, "%s:\n\n", escaped);
                  g_free (escaped);
                }
            }
        }

      if (g_str_equal (dependency_file, "-"))
        {
          g_print ("%s\n", dep_string->str);
        }
      else
        {
          if (!g_file_set_contents (dependency_file, dep_string->str, dep_string->len, &error))
            {
              g_printerr ("Error writing dependency file: %s\n", error->message);
              g_string_free (dep_string, TRUE);
              g_free (dependency_file);
              g_error_free (error);
              g_hash_table_unref (files);
              return 1;
            }
        }

      g_string_free (dep_string, TRUE);
      g_free (dependency_file);
    }

  if (generate_dependencies)
    {
      GHashTableIter iter;
      gpointer key, data;
      FileData *file_data;

      g_hash_table_iter_init (&iter, files);

      /* Generate list of files for direct use as dependencies in a Makefile */
      while (g_hash_table_iter_next (&iter, &key, &data))
        {
          file_data = data;
          g_print ("%s\n", file_data->filename);
        }
    }
  else if (generate_source || generate_header)
    {
      if (generate_source)
	{
	  int fd = g_file_open_tmp (NULL, &binary_target, NULL);
	  if (fd == -1)
	    {
	      g_printerr ("Can't open temp file\n");
	      g_free (c_name);
              g_hash_table_unref (files);
	      return 1;
	    }
	  close (fd);
	}

      if (c_name == NULL)
	{
	  char *base = g_path_get_basename (srcfile);
	  GString *s;
	  char *dot;
	  int i;

	  /* Remove extensions */
	  dot = strchr (base, '.');
	  if (dot)
	    *dot = 0;

	  s = g_string_new ("");

	  for (i = 0; base[i] != 0; i++)
	    {
	      const char *first = G_CSET_A_2_Z G_CSET_a_2_z "_";
	      const char *rest = G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "_";
	      if (strchr ((i == 0) ? first : rest, base[i]) != NULL)
		g_string_append_c (s, base[i]);
	      else if (base[i] == '-')
		g_string_append_c (s, '_');

	    }

	  c_name = g_string_free (s, FALSE);
	}
    }
  else
    binary_target = g_strdup (target);

  c_name_no_underscores = c_name;
  while (c_name_no_underscores && *c_name_no_underscores == '_')
    c_name_no_underscores++;

  if (binary_target != NULL &&
      !write_to_file (table, binary_target, &error))
    {
      g_printerr ("%s\n", error->message);
      g_free (target);
      g_free (c_name);
      g_hash_table_unref (files);
      return 1;
    }

  if (generate_header)
    {
      FILE *file;

      file = fopen (target, "w");
      if (file == NULL)
	{
	  g_printerr ("can't write to file %s", target);
	  g_free (c_name);
          g_hash_table_unref (files);
	  return 1;
	}

      fprintf (file,
	       "#ifndef __RESOURCE_%s_H__\n"
	       "#define __RESOURCE_%s_H__\n"
	       "\n"
	       "#include <gio/gio.h>\n"
	       "\n"
	       "%s GResource *%s_get_resource (void);\n",
	       c_name, c_name, linkage, c_name);

      if (manual_register)
	fprintf (file,
		 "\n"
		 "%s void %s_register_resource (void);\n"
		 "%s void %s_unregister_resource (void);\n"
		 "\n",
		 linkage, c_name, linkage, c_name);

      fprintf (file,
	       "#endif\n");

      fclose (file);
    }
  else if (generate_source)
    {
      FILE *file;
      guint8 *data;
      gsize data_size;
      gsize i;

      if (!g_file_get_contents (binary_target, (char **)&data,
				&data_size, NULL))
	{
	  g_printerr ("can't read back temporary file");
	  g_free (c_name);
          g_hash_table_unref (files);
	  return 1;
	}
      g_unlink (binary_target);

      file = fopen (target, "w");
      if (file == NULL)
	{
	  g_printerr ("can't write to file %s", target);
	  g_free (c_name);
          g_hash_table_unref (files);
	  return 1;
	}

      fprintf (file,
	       "#include <gio/gio.h>\n"
	       "\n"
	       "#if defined (__ELF__) && ( __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 6))\n"
	       "# define SECTION __attribute__ ((section (\".gresource.%s\"), aligned (8)))\n"
	       "#else\n"
	       "# define SECTION\n"
	       "#endif\n"
	       "\n"
	       "static const SECTION union { const guint8 data[%"G_GSIZE_FORMAT"]; const double alignment; void * const ptr;}  %s_resource_data = { {\n",
	       c_name_no_underscores, data_size, c_name);

      for (i = 0; i < data_size; i++) {
	if (i % 8 == 0)
	  fprintf (file, "  ");
	fprintf (file, "0x%2.2x", (int)data[i]);
	if (i != data_size - 1)
	  fprintf (file, ", ");
	if ((i % 8 == 7) || (i == data_size - 1))
	  fprintf (file, "\n");
      }

      fprintf (file, "} };\n");

      fprintf (file,
	       "\n"
	       "static GStaticResource static_resource = { %s_resource_data.data, sizeof (%s_resource_data.data), NULL, NULL, NULL };\n"
	       "%s GResource *%s_get_resource (void);\n"
	       "GResource *%s_get_resource (void)\n"
	       "{\n"
	       "  return g_static_resource_get_resource (&static_resource);\n"
	       "}\n",
	       c_name, c_name, linkage, c_name, c_name);


      if (manual_register)
	{
	  fprintf (file,
		   "\n"
		   "%s void %s_unregister_resource (void);\n"
		   "void %s_unregister_resource (void)\n"
		   "{\n"
		   "  g_static_resource_fini (&static_resource);\n"
		   "}\n"
		   "\n"
		   "%s void %s_register_resource (void);\n"
		   "void %s_register_resource (void)\n"
		   "{\n"
		   "  g_static_resource_init (&static_resource);\n"
		   "}\n",
		   linkage, c_name, c_name, linkage, c_name, c_name);
	}
      else
	{
	  fprintf (file, "%s", gconstructor_code);
	  fprintf (file,
		   "\n"
		   "#ifdef G_HAS_CONSTRUCTORS\n"
		   "\n"
		   "#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA\n"
		   "#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(resource_constructor)\n"
		   "#endif\n"
		   "G_DEFINE_CONSTRUCTOR(resource_constructor)\n"
		   "#ifdef G_DEFINE_DESTRUCTOR_NEEDS_PRAGMA\n"
		   "#pragma G_DEFINE_DESTRUCTOR_PRAGMA_ARGS(resource_destructor)\n"
		   "#endif\n"
		   "G_DEFINE_DESTRUCTOR(resource_destructor)\n"
		   "\n"
		   "#else\n"
		   "#warning \"Constructor not supported on this compiler, linking in resources will not work\"\n"
		   "#endif\n"
		   "\n"
		   "static void resource_constructor (void)\n"
		   "{\n"
		   "  g_static_resource_init (&static_resource);\n"
		   "}\n"
		   "\n"
		   "static void resource_destructor (void)\n"
		   "{\n"
		   "  g_static_resource_fini (&static_resource);\n"
		   "}\n");
	}

      fclose (file);

      g_free (data);
    }

  g_free (binary_target);
  g_free (target);
  g_hash_table_destroy (table);
  g_free (xmllint);
  g_free (c_name);
  g_hash_table_unref (files);

  return 0;
}
