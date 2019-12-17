/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997, 1999  Peter Mattis, Red Hat, Inc.
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

#include <string.h>

#include "gpattern.h"

#include "gmacros.h"
#include "gmessages.h"
#include "gmem.h"
#include "gunicode.h"
#include "gutils.h" 

/**
 * SECTION:patterns
 * @title: Glob-style pattern matching
 * @short_description: matches strings against patterns containing '*'
 *                     (wildcard) and '?' (joker)
 *
 * The g_pattern_match* functions match a string
 * against a pattern containing '*' and '?' wildcards with similar
 * semantics as the standard glob() function: '*' matches an arbitrary,
 * possibly empty, string, '?' matches an arbitrary character.
 *
 * Note that in contrast to glob(), the '/' character can be matched by
 * the wildcards, there are no '[...]' character ranges and '*' and '?'
 * can not be escaped to include them literally in a pattern.
 *
 * When multiple strings must be matched against the same pattern, it
 * is better to compile the pattern to a #GPatternSpec using
 * g_pattern_spec_new() and use g_pattern_match_string() instead of
 * g_pattern_match_simple(). This avoids the overhead of repeated
 * pattern compilation.
 **/

/**
 * GPatternSpec:
 *
 * A GPatternSpec struct is the 'compiled' form of a pattern. This
 * structure is opaque and its fields cannot be accessed directly.
 */

/* keep enum and structure of gpattern.c and patterntest.c in sync */
typedef enum
{
  G_MATCH_ALL,       /* "*A?A*" */
  G_MATCH_ALL_TAIL,  /* "*A?AA" */
  G_MATCH_HEAD,      /* "AAAA*" */
  G_MATCH_TAIL,      /* "*AAAA" */
  G_MATCH_EXACT,     /* "AAAAA" */
  G_MATCH_LAST
} GMatchType;

struct _GPatternSpec
{
  GMatchType match_type;
  guint      pattern_length;
  guint      min_length;
  guint      max_length;
  gchar     *pattern;
};


/* --- functions --- */
static inline gboolean
g_pattern_ph_match (const gchar *match_pattern,
		    const gchar *match_string,
		    gboolean    *wildcard_reached_p)
{
  const gchar *pattern, *string;
  gchar ch;

  pattern = match_pattern;
  string = match_string;

  ch = *pattern;
  pattern++;
  while (ch)
    {
      switch (ch)
	{
	case '?':
	  if (!*string)
	    return FALSE;
	  string = g_utf8_next_char (string);
	  break;

	case '*':
	  *wildcard_reached_p = TRUE;
	  do
	    {
	      ch = *pattern;
	      pattern++;
	      if (ch == '?')
		{
		  if (!*string)
		    return FALSE;
		  string = g_utf8_next_char (string);
		}
	    }
	  while (ch == '*' || ch == '?');
	  if (!ch)
	    return TRUE;
	  do
	    {
              gboolean next_wildcard_reached = FALSE;
	      while (ch != *string)
		{
		  if (!*string)
		    return FALSE;
		  string = g_utf8_next_char (string);
		}
	      string++;
	      if (g_pattern_ph_match (pattern, string, &next_wildcard_reached))
		return TRUE;
              if (next_wildcard_reached)
                /* the forthcoming pattern substring up to the next wildcard has
                 * been matched, but a mismatch occoured for the rest of the
                 * pattern, following the next wildcard.
                 * there's no need to advance the current match position any
                 * further if the rest pattern will not match.
                 */
		return FALSE;
	    }
	  while (*string);
	  break;

	default:
	  if (ch == *string)
	    string++;
	  else
	    return FALSE;
	  break;
	}

      ch = *pattern;
      pattern++;
    }

  return *string == 0;
}

/**
 * g_pattern_match:
 * @pspec: a #GPatternSpec
 * @string_length: the length of @string (in bytes, i.e. strlen(),
 *     not g_utf8_strlen())
 * @string: the UTF-8 encoded string to match
 * @string_reversed: (nullable): the reverse of @string or %NULL
 *
 * Matches a string against a compiled pattern. Passing the correct
 * length of the string given is mandatory. The reversed string can be
 * omitted by passing %NULL, this is more efficient if the reversed
 * version of the string to be matched is not at hand, as
 * g_pattern_match() will only construct it if the compiled pattern
 * requires reverse matches.
 *
 * Note that, if the user code will (possibly) match a string against a
 * multitude of patterns containing wildcards, chances are high that
 * some patterns will require a reversed string. In this case, it's
 * more efficient to provide the reversed string to avoid multiple
 * constructions thereof in the various calls to g_pattern_match().
 *
 * Note also that the reverse of a UTF-8 encoded string can in general
 * not be obtained by g_strreverse(). This works only if the string
 * does not contain any multibyte characters. GLib offers the
 * g_utf8_strreverse() function to reverse UTF-8 encoded strings.
 *
 * Returns: %TRUE if @string matches @pspec
 **/
