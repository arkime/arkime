/*
 * Copyright © 2010 Codethink Limited
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

/* Prologue {{{1 */
#include "config.h"

#include <gstdio.h>
#include <gi18n.h>

#include <string.h>
#include <stdio.h>
#include <locale.h>

#include "gvdb/gvdb-builder.h"
#include "strinfo.c"

#ifdef G_OS_WIN32
#include "glib/glib-private.h"
#endif

static void
strip_string (GString *string)
{
  gint i;

  for (i = 0; g_ascii_isspace (string->str[i]); i++);
  g_string_erase (string, 0, i);

  if (string->len > 0)
    {
      /* len > 0, so there must be at least one non-whitespace character */
      for (i = string->len - 1; g_ascii_isspace (string->str[i]); i--);
      g_string_truncate (string, i + 1);
    }
}

/* Handling of <enum> {{{1 */
typedef struct
{
  GString *strinfo;

  gboolean is_flags;
} EnumState;

static void
enum_state_free (gpointer data)
{
  EnumState *state = data;

  g_string_free (state->strinfo, TRUE);
  g_slice_free (EnumState, state);
}

static EnumState *
enum_state_new (gboolean is_flags)
{
  EnumState *state;

  state = g_slice_new (EnumState);
  state->strinfo = g_string_new (NULL);
  state->is_flags = is_flags;

  return state;
}

static void
enum_state_add_value (EnumState    *state,
                      const gchar  *nick,
                      const gchar  *valuestr,
                      GError      **error)
{
  gint64 value;
  gchar *end;

  if (nick[0] == '\0' || nick[1] == '\0')
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("nick must be a minimum of 2 characters"));
      return;
    }

  value = g_ascii_strtoll (valuestr, &end, 0);
  if (*end || (state->is_flags ?
                (value > G_MAXUINT32 || value < 0) :
                (value > G_MAXINT32 || value < G_MININT32)))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("Invalid numeric value"));
      return;
    }

  if (strinfo_builder_contains (state->strinfo, nick))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<value nick='%s'/> already specified"), nick);
      return;
    }

  if (strinfo_builder_contains_value (state->strinfo, value))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("value='%s' already specified"), valuestr);
      return;
    }

  /* Silently drop the null case if it is mentioned.
   * It is properly denoted with an empty array.
   */
  if (state->is_flags && value == 0)
    return;

  if (state->is_flags && (value & (value - 1)))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("flags values must have at most 1 bit set"));
      return;
    }

  /* Since we reject exact duplicates of value='' and we only allow one
   * bit to be set, it's not possible to have overlaps.
   *
   * If we loosen the one-bit-set restriction we need an overlap check.
   */

  strinfo_builder_append_item (state->strinfo, nick, value);
}

static void
enum_state_end (EnumState **state_ptr,
                GError    **error)
{
  EnumState *state;

  state = *state_ptr;
  *state_ptr = NULL;

  if (state->strinfo->len == 0)
    g_set_error (error,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 _("<%s> must contain at least one <value>"),
                 state->is_flags ? "flags" : "enum");
}

/* Handling of <key> {{{1 */
typedef struct
{
  /* for <child>, @child_schema will be set.
   * for <key>, everything else will be set.
   */
  gchar        *child_schema;


  GVariantType *type;
  gboolean      have_gettext_domain;

  gchar         l10n;
  gchar        *l10n_context;
  GString      *unparsed_default_value;
  GVariant     *default_value;

  GString      *strinfo;
  gboolean      is_enum;
  gboolean      is_flags;

  GVariant     *minimum;
  GVariant     *maximum;

  gboolean      has_choices;
  gboolean      has_aliases;
  gboolean      is_override;

  gboolean      checked;
  GVariant     *serialised;

  gboolean      summary_seen;
  gboolean      description_seen;
} KeyState;

static KeyState *
key_state_new (const gchar *type_string,
               const gchar *gettext_domain,
               gboolean     is_enum,
               gboolean     is_flags,
               GString     *strinfo)
{
  KeyState *state;

  state = g_slice_new0 (KeyState);
  state->type = g_variant_type_new (type_string);
  state->have_gettext_domain = gettext_domain != NULL;
  state->is_enum = is_enum;
  state->is_flags = is_flags;
  state->summary_seen = FALSE;
  state->description_seen = FALSE;

  if (strinfo)
    state->strinfo = g_string_new_len (strinfo->str, strinfo->len);
  else
    state->strinfo = g_string_new (NULL);

  return state;
}

static KeyState *
key_state_override (KeyState    *state,
                    const gchar *gettext_domain)
{
  KeyState *copy;

  copy = g_slice_new0 (KeyState);
  copy->type = g_variant_type_copy (state->type);
  copy->have_gettext_domain = gettext_domain != NULL;
  copy->strinfo = g_string_new_len (state->strinfo->str,
                                    state->strinfo->len);
  copy->is_enum = state->is_enum;
  copy->is_flags = state->is_flags;
  copy->is_override = TRUE;

  if (state->minimum)
    {
      copy->minimum = g_variant_ref (state->minimum);
      copy->maximum = g_variant_ref (state->maximum);
    }

  return copy;
}

static KeyState *
key_state_new_child (const gchar *child_schema)
{
  KeyState *state;

  state = g_slice_new0 (KeyState);
  state->child_schema = g_strdup (child_schema);

  return state;
}

static gboolean
is_valid_choices (GVariant *variant,
                  GString  *strinfo)
{
  switch (g_variant_classify (variant))
    {
      case G_VARIANT_CLASS_MAYBE:
      case G_VARIANT_CLASS_ARRAY:
        {
          gboolean valid = TRUE;
          GVariantIter iter;

          g_variant_iter_init (&iter, variant);

          while (valid && (variant = g_variant_iter_next_value (&iter)))
            {
              valid = is_valid_choices (variant, strinfo);
              g_variant_unref (variant);
            }

          return valid;
        }

      case G_VARIANT_CLASS_STRING:
        return strinfo_is_string_valid ((const guint32 *) strinfo->str,
                                        strinfo->len / 4,
                                        g_variant_get_string (variant, NULL));

      default:
        g_assert_not_reached ();
    }
}


/* Gets called at </default> </choices> or <range/> to check for
 * validity of the default value so that any inconsistency is
 * reported as soon as it is encountered.
 */
