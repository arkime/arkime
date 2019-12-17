/* gmarkup.c - Simple XML-like parser
 *
 *  Copyright 2000, 2003 Red Hat, Inc.
 *  Copyright 2007, 2008 Ryan Lortie <desrt@desrt.ca>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "gmarkup.h"

#include "gatomic.h"
#include "gslice.h"
#include "galloca.h"
#include "gstrfuncs.h"
#include "gstring.h"
#include "gtestutils.h"
#include "glibintl.h"
#include "gthread.h"

/**
 * SECTION:markup
 * @Title: Simple XML Subset Parser
 * @Short_description: parses a subset of XML
 * @See_also: [XML Specification](http://www.w3.org/TR/REC-xml/)
 *
 * The "GMarkup" parser is intended to parse a simple markup format
 * that's a subset of XML. This is a small, efficient, easy-to-use
 * parser. It should not be used if you expect to interoperate with
 * other applications generating full-scale XML. However, it's very
 * useful for application data files, config files, etc. where you
 * know your application will be the only one writing the file.
 * Full-scale XML parsers should be able to parse the subset used by
 * GMarkup, so you can easily migrate to full-scale XML at a later
 * time if the need arises.
 *
 * GMarkup is not guaranteed to signal an error on all invalid XML;
 * the parser may accept documents that an XML parser would not.
 * However, XML documents which are not well-formed (which is a
 * weaker condition than being valid. See the
 * [XML specification](http://www.w3.org/TR/REC-xml/)
 * for definitions of these terms.) are not considered valid GMarkup
 * documents.
 *
 * Simplifications to XML include:
 *
 * - Only UTF-8 encoding is allowed
 *
 * - No user-defined entities
 *
 * - Processing instructions, comments and the doctype declaration
 *   are "passed through" but are not interpreted in any way
 *
 * - No DTD or validation
 *
 * The markup format does support:
 *
 * - Elements
 *
 * - Attributes
 *
 * - 5 standard entities: &amp; &lt; &gt; &quot; &apos;
 *
 * - Character references
 *
 * - Sections marked as CDATA
 */

G_DEFINE_QUARK (g-markup-error-quark, g_markup_error)

typedef enum
{
  STATE_START,
  STATE_AFTER_OPEN_ANGLE,
  STATE_AFTER_CLOSE_ANGLE,
  STATE_AFTER_ELISION_SLASH, /* the slash that obviates need for end element */
  STATE_INSIDE_OPEN_TAG_NAME,
  STATE_INSIDE_ATTRIBUTE_NAME,
  STATE_AFTER_ATTRIBUTE_NAME,
  STATE_BETWEEN_ATTRIBUTES,
  STATE_AFTER_ATTRIBUTE_EQUALS_SIGN,
  STATE_INSIDE_ATTRIBUTE_VALUE_SQ,
  STATE_INSIDE_ATTRIBUTE_VALUE_DQ,
  STATE_INSIDE_TEXT,
  STATE_AFTER_CLOSE_TAG_SLASH,
  STATE_INSIDE_CLOSE_TAG_NAME,
  STATE_AFTER_CLOSE_TAG_NAME,
  STATE_INSIDE_PASSTHROUGH,
  STATE_ERROR
} GMarkupParseState;

typedef struct
{
  const char *prev_element;
  const GMarkupParser *prev_parser;
  gpointer prev_user_data;
} GMarkupRecursionTracker;

struct _GMarkupParseContext
{
  const GMarkupParser *parser;

  volatile gint ref_count;

  GMarkupParseFlags flags;

  gint line_number;
  gint char_number;

  GMarkupParseState state;

  gpointer user_data;
  GDestroyNotify dnotify;

  /* A piece of character data or an element that
   * hasn't "ended" yet so we haven't yet called
   * the callback for it.
   */
  GString *partial_chunk;
  GSList *spare_chunks;

  GSList *tag_stack;
  GSList *tag_stack_gstr;
  GSList *spare_list_nodes;

  GString **attr_names;
  GString **attr_values;
  gint cur_attr;
  gint alloc_attrs;

  const gchar *current_text;
  gssize       current_text_len;
  const gchar *current_text_end;

  /* used to save the start of the last interesting thingy */
  const gchar *start;

  const gchar *iter;

  guint document_empty : 1;
  guint parsing : 1;
  guint awaiting_pop : 1;
  gint balance;

  /* subparser support */
  GSList *subparser_stack; /* (GMarkupRecursionTracker *) */
  const char *subparser_element;
  gpointer held_user_data;
};

/*
 * Helpers to reduce our allocation overhead, we have
 * a well defined allocation lifecycle.
 */
static GSList *
get_list_node (GMarkupParseContext *context, gpointer data)
{
  GSList *node;
  if (context->spare_list_nodes != NULL)
    {
      node = context->spare_list_nodes;
      context->spare_list_nodes = g_slist_remove_link (context->spare_list_nodes, node);
    }
  else
    node = g_slist_alloc();
  node->data = data;
  return node;
}

static void
free_list_node (GMarkupParseContext *context, GSList *node)
{
  node->data = NULL;
  context->spare_list_nodes = g_slist_concat (node, context->spare_list_nodes);
}

static inline void
string_blank (GString *string)
{
  string->str[0] = '\0';
  string->len = 0;
}

/**
 * g_markup_parse_context_new:
 * @parser: a #GMarkupParser
 * @flags: one or more #GMarkupParseFlags
 * @user_data: user data to pass to #GMarkupParser functions
 * @user_data_dnotify: user data destroy notifier called when
 *     the parse context is freed
 *
 * Creates a new parse context. A parse context is used to parse
 * marked-up documents. You can feed any number of documents into
 * a context, as long as no errors occur; once an error occurs,
 * the parse context can't continue to parse text (you have to
 * free it and create a new parse context).
 *
 * Returns: a new #GMarkupParseContext
 **/
GMarkupParseContext *
g_markup_parse_context_new (const GMarkupParser *parser,
                            GMarkupParseFlags    flags,
                            gpointer             user_data,
                            GDestroyNotify       user_data_dnotify)
{
  GMarkupParseContext *context;

  g_return_val_if_fail (parser != NULL, NULL);

  context = g_new (GMarkupParseContext, 1);

  context->ref_count = 1;
  context->parser = parser;
  context->flags = flags;
  context->user_data = user_data;
  context->dnotify = user_data_dnotify;

  context->line_number = 1;
  context->char_number = 1;

  context->partial_chunk = NULL;
  context->spare_chunks = NULL;
  context->spare_list_nodes = NULL;

  context->state = STATE_START;
  context->tag_stack = NULL;
  context->tag_stack_gstr = NULL;
  context->attr_names = NULL;
  context->attr_values = NULL;
  context->cur_attr = -1;
  context->alloc_attrs = 0;

  context->current_text = NULL;
  context->current_text_len = -1;
  context->current_text_end = NULL;

  context->start = NULL;
  context->iter = NULL;

  context->document_empty = TRUE;
  context->parsing = FALSE;

  context->awaiting_pop = FALSE;
  context->subparser_stack = NULL;
  context->subparser_element = NULL;

  /* this is only looked at if awaiting_pop = TRUE.  initialise anyway. */
  context->held_user_data = NULL;

  context->balance = 0;

  return context;
}

/**
 * g_markup_parse_context_ref:
 * @context: a #GMarkupParseContext
 *
 * Increases the reference count of @context.
 *
 * Returns: the same @context
 *
 * Since: 2.36
 **/
GMarkupParseContext *
g_markup_parse_context_ref (GMarkupParseContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (context->ref_count > 0, NULL);

  g_atomic_int_inc (&context->ref_count);

  return context;
}

/**
 * g_markup_parse_context_unref:
 * @context: a #GMarkupParseContext
 *
 * Decreases the reference count of @context.  When its reference count
 * drops to 0, it is freed.
 *
 * Since: 2.36
 **/
void
g_markup_parse_context_unref (GMarkupParseContext *context)
{
  g_return_if_fail (context != NULL);
  g_return_if_fail (context->ref_count > 0);

  if (g_atomic_int_dec_and_test (&context->ref_count))
    g_markup_parse_context_free (context);
}

static void
string_full_free (gpointer ptr)
{
  g_string_free (ptr, TRUE);
}

static void clear_attributes (GMarkupParseContext *context);

/**
 * g_markup_parse_context_free:
 * @context: a #GMarkupParseContext
 *
 * Frees a #GMarkupParseContext.
 *
 * This function can't be called from inside one of the
 * #GMarkupParser functions or while a subparser is pushed.
 */
void
g_markup_parse_context_free (GMarkupParseContext *context)
{
  g_return_if_fail (context != NULL);
  g_return_if_fail (!context->parsing);
  g_return_if_fail (!context->subparser_stack);
  g_return_if_fail (!context->awaiting_pop);

  if (context->dnotify)
    (* context->dnotify) (context->user_data);

  clear_attributes (context);
  g_free (context->attr_names);
  g_free (context->attr_values);

  g_slist_free_full (context->tag_stack_gstr, string_full_free);
  g_slist_free (context->tag_stack);

  g_slist_free_full (context->spare_chunks, string_full_free);
  g_slist_free (context->spare_list_nodes);

  if (context->partial_chunk)
    g_string_free (context->partial_chunk, TRUE);

  g_free (context);
}

static void pop_subparser_stack (GMarkupParseContext *context);

static void
mark_error (GMarkupParseContext *context,
            GError              *error)
{
  context->state = STATE_ERROR;

  if (context->parser->error)
    (*context->parser->error) (context, error, context->user_data);

  /* report the error all the way up to free all the user-data */
  while (context->subparser_stack)
    {
      pop_subparser_stack (context);
      context->awaiting_pop = FALSE; /* already been freed */

      if (context->parser->error)
        (*context->parser->error) (context, error, context->user_data);
    }
}