gboolean
g_pattern_match (GPatternSpec *pspec,
		 guint         string_length,
		 const gchar  *string,
		 const gchar  *string_reversed)
{
  g_return_val_if_fail (pspec != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  if (string_length < pspec->min_length ||
      string_length > pspec->max_length)
    return FALSE;

  switch (pspec->match_type)
    {
      gboolean dummy;
    case G_MATCH_ALL:
      return g_pattern_ph_match (pspec->pattern, string, &dummy);
    case G_MATCH_ALL_TAIL:
      if (string_reversed)
	return g_pattern_ph_match (pspec->pattern, string_reversed, &dummy);
      else
	{
          gboolean result;
          gchar *tmp;
	  tmp = g_utf8_strreverse (string, string_length);
	  result = g_pattern_ph_match (pspec->pattern, tmp, &dummy);
	  g_free (tmp);
	  return result;
	}
    case G_MATCH_HEAD:
      if (pspec->pattern_length == string_length)
	return strcmp (pspec->pattern, string) == 0;
      else if (pspec->pattern_length)
	return strncmp (pspec->pattern, string, pspec->pattern_length) == 0;
      else
	return TRUE;
    case G_MATCH_TAIL:
      if (pspec->pattern_length)
        return strcmp (pspec->pattern, string + (string_length - pspec->pattern_length)) == 0;
      else
	return TRUE;
    case G_MATCH_EXACT:
      if (pspec->pattern_length != string_length)
        return FALSE;
      else
        return strcmp (pspec->pattern, string) == 0;
    default:
      g_return_val_if_fail (pspec->match_type < G_MATCH_LAST, FALSE);
      return FALSE;
    }
}

/**
 * g_pattern_spec_new:
 * @pattern: a zero-terminated UTF-8 encoded string
 *
 * Compiles a pattern to a #GPatternSpec.
 *
 * Returns: a newly-allocated #GPatternSpec
 **/
GPatternSpec*
g_pattern_spec_new (const gchar *pattern)
{
  GPatternSpec *pspec;
  gboolean seen_joker = FALSE, seen_wildcard = FALSE, more_wildcards = FALSE;
  gint hw_pos = -1, tw_pos = -1, hj_pos = -1, tj_pos = -1;
  gboolean follows_wildcard = FALSE;
  guint pending_jokers = 0;
  const gchar *s;
  gchar *d;
  guint i;
  
  g_return_val_if_fail (pattern != NULL, NULL);

  /* canonicalize pattern and collect necessary stats */
  pspec = g_new (GPatternSpec, 1);
  pspec->pattern_length = strlen (pattern);
  pspec->min_length = 0;
  pspec->max_length = 0;
  pspec->pattern = g_new (gchar, pspec->pattern_length + 1);
  d = pspec->pattern;
  for (i = 0, s = pattern; *s != 0; s++)
    {
      switch (*s)
	{
	case '*':
	  if (follows_wildcard)	/* compress multiple wildcards */
	    {
	      pspec->pattern_length--;
	      continue;
	    }
	  follows_wildcard = TRUE;
	  if (hw_pos < 0)
	    hw_pos = i;
	  tw_pos = i;
	  break;
	case '?':
	  pending_jokers++;
	  pspec->min_length++;
	  pspec->max_length += 4; /* maximum UTF-8 character length */
	  continue;
	default:
	  for (; pending_jokers; pending_jokers--, i++) {
	    *d++ = '?';
  	    if (hj_pos < 0)
	     hj_pos = i;
	    tj_pos = i;
	  }
	  follows_wildcard = FALSE;
	  pspec->min_length++;
	  pspec->max_length++;
	  break;
	}
      *d++ = *s;
      i++;
    }
  for (; pending_jokers; pending_jokers--) {
    *d++ = '?';
    if (hj_pos < 0)
      hj_pos = i;
    tj_pos = i;
  }
  *d++ = 0;
  seen_joker = hj_pos >= 0;
  seen_wildcard = hw_pos >= 0;
  more_wildcards = seen_wildcard && hw_pos != tw_pos;
  if (seen_wildcard)
    pspec->max_length = G_MAXUINT;

  /* special case sole head/tail wildcard or exact matches */
  if (!seen_joker && !more_wildcards)
    {
      if (pspec->pattern[0] == '*')
	{
	  pspec->match_type = G_MATCH_TAIL;
          memmove (pspec->pattern, pspec->pattern + 1, --pspec->pattern_length);
	  pspec->pattern[pspec->pattern_length] = 0;
	  return pspec;
	}
      if (pspec->pattern_length > 0 &&
	  pspec->pattern[pspec->pattern_length - 1] == '*')
	{
	  pspec->match_type = G_MATCH_HEAD;
	  pspec->pattern[--pspec->pattern_length] = 0;
	  return pspec;
	}
      if (!seen_wildcard)
	{
	  pspec->match_type = G_MATCH_EXACT;
	  return pspec;
	}
    }

  /* now just need to distinguish between head or tail match start */
  tw_pos = pspec->pattern_length - 1 - tw_pos;	/* last pos to tail distance */
  tj_pos = pspec->pattern_length - 1 - tj_pos;	/* last pos to tail distance */
  if (seen_wildcard)
    pspec->match_type = tw_pos > hw_pos ? G_MATCH_ALL_TAIL : G_MATCH_ALL;
  else /* seen_joker */
    pspec->match_type = tj_pos > hj_pos ? G_MATCH_ALL_TAIL : G_MATCH_ALL;
  if (pspec->match_type == G_MATCH_ALL_TAIL) {
    gchar *tmp = pspec->pattern;
    pspec->pattern = g_utf8_strreverse (pspec->pattern, pspec->pattern_length);
    g_free (tmp);
  }
  return pspec;
}

/**
 * g_pattern_spec_free:
 * @pspec: a #GPatternSpec
 *
 * Frees the memory allocated for the #GPatternSpec.
 **/
void
g_pattern_spec_free (GPatternSpec *pspec)
{
  g_return_if_fail (pspec != NULL);

  g_free (pspec->pattern);
  g_free (pspec);
}

/**
 * g_pattern_spec_equal:
 * @pspec1: a #GPatternSpec
 * @pspec2: another #GPatternSpec
 *
 * Compares two compiled pattern specs and returns whether they will
 * match the same set of strings.
 *
 * Returns: Whether the compiled patterns are equal
 **/
gboolean
g_pattern_spec_equal (GPatternSpec *pspec1,
		      GPatternSpec *pspec2)
{
  g_return_val_if_fail (pspec1 != NULL, FALSE);
  g_return_val_if_fail (pspec2 != NULL, FALSE);

  return (pspec1->pattern_length == pspec2->pattern_length &&
	  pspec1->match_type == pspec2->match_type &&
	  strcmp (pspec1->pattern, pspec2->pattern) == 0);
}

/**
 * g_pattern_match_string:
 * @pspec: a #GPatternSpec
 * @string: the UTF-8 encoded string to match
 *
 * Matches a string against a compiled pattern. If the string is to be
 * matched against more than one pattern, consider using
 * g_pattern_match() instead while supplying the reversed string.
 *
 * Returns: %TRUE if @string matches @pspec
 **/
gboolean
g_pattern_match_string (GPatternSpec *pspec,
			const gchar  *string)
{
  g_return_val_if_fail (pspec != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  return g_pattern_match (pspec, strlen (string), string, NULL);
}

/**
 * g_pattern_match_simple:
 * @pattern: the UTF-8 encoded pattern
 * @string: the UTF-8 encoded string to match
 *
 * Matches a string against a pattern given as a string. If this
 * function is to be called in a loop, it's more efficient to compile
 * the pattern once with g_pattern_spec_new() and call
 * g_pattern_match_string() repeatedly.
 *
 * Returns: %TRUE if @string matches @pspec
 **/
gboolean
g_pattern_match_simple (const gchar *pattern,
			const gchar *string)
{
  GPatternSpec *pspec;
  gboolean ergo;

  g_return_val_if_fail (pattern != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  pspec = g_pattern_spec_new (pattern);
  ergo = g_pattern_match (pspec, strlen (string), string, NULL);
  g_pattern_spec_free (pspec);

  return ergo;
}