static void
key_state_check_range (KeyState  *state,
                       GError   **error)
{
  if (state->default_value)
    {
      const gchar *tag;

      tag = state->is_override ? "override" : "default";

      if (state->minimum)
        {
          if (g_variant_compare (state->default_value, state->minimum) < 0 ||
              g_variant_compare (state->default_value, state->maximum) > 0)
            {
              g_set_error (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("<%s> is not contained in "
                           "the specified range"), tag);
            }
        }

      else if (state->strinfo->len)
        {
          if (!is_valid_choices (state->default_value, state->strinfo))
            {
              if (state->is_enum)
                g_set_error (error, G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             _("<%s> is not a valid member of "
                             "the specified enumerated type"), tag);

              else if (state->is_flags)
                g_set_error (error, G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             _("<%s> contains string not in the "
                             "specified flags type"), tag);

              else
                g_set_error (error, G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             _("<%s> contains a string not in "
                             "<choices>"), tag);
            }
        }
    }
}

static void
key_state_set_range (KeyState     *state,
                     const gchar  *min_str,
                     const gchar  *max_str,
                     GError      **error)
{
  const struct {
    const gchar  type;
    const gchar *min;
    const gchar *max;
  } table[] = {
    { 'y',                    "0",                  "255" },
    { 'n',               "-32768",                "32767" },
    { 'q',                    "0",                "65535" },
    { 'i',          "-2147483648",           "2147483647" },
    { 'u',                    "0",           "4294967295" },
    { 'x', "-9223372036854775808",  "9223372036854775807" },
    { 't',                    "0", "18446744073709551615" },
    { 'd',                 "-inf",                  "inf" },
  };
  gboolean type_ok = FALSE;
  gint i;

  if (state->minimum)
    {
      g_set_error_literal (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("<range/> already specified for this key"));
      return;
    }

  for (i = 0; i < G_N_ELEMENTS (table); i++)
    if (*(char *) state->type == table[i].type)
      {
        min_str = min_str ? min_str : table[i].min;
        max_str = max_str ? max_str : table[i].max;
        type_ok = TRUE;
        break;
      }

  if (!type_ok)
    {
      gchar *type = g_variant_type_dup_string (state->type);
      g_set_error (error, G_MARKUP_ERROR,
                  G_MARKUP_ERROR_INVALID_CONTENT,
                  _("<range> not allowed for keys of type “%s”"), type);
      g_free (type);
      return;
    }

  state->minimum = g_variant_parse (state->type, min_str, NULL, NULL, error);
  if (state->minimum == NULL)
    return;

  state->maximum = g_variant_parse (state->type, max_str, NULL, NULL, error);
  if (state->maximum == NULL)
    return;

  if (g_variant_compare (state->minimum, state->maximum) > 0)
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<range> specified minimum is greater than maximum"));
      return;
    }

  key_state_check_range (state, error);
}

static GString *
key_state_start_default (KeyState     *state,
                         const gchar  *l10n,
                         const gchar  *context,
                         GError      **error)
{
  if (l10n != NULL)
    {
      if (strcmp (l10n, "messages") == 0)
        state->l10n = 'm';

      else if (strcmp (l10n, "time") == 0)
        state->l10n = 't';

      else
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       _("unsupported l10n category: %s"), l10n);
          return NULL;
        }

      if (!state->have_gettext_domain)
        {
          g_set_error_literal (error, G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               _("l10n requested, but no "
                               "gettext domain given"));
          return NULL;
        }

      state->l10n_context = g_strdup (context);
    }

  else if (context != NULL)
    {
      g_set_error_literal (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("translation context given for "
                           "value without l10n enabled"));
      return NULL;
    }

  return g_string_new (NULL);
}

static void
key_state_end_default (KeyState  *state,
                       GString  **string,
                       GError   **error)
{
  state->unparsed_default_value = *string;
  *string = NULL;

  state->default_value = g_variant_parse (state->type,
                                          state->unparsed_default_value->str,
                                          NULL, NULL, error);
  if (!state->default_value)
    {
      gchar *type = g_variant_type_dup_string (state->type);
      g_prefix_error (error, _("Failed to parse <default> value of type “%s”: "), type);
      g_free (type);
    }

  key_state_check_range (state, error);
}

static void
key_state_start_choices (KeyState  *state,
                         GError   **error)
{
  const GVariantType *type = state->type;

  if (state->is_enum)
    {
      g_set_error_literal (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("<choices> cannot be specified for keys "
                           "tagged as having an enumerated type"));
      return;
    }

  if (state->has_choices)
    {
      g_set_error_literal (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("<choices> already specified for this key"));
      return;
    }

  while (g_variant_type_is_maybe (type) || g_variant_type_is_array (type))
    type = g_variant_type_element (type);

  if (!g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      gchar *type_string = g_variant_type_dup_string (state->type);
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<choices> not allowed for keys of type “%s”"),
                   type_string);
      g_free (type_string);
      return;
    }
}

static void
key_state_add_choice (KeyState     *state,
                      const gchar  *choice,
                      GError      **error)
{
  if (strinfo_builder_contains (state->strinfo, choice))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<choice value='%s'/> already given"), choice);
      return;
    }

  strinfo_builder_append_item (state->strinfo, choice, 0);
  state->has_choices = TRUE;
}

static void
key_state_end_choices (KeyState  *state,
                       GError   **error)
{
  if (!state->has_choices)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<choices> must contain at least one <choice>"));
      return;
    }

  key_state_check_range (state, error);
}

static void
key_state_start_aliases (KeyState  *state,
                         GError   **error)
{
  if (state->has_aliases)
    g_set_error_literal (error, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_INVALID_CONTENT,
                         _("<aliases> already specified for this key"));
  else if (!state->is_flags && !state->is_enum && !state->has_choices)
    g_set_error_literal (error, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_INVALID_CONTENT,
                         _("<aliases> can only be specified for keys with "
                         "enumerated or flags types or after <choices>"));
}

static void
key_state_add_alias (KeyState     *state,
                     const gchar  *alias,
                     const gchar  *target,
                     GError      **error)
{
  if (strinfo_builder_contains (state->strinfo, alias))
    {
      if (strinfo_is_string_valid ((guint32 *) state->strinfo->str,
                                   state->strinfo->len / 4,
                                   alias))
        {
          if (state->is_enum)
            g_set_error (error, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_INVALID_CONTENT,
                         _("<alias value='%s'/> given when “%s” is already "
                         "a member of the enumerated type"), alias, alias);

          else
            g_set_error (error, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_INVALID_CONTENT,
                         _("<alias value='%s'/> given when "
                         "<choice value='%s'/> was already given"),
                         alias, alias);
        }

      else
        g_set_error (error, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_INVALID_CONTENT,
                     _("<alias value='%s'/> already specified"), alias);

      return;
    }

  if (!strinfo_builder_append_alias (state->strinfo, alias, target))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   state->is_enum ?
                     _("alias target “%s” is not in enumerated type") :
                     _("alias target “%s” is not in <choices>"),
                   target);
      return;
    }

  state->has_aliases = TRUE;
}