static void
set_error (GMarkupParseContext  *context,
           GError              **error,
           GMarkupError          code,
           const gchar          *format,
           ...) G_GNUC_PRINTF (4, 5);

static void
set_error_literal (GMarkupParseContext  *context,
                   GError              **error,
                   GMarkupError          code,
                   const gchar          *message)
{
  GError *tmp_error;

  tmp_error = g_error_new_literal (G_MARKUP_ERROR, code, message);

  g_prefix_error (&tmp_error,
                  _("Error on line %d char %d: "),
                  context->line_number,
                  context->char_number);

  mark_error (context, tmp_error);

  g_propagate_error (error, tmp_error);
}

G_GNUC_PRINTF(4, 5)
static void
set_error (GMarkupParseContext  *context,
           GError              **error,
           GMarkupError          code,
           const gchar          *format,
           ...)
{
  gchar *s;
  gchar *s_valid;
  va_list args;

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  va_end (args);

  /* Make sure that the GError message is valid UTF-8
   * even if it is complaining about invalid UTF-8 in the markup
   */
  s_valid = g_utf8_make_valid (s, -1);
  set_error_literal (context, error, code, s);

  g_free (s);
  g_free (s_valid);
}

static void
propagate_error (GMarkupParseContext  *context,
                 GError              **dest,
                 GError               *src)
{
  if (context->flags & G_MARKUP_PREFIX_ERROR_POSITION)
    g_prefix_error (&src,
                    _("Error on line %d char %d: "),
                    context->line_number,
                    context->char_number);

  mark_error (context, src);

  g_propagate_error (dest, src);
}

#define IS_COMMON_NAME_END_CHAR(c) \
  ((c) == '=' || (c) == '/' || (c) == '>' || (c) == ' ')

static gboolean
slow_name_validate (GMarkupParseContext  *context,
                    const gchar          *name,
                    GError              **error)
{
  const gchar *p = name;

  if (!g_utf8_validate (name, strlen (name), NULL))
    {
      set_error (context, error, G_MARKUP_ERROR_BAD_UTF8,
                 _("Invalid UTF-8 encoded text in name - not valid '%s'"), name);
      return FALSE;
    }

  if (!(g_ascii_isalpha (*p) ||
        (!IS_COMMON_NAME_END_CHAR (*p) &&
         (*p == '_' ||
          *p == ':' ||
          g_unichar_isalpha (g_utf8_get_char (p))))))
    {
      set_error (context, error, G_MARKUP_ERROR_PARSE,
                 _("'%s' is not a valid name"), name);
      return FALSE;
    }

  for (p = g_utf8_next_char (name); *p != '\0'; p = g_utf8_next_char (p))
    {
      /* is_name_char */
      if (!(g_ascii_isalnum (*p) ||
            (!IS_COMMON_NAME_END_CHAR (*p) &&
             (*p == '.' ||
              *p == '-' ||
              *p == '_' ||
              *p == ':' ||
              g_unichar_isalpha (g_utf8_get_char (p))))))
        {
          set_error (context, error, G_MARKUP_ERROR_PARSE,
                     _("'%s' is not a valid name: '%c'"), name, *p);
          return FALSE;
        }
    }
  return TRUE;
}

/*
 * Use me for elements, attributes etc.
 */
static gboolean
name_validate (GMarkupParseContext  *context,
               const gchar          *name,
               GError              **error)
{
  char mask;
  const char *p;

  /* name start char */
  p = name;
  if (G_UNLIKELY (IS_COMMON_NAME_END_CHAR (*p) ||
                  !(g_ascii_isalpha (*p) || *p == '_' || *p == ':')))
    goto slow_validate;

  for (mask = *p++; *p != '\0'; p++)
    {
      mask |= *p;

      /* is_name_char */
      if (G_UNLIKELY (!(g_ascii_isalnum (*p) ||
                        (!IS_COMMON_NAME_END_CHAR (*p) &&
                         (*p == '.' ||
                          *p == '-' ||
                          *p == '_' ||
                          *p == ':')))))
        goto slow_validate;
    }

  if (mask & 0x80) /* un-common / non-ascii */
    goto slow_validate;

  return TRUE;

 slow_validate:
  return slow_name_validate (context, name, error);
}

static gboolean
text_validate (GMarkupParseContext  *context,
               const gchar          *p,
               gint                  len,
               GError              **error)
{
  if (!g_utf8_validate (p, len, NULL))
    {
      set_error (context, error, G_MARKUP_ERROR_BAD_UTF8,
                 _("Invalid UTF-8 encoded text in name - not valid '%s'"), p);
      return FALSE;
    }
  else
    return TRUE;
}

static gchar*
char_str (gunichar c,
          gchar   *buf)
{
  memset (buf, 0, 8);
  g_unichar_to_utf8 (c, buf);
  return buf;
}

static gchar*
utf8_str (const gchar *utf8,
          gchar       *buf)
{
  char_str (g_utf8_get_char (utf8), buf);
  return buf;
}

G_GNUC_PRINTF(5, 6)
static void
set_unescape_error (GMarkupParseContext  *context,
                    GError              **error,
                    const gchar          *remaining_text,
                    GMarkupError          code,
                    const gchar          *format,
                    ...)
{
  GError *tmp_error;
  gchar *s;
  va_list args;
  gint remaining_newlines;
  const gchar *p;

  remaining_newlines = 0;
  p = remaining_text;
  while (*p != '\0')
    {
      if (*p == '\n')
        ++remaining_newlines;
      ++p;
    }

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  va_end (args);

  tmp_error = g_error_new (G_MARKUP_ERROR,
                           code,
                           _("Error on line %d: %s"),
                           context->line_number - remaining_newlines,
                           s);

  g_free (s);

  mark_error (context, tmp_error);

  g_propagate_error (error, tmp_error);
}

/*
 * re-write the GString in-place, unescaping anything that escaped.
 * most XML does not contain entities, or escaping.
 */
static gboolean
unescape_gstring_inplace (GMarkupParseContext  *context,
                          GString              *string,
                          gboolean             *is_ascii,
                          GError              **error)
{
  char mask, *to;
  const char *from;
  gboolean normalize_attribute;

  *is_ascii = FALSE;

  /* are we unescaping an attribute or not ? */
  if (context->state == STATE_INSIDE_ATTRIBUTE_VALUE_SQ ||
      context->state == STATE_INSIDE_ATTRIBUTE_VALUE_DQ)
    normalize_attribute = TRUE;
  else
    normalize_attribute = FALSE;

  /*
   * Meeks' theorem: unescaping can only shrink text.
   * for &lt; etc. this is obvious, for &#xffff; more
   * thought is required, but this is patently so.
   */
  mask = 0;
  for (from = to = string->str; *from != '\0'; from++, to++)
    {
      *to = *from;

      mask |= *to;
      if (normalize_attribute && (*to == '\t' || *to == '\n'))
        *to = ' ';
      if (*to == '\r')
        {
          *to = normalize_attribute ? ' ' : '\n';
          if (from[1] == '\n')
            from++;
        }
      if (*from == '&')
        {
          from++;
          if (*from == '#')
            {
              gint base = 10;
              gulong l;
              gchar *end = NULL;

              from++;

              if (*from == 'x')
                {
                  base = 16;
                  from++;
                }

              errno = 0;
              l = strtoul (from, &end, base);

              if (end == from || errno != 0)
                {
                  set_unescape_error (context, error,
                                      from, G_MARKUP_ERROR_PARSE,
                                      _("Failed to parse '%-.*s', which "
                                        "should have been a digit "
                                        "inside a character reference "
                                        "(&#234; for example) - perhaps "
                                        "the digit is too large"),
                                      (int)(end - from), from);
                  return FALSE;
                }
              else if (*end != ';')
                {
                  set_unescape_error (context, error,
                                      from, G_MARKUP_ERROR_PARSE,
                                      _("Character reference did not end with a "
                                        "semicolon; "
                                        "most likely you used an ampersand "
                                        "character without intending to start "
                                        "an entity - escape ampersand as &amp;"));
                  return FALSE;
                }
              else
                {
                  /* characters XML 1.1 permits */
                  if ((0 < l && l <= 0xD7FF) ||
                      (0xE000 <= l && l <= 0xFFFD) ||
                      (0x10000 <= l && l <= 0x10FFFF))
                    {
                      gchar buf[8];
                      char_str (l, buf);
                      strcpy (to, buf);
                      to += strlen (buf) - 1;
                      from = end;
                      if (l >= 0x80) /* not ascii */
                        mask |= 0x80;
                    }
                  else
                    {
                      set_unescape_error (context, error,
                                          from, G_MARKUP_ERROR_PARSE,
                                          _("Character reference '%-.*s' does not "
                                            "encode a permitted character"),
                                          (int)(end - from), from);
                      return FALSE;
                    }
                }
            }

          else if (strncmp (from, "lt;", 3) == 0)
            {
              *to = '<';
              from += 2;
            }
          else if (strncmp (from, "gt;", 3) == 0)
            {
              *to = '>';
              from += 2;
            }
          else if (strncmp (from, "amp;", 4) == 0)
            {
              *to = '&';
              from += 3;
            }
          else if (strncmp (from, "quot;", 5) == 0)
            {
              *to = '"';
              from += 4;
            }
          else if (strncmp (from, "apos;", 5) == 0)
            {
              *to = '\'';
              from += 4;
            }
          else
            {
              if (*from == ';')
                set_unescape_error (context, error,
                                    from, G_MARKUP_ERROR_PARSE,
                                    _("Empty entity '&;' seen; valid "
                                      "entities are: &amp; &quot; &lt; &gt; &apos;"));
              else
                {
                  const char *end = strchr (from, ';');
                  if (end)
                    set_unescape_error (context, error,
                                        from, G_MARKUP_ERROR_PARSE,
                                        _("Entity name '%-.*s' is not known"),
                                        (int)(end - from), from);
                  else
                    set_unescape_error (context, error,
                                        from, G_MARKUP_ERROR_PARSE,
                                        _("Entity did not end with a semicolon; "
                                          "most likely you used an ampersand "
                                          "character without intending to start "
                                          "an entity - escape ampersand as &amp;"));
                }
              return FALSE;
            }
        }
    }

  g_assert (to - string->str <= string->len);
  if (to - string->str != string->len)
    g_string_truncate (string, to - string->str);

  *is_ascii = !(mask & 0x80);

  return TRUE;
}

