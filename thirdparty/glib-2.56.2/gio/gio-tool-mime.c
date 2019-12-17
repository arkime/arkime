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
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "gio-tool.h"

static const GOptionEntry entries[] = {
  { NULL }
};

static GAppInfo *
get_app_info_for_id (const char *id)
{
  GList *list, *l;
  GAppInfo *ret_info;

  list = g_app_info_get_all ();
  ret_info = NULL;
  for (l = list; l != NULL; l = l->next)
    {
      GAppInfo *info;

      info = l->data;
      if (ret_info == NULL && g_strcmp0 (g_app_info_get_id (info), id) == 0)
        ret_info = info;
      else
        g_object_unref (info);
    }
  g_list_free (list);

  return ret_info;
}

int
handle_mime (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  GError *error = NULL;
  gchar *param;
  const gchar *mimetype;
  const char *handler;

  g_set_prgname ("gio mime");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s [%s]", _("MIMETYPE"), _("HANDLER"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("Get or set the handler for a mimetype."));
  g_option_context_set_description (context,
      _("If no handler is given, lists registered and recommended applications\n"
        "for the mimetype. If a handler is given, it is set as the default\n"
        "handler for the mimetype."));
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

  if (argc != 2 && argc != 3)
    {
      show_help (context, _("Must specify a single mimetype, and maybe a handler"));
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  if (argc == 2)
    {
      GAppInfo *info;

      mimetype = argv[1];

      info = g_app_info_get_default_for_type (mimetype, FALSE);
      if (!info)
        {
          g_print (_("No default applications for “%s”\n"), mimetype);
        }
      else
        {
          GList *list, *l;

          g_print (_("Default application for “%s”: %s\n"), mimetype, g_app_info_get_id (info));
          g_object_unref (info);

          list = g_app_info_get_all_for_type (mimetype);
          if (list != NULL)
            g_print (_("Registered applications:\n"));
          else
            g_print (_("No registered applications\n"));
          for (l = list; l != NULL; l = l->next)
            {
              info = l->data;
              g_print ("\t%s\n", g_app_info_get_id (info));
              g_object_unref (info);
            }
          g_list_free (list);

          list = g_app_info_get_recommended_for_type (mimetype);
          if (list != NULL)
            g_print (_("Recommended applications:\n"));
          else
            g_print (_("No recommended applications\n"));
          for (l = list; l != NULL; l = l->next)
            {
              info = l->data;
              g_print ("\t%s\n", g_app_info_get_id (info));
              g_object_unref (info);
            }
          g_list_free (list);
        }
    }
  else
    {
      GAppInfo *info;

      mimetype = argv[1];
      handler = argv[2];

      info = get_app_info_for_id (handler);
      if (info == NULL)
        {
          print_error (_("Failed to load info for handler “%s”"), handler);
          return 1;
        }

      if (g_app_info_set_as_default_for_type (info, mimetype, &error) == FALSE)
        {
          print_error (_("Failed to set “%s” as the default handler for “%s”: %s\n"),
                       handler, mimetype, error->message);
          g_error_free (error);
          g_object_unref (info);
          return 1;
        }
      g_print ("Set %s as the default for %s\n", g_app_info_get_id (info), mimetype);
      g_object_unref (info);
    }

  return 0;
}