static void
key_state_end_aliases (KeyState  *state,
                       GError   **error)
{
  if (!state->has_aliases)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<aliases> must contain at least one <alias>"));
      return;
    }
}

static gboolean
key_state_check (KeyState  *state,
                 GError   **error)
{
  if (state->checked)
    return TRUE;

  return state->checked = TRUE;
}

static GVariant *
key_state_serialise (KeyState *state)
{
  if (state->serialised == NULL)
    {
      if (state->child_schema)
        {
          state->serialised = g_variant_new_string (state->child_schema);
        }

      else
        {
          GVariantBuilder builder;

          g_assert (key_state_check (state, NULL));

          g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);

          /* default value */
          g_variant_builder_add_value (&builder, state->default_value);

          /* translation */
          if (state->l10n)
            {
              /* We are going to store the untranslated default for
               * runtime translation according to the current locale.
               * We need to strip leading and trailing whitespace from
               * the string so that it's exactly the same as the one
               * that ended up in the .po file for translation.
               *
               * We want to do this so that
               *
               *   <default l10n='messages'>
               *     ['a', 'b', 'c']
               *   </default>
               *
               * ends up in the .po file like "['a', 'b', 'c']",
               * omitting the extra whitespace at the start and end.
               */
              strip_string (state->unparsed_default_value);

              if (state->l10n_context)
                {
                  gint len;

                  /* Contextified messages are supported by prepending
                   * the context, followed by '\004' to the start of the
                   * message string.  We do that here to save GSettings
                   * the work later on.
                   */
                  len = strlen (state->l10n_context);
                  state->l10n_context[len] = '\004';
                  g_string_prepend_len (state->unparsed_default_value,
                                        state->l10n_context, len + 1);
                  g_free (state->l10n_context);
                  state->l10n_context = NULL;
                }

              g_variant_builder_add (&builder, "(y(y&s))", 'l', state->l10n,
                                     state->unparsed_default_value->str);
              g_string_free (state->unparsed_default_value, TRUE);
              state->unparsed_default_value = NULL;
            }

          /* choice, aliases, enums */
          if (state->strinfo->len)
            {
              GVariant *array;
              guint32 *words;
              gpointer data;
              gsize size;
              gint i;

              data = state->strinfo->str;
              size = state->strinfo->len;

              words = data;
              for (i = 0; i < size / sizeof (guint32); i++)
                words[i] = GUINT32_TO_LE (words[i]);

              array = g_variant_new_from_data (G_VARIANT_TYPE ("au"),
                                               data, size, TRUE,
                                               g_free, data);

              g_string_free (state->strinfo, FALSE);
              state->strinfo = NULL;

              g_variant_builder_add (&builder, "(y@au)",
                                     state->is_flags ? 'f' :
                                     state->is_enum ? 'e' : 'c',
                                     array);
            }

          /* range */
          if (state->minimum || state->maximum)
            g_variant_builder_add (&builder, "(y(**))", 'r',
                                   state->minimum, state->maximum);

          state->serialised = g_variant_builder_end (&builder);
        }

      g_variant_ref_sink (state->serialised);
    }

  return g_variant_ref (state->serialised);
}

static void
key_state_free (gpointer data)
{
  KeyState *state = data;

  if (state->type)
    g_variant_type_free (state->type);

  g_free (state->l10n_context);

  if (state->unparsed_default_value)
    g_string_free (state->unparsed_default_value, TRUE);

  if (state->default_value)
    g_variant_unref (state->default_value);

  if (state->strinfo)
    g_string_free (state->strinfo, TRUE);

  if (state->minimum)
    g_variant_unref (state->minimum);

  if (state->maximum)
    g_variant_unref (state->maximum);

  if (state->serialised)
    g_variant_unref (state->serialised);

  g_slice_free (KeyState, state);
}

/* Key name validity {{{1 */
static gboolean allow_any_name = FALSE;

static gboolean
is_valid_keyname (const gchar  *key,
                  GError      **error)
{
  gint i;

  if (key[0] == '\0')
    {
      g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           _("Empty names are not permitted"));
      return FALSE;
    }

  if (allow_any_name)
    return TRUE;

  if (!g_ascii_islower (key[0]))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("Invalid name “%s”: names must begin "
                     "with a lowercase letter"), key);
      return FALSE;
    }

  for (i = 1; key[i]; i++)
    {
      if (key[i] != '-' &&
          !g_ascii_islower (key[i]) &&
          !g_ascii_isdigit (key[i]))
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("Invalid name “%s”: invalid character “%c”; "
                         "only lowercase letters, numbers and hyphen (“-”) "
                         "are permitted"), key, key[i]);
          return FALSE;
        }

      if (key[i] == '-' && key[i + 1] == '-')
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("Invalid name “%s”: two successive hyphens (“--”) "
                         "are not permitted"), key);
          return FALSE;
        }
    }

  if (key[i - 1] == '-')
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("Invalid name “%s”: the last character may not be a "
                     "hyphen (“-”)"), key);
      return FALSE;
    }

  if (i > 1024)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("Invalid name “%s”: maximum length is 1024"), key);
      return FALSE;
    }

  return TRUE;
}

/* Handling of <schema> {{{1 */
typedef struct _SchemaState SchemaState;
struct _SchemaState
{
  SchemaState *extends;

  gchar       *path;
  gchar       *gettext_domain;
  gchar       *extends_name;
  gchar       *list_of;

  GHashTable  *keys;
};

static SchemaState *
schema_state_new (const gchar  *path,
                  const gchar  *gettext_domain,
                  SchemaState  *extends,
                  const gchar  *extends_name,
                  const gchar  *list_of)
{
  SchemaState *state;

  state = g_slice_new (SchemaState);
  state->path = g_strdup (path);
  state->gettext_domain = g_strdup (gettext_domain);
  state->extends = extends;
  state->extends_name = g_strdup (extends_name);
  state->list_of = g_strdup (list_of);
  state->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, key_state_free);

  return state;
}

static void
schema_state_free (gpointer data)
{
  SchemaState *state = data;

  g_free (state->path);
  g_free (state->gettext_domain);
  g_hash_table_unref (state->keys);
  g_slice_free (SchemaState, state);
}

static void
schema_state_add_child (SchemaState  *state,
                        const gchar  *name,
                        const gchar  *schema,
                        GError      **error)
{
  gchar *childname;

  if (!is_valid_keyname (name, error))
    return;

  childname = g_strconcat (name, "/", NULL);

  if (g_hash_table_lookup (state->keys, childname))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<child name='%s'> already specified"), name);
      return;
    }

  g_hash_table_insert (state->keys, childname,
                       key_state_new_child (schema));
}