static inline gboolean
advance_char (GMarkupParseContext *context)
{
  context->iter++;
  context->char_number++;

  if (G_UNLIKELY (context->iter == context->current_text_end))
      return FALSE;

  else if (G_UNLIKELY (*context->iter == '\n'))
    {
      context->line_number++;
      context->char_number = 1;
    }

  return TRUE;
}

static inline gboolean
xml_isspace (char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void
skip_spaces (GMarkupParseContext *context)
{
  do
    {
      if (!xml_isspace (*context->iter))
        return;
    }
  while (advance_char (context));
}

static void
advance_to_name_end (GMarkupParseContext *context)
{
  do
    {
      if (IS_COMMON_NAME_END_CHAR (*(context->iter)))
        return;
      if (xml_isspace (*(context->iter)))
        return;
    }
  while (advance_char (context));
}

static void
release_chunk (GMarkupParseContext *context, GString *str)
{
  GSList *node;
  if (!str)
    return;
  if (str->allocated_len > 256)
    { /* large strings are unusual and worth freeing */
      g_string_free (str, TRUE);
      return;
    }
  string_blank (str);
  node = get_list_node (context, str);
  context->spare_chunks = g_slist_concat (node, context->spare_chunks);
}

static void
add_to_partial (GMarkupParseContext *context,
                const gchar         *text_start,
                const gchar         *text_end)
{
  if (context->partial_chunk == NULL)
    { /* allocate a new chunk to parse into */

      if (context->spare_chunks != NULL)
        {
          GSList *node = context->spare_chunks;
          context->spare_chunks = g_slist_remove_link (context->spare_chunks, node);
          context->partial_chunk = node->data;
          free_list_node (context, node);
        }
      else
        context->partial_chunk = g_string_sized_new (MAX (28, text_end - text_start));
    }

  if (text_start != text_end)
    g_string_insert_len (context->partial_chunk, -1,
                         text_start, text_end - text_start);
}

static inline void
truncate_partial (GMarkupParseContext *context)
{
  if (context->partial_chunk != NULL)
    string_blank (context->partial_chunk);
}

static inline const gchar*
current_element (GMarkupParseContext *context)
{
  return context->tag_stack->data;
}

static void
pop_subparser_stack (GMarkupParseContext *context)
{
  GMarkupRecursionTracker *tracker;

  g_assert (context->subparser_stack);

  tracker = context->subparser_stack->data;

  context->awaiting_pop = TRUE;
  context->held_user_data = context->user_data;

  context->user_data = tracker->prev_user_data;
  context->parser = tracker->prev_parser;
  context->subparser_element = tracker->prev_element;
  g_slice_free (GMarkupRecursionTracker, tracker);

  context->subparser_stack = g_slist_delete_link (context->subparser_stack,
                                                  context->subparser_stack);
}

static void
push_partial_as_tag (GMarkupParseContext *context)
{
  GString *str = context->partial_chunk;
  /* sadly, this is exported by gmarkup_get_element_stack as-is */
  context->tag_stack = g_slist_concat (get_list_node (context, str->str), context->tag_stack);
  context->tag_stack_gstr = g_slist_concat (get_list_node (context, str), context->tag_stack_gstr);
  context->partial_chunk = NULL;
}

static void
pop_tag (GMarkupParseContext *context)
{
  GSList *nodea, *nodeb;

  nodea = context->tag_stack;
  nodeb = context->tag_stack_gstr;
  release_chunk (context, nodeb->data);
  context->tag_stack = g_slist_remove_link (context->tag_stack, nodea);
  context->tag_stack_gstr = g_slist_remove_link (context->tag_stack_gstr, nodeb);
  free_list_node (context, nodea);
  free_list_node (context, nodeb);
}

static void
possibly_finish_subparser (GMarkupParseContext *context)
{
  if (current_element (context) == context->subparser_element)
    pop_subparser_stack (context);
}

static void
ensure_no_outstanding_subparser (GMarkupParseContext *context)
{
  if (context->awaiting_pop)
    g_critical ("During the first end_element call after invoking a "
                "subparser you must pop the subparser stack and handle "
                "the freeing of the subparser user_data.  This can be "
                "done by calling the end function of the subparser.  "
                "Very probably, your program just leaked memory.");

  /* let valgrind watch the pointer disappear... */
  context->held_user_data = NULL;
  context->awaiting_pop = FALSE;
}

static const gchar*
current_attribute (GMarkupParseContext *context)
{
  g_assert (context->cur_attr >= 0);
  return context->attr_names[context->cur_attr]->str;
}

static void
add_attribute (GMarkupParseContext *context, GString *str)
{
  if (context->cur_attr + 2 >= context->alloc_attrs)
    {
      context->alloc_attrs += 5; /* silly magic number */
      context->attr_names = g_realloc (context->attr_names, sizeof(GString*)*context->alloc_attrs);
      context->attr_values = g_realloc (context->attr_values, sizeof(GString*)*context->alloc_attrs);
    }
  context->cur_attr++;
  context->attr_names[context->cur_attr] = str;
  context->attr_values[context->cur_attr] = NULL;
  context->attr_names[context->cur_attr+1] = NULL;
  context->attr_values[context->cur_attr+1] = NULL;
}

static void
clear_attributes (GMarkupParseContext *context)
{
  /* Go ahead and free the attributes. */
  for (; context->cur_attr >= 0; context->cur_attr--)
    {
      int pos = context->cur_attr;
      release_chunk (context, context->attr_names[pos]);
      release_chunk (context, context->attr_values[pos]);
      context->attr_names[pos] = context->attr_values[pos] = NULL;
    }
  g_assert (context->cur_attr == -1);
  g_assert (context->attr_names == NULL ||
            context->attr_names[0] == NULL);
  g_assert (context->attr_values == NULL ||
            context->attr_values[0] == NULL);
}

/* This has to be a separate function to ensure the alloca's
 * are unwound on exit - otherwise we grow & blow the stack
 * with large documents
 */
static inline void
emit_start_element (GMarkupParseContext  *context,
                    GError              **error)
{
  int i, j = 0;
  const gchar *start_name;
  const gchar **attr_names;
  const gchar **attr_values;
  GError *tmp_error;

  /* In case we want to ignore qualified tags and we see that we have
   * one here, we push a subparser.  This will ignore all tags inside of
   * the qualified tag.
   *
   * We deal with the end of the subparser from emit_end_element.
   */
  if ((context->flags & G_MARKUP_IGNORE_QUALIFIED) && strchr (current_element (context), ':'))
    {
      static const GMarkupParser ignore_parser;
      g_markup_parse_context_push (context, &ignore_parser, NULL);
      clear_attributes (context);
      return;
    }

  attr_names = g_newa (const gchar *, context->cur_attr + 2);
  attr_values = g_newa (const gchar *, context->cur_attr + 2);
  for (i = 0; i < context->cur_attr + 1; i++)
    {
      /* Possibly omit qualified attribute names from the list */
      if ((context->flags & G_MARKUP_IGNORE_QUALIFIED) && strchr (context->attr_names[i]->str, ':'))
        continue;

      attr_names[j] = context->attr_names[i]->str;
      attr_values[j] = context->attr_values[i]->str;
      j++;
    }
  attr_names[j] = NULL;
  attr_values[j] = NULL;

  /* Call user callback for element start */
  tmp_error = NULL;
  start_name = current_element (context);

  if (context->parser->start_element &&
      name_validate (context, start_name, error))
    (* context->parser->start_element) (context,
                                        start_name,
                                        (const gchar **)attr_names,
                                        (const gchar **)attr_values,
                                        context->user_data,
                                        &tmp_error);
  clear_attributes (context);

  if (tmp_error != NULL)
    propagate_error (context, error, tmp_error);
}

static void
emit_end_element (GMarkupParseContext  *context,
                  GError              **error)
{
  /* We need to pop the tag stack and call the end_element
   * function, since this is the close tag
   */
  GError *tmp_error = NULL;

  g_assert (context->tag_stack != NULL);

  possibly_finish_subparser (context);

  /* We might have just returned from our ignore subparser */
  if ((context->flags & G_MARKUP_IGNORE_QUALIFIED) && strchr (current_element (context), ':'))
    {
      g_markup_parse_context_pop (context);
      pop_tag (context);
      return;
    }

  tmp_error = NULL;
  if (context->parser->end_element)
    (* context->parser->end_element) (context,
                                      current_element (context),
                                      context->user_data,
                                      &tmp_error);

  ensure_no_outstanding_subparser (context);

  if (tmp_error)
    {
      mark_error (context, tmp_error);
      g_propagate_error (error, tmp_error);
    }

  pop_tag (context);
}

/**
 * g_markup_parse_context_parse:
 * @context: a #GMarkupParseContext
 * @text: chunk of text to parse
 * @text_len: length of @text in bytes
 * @error: return location for a #GError
 *
 * Feed some data to the #GMarkupParseContext.
 *
 * The data need not be valid UTF-8; an error will be signaled if
 * it's invalid. The data need not be an entire document; you can
 * feed a document into the parser incrementally, via multiple calls
 * to this function. Typically, as you receive data from a network
 * connection or file, you feed each received chunk of data into this
 * function, aborting the process if an error occurs. Once an error
 * is reported, no further data may be fed to the #GMarkupParseContext;
 * all errors are fatal.
 *
 * Returns: %FALSE if an error occurred, %TRUE on success
 */
gboolean
g_markup_parse_context_parse (GMarkupParseContext  *context,
                              const gchar          *text,
                              gssize                text_len,
                              GError              **error)
{
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (text != NULL, FALSE);
  g_return_val_if_fail (context->state != STATE_ERROR, FALSE);
  g_return_val_if_fail (!context->parsing, FALSE);

  if (text_len < 0)
    text_len = strlen (text);

  if (text_len == 0)
    return TRUE;

  context->parsing = TRUE;


  context->current_text = text;
  context->current_text_len = text_len;
  context->current_text_end = context->current_text + text_len;
  context->iter = context->current_text;
  context->start = context->iter;

  while (context->iter != context->current_text_end)
    {
      switch (context->state)
        {
        case STATE_START:
          /* Possible next state: AFTER_OPEN_ANGLE */

          g_assert (context->tag_stack == NULL);

          /* whitespace is ignored outside of any elements */
          skip_spaces (context);

          if (context->iter != context->current_text_end)
            {
              if (*context->iter == '<')
                {
                  /* Move after the open angle */
                  advance_char (context);

                  context->state = STATE_AFTER_OPEN_ANGLE;

                  /* this could start a passthrough */
                  context->start = context->iter;

                  /* document is now non-empty */
                  context->document_empty = FALSE;
                }
              else
                {
                  set_error_literal (context,
                                     error,
                                     G_MARKUP_ERROR_PARSE,
                                     _("Document must begin with an element (e.g. <book>)"));
                }
            }
          break;

        case STATE_AFTER_OPEN_ANGLE:
          /* Possible next states: INSIDE_OPEN_TAG_NAME,
           *  AFTER_CLOSE_TAG_SLASH, INSIDE_PASSTHROUGH
           */
          if (*context->iter == '?' ||
              *context->iter == '!')
            {
              /* include < in the passthrough */
              const gchar *openangle = "<";
              add_to_partial (context, openangle, openangle + 1);
              context->start = context->iter;
              context->balance = 1;
              context->state = STATE_INSIDE_PASSTHROUGH;
            }
          else if (*context->iter == '/')
            {
              /* move after it */
              advance_char (context);

              context->state = STATE_AFTER_CLOSE_TAG_SLASH;
            }
          else if (!IS_COMMON_NAME_END_CHAR (*(context->iter)))
            {
              context->state = STATE_INSIDE_OPEN_TAG_NAME;

              /* start of tag name */
              context->start = context->iter;
            }
          else
            {
              gchar buf[8];

              set_error (context,
                         error,
                         G_MARKUP_ERROR_PARSE,
                         _("'%s' is not a valid character following "
                           "a '<' character; it may not begin an "
                           "element name"),
                         utf8_str (context->iter, buf));
            }
          break;

          /* The AFTER_CLOSE_ANGLE state is actually sort of
           * broken, because it doesn't correspond to a range
           * of characters in the input stream as the others do,
           * and thus makes things harder to conceptualize
           */
        case STATE_AFTER_CLOSE_ANGLE:
          /* Possible next states: INSIDE_TEXT, STATE_START */
          if (context->tag_stack == NULL)
            {
              context->start = NULL;
              context->state = STATE_START;
            }
          else
            {
              context->start = context->iter;
              context->state = STATE_INSIDE_TEXT;
            }
          break;

        case STATE_AFTER_ELISION_SLASH:
          /* Possible next state: AFTER_CLOSE_ANGLE */
          if (*context->iter == '>')
            {
              /* move after the close angle */
              advance_char (context);
              context->state = STATE_AFTER_CLOSE_ANGLE;
              emit_end_element (context, error);
            }
          else
            {
              gchar buf[8];

              set_error (context,
                         error,
                         G_MARKUP_ERROR_PARSE,
                         _("Odd character '%s', expected a '>' character "
                           "to end the empty-element tag '%s'"),
                         utf8_str (context->iter, buf),
                         current_element (context));
            }
          break;

        case STATE_INSIDE_OPEN_TAG_NAME:
          /* Possible next states: BETWEEN_ATTRIBUTES */

          /* if there's a partial chunk then it's the first part of the
           * tag name. If there's a context->start then it's the start
           * of the tag name in current_text, the partial chunk goes
           * before that start though.
           */
          advance_to_name_end (context);

          if (context->iter == context->current_text_end)
            {
              /* The name hasn't necessarily ended. Merge with
               * partial chunk, leave state unchanged.
               */
              add_to_partial (context, context->start, context->iter);
            }
          else
            {
              /* The name has ended. Combine it with the partial chunk
               * if any; push it on the stack; enter next state.
               */
              add_to_partial (context, context->start, context->iter);
              push_partial_as_tag (context);

              context->state = STATE_BETWEEN_ATTRIBUTES;
              context->start = NULL;
            }
          break;

        case STATE_INSIDE_ATTRIBUTE_NAME:
          /* Possible next states: AFTER_ATTRIBUTE_NAME */

          advance_to_name_end (context);
          add_to_partial (context, context->start, context->iter);

          /* read the full name, if we enter the equals sign state
           * then add the attribute to the list (without the value),
           * otherwise store a partial chunk to be prepended later.
           */
          if (context->iter != context->current_text_end)
            context->state = STATE_AFTER_ATTRIBUTE_NAME;
          break;

        case STATE_AFTER_ATTRIBUTE_NAME:
          /* Possible next states: AFTER_ATTRIBUTE_EQUALS_SIGN */

          skip_spaces (context);

          if (context->iter != context->current_text_end)
            {
              /* The name has ended. Combine it with the partial chunk
               * if any; push it on the stack; enter next state.
               */
              if (!name_validate (context, context->partial_chunk->str, error))
                break;

              add_attribute (context, context->partial_chunk);

              context->partial_chunk = NULL;
              context->start = NULL;

              if (*context->iter == '=')
                {
                  advance_char (context);
                  context->state = STATE_AFTER_ATTRIBUTE_EQUALS_SIGN;
                }
              else
                {
                  gchar buf[8];

                  set_error (context,
                             error,
                             G_MARKUP_ERROR_PARSE,
                             _("Odd character '%s', expected a '=' after "
                               "attribute name '%s' of element '%s'"),
                             utf8_str (context->iter, buf),
                             current_attribute (context),
                             current_element (context));

                }
            }
          break;

        case STATE_BETWEEN_ATTRIBUTES:
          /* Possible next states: AFTER_CLOSE_ANGLE,
           * AFTER_ELISION_SLASH, INSIDE_ATTRIBUTE_NAME
           */
          skip_spaces (context);

          if (context->iter != context->current_text_end)
            {
              if (*context->iter == '/')
                {
                  advance_char (context);
                  context->state = STATE_AFTER_ELISION_SLASH;
                }
              else if (*context->iter == '>')
                {
                  advance_char (context);
                  context->state = STATE_AFTER_CLOSE_ANGLE;
                }
              else if (!IS_COMMON_NAME_END_CHAR (*(context->iter)))
                {
                  context->state = STATE_INSIDE_ATTRIBUTE_NAME;
                  /* start of attribute name */
                  context->start = context->iter;
                }
              else
                {
                  gchar buf[8];

                  set_error (context,
                             error,
                             G_MARKUP_ERROR_PARSE,
                             _("Odd character '%s', expected a '>' or '/' "
                               "character to end the start tag of "
                               "element '%s', or optionally an attribute; "
                               "perhaps you used an invalid character in "
                               "an attribute name"),
                             utf8_str (context->iter, buf),
                             current_element (context));
                }

              /* If we're done with attributes, invoke
               * the start_element callback
               */
              if (context->state == STATE_AFTER_ELISION_SLASH ||
                  context->state == STATE_AFTER_CLOSE_ANGLE)
                emit_start_element (context, error);
            }
          break;

        case STATE_AFTER_ATTRIBUTE_EQUALS_SIGN:
          /* Possible next state: INSIDE_ATTRIBUTE_VALUE_[SQ/DQ] */

          skip_spaces (context);

          if (context->iter != context->current_text_end)
            {
              if (*context->iter == '"')
                {
                  advance_char (context);
                  context->state = STATE_INSIDE_ATTRIBUTE_VALUE_DQ;
                  context->start = context->iter;
                }
              else if (*context->iter == '\'')
                {
                  advance_char (context);
                  context->state = STATE_INSIDE_ATTRIBUTE_VALUE_SQ;
                  context->start = context->iter;
                }
              else
                {
                  gchar buf[8];

                  set_error (context,
                             error,
                             G_MARKUP_ERROR_PARSE,
                             _("Odd character '%s', expected an open quote mark "
                               "after the equals sign when giving value for "
                               "attribute '%s' of element '%s'"),
                             utf8_str (context->iter, buf),
                             current_attribute (context),
                             current_element (context));
                }
            }
          break;

        case STATE_INSIDE_ATTRIBUTE_VALUE_SQ:
        case STATE_INSIDE_ATTRIBUTE_VALUE_DQ:
          /* Possible next states: BETWEEN_ATTRIBUTES */
          {
            gchar delim;

            if (context->state == STATE_INSIDE_ATTRIBUTE_VALUE_SQ)
              {
                delim = '\'';
              }
            else
              {
                delim = '"';
              }

            do
              {
                if (*context->iter == delim)
                  break;
              }
            while (advance_char (context));
          }
          if (context->iter == context->current_text_end)
            {
              /* The value hasn't necessarily ended. Merge with
               * partial chunk, leave state unchanged.
               */
              add_to_partial (context, context->start, context->iter);
            }
          else
            {
              gboolean is_ascii;
              /* The value has ended at the quote mark. Combine it
               * with the partial chunk if any; set it for the current
               * attribute.
               */
              add_to_partial (context, context->start, context->iter);

              g_assert (context->cur_attr >= 0);

              if (unescape_gstring_inplace (context, context->partial_chunk, &is_ascii, error) &&
                  (is_ascii || text_validate (context, context->partial_chunk->str,
                                              context->partial_chunk->len, error)))
                {
                  /* success, advance past quote and set state. */
                  context->attr_values[context->cur_attr] = context->partial_chunk;
                  context->partial_chunk = NULL;
                  advance_char (context);
                  context->state = STATE_BETWEEN_ATTRIBUTES;
                  context->start = NULL;
                }

              truncate_partial (context);
            }
          break;

        case STATE_INSIDE_TEXT:
          /* Possible next states: AFTER_OPEN_ANGLE */
          do
            {
              if (*context->iter == '<')
                break;
            }
          while (advance_char (context));

          /* The text hasn't necessarily ended. Merge with
           * partial chunk, leave state unchanged.
           */

          add_to_partial (context, context->start, context->iter);

          if (context->iter != context->current_text_end)
            {
              gboolean is_ascii;

              /* The text has ended at the open angle. Call the text
               * callback.
               */
              if (unescape_gstring_inplace (context, context->partial_chunk, &is_ascii, error) &&
                  (is_ascii || text_validate (context, context->partial_chunk->str,
                                              context->partial_chunk->len, error)))
                {
                  GError *tmp_error = NULL;

                  if (context->parser->text)
                    (*context->parser->text) (context,
                                              context->partial_chunk->str,
                                              context->partial_chunk->len,
                                              context->user_data,
                                              &tmp_error);

                  if (tmp_error == NULL)
                    {
                      /* advance past open angle and set state. */
                      advance_char (context);
                      context->state = STATE_AFTER_OPEN_ANGLE;
                      /* could begin a passthrough */
                      context->start = context->iter;
                    }
                  else
                    propagate_error (context, error, tmp_error);
                }

              truncate_partial (context);
            }
          break;

        case STATE_AFTER_CLOSE_TAG_SLASH:
          /* Possible next state: INSIDE_CLOSE_TAG_NAME */
          if (!IS_COMMON_NAME_END_CHAR (*(context->iter)))
            {
              context->state = STATE_INSIDE_CLOSE_TAG_NAME;

              /* start of tag name */
              context->start = context->iter;
            }
          else
            {
              gchar buf[8];

              set_error (context,
                         error,
                         G_MARKUP_ERROR_PARSE,
                         _("'%s' is not a valid character following "
                           "the characters '</'; '%s' may not begin an "
                           "element name"),
                         utf8_str (context->iter, buf),
                         utf8_str (context->iter, buf));
            }
          break;

        case STATE_INSIDE_CLOSE_TAG_NAME:
          /* Possible next state: AFTER_CLOSE_TAG_NAME */
          advance_to_name_end (context);
          add_to_partial (context, context->start, context->iter);

          if (context->iter != context->current_text_end)
            context->state = STATE_AFTER_CLOSE_TAG_NAME;
          break;

        case STATE_AFTER_CLOSE_TAG_NAME:
          /* Possible next state: AFTER_CLOSE_TAG_SLASH */

          skip_spaces (context);

          if (context->iter != context->current_text_end)
            {
              GString *close_name;

              close_name = context->partial_chunk;
              context->partial_chunk = NULL;

              if (*context->iter != '>')
                {
                  gchar buf[8];

                  set_error (context,
                             error,
                             G_MARKUP_ERROR_PARSE,
                             _("'%s' is not a valid character following "
                               "the close element name '%s'; the allowed "
                               "character is '>'"),
                             utf8_str (context->iter, buf),
                             close_name->str);
                }
              else if (context->tag_stack == NULL)
                {
                  set_error (context,
                             error,
                             G_MARKUP_ERROR_PARSE,
                             _("Element '%s' was closed, no element "
                               "is currently open"),
                             close_name->str);
                }
              else if (strcmp (close_name->str, current_element (context)) != 0)
                {
                  set_error (context,
                             error,
                             G_MARKUP_ERROR_PARSE,
                             _("Element '%s' was closed, but the currently "
                               "open element is '%s'"),
                             close_name->str,
                             current_element (context));
                }
              else
                {
                  advance_char (context);
                  context->state = STATE_AFTER_CLOSE_ANGLE;
                  context->start = NULL;

                  emit_end_element (context, error);
                }
              context->partial_chunk = close_name;
              truncate_partial (context);
            }
          break;

        case STATE_INSIDE_PASSTHROUGH:
          /* Possible next state: AFTER_CLOSE_ANGLE */
          do
            {
              if (*context->iter == '<')
                context->balance++;
              if (*context->iter == '>')
                {
                  gchar *str;
                  gsize len;

                  context->balance--;
                  add_to_partial (context, context->start, context->iter);
                  context->start = context->iter;

                  str = context->partial_chunk->str;
                  len = context->partial_chunk->len;

                  if (str[1] == '?' && str[len - 1] == '?')
                    break;
                  if (strncmp (str, "<!--", 4) == 0 &&
                      strcmp (str + len - 2, "--") == 0)
                    break;
                  if (strncmp (str, "<![CDATA[", 9) == 0 &&
                      strcmp (str + len - 2, "]]") == 0)
                    break;
                  if (strncmp (str, "<!DOCTYPE", 9) == 0 &&
                      context->balance == 0)
                    break;
                }
            }
          while (advance_char (context));

          if (context->iter == context->current_text_end)
            {
              /* The passthrough hasn't necessarily ended. Merge with
               * partial chunk, leave state unchanged.
               */
               add_to_partial (context, context->start, context->iter);
            }
          else
            {
              /* The passthrough has ended at the close angle. Combine
               * it with the partial chunk if any. Call the passthrough
               * callback. Note that the open/close angles are
               * included in the text of the passthrough.
               */
              GError *tmp_error = NULL;

              advance_char (context); /* advance past close angle */
              add_to_partial (context, context->start, context->iter);

              if (context->flags & G_MARKUP_TREAT_CDATA_AS_TEXT &&
                  strncmp (context->partial_chunk->str, "<![CDATA[", 9) == 0)
                {
                  if (context->parser->text &&
                      text_validate (context,
                                     context->partial_chunk->str + 9,
                                     context->partial_chunk->len - 12,
                                     error))
                    (*context->parser->text) (context,
                                              context->partial_chunk->str + 9,
                                              context->partial_chunk->len - 12,
                                              context->user_data,
                                              &tmp_error);
                }
              else if (context->parser->passthrough &&
                       text_validate (context,
                                      context->partial_chunk->str,
                                      context->partial_chunk->len,
                                      error))
                (*context->parser->passthrough) (context,
                                                 context->partial_chunk->str,
                                                 context->partial_chunk->len,
                                                 context->user_data,
                                                 &tmp_error);

              truncate_partial (context);

              if (tmp_error == NULL)
                {
                  context->state = STATE_AFTER_CLOSE_ANGLE;
                  context->start = context->iter; /* could begin text */
                }
              else
                propagate_error (context, error, tmp_error);
            }
          break;

        case STATE_ERROR:
          goto finished;
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

 finished:
  context->parsing = FALSE;

  return context->state != STATE_ERROR;
}

/**
 * g_markup_parse_context_end_parse:
 * @context: a #GMarkupParseContext
 * @error: return location for a #GError
 *
 * Signals to the #GMarkupParseContext that all data has been
 * fed into the parse context with g_markup_parse_context_parse().
 *
 * This function reports an error if the document isn't complete,
 * for example if elements are still open.
 *
 * Returns: %TRUE on success, %FALSE if an error was set
 */
gboolean
g_markup_parse_context_end_parse (GMarkupParseContext  *context,
                                  GError              **error)
{
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (!context->parsing, FALSE);
  g_return_val_if_fail (context->state != STATE_ERROR, FALSE);

  if (context->partial_chunk != NULL)
    {
      g_string_free (context->partial_chunk, TRUE);
      context->partial_chunk = NULL;
    }

  if (context->document_empty)
    {
      set_error_literal (context, error, G_MARKUP_ERROR_EMPTY,
                         _("Document was empty or contained only whitespace"));
      return FALSE;
    }

  context->parsing = TRUE;

  switch (context->state)
    {
    case STATE_START:
      /* Nothing to do */
      break;

    case STATE_AFTER_OPEN_ANGLE:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly just after an open angle bracket '<'"));
      break;

    case STATE_AFTER_CLOSE_ANGLE:
      if (context->tag_stack != NULL)
        {
          /* Error message the same as for INSIDE_TEXT */
          set_error (context, error, G_MARKUP_ERROR_PARSE,
                     _("Document ended unexpectedly with elements still open - "
                       "'%s' was the last element opened"),
                     current_element (context));
        }
      break;

    case STATE_AFTER_ELISION_SLASH:
      set_error (context, error, G_MARKUP_ERROR_PARSE,
                 _("Document ended unexpectedly, expected to see a close angle "
                   "bracket ending the tag <%s/>"), current_element (context));
      break;

    case STATE_INSIDE_OPEN_TAG_NAME:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside an element name"));
      break;

    case STATE_INSIDE_ATTRIBUTE_NAME:
    case STATE_AFTER_ATTRIBUTE_NAME:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside an attribute name"));
      break;

    case STATE_BETWEEN_ATTRIBUTES:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside an element-opening "
                           "tag."));
      break;

    case STATE_AFTER_ATTRIBUTE_EQUALS_SIGN:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly after the equals sign "
                           "following an attribute name; no attribute value"));
      break;

    case STATE_INSIDE_ATTRIBUTE_VALUE_SQ:
    case STATE_INSIDE_ATTRIBUTE_VALUE_DQ:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly while inside an attribute "
                           "value"));
      break;

    case STATE_INSIDE_TEXT:
      g_assert (context->tag_stack != NULL);
      set_error (context, error, G_MARKUP_ERROR_PARSE,
                 _("Document ended unexpectedly with elements still open - "
                   "'%s' was the last element opened"),
                 current_element (context));
      break;

    case STATE_AFTER_CLOSE_TAG_SLASH:
    case STATE_INSIDE_CLOSE_TAG_NAME:
    case STATE_AFTER_CLOSE_TAG_NAME:
      set_error (context, error, G_MARKUP_ERROR_PARSE,
                 _("Document ended unexpectedly inside the close tag for "
                   "element '%s'"), current_element (context));
      break;

    case STATE_INSIDE_PASSTHROUGH:
      set_error_literal (context, error, G_MARKUP_ERROR_PARSE,
                         _("Document ended unexpectedly inside a comment or "
                           "processing instruction"));
      break;

    case STATE_ERROR:
    default:
      g_assert_not_reached ();
      break;
    }

  context->parsing = FALSE;

  return context->state != STATE_ERROR;
}

