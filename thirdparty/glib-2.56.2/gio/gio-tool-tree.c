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

#include "gio-tool.h"


static gboolean show_hidden = FALSE;
static gboolean follow_symlinks = FALSE;

static const GOptionEntry entries[] = {
  { "hidden", 'h', 0, G_OPTION_ARG_NONE, &show_hidden, N_("Show hidden files"), NULL },
  { "follow-symlinks", 'l', 0, G_OPTION_ARG_NONE, &follow_symlinks, N_("Follow symbolic links, mounts and shortcuts"), NULL },
  { NULL }
};

static gint
sort_info_by_name (GFileInfo *a, GFileInfo *b)
{
  const char *na;
  const char *nb;

  na = g_file_info_get_name (a);
  nb = g_file_info_get_name (b);

  if (na == NULL)
    na = "";
  if (nb == NULL)
    nb = "";

  return strcmp (na, nb);
}

static void
do_tree (GFile *f, int level, guint64 pattern)
{
  GFileEnumerator *enumerator;
  GError *error = NULL;
  unsigned int n;
  GFileInfo *info;

  info = g_file_query_info (f,
			    G_FILE_ATTRIBUTE_STANDARD_TYPE ","
			    G_FILE_ATTRIBUTE_STANDARD_TARGET_URI,
			    0,
			    NULL, NULL);
  if (info != NULL)
    {
      if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_MOUNTABLE)
	{
	  /* don't process mountables; we avoid these by getting the target_uri below */
	  g_object_unref (info);
	  return;
	}
      g_object_unref (info);
    }

  enumerator = g_file_enumerate_children (f,
					  G_FILE_ATTRIBUTE_STANDARD_NAME ","
					  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
					  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
					  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
					  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET ","
					  G_FILE_ATTRIBUTE_STANDARD_TARGET_URI,
					  0,
					  NULL,
					  &error);
  if (enumerator != NULL)
    {
      GList *l;
      GList *info_list;

      info_list = NULL;
      while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
	{
	  if (g_file_info_get_is_hidden (info) && !show_hidden)
	    {
	      g_object_unref (info);
	    }
	  else
	    {
	      info_list = g_list_prepend (info_list, info);
	    }
	}
      g_file_enumerator_close (enumerator, NULL, NULL);

      info_list = g_list_sort (info_list, (GCompareFunc) sort_info_by_name);

      for (l = info_list; l != NULL; l = l->next)
	{
	  const char *name;
	  const char *target_uri;
	  GFileType type;
	  gboolean is_last_item;

	  info = l->data;
	  is_last_item = (l->next == NULL);

	  name = g_file_info_get_name (info);
	  type = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE);
	  if (name != NULL)
	    {

	      for (n = 0; n < level; n++)
		{
		  if (pattern & (1<<n))
		    {
		      g_print ("|   ");
		    }
		  else
		    {
		      g_print ("    ");
		    }
		}

	      if (is_last_item)
		{
		  g_print ("`-- %s", name);
		}
	      else
		{
		  g_print ("|-- %s", name);
		}

	      target_uri = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
	      if (target_uri != NULL)
		{
		  g_print (" -> %s", target_uri);
		}
	      else
		{
		  if (g_file_info_get_is_symlink (info))
		    {
		      const char *target;
		      target = g_file_info_get_symlink_target (info);
		      g_print (" -> %s", target);
		    }
		}

	      g_print ("\n");

	      if ((type & G_FILE_TYPE_DIRECTORY) &&
		  (follow_symlinks || !g_file_info_get_is_symlink (info)))
		{
		  guint64 new_pattern;
		  GFile *child;

		  if (is_last_item)
		    new_pattern = pattern;
		  else
		    new_pattern = pattern | (1<<level);

		  child = NULL;
		  if (target_uri != NULL)
		    {
		      if (follow_symlinks)
			child = g_file_new_for_uri (target_uri);
		    }
		  else
		    {
		      child = g_file_get_child (f, name);
		    }

		  if (child != NULL)
		    {
		      do_tree (child, level + 1, new_pattern);
		      g_object_unref (child);
		    }
		}
	    }
	  g_object_unref (info);
	}
      g_list_free (info_list);
    }
  else
    {
      for (n = 0; n < level; n++)
	{
	  if (pattern & (1<<n))
	    {
	      g_print ("|   ");
	    }
	  else
	    {
	      g_print ("    ");
	    }
	}

      g_print ("    [%s]\n", error->message);

      g_error_free (error);
    }
}

static void
tree (GFile *f)
{
  char *uri;

  uri = g_file_get_uri (f);
  g_print ("%s\n", uri);
  g_free (uri);

  do_tree (f, 0, 0);
}

int
handle_tree (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  GError *error = NULL;
  GFile *file;
  gchar *param;
  int i;

  g_set_prgname ("gio tree");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("[%s...]", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("List contents of directories in a tree-like format."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (do_help)
    {
      show_help (context, NULL);
      g_option_context_free (context);
      return 0;
    }

  g_option_context_parse (context, &argc, &argv, &error);

  if (error != NULL)
    {
      show_help (context, error->message);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  if (argc > 1)
    {
      for (i = 1; i < argc; i++)
        {
          file = g_file_new_for_commandline_arg (argv[i]);
          tree (file);
          g_object_unref (file);
        }
    }
  else
    {
      char *cwd;

      cwd = g_get_current_dir ();
      file = g_file_new_for_path (cwd);
      g_free (cwd);
      tree (file);
      g_object_unref (file);
    }

  return 0;
}