static KeyState *
schema_state_add_key (SchemaState  *state,
                      GHashTable   *enum_table,
                      GHashTable   *flags_table,
                      const gchar  *name,
                      const gchar  *type_string,
                      const gchar  *enum_type,
                      const gchar  *flags_type,
                      GError      **error)
{
  SchemaState *node;
  GString *strinfo;
  KeyState *key;

  if (state->list_of)
    {
      g_set_error_literal (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("Cannot add keys to a “list-of” schema"));
      return NULL;
    }

  if (!is_valid_keyname (name, error))
    return NULL;

  if (g_hash_table_lookup (state->keys, name))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<key name='%s'> already specified"), name);
      return NULL;
    }

  for (node = state; node; node = node->extends)
    if (node->extends)
      {
        KeyState *shadow;

        shadow = g_hash_table_lookup (node->extends->keys, name);

        /* in case of <key> <override> <key> make sure we report the
         * location of the original <key>, not the <override>.
         */
        if (shadow && !shadow->is_override)
          {
            g_set_error (error, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_INVALID_CONTENT,
                         _("<key name='%s'> shadows <key name='%s'> in "
                           "<schema id='%s'>; use <override> to modify value"),
                         name, name, node->extends_name);
            return NULL;
          }
      }

  if ((type_string != NULL) + (enum_type != NULL) + (flags_type != NULL) != 1)
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                   _("Exactly one of “type”, “enum” or “flags” must "
                     "be specified as an attribute to <key>"));
      return NULL;
    }

  if (type_string == NULL) /* flags or enums was specified */
    {
      EnumState *enum_state;

      if (enum_type)
        enum_state = g_hash_table_lookup (enum_table, enum_type);
      else
        enum_state = g_hash_table_lookup (flags_table, flags_type);


      if (enum_state == NULL)
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       _("<%s id='%s'> not (yet) defined."),
                       flags_type ? "flags"    : "enum",
                       flags_type ? flags_type : enum_type);
          return NULL;
        }

      type_string = flags_type ? "as" : "s";
      strinfo = enum_state->strinfo;
    }
  else
    {
      if (!g_variant_type_string_is_valid (type_string))
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       _("Invalid GVariant type string “%s”"), type_string);
          return NULL;
        }

      strinfo = NULL;
    }

  key = key_state_new (type_string, state->gettext_domain,
                       enum_type != NULL, flags_type != NULL, strinfo);
  g_hash_table_insert (state->keys, g_strdup (name), key);

  return key;
}

static void
schema_state_add_override (SchemaState  *state,
                           KeyState    **key_state,
                           GString     **string,
                           const gchar  *key,
                           const gchar  *l10n,
                           const gchar  *context,
                           GError      **error)
{
  SchemaState *parent;
  KeyState *original;

  if (state->extends == NULL)
    {
      g_set_error_literal (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("<override> given but schema isn’t "
                             "extending anything"));
      return;
    }

  for (parent = state->extends; parent; parent = parent->extends)
    if ((original = g_hash_table_lookup (parent->keys, key)))
      break;

  if (original == NULL)
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("No <key name='%s'> to override"), key);
      return;
    }

  if (g_hash_table_lookup (state->keys, key))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<override name='%s'> already specified"), key);
      return;
    }

  *key_state = key_state_override (original, state->gettext_domain);
  *string = key_state_start_default (*key_state, l10n, context, error);
  g_hash_table_insert (state->keys, g_strdup (key), *key_state);
}

static void
override_state_end (KeyState **key_state,
                    GString  **string,
                    GError   **error)
{
  key_state_end_default (*key_state, string, error);
  *key_state = NULL;
}

/* Handling of toplevel state {{{1 */
typedef struct
{
  gboolean     strict;                  /* TRUE if --strict was given */

  GHashTable  *schema_table;            /* string -> SchemaState */
  GHashTable  *flags_table;             /* string -> EnumState */
  GHashTable  *enum_table;              /* string -> EnumState */

  GSList      *this_file_schemas;       /* strings: <schema>s in this file */
  GSList      *this_file_flagss;        /* strings: <flags>s in this file */
  GSList      *this_file_enums;         /* strings: <enum>s in this file */

  gchar       *schemalist_domain;       /* the <schemalist> gettext domain */

  SchemaState *schema_state;            /* non-NULL when inside <schema> */
  KeyState    *key_state;               /* non-NULL when inside <key> */
  EnumState   *enum_state;              /* non-NULL when inside <enum> */

  GString     *string;                  /* non-NULL when accepting text */
} ParseState;

static gboolean
is_subclass (const gchar *class_name,
             const gchar *possible_parent,
             GHashTable  *schema_table)
{
  SchemaState *class;

  if (strcmp (class_name, possible_parent) == 0)
    return TRUE;

  class = g_hash_table_lookup (schema_table, class_name);
  g_assert (class != NULL);

  return class->extends_name &&
         is_subclass (class->extends_name, possible_parent, schema_table);
}

static void
parse_state_start_schema (ParseState  *state,
                          const gchar  *id,
                          const gchar  *path,
                          const gchar  *gettext_domain,
                          const gchar  *extends_name,
                          const gchar  *list_of,
                          GError      **error)
{
  SchemaState *extends;
  gchar *my_id;

  if (g_hash_table_lookup (state->schema_table, id))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<schema id='%s'> already specified"), id);
      return;
    }

  if (extends_name)
    {
      extends = g_hash_table_lookup (state->schema_table, extends_name);

      if (extends == NULL)
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       _("<schema id='%s'> extends not yet existing "
                         "schema “%s”"), id, extends_name);
          return;
        }
    }
  else
    extends = NULL;

  if (list_of)
    {
      SchemaState *tmp;

      if (!(tmp = g_hash_table_lookup (state->schema_table, list_of)))
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       _("<schema id='%s'> is list of not yet existing "
                         "schema “%s”"), id, list_of);
          return;
        }

      if (tmp->path)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("Cannot be a list of a schema with a path"));
          return;
        }
    }

  if (extends)
    {
      if (extends->path)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("Cannot extend a schema with a path"));
          return;
        }

      if (list_of)
        {
          if (extends->list_of == NULL)
            {
              g_set_error (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("<schema id='%s'> is a list, extending "
                             "<schema id='%s'> which is not a list"),
                           id, extends_name);
              return;
            }

          if (!is_subclass (list_of, extends->list_of, state->schema_table))
            {
              g_set_error (error, G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           _("<schema id='%s' list-of='%s'> extends <schema "
                             "id='%s' list-of='%s'> but “%s” does not "
                             "extend “%s”"), id, list_of, extends_name,
                           extends->list_of, list_of, extends->list_of);
              return;
            }
        }
      else
        /* by default we are a list of the same thing that the schema
         * we are extending is a list of (which might be nothing)
         */
        list_of = extends->list_of;
    }

  if (path && !(g_str_has_prefix (path, "/") && g_str_has_suffix (path, "/")))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("A path, if given, must begin and end with a slash"));
      return;
    }

  if (path && list_of && !g_str_has_suffix (path, ":/"))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("The path of a list must end with “:/”"));
      return;
    }

  if (path && (g_str_has_prefix (path, "/apps/") ||
               g_str_has_prefix (path, "/desktop/") ||
               g_str_has_prefix (path, "/system/")))
    {
      gchar *message = NULL;
      message = g_strdup_printf (_("Warning: Schema “%s” has path “%s”.  "
                                   "Paths starting with "
                                   "“/apps/”, “/desktop/” or “/system/” are deprecated."),
                                 id, path);
      g_printerr ("%s\n", message);
      g_free (message);
    }

  state->schema_state = schema_state_new (path, gettext_domain,
                                          extends, extends_name, list_of);

  my_id = g_strdup (id);
  state->this_file_schemas = g_slist_prepend (state->this_file_schemas, my_id);
  g_hash_table_insert (state->schema_table, my_id, state->schema_state);
}