/**
 * g_markup_parse_context_get_element:
 * @context: a #GMarkupParseContext
 *
 * Retrieves the name of the currently open element.
 *
 * If called from the start_element or end_element handlers this will
 * give the element_name as passed to those functions. For the parent
 * elements, see g_markup_parse_context_get_element_stack().
 *
 * Returns: the name of the currently open element, or %NULL
 *
 * Since: 2.2
 */
const gchar *
g_markup_parse_context_get_element (GMarkupParseContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);

  if (context->tag_stack == NULL)
    return NULL;
  else
    return current_element (context);
}

/**
 * g_markup_parse_context_get_element_stack:
 * @context: a #GMarkupParseContext
 *
 * Retrieves the element stack from the internal state of the parser.
 *
 * The returned #GSList is a list of strings where the first item is
 * the currently open tag (as would be returned by
 * g_markup_parse_context_get_element()) and the next item is its
 * immediate parent.
 *
 * This function is intended to be used in the start_element and
 * end_element handlers where g_markup_parse_context_get_element()
 * would merely return the name of the element that is being
 * processed.
 *
 * Returns: the element stack, which must not be modified
 *
 * Since: 2.16
 */
const GSList *
g_markup_parse_context_get_element_stack (GMarkupParseContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);
  return context->tag_stack;
}

