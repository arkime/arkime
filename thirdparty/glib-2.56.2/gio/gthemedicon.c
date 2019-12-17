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

#include <string.h>

#include "gthemedicon.h"
#include "gicon.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gthemedicon
 * @short_description: Icon theming support
 * @include: gio/gio.h
 * @see_also: #GIcon, #GLoadableIcon
 *
 * #GThemedIcon is an implementation of #GIcon that supports icon themes.
 * #GThemedIcon contains a list of all of the icons present in an icon
 * theme, so that icons can be looked up quickly. #GThemedIcon does
 * not provide actual pixmaps for icons, just the icon names.
 * Ideally something like gtk_icon_theme_choose_icon() should be used to
 * resolve the list of names so that fallback icons work nicely with
 * themes that inherit other themes.
 **/

static void g_themed_icon_icon_iface_init (GIconIface *iface);

struct _GThemedIcon
{
  GObject parent_instance;
  
  char     **names;
  gboolean   use_default_fallbacks;
};

struct _GThemedIconClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_NAMES,
  PROP_USE_DEFAULT_FALLBACKS
};

G_DEFINE_TYPE_WITH_CODE (GThemedIcon, g_themed_icon, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_ICON,
						g_themed_icon_icon_iface_init))

static void
g_themed_icon_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GThemedIcon *icon = G_THEMED_ICON (object);

  switch (prop_id)
    {
      case PROP_NAMES:
        g_value_set_boxed (value, icon->names);
        break;

      case PROP_USE_DEFAULT_FALLBACKS:
        g_value_set_boolean (value, icon->use_default_fallbacks);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_themed_icon_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GThemedIcon *icon = G_THEMED_ICON (object);
  gchar **names;
  const gchar *name;

  switch (prop_id)
    {
      case PROP_NAME:
        name = g_value_get_string (value);

        if (!name)
          break;

        if (icon->names)
          g_strfreev (icon->names);

        icon->names = g_new (char *, 2);
        icon->names[0] = g_strdup (name);
        icon->names[1] = NULL;
        break;

      case PROP_NAMES:
        names = g_value_dup_boxed (value);

        if (!names)
          break;

        if (icon->names)
          g_strfreev (icon->names);

        icon->names = names;
        break;

      case PROP_USE_DEFAULT_FALLBACKS:
        icon->use_default_fallbacks = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_themed_icon_constructed (GObject *object)
{
  GThemedIcon *themed = G_THEMED_ICON (object);

  g_return_if_fail (themed->names != NULL && themed->names[0] != NULL);

  if (themed->use_default_fallbacks)
    {
      int i = 0, dashes = 0;
      const char *p;
      char *dashp;
      char *last;
      gboolean is_symbolic;
      char *name;
      char **names;

      is_symbolic = g_str_has_suffix (themed->names[0], "-symbolic");
      if (is_symbolic)
        name = g_strndup (themed->names[0], strlen (themed->names[0]) - 9);
      else
        name = g_strdup (themed->names[0]);

      p = name;
      while (*p)
        {
          if (*p == '-')
            dashes++;
          p++;
        }

      last = name;

      g_strfreev (themed->names);

      names = g_new (char *, dashes + 1 + 1);
      names[i++] = last;

      while ((dashp = strrchr (last, '-')) != NULL)
        names[i++] = last = g_strndup (last, dashp - last);

      names[i++] = NULL;

      if (is_symbolic)
        {
          themed->names = g_new (char *, 2 * dashes + 3);
          for (i = 0; names[i] != NULL; i++)
            {
              themed->names[i] = g_strconcat (names[i], "-symbolic", NULL);
              themed->names[dashes + 1 + i] = names[i];
            }

          themed->names[dashes + 1 + i] = NULL;
          g_free (names);
        }
      else
        {
          themed->names = names;
        }
    }
}

static void
g_themed_icon_finalize (GObject *object)
{
  GThemedIcon *themed;

  themed = G_THEMED_ICON (object);

  g_strfreev (themed->names);

  G_OBJECT_CLASS (g_themed_icon_parent_class)->finalize (object);
}

static void
g_themed_icon_class_init (GThemedIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_themed_icon_finalize;
  gobject_class->constructed = g_themed_icon_constructed;
  gobject_class->set_property = g_themed_icon_set_property;
  gobject_class->get_property = g_themed_icon_get_property;

  /**
   * GThemedIcon:name:
   *
   * The icon name.
   */
  g_object_class_install_property (gobject_class, PROP_NAME,
                                   g_param_spec_string ("name",
                                                        P_("name"),
                                                        P_("The name of the icon"),
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK));

  /**
   * GThemedIcon:names:
   *
   * A %NULL-terminated array of icon names.
   */
  g_object_class_install_property (gobject_class, PROP_NAMES,
                                   g_param_spec_boxed ("names",
                                                       P_("names"),
                                                       P_("An array containing the icon names"),
                                                       G_TYPE_STRV,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK));

  /**
   * GThemedIcon:use-default-fallbacks:
   *
   * Whether to use the default fallbacks found by shortening the icon name 
   * at '-' characters. If the "names" array has more than one element, 
   * ignores any past the first.
   *
   * For example, if the icon name was "gnome-dev-cdrom-audio", the array 
   * would become
   * |[<!-- language="C" -->
   * {
   *   "gnome-dev-cdrom-audio",
   *   "gnome-dev-cdrom",
   *   "gnome-dev",
   *   "gnome",
   *   NULL
   * };
   * ]|
   */
  g_object_class_install_property (gobject_class, PROP_USE_DEFAULT_FALLBACKS,
                                   g_param_spec_boolean ("use-default-fallbacks",
                                                         P_("use default fallbacks"),
                                                         P_("Whether to use default fallbacks found by shortening the name at “-” characters. Ignores names after the first if multiple names are given."),
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK));
}

static void
g_themed_icon_init (GThemedIcon *themed)
{
  themed->names = NULL;
}

/**
 * g_themed_icon_new:
 * @iconname: a string containing an icon name.
 * 
 * Creates a new themed icon for @iconname.
 * 
 * Returns: (transfer full) (type GThemedIcon): a new #GThemedIcon.
 **/
GIcon *
g_themed_icon_new (const char *iconname)
{
  g_return_val_if_fail (iconname != NULL, NULL);

  return G_ICON (g_object_new (G_TYPE_THEMED_ICON, "name", iconname, NULL));
}

/**
 * g_themed_icon_new_from_names:
 * @iconnames: (array length=len): an array of strings containing icon names.
 * @len: the length of the @iconnames array, or -1 if @iconnames is 
 *     %NULL-terminated
 * 
 * Creates a new themed icon for @iconnames.
 * 
 * Returns: (transfer full) (type GThemedIcon): a new #GThemedIcon
 **/
GIcon *
g_themed_icon_new_from_names (char **iconnames,
                              int    len)
{
  GIcon *icon;

  g_return_val_if_fail (iconnames != NULL, NULL);

  if (len >= 0)
    {
      char **names;
      int i;

      names = g_new (char *, len + 1);

      for (i = 0; i < len; i++)
        names[i] = iconnames[i];

      names[i] = NULL;

      icon = G_ICON (g_object_new (G_TYPE_THEMED_ICON, "names", names, NULL));

      g_free (names);
    }
  else
    icon = G_ICON (g_object_new (G_TYPE_THEMED_ICON, "names", iconnames, NULL));

  return icon;
}

/**
 * g_themed_icon_new_with_default_fallbacks:
 * @iconname: a string containing an icon name
 *
 * Creates a new themed icon for @iconname, and all the names
 * that can be created by shortening @iconname at '-' characters.
 * 
 * In the following example, @icon1 and @icon2 are equivalent:
 * |[<!-- language="C" -->
 * const char *names[] = { 
 *   "gnome-dev-cdrom-audio",
 *   "gnome-dev-cdrom",
 *   "gnome-dev",
 *   "gnome"
 * };
 *
 * icon1 = g_themed_icon_new_from_names (names, 4);
 * icon2 = g_themed_icon_new_with_default_fallbacks ("gnome-dev-cdrom-audio");
 * ]|
 *
 * Returns: (transfer full) (type GThemedIcon): a new #GThemedIcon.
 */
GIcon *
g_themed_icon_new_with_default_fallbacks (const char *iconname)
{
  g_return_val_if_fail (iconname != NULL, NULL);

  return G_ICON (g_object_new (G_TYPE_THEMED_ICON, "name", iconname, "use-default-fallbacks", TRUE, NULL));
}


/**
 * g_themed_icon_get_names:
 * @icon: a #GThemedIcon.
 *
 * Gets the names of icons from within @icon.
 *
 * Returns: (transfer none): a list of icon names.
 */
const char * const *
g_themed_icon_get_names (GThemedIcon *icon)
{
  g_return_val_if_fail (G_IS_THEMED_ICON (icon), NULL);
  return (const char * const *)icon->names;
}

/**
 * g_themed_icon_append_name:
 * @icon: a #GThemedIcon
 * @iconname: name of icon to append to list of icons from within @icon.
 *
 * Append a name to the list of icons from within @icon.
 *
 * Note that doing so invalidates the hash computed by prior calls
 * to g_icon_hash().
 */
void
g_themed_icon_append_name (GThemedIcon *icon, 
                           const char  *iconname)
{
  guint num_names;

  g_return_if_fail (G_IS_THEMED_ICON (icon));
  g_return_if_fail (iconname != NULL);

  num_names = g_strv_length (icon->names);
  icon->names = g_realloc (icon->names, sizeof (char*) * (num_names + 2));
  icon->names[num_names] = g_strdup (iconname);
  icon->names[num_names + 1] = NULL;

  g_object_notify (G_OBJECT (icon), "names");
}

/**
 * g_themed_icon_prepend_name:
 * @icon: a #GThemedIcon
 * @iconname: name of icon to prepend to list of icons from within @icon.
 *
 * Prepend a name to the list of icons from within @icon.
 *
 * Note that doing so invalidates the hash computed by prior calls
 * to g_icon_hash().
 *
 * Since: 2.18
 */
void
g_themed_icon_prepend_name (GThemedIcon *icon, 
                            const char  *iconname)
{
  guint num_names;
  gchar **names;
  gint i;

  g_return_if_fail (G_IS_THEMED_ICON (icon));
  g_return_if_fail (iconname != NULL);

  num_names = g_strv_length (icon->names);
  names = g_new (char*, num_names + 2);
  for (i = 0; icon->names[i]; i++)
    names[i + 1] = icon->names[i];
  names[0] = g_strdup (iconname);
  names[num_names + 1] = NULL;

  g_free (icon->names);
  icon->names = names;

  g_object_notify (G_OBJECT (icon), "names");
}

static guint
g_themed_icon_hash (GIcon *icon)
{
  GThemedIcon *themed = G_THEMED_ICON (icon);
  guint hash;
  int i;

  hash = 0;

  for (i = 0; themed->names[i] != NULL; i++)
    hash ^= g_str_hash (themed->names[i]);
  
  return hash;
}

static gboolean
g_themed_icon_equal (GIcon *icon1,
                     GIcon *icon2)
{
  GThemedIcon *themed1 = G_THEMED_ICON (icon1);
  GThemedIcon *themed2 = G_THEMED_ICON (icon2);
  int i;

  for (i = 0; themed1->names[i] != NULL && themed2->names[i] != NULL; i++)
    {
      if (!g_str_equal (themed1->names[i], themed2->names[i]))
	return FALSE;
    }

  return themed1->names[i] == NULL && themed2->names[i] == NULL;
}


static gboolean
g_themed_icon_to_tokens (GIcon *icon,
			 GPtrArray *tokens,
                         gint  *out_version)
{
  GThemedIcon *themed_icon = G_THEMED_ICON (icon);
  int n;

  g_return_val_if_fail (out_version != NULL, FALSE);

  *out_version = 0;

  for (n = 0; themed_icon->names[n] != NULL; n++)
    g_ptr_array_add (tokens,
		     g_strdup (themed_icon->names[n]));
  
  return TRUE;
}

static GIcon *
g_themed_icon_from_tokens (gchar  **tokens,
                           gint     num_tokens,
                           gint     version,
                           GError **error)
{
  GIcon *icon;
  gchar **names;
  int n;

  icon = NULL;

  if (version != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Can’t handle version %d of GThemedIcon encoding"),
                   version);
      goto out;
    }
  
  names = g_new0 (gchar *, num_tokens + 1);
  for (n = 0; n < num_tokens; n++)
    names[n] = tokens[n];
  names[n] = NULL;

  icon = g_themed_icon_new_from_names (names, num_tokens);
  g_free (names);

 out:
  return icon;
}

static GVariant *
g_themed_icon_serialize (GIcon *icon)
{
  GThemedIcon *themed_icon = G_THEMED_ICON (icon);

  return g_variant_new ("(sv)", "themed", g_variant_new ("^as", themed_icon->names));
}

static void
g_themed_icon_icon_iface_init (GIconIface *iface)
{
  iface->hash = g_themed_icon_hash;
  iface->equal = g_themed_icon_equal;
  iface->to_tokens = g_themed_icon_to_tokens;
  iface->from_tokens = g_themed_icon_from_tokens;
  iface->serialize = g_themed_icon_serialize;
}