static void
parse_state_start_enum (ParseState   *state,
                        const gchar  *id,
                        gboolean      is_flags,
                        GError      **error)
{
  GSList **list = is_flags ? &state->this_file_flagss : &state->this_file_enums;
  GHashTable *table = is_flags ? state->flags_table : state->enum_table;
  gchar *my_id;

  if (g_hash_table_lookup (table, id))
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   _("<%s id='%s'> already specified"),
                   is_flags ? "flags" : "enum", id);
      return;
    }

  state->enum_state = enum_state_new (is_flags);

  my_id = g_strdup (id);
  *list = g_slist_prepend (*list, my_id);
  g_hash_table_insert (table, my_id, state->enum_state);
}

/* GMarkup Parser Functions {{{1 */

/* Start element {{{2 */
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
#define NO_ATTRS()  COLLECT (G_MARKUP_COLLECT_INVALID, NULL)

  /* Toplevel items {{{3 */
  if (container == NULL)
    {
      if (strcmp (element_name, "schemalist") == 0)
        {
          COLLECT (OPTIONAL | STRDUP,
                   "gettext-domain",
                   &state->schemalist_domain);
          return;
        }
    }


  /* children of <schemalist> {{{3 */
  else if (strcmp (container, "schemalist") == 0)
    {
      if (strcmp (element_name, "schema") == 0)
        {
          const gchar *id, *path, *gettext_domain, *extends, *list_of;
          if (COLLECT (STRING, "id", &id,
                       OPTIONAL | STRING, "path", &path,
                       OPTIONAL | STRING, "gettext-domain", &gettext_domain,
                       OPTIONAL | STRING, "extends", &extends,
                       OPTIONAL | STRING, "list-of", &list_of))
            parse_state_start_schema (state, id, path,
                                      gettext_domain ? gettext_domain
                                                     : state->schemalist_domain,
                                      extends, list_of, error);
          return;
        }

      else if (strcmp (element_name, "enum") == 0)
        {
          const gchar *id;
          if (COLLECT (STRING, "id", &id))
            parse_state_start_enum (state, id, FALSE, error);
          return;
        }

      else if (strcmp (element_name, "flags") == 0)
        {
          const gchar *id;
          if (COLLECT (STRING, "id", &id))
            parse_state_start_enum (state, id, TRUE, error);
          return;
        }
    }


  /* children of <schema> {{{3 */
  else if (strcmp (container, "schema") == 0)
    {
      if (strcmp (element_name, "key") == 0)
        {
          const gchar *name, *type_string, *enum_type, *flags_type;

          if (COLLECT (STRING,            "name",  &name,
                       OPTIONAL | STRING, "type",  &type_string,
                       OPTIONAL | STRING, "enum",  &enum_type,
                       OPTIONAL | STRING, "flags", &flags_type))

            state->key_state = schema_state_add_key (state->schema_state,
                                                     state->enum_table,
                                                     state->flags_table,
                                                     name, type_string,
                                                     enum_type, flags_type,
                                                     error);
          return;
        }
      else if (strcmp (element_name, "child") == 0)
        {
          const gchar *name, *schema;

          if (COLLECT (STRING, "name", &name, STRING, "schema", &schema))
            schema_state_add_child (state->schema_state,
                                    name, schema, error);
          return;
        }
      else if (strcmp (element_name, "override") == 0)
        {
          const gchar *name, *l10n, *context;

          if (COLLECT (STRING,            "name",    &name,
                       OPTIONAL | STRING, "l10n",    &l10n,
                       OPTIONAL | STRING, "context", &context))
            schema_state_add_override (state->schema_state,
                                       &state->key_state, &state->string,
                                       name, l10n, context, error);
          return;
        }
    }

  /* children of <key> {{{3 */
  else if (strcmp (container, "key") == 0)
    {
      if (strcmp (element_name, "default") == 0)
        {
          const gchar *l10n, *context;
          if (COLLECT (STRING | OPTIONAL, "l10n",    &l10n,
                       STRING | OPTIONAL, "context", &context))
            state->string = key_state_start_default (state->key_state,
                                                     l10n, context, error);
          return;
        }

      else if (strcmp (element_name, "summary") == 0)
        {
          if (NO_ATTRS ())
            {
              if (state->key_state->summary_seen && state->strict)
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             _("Only one <%s> element allowed inside <%s>"),
                             element_name, container);
              else
                state->string = g_string_new (NULL);

              state->key_state->summary_seen = TRUE;
            }
          return;
        }

      else if (strcmp (element_name, "description") == 0)
        {
          if (NO_ATTRS ())
            {
              if (state->key_state->description_seen && state->strict)
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             _("Only one <%s> element allowed inside <%s>"),
                             element_name, container);
              else
                state->string = g_string_new (NULL);

            state->key_state->description_seen = TRUE;
            }
          return;
        }

      else if (strcmp (element_name, "range") == 0)
        {
          const gchar *min, *max;
          if (COLLECT (STRING | OPTIONAL, "min", &min,
                       STRING | OPTIONAL, "max", &max))
            key_state_set_range (state->key_state, min, max, error);
          return;
        }

      else if (strcmp (element_name, "choices") == 0)
        {
          if (NO_ATTRS ())
            key_state_start_choices (state->key_state, error);
          return;
        }

      else if (strcmp (element_name, "aliases") == 0)
        {
          if (NO_ATTRS ())
            key_state_start_aliases (state->key_state, error);
          return;
        }
    }


  /* children of <choices> {{{3 */
  else if (strcmp (container, "choices") == 0)
    {
      if (strcmp (element_name, "choice") == 0)
        {
          const gchar *value;
          if (COLLECT (STRING, "value", &value))
            key_state_add_choice (state->key_state, value, error);
          return;
        }
    }


  /* children of <aliases> {{{3 */
  else if (strcmp (container, "aliases") == 0)
    {
      if (strcmp (element_name, "alias") == 0)
        {
          const gchar *value, *target;
          if (COLLECT (STRING, "value", &value, STRING, "target", &target))
            key_state_add_alias (state->key_state, value, target, error);
          return;
        }
    }


  /* children of <enum> {{{3 */
  else if (strcmp (container, "enum") == 0 ||
           strcmp (container, "flags") == 0)
    {
      if (strcmp (element_name, "value") == 0)
        {
          const gchar *nick, *valuestr;
          if (COLLECT (STRING, "nick", &nick,
                       STRING, "value", &valuestr))
            enum_state_add_value (state->enum_state, nick, valuestr, error);
          return;
        }
    }
  /* 3}}} */

  if (container)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                 _("Element <%s> not allowed inside <%s>"),
                 element_name, container);
  else
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                 _("Element <%s> not allowed at the top level"), element_name);
}
/* 2}}} */
/* End element {{{2 */