/**
 * g_markup_parse_context_get_position:
 * @context: a #GMarkupParseContext
 * @line_number: (nullable): return location for a line number, or %NULL
 * @char_number: (nullable): return location for a char-on-line number, or %NULL
 *
 * Retrieves the current line number and the number of the character on
 * that line. Intended for use in error messages; there are no strict
 * semantics for what constitutes the "current" line number other than
 * "the best number we could come up with for error messages."
 */
void
g_markup_parse_context_get_position (GMarkupParseContext *context,
                                     gint                *line_number,
                                     gint                *char_number)
{
  g_return_if_fail (context != NULL);

  if (line_number)
    *line_number = context->line_number;

  if (char_number)
    *char_number = context->char_number;
}

/**
 * g_markup_parse_context_get_user_data:
 * @context: a #GMarkupParseContext
 *
 * Returns the user_data associated with @context.
 *
 * This will either be the user_data that was provided to
 * g_markup_parse_context_new() or to the most recent call
 * of g_markup_parse_context_push().
 *
 * Returns: the provided user_data. The returned data belongs to
 *     the markup context and will be freed when
 *     g_markup_parse_context_free() is called.
 *
 * Since: 2.18
 */
gpointer
g_markup_parse_context_get_user_data (GMarkupParseContext *context)
{
  return context->user_data;
}

