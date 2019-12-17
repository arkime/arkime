/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 */

#include "config.h"
#include "gfilenamecompleter.h"
#include "gfileenumerator.h"
#include "gfileattribute.h"
#include "gfile.h"
#include "gfileinfo.h"
#include "gcancellable.h"
#include <string.h>
#include "glibintl.h"


/**
 * SECTION:gfilenamecompleter
 * @short_description: Filename Completer
 * @include: gio/gio.h
 * 
 * Completes partial file and directory names given a partial string by
 * looking in the file system for clues. Can return a list of possible 
 * completion strings for widget implementations.
 * 
 **/

enum {
  GOT_COMPLETION_DATA,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
  GFilenameCompleter *completer;
  GFileEnumerator *enumerator;
  GCancellable *cancellable;
  gboolean should_escape;
  GFile *dir;
  GList *basenames;
  gboolean dirs_only;
} LoadBasenamesData;

struct _GFilenameCompleter {
  GObject parent;

  GFile *basenames_dir;
  gboolean basenames_are_escaped;
  gboolean dirs_only;
  GList *basenames;

  LoadBasenamesData *basename_loader;
};

G_DEFINE_TYPE (GFilenameCompleter, g_filename_completer, G_TYPE_OBJECT)

static void cancel_load_basenames (GFilenameCompleter *completer);

static void
g_filename_completer_finalize (GObject *object)
{
  GFilenameCompleter *completer;

  completer = G_FILENAME_COMPLETER (object);

  cancel_load_basenames (completer);

  if (completer->basenames_dir)
    g_object_unref (completer->basenames_dir);

  g_list_free_full (completer->basenames, g_free);

  G_OBJECT_CLASS (g_filename_completer_parent_class)->finalize (object);
}

static void
g_filename_completer_class_init (GFilenameCompleterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_filename_completer_finalize;
  /**
   * GFilenameCompleter::got-completion-data:
   * 
   * Emitted when the file name completion information comes available.
   **/
  signals[GOT_COMPLETION_DATA] = g_signal_new (I_("got-completion-data"),
					  G_TYPE_FILENAME_COMPLETER,
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (GFilenameCompleterClass, got_completion_data),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);
}

static void
g_filename_completer_init (GFilenameCompleter *completer)
{
}

/**
 * g_filename_completer_new:
 * 
 * Creates a new filename completer.
 * 
 * Returns: a #GFilenameCompleter.
 **/
GFilenameCompleter *
g_filename_completer_new (void)
{
  return g_object_new (G_TYPE_FILENAME_COMPLETER, NULL);
}

static char *
longest_common_prefix (char *a, char *b)
{
  char *start;

  start = a;

  while (g_utf8_get_char (a) == g_utf8_get_char (b))
    {
      a = g_utf8_next_char (a);
      b = g_utf8_next_char (b);
    }

  return g_strndup (start, a - start);
}

static void
load_basenames_data_free (LoadBasenamesData *data)
{
  if (data->enumerator)
    g_object_unref (data->enumerator);
  
  g_object_unref (data->cancellable);
  g_object_unref (data->dir);
  
  g_list_free_full (data->basenames, g_free);
  
  g_free (data);
}