static void
key_state_end (KeyState **state_ptr,
               GError   **error)
{
  KeyState *state;

  state = *state_ptr;
  *state_ptr = NULL;

  if (state->default_value == NULL)
    {
      g_set_error_literal (error,
                           G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           _("Element <default> is required in <key>"));
      return;
    }
}

static void
schema_state_end (SchemaState **state_ptr,
                  GError      **error)
{
  *state_ptr = NULL;
}

static void
end_element (GMarkupParseContext  *context,
             const gchar          *element_name,
             gpointer              user_data,
             GError              **error)
{
  ParseState *state = user_data;

  if (strcmp (element_name, "schemalist") == 0)
    {
      g_free (state->schemalist_domain);
      state->schemalist_domain = NULL;
    }

  else if (strcmp (element_name, "enum") == 0 ||
           strcmp (element_name, "flags") == 0)
    enum_state_end (&state->enum_state, error);

  else if (strcmp (element_name, "schema") == 0)
    schema_state_end (&state->schema_state, error);

  else if (strcmp (element_name, "override") == 0)
    override_state_end (&state->key_state, &state->string, error);

  else if (strcmp (element_name, "key") == 0)
    key_state_end (&state->key_state, error);

  else if (strcmp (element_name, "default") == 0)
    key_state_end_default (state->key_state, &state->string, error);

  else if (strcmp (element_name, "choices") == 0)
    key_state_end_choices (state->key_state, error);

  else if (strcmp (element_name, "aliases") == 0)
    key_state_end_aliases (state->key_state, error);

  if (state->string)
    {
      g_string_free (state->string, TRUE);
      state->string = NULL;
    }
}
/* Text {{{2 */
static void
text (GMarkupParseContext  *context,
      const gchar          *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
  ParseState *state = user_data;

  if (state->string)
    {
      /* we are expecting a string, so store the text data.
       *
       * we store the data verbatim here and deal with whitespace
       * later on.  there are two reasons for that:
       *
       *  1) whitespace is handled differently depending on the tag
       *     type.
       *
       *  2) we could do leading whitespace removal by refusing to
       *     insert it into state->string if it's at the start, but for
       *     trailing whitespace, we have no idea if there is another
       *     text() call coming or not.
       */
      g_string_append_len (state->string, text, text_len);
    }
  else
    {
      /* string is not expected: accept (and ignore) pure whitespace */
      gsize i;

      for (i = 0; i < text_len; i++)
        if (!g_ascii_isspace (text[i]))
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         _("Text may not appear inside <%s>"),
                         g_markup_parse_context_get_element (context));
            break;
          }
    }
}

/* Write to GVDB {{{1 */
typedef struct
{
  GHashTable *table;
  GvdbItem *root;
} GvdbPair;

static void
gvdb_pair_init (GvdbPair *pair)
{
  pair->table = gvdb_hash_table_new (NULL, NULL);
  pair->root = gvdb_hash_table_insert (pair->table, "");
}

static void
gvdb_pair_clear (GvdbPair *pair)
{
  g_hash_table_unref (pair->table);
}

typedef struct
{
  GHashTable *schema_table;
  GvdbPair root_pair;
} WriteToFileData;

typedef struct
{
  GHashTable *schema_table;
  GvdbPair pair;
  gboolean l10n;
} OutputSchemaData;

static void
output_key (gpointer key,
            gpointer value,
            gpointer user_data)
{
  OutputSchemaData *data;
  const gchar *name;
  KeyState *state;
  GvdbItem *item;
  GVariant *serialised = NULL;

  name = key;
  state = value;
  data = user_data;

  item = gvdb_hash_table_insert (data->pair.table, name);
  gvdb_item_set_parent (item, data->pair.root);
  serialised = key_state_serialise (state);
  gvdb_item_set_value (item, serialised);
  g_variant_unref (serialised);

  if (state->l10n)
    data->l10n = TRUE;

  if (state->child_schema &&
      !g_hash_table_lookup (data->schema_table, state->child_schema))
    {
      gchar *message = NULL;
      message = g_strdup_printf (_("Warning: undefined reference to <schema id='%s'/>"),
                                 state->child_schema);
      g_printerr ("%s\n", message);
      g_free (message);
    }
}

static void
output_schema (gpointer key,
               gpointer value,
               gpointer user_data)
{
  WriteToFileData *wtf_data = user_data;
  OutputSchemaData data;
  GvdbPair *root_pair;
  SchemaState *state;
  const gchar *id;
  GvdbItem *item;

  id = key;
  state = value;
  root_pair = &wtf_data->root_pair;

  data.schema_table = wtf_data->schema_table;
  gvdb_pair_init (&data.pair);
  data.l10n = FALSE;

  item = gvdb_hash_table_insert (root_pair->table, id);
  gvdb_item_set_parent (item, root_pair->root);
  gvdb_item_set_hash_table (item, data.pair.table);

  g_hash_table_foreach (state->keys, output_key, &data);

  if (state->path)
    gvdb_hash_table_insert_string (data.pair.table, ".path", state->path);

  if (state->extends_name)
    gvdb_hash_table_insert_string (data.pair.table, ".extends",
                                   state->extends_name);

  if (state->list_of)
    gvdb_hash_table_insert_string (data.pair.table, ".list-of",
                                   state->list_of);

  if (data.l10n)
    gvdb_hash_table_insert_string (data.pair.table,
                                   ".gettext-domain",
                                   state->gettext_domain);

  gvdb_pair_clear (&data.pair);
}