/**
 * g_markup_parse_context_push:
 * @context: a #GMarkupParseContext
 * @parser: a #GMarkupParser
 * @user_data: user data to pass to #GMarkupParser functions
 *
 * Temporarily redirects markup data to a sub-parser.
 *
 * This function may only be called from the start_element handler of
 * a #GMarkupParser. It must be matched with a corresponding call to
 * g_markup_parse_context_pop() in the matching end_element handler
 * (except in the case that the parser aborts due to an error).
 *
 * All tags, text and other data between the matching tags is
 * redirected to the subparser given by @parser. @user_data is used
 * as the user_data for that parser. @user_data is also passed to the
 * error callback in the event that an error occurs. This includes
 * errors that occur in subparsers of the subparser.
 *
 * The end tag matching the start tag for which this call was made is
 * handled by the previous parser (which is given its own user_data)
 * which is why g_markup_parse_context_pop() is provided to allow "one
 * last access" to the @user_data provided to this function. In the
 * case of error, the @user_data provided here is passed directly to
 * the error callback of the subparser and g_markup_parse_context_pop()
 * should not be called. In either case, if @user_data was allocated
 * then it ought to be freed from both of these locations.
 *
 * This function is not intended to be directly called by users
 * interested in invoking subparsers. Instead, it is intended to be
 * used by the subparsers themselves to implement a higher-level
 * interface.
 *
 * As an example, see the following implementation of a simple
 * parser that counts the number of tags encountered.
 *
 * |[<!-- language="C" --> 
 * typedef struct
 * {
 *   gint tag_count;
 * } CounterData;
 *
 * static void
 * counter_start_element (GMarkupParseContext  *context,
 *                        const gchar          *element_name,
 *                        const gchar         **attribute_names,
 *                        const gchar         **attribute_values,
 *                        gpointer              user_data,
 *                        GError              **error)
 * {
 *   CounterData *data = user_data;
 *
 *   data->tag_count++;
 * }
 *
 * static void
 * counter_error (GMarkupParseContext *context,
 *                GError              *error,
 *                gpointer             user_data)
 * {
 *   CounterData *data = user_data;
 *
 *   g_slice_free (CounterData, data);
 * }
 *
 * static GMarkupParser counter_subparser =
 * {
 *   counter_start_element,
 *   NULL,
 *   NULL,
 *   NULL,
 *   counter_error
 * };
 * ]|
 *
 * In order to allow this parser to be easily used as a subparser, the
 * following interface is provided:
 *
 * |[<!-- language="C" --> 
 * void
 * start_counting (GMarkupParseContext *context)
 * {
 *   CounterData *data = g_slice_new (CounterData);
 *
 *   data->tag_count = 0;
 *   g_markup_parse_context_push (context, &counter_subparser, data);
 * }
 *
 * gint
 * end_counting (GMarkupParseContext *context)
 * {
 *   CounterData *data = g_markup_parse_context_pop (context);
 *   int result;
 *
 *   result = data->tag_count;
 *   g_slice_free (CounterData, data);
 *
 *   return result;
 * }
 * ]|
 *
 * The subparser would then be used as follows:
 *
 * |[<!-- language="C" --> 
 * static void start_element (context, element_name, ...)
 * {
 *   if (strcmp (element_name, "count-these") == 0)
 *     start_counting (context);
 *
 *   // else, handle other tags...
 * }
 *
 * static void end_element (context, element_name, ...)
 * {
 *   if (strcmp (element_name, "count-these") == 0)
 *     g_print ("Counted %d tags\n", end_counting (context));
 *
 *   // else, handle other tags...
 * }
 * ]|
 *
 * Since: 2.18
 **/
void
g_markup_parse_context_push (GMarkupParseContext *context,
                             const GMarkupParser *parser,
                             gpointer             user_data)
{
  GMarkupRecursionTracker *tracker;

  tracker = g_slice_new (GMarkupRecursionTracker);
  tracker->prev_element = context->subparser_element;
  tracker->prev_parser = context->parser;
  tracker->prev_user_data = context->user_data;

  context->subparser_element = current_element (context);
  context->parser = parser;
  context->user_data = user_data;

  context->subparser_stack = g_slist_prepend (context->subparser_stack,
                                              tracker);
}

/**
 * g_markup_parse_context_pop:
 * @context: a #GMarkupParseContext
 *
 * Completes the process of a temporary sub-parser redirection.
 *
 * This function exists to collect the user_data allocated by a
 * matching call to g_markup_parse_context_push(). It must be called
 * in the end_element handler corresponding to the start_element
 * handler during which g_markup_parse_context_push() was called.
 * You must not call this function from the error callback -- the
 * @user_data is provided directly to the callback in that case.
 *
 * This function is not intended to be directly called by users
 * interested in invoking subparsers. Instead, it is intended to
 * be used by the subparsers themselves to implement a higher-level
 * interface.
 *
 * Returns: the user data passed to g_markup_parse_context_push()
 *
 * Since: 2.18
 */
gpointer
g_markup_parse_context_pop (GMarkupParseContext *context)
{
  gpointer user_data;

  if (!context->awaiting_pop)
    possibly_finish_subparser (context);

  g_assert (context->awaiting_pop);

  context->awaiting_pop = FALSE;

  /* valgrind friendliness */
  user_data = context->held_user_data;
  context->held_user_data = NULL;

  return user_data;
}