static void
got_more_files (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
  LoadBasenamesData *data = user_data;
  GList *infos, *l;
  GFileInfo *info;
  const char *name;
  gboolean append_slash;
  char *t;
  char *basename;

  if (data->completer == NULL)
    {
      /* Was cancelled */
      load_basenames_data_free (data);
      return;
    }

  infos = g_file_enumerator_next_files_finish (data->enumerator, res, NULL);

  for (l = infos; l != NULL; l = l->next)
    {
      info = l->data;

      if (data->dirs_only &&
	  g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
	{
	  g_object_unref (info);
	  continue;
	}
      
      append_slash = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
      name = g_file_info_get_name (info);
      if (name == NULL)
	{
	  g_object_unref (info);
	  continue;
	}

      
      if (data->should_escape)
	basename = g_uri_escape_string (name,
					G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
					TRUE);
      else
	/* If not should_escape, must be a local filename, convert to utf8 */
	basename = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);
      
      if (basename)
	{
	  if (append_slash)
	    {
	      t = basename;
	      basename = g_strconcat (basename, "/", NULL);
	      g_free (t);
	    }
	  
	  data->basenames = g_list_prepend (data->basenames, basename);
	}
      
      g_object_unref (info);
    }
  
  g_list_free (infos);
  
  if (infos)
    {
      /* Not last, get more files */
      g_file_enumerator_next_files_async (data->enumerator,
					  100,
					  0,
					  data->cancellable,
					  got_more_files, data);
    }
  else
    {
      data->completer->basename_loader = NULL;
      
      if (data->completer->basenames_dir)
	g_object_unref (data->completer->basenames_dir);
      g_list_free_full (data->completer->basenames, g_free);
      
      data->completer->basenames_dir = g_object_ref (data->dir);
      data->completer->basenames = data->basenames;
      data->completer->basenames_are_escaped = data->should_escape;
      data->basenames = NULL;
      
      g_file_enumerator_close_async (data->enumerator, 0, NULL, NULL, NULL);

      g_signal_emit (data->completer, signals[GOT_COMPLETION_DATA], 0);
      load_basenames_data_free (data);
    }
}


static void
got_enum (GObject *source_object,
	  GAsyncResult *res,
	  gpointer user_data)
{
  LoadBasenamesData *data = user_data;

  if (data->completer == NULL)
    {
      /* Was cancelled */
      load_basenames_data_free (data);
      return;
    }
  
  data->enumerator = g_file_enumerate_children_finish (G_FILE (source_object), res, NULL);
  
  if (data->enumerator == NULL)
    {
      data->completer->basename_loader = NULL;

      if (data->completer->basenames_dir)
	g_object_unref (data->completer->basenames_dir);
      g_list_free_full (data->completer->basenames, g_free);

      /* Mark uptodate with no basenames */
      data->completer->basenames_dir = g_object_ref (data->dir);
      data->completer->basenames = NULL;
      data->completer->basenames_are_escaped = data->should_escape;
      
      load_basenames_data_free (data);
      return;
    }
  
  g_file_enumerator_next_files_async (data->enumerator,
				      100,
				      0,
				      data->cancellable,
				      got_more_files, data);
}

static void
schedule_load_basenames (GFilenameCompleter *completer,
			 GFile *dir,
			 gboolean should_escape)
{
  LoadBasenamesData *data;

  cancel_load_basenames (completer);

  data = g_new0 (LoadBasenamesData, 1);
  data->completer = completer;
  data->cancellable = g_cancellable_new ();
  data->dir = g_object_ref (dir);
  data->should_escape = should_escape;
  data->dirs_only = completer->dirs_only;

  completer->basename_loader = data;
  
  g_file_enumerate_children_async (dir,
				   G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
				   0, 0,
				   data->cancellable,
				   got_enum, data);
}

static void
cancel_load_basenames (GFilenameCompleter *completer)
{
  LoadBasenamesData *loader;
  
  if (completer->basename_loader)
    {
      loader = completer->basename_loader; 
      loader->completer = NULL;
      
      g_cancellable_cancel (loader->cancellable);
      
      completer->basename_loader = NULL;
    }
}