static gboolean
write_to_file (GHashTable   *schema_table,
               const gchar  *filename,
               GError      **error)
{
  WriteToFileData data;
  gboolean success;

  data.schema_table = schema_table;

  gvdb_pair_init (&data.root_pair);

  g_hash_table_foreach (schema_table, output_schema, &data);

  success = gvdb_table_write_contents (data.root_pair.table, filename,
                                       G_BYTE_ORDER != G_LITTLE_ENDIAN,
                                       error);
  g_hash_table_unref (data.root_pair.table);

  return success;
}

/* Parser driver {{{1 */
static GHashTable *
parse_gschema_files (gchar    **files,
                     gboolean   strict)
{
  GMarkupParser parser = { start_element, end_element, text };
  ParseState state = { 0, };
  const gchar *filename;
  GError *error = NULL;

  state.strict = strict;

  state.enum_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, enum_state_free);

  state.flags_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, enum_state_free);

  state.schema_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, schema_state_free);

  while ((filename = *files++) != NULL)
    {
      GMarkupParseContext *context;
      gchar *contents;
      gsize size;
      gint line, col;

      if (!g_file_get_contents (filename, &contents, &size, &error))
        {
          fprintf (stderr, "%s\n", error->message);
          g_clear_error (&error);
          continue;
        }

      context = g_markup_parse_context_new (&parser,
                                            G_MARKUP_TREAT_CDATA_AS_TEXT |
                                            G_MARKUP_PREFIX_ERROR_POSITION |
                                            G_MARKUP_IGNORE_QUALIFIED,
                                            &state, NULL);


      if (!g_markup_parse_context_parse (context, contents, size, &error) ||
          !g_markup_parse_context_end_parse (context, &error))
        {
          GSList *item;

          /* back out any changes from this file */
          for (item = state.this_file_schemas; item; item = item->next)
            g_hash_table_remove (state.schema_table, item->data);

          for (item = state.this_file_flagss; item; item = item->next)
            g_hash_table_remove (state.flags_table, item->data);

          for (item = state.this_file_enums; item; item = item->next)
            g_hash_table_remove (state.enum_table, item->data);

          /* let them know */
          g_markup_parse_context_get_position (context, &line, &col);
          fprintf (stderr, "%s:%d:%d  %s.  ", filename, line, col, error->message);
          g_clear_error (&error);

          if (strict)
            {
              /* Translators: Do not translate "--strict". */
              fprintf (stderr, _("--strict was specified; exiting.\n"));
              g_hash_table_unref (state.schema_table);
              g_hash_table_unref (state.flags_table);
              g_hash_table_unref (state.enum_table);

              g_free (contents);

              return NULL;
            }
          else
            fprintf (stderr, _("This entire file has been ignored.\n"));
        }

      /* cleanup */
      g_free (contents);
      g_markup_parse_context_free (context);
      g_slist_free (state.this_file_schemas);
      g_slist_free (state.this_file_flagss);
      g_slist_free (state.this_file_enums);
      state.this_file_schemas = NULL;
      state.this_file_flagss = NULL;
      state.this_file_enums = NULL;
    }

  g_hash_table_unref (state.flags_table);
  g_hash_table_unref (state.enum_table);

  return state.schema_table;
}

static gint
compare_strings (gconstpointer a,
                 gconstpointer b)
{
  gchar *one = *(gchar **) a;
  gchar *two = *(gchar **) b;
  gint cmp;

  cmp = g_str_has_suffix (two, ".enums.xml") -
        g_str_has_suffix (one, ".enums.xml");

  if (!cmp)
    cmp = strcmp (one, two);

  return cmp;
}