static void
append_escaped_text (GString     *str,
                     const gchar *text,
                     gssize       length)
{
  const gchar *p;
  const gchar *end;
  gunichar c;

  p = text;
  end = text + length;

  while (p < end)
    {
      const gchar *next;
      next = g_utf8_next_char (p);

      switch (*p)
        {
        case '&':
          g_string_append (str, "&amp;");
          break;

        case '<':
          g_string_append (str, "&lt;");
          break;

        case '>':
          g_string_append (str, "&gt;");
          break;

        case '\'':
          g_string_append (str, "&apos;");
          break;

        case '"':
          g_string_append (str, "&quot;");
          break;

        default:
          c = g_utf8_get_char (p);
          if ((0x1 <= c && c <= 0x8) ||
              (0xb <= c && c  <= 0xc) ||
              (0xe <= c && c <= 0x1f) ||
              (0x7f <= c && c <= 0x84) ||
              (0x86 <= c && c <= 0x9f))
            g_string_append_printf (str, "&#x%x;", c);
          else
            g_string_append_len (str, p, next - p);
          break;
        }

      p = next;
    }
}

/**
 * g_markup_escape_text:
 * @text: some valid UTF-8 text
 * @length: length of @text in bytes, or -1 if the text is nul-terminated
 *
 * Escapes text so that the markup parser will parse it verbatim.
 * Less than, greater than, ampersand, etc. are replaced with the
 * corresponding entities. This function would typically be used
 * when writing out a file to be parsed with the markup parser.
 *
 * Note that this function doesn't protect whitespace and line endings
 * from being processed according to the XML rules for normalization
 * of line endings and attribute values.
 *
 * Note also that this function will produce character references in
 * the range of &#x1; ... &#x1f; for all control sequences
 * except for tabstop, newline and carriage return.  The character
 * references in this range are not valid XML 1.0, but they are
 * valid XML 1.1 and will be accepted by the GMarkup parser.
 *
 * Returns: a newly allocated string with the escaped text
 */
gchar*
g_markup_escape_text (const gchar *text,
                      gssize       length)
{
  GString *str;

  g_return_val_if_fail (text != NULL, NULL);

  if (length < 0)
    length = strlen (text);

  /* prealloc at least as long as original text */
  str = g_string_sized_new (length);
  append_escaped_text (str, text, length);

  return g_string_free (str, FALSE);
}

/*
 * find_conversion:
 * @format: a printf-style format string
 * @after: location to store a pointer to the character after
 *     the returned conversion. On a %NULL return, returns the
 *     pointer to the trailing NUL in the string
 *
 * Find the next conversion in a printf-style format string.
 * Partially based on code from printf-parser.c,
 * Copyright (C) 1999-2000, 2002-2003 Free Software Foundation, Inc.
 *
 * Returns: pointer to the next conversion in @format,
 *  or %NULL, if none.
 */
static const char *
find_conversion (const char  *format,
                 const char **after)
{
  const char *start = format;
  const char *cp;

  while (*start != '\0' && *start != '%')
    start++;

  if (*start == '\0')
    {
      *after = start;
      return NULL;
    }

  cp = start + 1;

  if (*cp == '\0')
    {
      *after = cp;
      return NULL;
    }

  /* Test for positional argument.  */
  if (*cp >= '0' && *cp <= '9')
    {
      const char *np;

      for (np = cp; *np >= '0' && *np <= '9'; np++)
        ;
      if (*np == '$')
        cp = np + 1;
    }

  /* Skip the flags.  */
  for (;;)
    {
      if (*cp == '\'' ||
          *cp == '-' ||
          *cp == '+' ||
          *cp == ' ' ||
          *cp == '#' ||
          *cp == '0')
        cp++;
      else
        break;
    }

  /* Skip the field width.  */
  if (*cp == '*')
    {
      cp++;

      /* Test for positional argument.  */
      if (*cp >= '0' && *cp <= '9')
        {
          const char *np;

          for (np = cp; *np >= '0' && *np <= '9'; np++)
            ;
          if (*np == '$')
            cp = np + 1;
        }
    }
  else
    {
      for (; *cp >= '0' && *cp <= '9'; cp++)
        ;
    }

  /* Skip the precision.  */
  if (*cp == '.')
    {
      cp++;
      if (*cp == '*')
        {
          /* Test for positional argument.  */
          if (*cp >= '0' && *cp <= '9')
            {
              const char *np;

              for (np = cp; *np >= '0' && *np <= '9'; np++)
                ;
              if (*np == '$')
                cp = np + 1;
            }
        }
      else
        {
          for (; *cp >= '0' && *cp <= '9'; cp++)
            ;
        }
    }

  /* Skip argument type/size specifiers.  */
  while (*cp == 'h' ||
         *cp == 'L' ||
         *cp == 'l' ||
         *cp == 'j' ||
         *cp == 'z' ||
         *cp == 'Z' ||
         *cp == 't')
    cp++;

  /* Skip the conversion character.  */
  cp++;

  *after = cp;
  return start;
}

/**
 * g_markup_vprintf_escaped:
 * @format: printf() style format string
 * @args: variable argument list, similar to vprintf()
 *
 * Formats the data in @args according to @format, escaping
 * all string and character arguments in the fashion
 * of g_markup_escape_text(). See g_markup_printf_escaped().
 *
 * Returns: newly allocated result from formatting
 *  operation. Free with g_free().
 *
 * Since: 2.4
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

gchar *
g_markup_vprintf_escaped (const gchar *format,
                          va_list      args)
{
  GString *format1;
  GString *format2;
  GString *result = NULL;
  gchar *output1 = NULL;
  gchar *output2 = NULL;
  const char *p, *op1, *op2;
  va_list args2;

  /* The technique here, is that we make two format strings that
   * have the identical conversions in the identical order to the
   * original strings, but differ in the text in-between. We
   * then use the normal g_strdup_vprintf() to format the arguments
   * with the two new format strings. By comparing the results,
   * we can figure out what segments of the output come from
   * the original format string, and what from the arguments,
   * and thus know what portions of the string to escape.
   *
   * For instance, for:
   *
   *  g_markup_printf_escaped ("%s ate %d apples", "Susan & Fred", 5);
   *
   * We form the two format strings "%sX%dX" and %sY%sY". The results
   * of formatting with those two strings are
   *
   * "%sX%dX" => "Susan & FredX5X"
   * "%sY%dY" => "Susan & FredY5Y"
   *
   * To find the span of the first argument, we find the first position
   * where the two arguments differ, which tells us that the first
   * argument formatted to "Susan & Fred". We then escape that
   * to "Susan & Fred" and join up with the intermediate portions
   * of the format string and the second argument to get
   * "Susan & Fred ate 5 apples".
   */

  /* Create the two modified format strings
   */
  format1 = g_string_new (NULL);
  format2 = g_string_new (NULL);
  p = format;
  while (TRUE)
    {
      const char *after;
      const char *conv = find_conversion (p, &after);
      if (!conv)
        break;

      g_string_append_len (format1, conv, after - conv);
      g_string_append_c (format1, 'X');
      g_string_append_len (format2, conv, after - conv);
      g_string_append_c (format2, 'Y');

      p = after;
    }

  /* Use them to format the arguments
   */
  G_VA_COPY (args2, args);

  output1 = g_strdup_vprintf (format1->str, args);

  if (!output1)
    {
      va_end (args2);
      goto cleanup;
    }

  output2 = g_strdup_vprintf (format2->str, args2);
  va_end (args2);
  if (!output2)
    goto cleanup;
  result = g_string_new (NULL);

  /* Iterate through the original format string again,
   * copying the non-conversion portions and the escaped
   * converted arguments to the output string.
   */
  op1 = output1;
  op2 = output2;
  p = format;
  while (TRUE)
    {
      const char *after;
      const char *output_start;
      const char *conv = find_conversion (p, &after);
      char *escaped;

      if (!conv)        /* The end, after points to the trailing \0 */
        {
          g_string_append_len (result, p, after - p);
          break;
        }

      g_string_append_len (result, p, conv - p);
      output_start = op1;
      while (*op1 == *op2)
        {
          op1++;
          op2++;
        }

      escaped = g_markup_escape_text (output_start, op1 - output_start);
      g_string_append (result, escaped);
      g_free (escaped);

      p = after;
      op1++;
      op2++;
    }

 cleanup:
  g_string_free (format1, TRUE);
  g_string_free (format2, TRUE);
  g_free (output1);
  g_free (output2);

  if (result)
    return g_string_free (result, FALSE);
  else
    return NULL;
}

#pragma GCC diagnostic pop

/**
 * g_markup_printf_escaped:
 * @format: printf() style format string
 * @...: the arguments to insert in the format string
 *
 * Formats arguments according to @format, escaping
 * all string and character arguments in the fashion
 * of g_markup_escape_text(). This is useful when you
 * want to insert literal strings into XML-style markup
 * output, without having to worry that the strings
 * might themselves contain markup.
 *
 * |[<!-- language="C" --> 
 * const char *store = "Fortnum & Mason";
 * const char *item = "Tea";
 * char *output;
 * 
 * output = g_markup_printf_escaped ("<purchase>"
 *                                   "<store>%s</store>"
 *                                   "<item>%s</item>"
 *                                   "</purchase>",
 *                                   store, item);
 * ]|
 *
 * Returns: newly allocated result from formatting
 *    operation. Free with g_free().
 *
 * Since: 2.4
 */
gchar *
g_markup_printf_escaped (const gchar *format, ...)
{
  char *result;
  va_list args;

  va_start (args, format);
  result = g_markup_vprintf_escaped (format, args);
  va_end (args);

  return result;
}