/* Returns a list of possible matches and the basename to use for it */
static GList *
init_completion (GFilenameCompleter *completer,
		 const char *initial_text,
		 char **basename_out)
{
  gboolean should_escape;
  GFile *file, *parent;
  char *basename;
  char *t;
  int len;

  *basename_out = NULL;
  
  should_escape = ! (g_path_is_absolute (initial_text) || *initial_text == '~');

  len = strlen (initial_text);
  
  if (len > 0 &&
      initial_text[len - 1] == '/')
    return NULL;
  
  file = g_file_parse_name (initial_text);
  parent = g_file_get_parent (file);
  if (parent == NULL)
    {
      g_object_unref (file);
      return NULL;
    }

  if (completer->basenames_dir == NULL ||
      completer->basenames_are_escaped != should_escape ||
      !g_file_equal (parent, completer->basenames_dir))
    {
      schedule_load_basenames (completer, parent, should_escape);
      g_object_unref (file);
      return NULL;
    }
  
  basename = g_file_get_basename (file);
  if (should_escape)
    {
      t = basename;
      basename = g_uri_escape_string (basename, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
      g_free (t);
    }
  else
    {
      t = basename;
      basename = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);
      g_free (t);
      
      if (basename == NULL)
	return NULL;
    }

  *basename_out = basename;

  return completer->basenames;
}

/**
 * g_filename_completer_get_completion_suffix:
 * @completer: the filename completer.
 * @initial_text: text to be completed.
 *
 * Obtains a completion for @initial_text from @completer.
 *  
 * Returns: a completed string, or %NULL if no completion exists. 
 *     This string is not owned by GIO, so remember to g_free() it 
 *     when finished.
 **/
char *
g_filename_completer_get_completion_suffix (GFilenameCompleter *completer,
					    const char *initial_text)
{
  GList *possible_matches, *l;
  char *prefix;
  char *suffix;
  char *possible_match;
  char *lcp;

  g_return_val_if_fail (G_IS_FILENAME_COMPLETER (completer), NULL);
  g_return_val_if_fail (initial_text != NULL, NULL);

  possible_matches = init_completion (completer, initial_text, &prefix);

  suffix = NULL;
  
  for (l = possible_matches; l != NULL; l = l->next)
    {
      possible_match = l->data;
      
      if (g_str_has_prefix (possible_match, prefix))
	{
	  if (suffix == NULL)
	    suffix = g_strdup (possible_match + strlen (prefix));
	  else
	    {
	      lcp = longest_common_prefix (suffix,
					   possible_match + strlen (prefix));
	      g_free (suffix);
	      suffix = lcp;
	      
	      if (*suffix == 0)
		break;
	    }
	}
    }

  g_free (prefix);
  
  return suffix;
}

/**
 * g_filename_completer_get_completions:
 * @completer: the filename completer.
 * @initial_text: text to be completed.
 * 
 * Gets an array of completion strings for a given initial text.
 * 
 * Returns: (array zero-terminated=1) (transfer full): array of strings with possible completions for @initial_text.
 * This array must be freed by g_strfreev() when finished. 
 **/
char **
g_filename_completer_get_completions (GFilenameCompleter *completer,
				      const char         *initial_text)
{
  GList *possible_matches, *l;
  char *prefix;
  char *possible_match;
  GPtrArray *res;

  g_return_val_if_fail (G_IS_FILENAME_COMPLETER (completer), NULL);
  g_return_val_if_fail (initial_text != NULL, NULL);

  possible_matches = init_completion (completer, initial_text, &prefix);

  res = g_ptr_array_new ();
  for (l = possible_matches; l != NULL; l = l->next)
    {
      possible_match = l->data;

      if (g_str_has_prefix (possible_match, prefix))
	g_ptr_array_add (res,
			 g_strconcat (initial_text, possible_match + strlen (prefix), NULL));
    }

  g_free (prefix);

  g_ptr_array_add (res, NULL);

  return (char**)g_ptr_array_free (res, FALSE);
}

/**
 * g_filename_completer_set_dirs_only:
 * @completer: the filename completer.
 * @dirs_only: a #gboolean.
 * 
 * If @dirs_only is %TRUE, @completer will only 
 * complete directory names, and not file names.
 **/
void
g_filename_completer_set_dirs_only (GFilenameCompleter *completer,
				    gboolean dirs_only)
{
  g_return_if_fail (G_IS_FILENAME_COMPLETER (completer));

  completer->dirs_only = dirs_only;
}