static gboolean
set_overrides (GHashTable  *schema_table,
               gchar      **files,
               gboolean     strict)
{
  const gchar *filename;
  GError *error = NULL;

  while ((filename = *files++))
    {
      GKeyFile *key_file;
      gchar **groups;
      gint i;

      key_file = g_key_file_new ();
      if (!g_key_file_load_from_file (key_file, filename, 0, &error))
        {
          fprintf (stderr, "%s: %s.  ", filename, error->message);
          g_key_file_free (key_file);
          g_clear_error (&error);

          if (!strict)
            {
              fprintf (stderr, _("Ignoring this file.\n"));
              continue;
            }

          fprintf (stderr, _("--strict was specified; exiting.\n"));
          return FALSE;
        }

      groups = g_key_file_get_groups (key_file, NULL);

      for (i = 0; groups[i]; i++)
        {
          const gchar *group = groups[i];
          SchemaState *schema;
          gchar **keys;
          gint j;

          schema = g_hash_table_lookup (schema_table, group);

          if (schema == NULL)
            /* Having the schema not be installed is expected to be a
             * common case.  Don't even emit an error message about
             * that.
             */
            continue;

          keys = g_key_file_get_keys (key_file, group, NULL, NULL);
          g_assert (keys != NULL);

          for (j = 0; keys[j]; j++)
            {
              const gchar *key = keys[j];
              KeyState *state;
              GVariant *value;
              gchar *string;

              state = g_hash_table_lookup (schema->keys, key);

              if (state == NULL)
                {
                  fprintf (stderr, _("No such key '%s' in schema '%s' as "
                                     "specified in override file '%s'"),
                           key, group, filename);

                  if (!strict)
                    {
                      fprintf (stderr, _("; ignoring override for this key.\n"));
                      continue;
                    }

                  fprintf (stderr, _(" and --strict was specified; exiting.\n"));
                  g_key_file_free (key_file);
                  g_strfreev (groups);
                  g_strfreev (keys);

                  return FALSE;
                }

              string = g_key_file_get_value (key_file, group, key, NULL);
              g_assert (string != NULL);

              value = g_variant_parse (state->type, string,
                                       NULL, NULL, &error);

              if (value == NULL)
                {
                  fprintf (stderr, _("error parsing key '%s' in schema '%s' "
                                     "as specified in override file '%s': "
                                     "%s."),
                           key, group, filename, error->message);

                  g_clear_error (&error);
                  g_free (string);

                  if (!strict)
                    {
                      fprintf (stderr, _("Ignoring override for this key.\n"));
                      continue;
                    }

                  fprintf (stderr, _("--strict was specified; exiting.\n"));
                  g_key_file_free (key_file);
                  g_strfreev (groups);
                  g_strfreev (keys);

                  return FALSE;
                }

              if (state->minimum)
                {
                  if (g_variant_compare (value, state->minimum) < 0 ||
                      g_variant_compare (value, state->maximum) > 0)
                    {
                      fprintf (stderr,
                               _("override for key '%s' in schema '%s' in "
                                 "override file '%s' is outside the range "
                                 "given in the schema"),
                               key, group, filename);

                      g_variant_unref (value);
                      g_free (string);

                      if (!strict)
                        {
                          fprintf (stderr, _("; ignoring override for this key.\n"));
                          continue;
                        }

                      fprintf (stderr, _(" and --strict was specified; exiting.\n"));
                      g_key_file_free (key_file);
                      g_strfreev (groups);
                      g_strfreev (keys);

                      return FALSE;
                    }
                }

              else if (state->strinfo->len)
                {
                  if (!is_valid_choices (value, state->strinfo))
                    {
                      fprintf (stderr,
                               _("override for key '%s' in schema '%s' in "
                                 "override file '%s' is not in the list "
                                 "of valid choices"),
                               key, group, filename);

                      g_variant_unref (value);
                      g_free (string);

                      if (!strict)
                        {
                          fprintf (stderr, _("; ignoring override for this key.\n"));
                          continue;
                        }

                      fprintf (stderr, _(" and --strict was specified; exiting.\n"));
                      g_key_file_free (key_file);
                      g_strfreev (groups);
                      g_strfreev (keys);

                      return FALSE;
                    }
                }

              g_variant_unref (state->default_value);
              state->default_value = value;
              g_free (string);
            }

          g_strfreev (keys);
        }

      g_strfreev (groups);
    }

  return TRUE;
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  GHashTable *table = NULL;
  GDir *dir = NULL;
  const gchar *file;
  const gchar *srcdir;
  gboolean show_version_and_exit = FALSE;
  gchar *targetdir = NULL;
  gchar *target = NULL;
  gboolean dry_run = FALSE;
  gboolean strict = FALSE;
  gchar **schema_files = NULL;
  gchar **override_files = NULL;
  GOptionContext *context = NULL;
  gint retval;
  GOptionEntry entries[] = {
    { "version", 0, 0, G_OPTION_ARG_NONE, &show_version_and_exit, N_("Show program version and exit"), NULL },
    { "targetdir", 0, 0, G_OPTION_ARG_FILENAME, &targetdir, N_("where to store the gschemas.compiled file"), N_("DIRECTORY") },
    { "strict", 0, 0, G_OPTION_ARG_NONE, &strict, N_("Abort on any errors in schemas"), NULL },
    { "dry-run", 0, 0, G_OPTION_ARG_NONE, &dry_run, N_("Do not write the gschema.compiled file"), NULL },
    { "allow-any-name", 0, 0, G_OPTION_ARG_NONE, &allow_any_name, N_("Do not enforce key name restrictions") },

    /* These options are only for use in the gschema-compile tests */
    { "schema-file", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_FILENAME_ARRAY, &schema_files, NULL, NULL },
    { NULL }
  };

#ifdef G_OS_WIN32
  gchar *tmp = NULL;
#endif

  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);

#ifdef G_OS_WIN32
  tmp = _glib_get_locale_dir ();
  bindtextdomain (GETTEXT_PACKAGE, tmp);
#else
  bindtextdomain (GETTEXT_PACKAGE, GLIB_LOCALE_DIR);
#endif

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  context = g_option_context_new (N_("DIRECTORY"));
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_summary (context,
    N_("Compile all GSettings schema files into a schema cache.\n"
       "Schema files are required to have the extension .gschema.xml,\n"
       "and the cache file is called gschemas.compiled."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      fprintf (stderr, "%s\n", error->message);
      retval = 1;
      goto done;
    }

  if (show_version_and_exit)
    {
      g_print (PACKAGE_VERSION "\n");
      retval = 0;
      goto done;
    }

  if (!schema_files && argc != 2)
    {
      fprintf (stderr, _("You should give exactly one directory name\n"));
      retval = 1;
      goto done;
    }

  srcdir = argv[1];

  target = g_build_filename (targetdir ? targetdir : srcdir, "gschemas.compiled", NULL);

  if (!schema_files)
    {
      GPtrArray *overrides;
      GPtrArray *files;

      files = g_ptr_array_new ();
      overrides = g_ptr_array_new ();

      dir = g_dir_open (srcdir, 0, &error);
      if (dir == NULL)
        {
          fprintf (stderr, "%s\n", error->message);

          g_ptr_array_unref (files);
          g_ptr_array_unref (overrides);

          retval = 1;
          goto done;
        }

      while ((file = g_dir_read_name (dir)) != NULL)
        {
          if (g_str_has_suffix (file, ".gschema.xml") ||
              g_str_has_suffix (file, ".enums.xml"))
            g_ptr_array_add (files, g_build_filename (srcdir, file, NULL));

          else if (g_str_has_suffix (file, ".gschema.override"))
            g_ptr_array_add (overrides,
                             g_build_filename (srcdir, file, NULL));
        }

      if (files->len == 0)
        {
          fprintf (stdout, _("No schema files found: "));

          if (g_unlink (target))
            fprintf (stdout, _("doing nothing.\n"));

          else
            fprintf (stdout, _("removed existing output file.\n"));

          g_ptr_array_unref (files);
          g_ptr_array_unref (overrides);

          retval = 0;
          goto done;
        }
      g_ptr_array_sort (files, compare_strings);
      g_ptr_array_add (files, NULL);

      g_ptr_array_sort (overrides, compare_strings);
      g_ptr_array_add (overrides, NULL);

      schema_files = (char **) g_ptr_array_free (files, FALSE);
      override_files = (gchar **) g_ptr_array_free (overrides, FALSE);
    }

  if ((table = parse_gschema_files (schema_files, strict)) == NULL)
    {
      retval = 1;
      goto done;
    }

  if (override_files != NULL &&
      !set_overrides (table, override_files, strict))
    {
      retval = 1;
      goto done;
    }

  if (!dry_run && !write_to_file (table, target, &error))
    {
      fprintf (stderr, "%s\n", error->message);
      retval = 1;
      goto done;
    }

  /* Success. */
  retval = 0;

done:
  g_clear_error (&error);
  g_clear_pointer (&table, g_hash_table_unref);
  g_clear_pointer (&dir, g_dir_close);
  g_free (targetdir);
  g_free (target);
  g_strfreev (schema_files);
  g_strfreev (override_files);
  g_option_context_free (context);

#ifdef G_OS_WIN32
  g_free (tmp);
#endif

  return retval;
}

/* Epilogue {{{1 */

/* vim:set foldmethod=marker: */