static gboolean
g_markup_parse_boolean (const char  *string,
                        gboolean    *value)
{
  char const * const falses[] = { "false", "f", "no", "n", "0" };
  char const * const trues[] = { "true", "t", "yes", "y", "1" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (falses); i++)
    {
      if (g_ascii_strcasecmp (string, falses[i]) == 0)
        {
          if (value != NULL)
            *value = FALSE;

          return TRUE;
        }
    }

  for (i = 0; i < G_N_ELEMENTS (trues); i++)
    {
      if (g_ascii_strcasecmp (string, trues[i]) == 0)
        {
          if (value != NULL)
            *value = TRUE;

          return TRUE;
        }
    }

  return FALSE;
}

/**
 * GMarkupCollectType:
 * @G_MARKUP_COLLECT_INVALID: used to terminate the list of attributes
 *     to collect
 * @G_MARKUP_COLLECT_STRING: collect the string pointer directly from
 *     the attribute_values[] array. Expects a parameter of type (const
 *     char **). If %G_MARKUP_COLLECT_OPTIONAL is specified and the
 *     attribute isn't present then the pointer will be set to %NULL
 * @G_MARKUP_COLLECT_STRDUP: as with %G_MARKUP_COLLECT_STRING, but
 *     expects a parameter of type (char **) and g_strdup()s the
 *     returned pointer. The pointer must be freed with g_free()
 * @G_MARKUP_COLLECT_BOOLEAN: expects a parameter of type (gboolean *)
 *     and parses the attribute value as a boolean. Sets %FALSE if the
 *     attribute isn't present. Valid boolean values consist of
 *     (case-insensitive) "false", "f", "no", "n", "0" and "true", "t",
 *     "yes", "y", "1"
 * @G_MARKUP_COLLECT_TRISTATE: as with %G_MARKUP_COLLECT_BOOLEAN, but
 *     in the case of a missing attribute a value is set that compares
 *     equal to neither %FALSE nor %TRUE G_MARKUP_COLLECT_OPTIONAL is
 *     implied
 * @G_MARKUP_COLLECT_OPTIONAL: can be bitwise ORed with the other fields.
 *     If present, allows the attribute not to appear. A default value
 *     is set depending on what value type is used
 *
 * A mixed enumerated type and flags field. You must specify one type
 * (string, strdup, boolean, tristate).  Additionally, you may  optionally
 * bitwise OR the type with the flag %G_MARKUP_COLLECT_OPTIONAL.
 *
 * It is likely that this enum will be extended in the future to
 * support other types.
 */

/**
 * g_markup_collect_attributes:
 * @element_name: the current tag name
 * @attribute_names: the attribute names
 * @attribute_values: the attribute values
 * @error: a pointer to a #GError or %NULL
 * @first_type: the #GMarkupCollectType of the first attribute
 * @first_attr: the name of the first attribute
 * @...: a pointer to the storage location of the first attribute
 *     (or %NULL), followed by more types names and pointers, ending
 *     with %G_MARKUP_COLLECT_INVALID
 *
 * Collects the attributes of the element from the data passed to the
 * #GMarkupParser start_element function, dealing with common error
 * conditions and supporting boolean values.
 *
 * This utility function is not required to write a parser but can save
 * a lot of typing.
 *
 * The @element_name, @attribute_names, @attribute_values and @error
 * parameters passed to the start_element callback should be passed
 * unmodified to this function.
 *
 * Following these arguments is a list of "supported" attributes to collect.
 * It is an error to specify multiple attributes with the same name. If any
 * attribute not in the list appears in the @attribute_names array then an
 * unknown attribute error will result.
 *
 * The #GMarkupCollectType field allows specifying the type of collection
 * to perform and if a given attribute must appear or is optional.
 *
 * The attribute name is simply the name of the attribute to collect.
 *
 * The pointer should be of the appropriate type (see the descriptions
 * under #GMarkupCollectType) and may be %NULL in case a particular
 * attribute is to be allowed but ignored.
 *
 * This function deals with issuing errors for missing attributes
 * (of type %G_MARKUP_ERROR_MISSING_ATTRIBUTE), unknown attributes
 * (of type %G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE) and duplicate
 * attributes (of type %G_MARKUP_ERROR_INVALID_CONTENT) as well
 * as parse errors for boolean-valued attributes (again of type
 * %G_MARKUP_ERROR_INVALID_CONTENT). In all of these cases %FALSE
 * will be returned and @error will be set as appropriate.
 *
 * Returns: %TRUE if successful
 *
 * Since: 2.16
 **/
gboolean
g_markup_collect_attributes (const gchar         *element_name,
                             const gchar        **attribute_names,
                             const gchar        **attribute_values,
                             GError             **error,
                             GMarkupCollectType   first_type,
                             const gchar         *first_attr,
                             ...)
{
  GMarkupCollectType type;
  const gchar *attr;
  guint64 collected;
  int written;
  va_list ap;
  int i;

  type = first_type;
  attr = first_attr;
  collected = 0;
  written = 0;

  va_start (ap, first_attr);
  while (type != G_MARKUP_COLLECT_INVALID)
    {
      gboolean mandatory;
      const gchar *value;

      mandatory = !(type & G_MARKUP_COLLECT_OPTIONAL);
      type &= (G_MARKUP_COLLECT_OPTIONAL - 1);

      /* tristate records a value != TRUE and != FALSE
       * for the case where the attribute is missing
       */
      if (type == G_MARKUP_COLLECT_TRISTATE)
        mandatory = FALSE;

      for (i = 0; attribute_names[i]; i++)
        if (i >= 40 || !(collected & (G_GUINT64_CONSTANT(1) << i)))
          if (!strcmp (attribute_names[i], attr))
            break;

      /* ISO C99 only promises that the user can pass up to 127 arguments.
       * Subtracting the first 4 arguments plus the final NULL and dividing
       * by 3 arguments per collected attribute, we are left with a maximum
       * number of supported attributes of (127 - 5) / 3 = 40.
       *
       * In reality, nobody is ever going to call us with anywhere close to
       * 40 attributes to collect, so it is safe to assume that if i > 40
       * then the user has given some invalid or repeated arguments.  These
       * problems will be caught and reported at the end of the function.
       *
       * We know at this point that we have an error, but we don't know
       * what error it is, so just continue...
       */
      if (i < 40)
        collected |= (G_GUINT64_CONSTANT(1) << i);

      value = attribute_values[i];

      if (value == NULL && mandatory)
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                       "element '%s' requires attribute '%s'",
                       element_name, attr);

          va_end (ap);
          goto failure;
        }

      switch (type)
        {
        case G_MARKUP_COLLECT_STRING:
          {
            const char **str_ptr;

            str_ptr = va_arg (ap, const char **);

            if (str_ptr != NULL)
              *str_ptr = value;
          }
          break;

        case G_MARKUP_COLLECT_STRDUP:
          {
            char **str_ptr;

            str_ptr = va_arg (ap, char **);

            if (str_ptr != NULL)
              *str_ptr = g_strdup (value);
          }
          break;

        case G_MARKUP_COLLECT_BOOLEAN:
        case G_MARKUP_COLLECT_TRISTATE:
          if (value == NULL)
            {
              gboolean *bool_ptr;

              bool_ptr = va_arg (ap, gboolean *);

              if (bool_ptr != NULL)
                {
                  if (type == G_MARKUP_COLLECT_TRISTATE)
                    /* constructivists rejoice!
                     * neither false nor true...
                     */
                    *bool_ptr = -1;

                  else /* G_MARKUP_COLLECT_BOOLEAN */
                    *bool_ptr = FALSE;
                }
            }
          else
            {
              if (!g_markup_parse_boolean (value, va_arg (ap, gboolean *)))
                {
                  g_set_error (error, G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "element '%s', attribute '%s', value '%s' "
                               "cannot be parsed as a boolean value",
                               element_name, attr, value);

                  va_end (ap);
                  goto failure;
                }
            }

          break;

        default:
          g_assert_not_reached ();
        }

      type = va_arg (ap, GMarkupCollectType);
      attr = va_arg (ap, const char *);
      written++;
    }
  va_end (ap);

  /* ensure we collected all the arguments */
  for (i = 0; attribute_names[i]; i++)
    if ((collected & (G_GUINT64_CONSTANT(1) << i)) == 0)
      {
        /* attribute not collected:  could be caused by two things.
         *
         * 1) it doesn't exist in our list of attributes
         * 2) it existed but was matched by a duplicate attribute earlier
         *
         * find out.
         */
        int j;

        for (j = 0; j < i; j++)
          if (strcmp (attribute_names[i], attribute_names[j]) == 0)
            /* duplicate! */
            break;

        /* j is now the first occurrence of attribute_names[i] */
        if (i == j)
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                       "attribute '%s' invalid for element '%s'",
                       attribute_names[i], element_name);
        else
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "attribute '%s' given multiple times for element '%s'",
                       attribute_names[i], element_name);

        goto failure;
      }

  return TRUE;

failure:
  /* replay the above to free allocations */
  type = first_type;
  attr = first_attr;

  va_start (ap, first_attr);
  while (type != G_MARKUP_COLLECT_INVALID)
    {
      gpointer ptr;

      ptr = va_arg (ap, gpointer);

      if (ptr != NULL)
        {
          switch (type & (G_MARKUP_COLLECT_OPTIONAL - 1))
            {
            case G_MARKUP_COLLECT_STRDUP:
              if (written)
                g_free (*(char **) ptr);

            case G_MARKUP_COLLECT_STRING:
              *(char **) ptr = NULL;
              break;

            case G_MARKUP_COLLECT_BOOLEAN:
              *(gboolean *) ptr = FALSE;
              break;

            case G_MARKUP_COLLECT_TRISTATE:
              *(gboolean *) ptr = -1;
              break;
            }
        }

      type = va_arg (ap, GMarkupCollectType);
      attr = va_arg (ap, const char *);
    }
  va_end (ap);

  return FALSE;
}
