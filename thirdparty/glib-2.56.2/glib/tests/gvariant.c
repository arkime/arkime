/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include <glib/gvariant-internal.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#define BASIC "bynqiuxthdsog?"
#define N_BASIC (G_N_ELEMENTS (BASIC) - 1)

#define INVALIDS "cefjklpwz&@^$"
#define N_INVALIDS (G_N_ELEMENTS (INVALIDS) - 1)

/* see comment in gvariant-serialiser.c about this madness.
 *
 * we use this to get testing of non-strictly-aligned GVariant instances
 * on machines that can tolerate it.  it is necessary to support this
 * because some systems have malloc() that returns non-8-aligned
 * pointers.  it is necessary to have special support in the tests
 * because on most machines malloc() is 8-aligned.
 */
#define ALIGN_BITS (sizeof (struct { char a; union {                       \
                      guint64 x; void *y; gdouble z; } b; }) - 9)

static gboolean
randomly (gdouble prob)
{
  return g_test_rand_double_range (0, 1) < prob;
}

/* corecursion */
static GVariantType *
append_tuple_type_string (GString *, GString *, gboolean, gint);

/* append a random GVariantType to a GString
 * append a description of the type to another GString
 * return what the type is
 */
static GVariantType *
append_type_string (GString  *string,
                    GString  *description,
                    gboolean  definite,
                    gint      depth)
{
  if (!depth-- || randomly (0.3))
    {
      gchar b = BASIC[g_test_rand_int_range (0, N_BASIC - definite)];
      g_string_append_c (string, b);
      g_string_append_c (description, b);

      switch (b)
        {
        case 'b':
          return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
        case 'y':
          return g_variant_type_copy (G_VARIANT_TYPE_BYTE);
        case 'n':
          return g_variant_type_copy (G_VARIANT_TYPE_INT16);
        case 'q':
          return g_variant_type_copy (G_VARIANT_TYPE_UINT16);
        case 'i':
          return g_variant_type_copy (G_VARIANT_TYPE_INT32);
        case 'u':
          return g_variant_type_copy (G_VARIANT_TYPE_UINT32);
        case 'x':
          return g_variant_type_copy (G_VARIANT_TYPE_INT64);
        case 't':
          return g_variant_type_copy (G_VARIANT_TYPE_UINT64);
        case 'h':
          return g_variant_type_copy (G_VARIANT_TYPE_HANDLE);
        case 'd':
          return g_variant_type_copy (G_VARIANT_TYPE_DOUBLE);
        case 's':
          return g_variant_type_copy (G_VARIANT_TYPE_STRING);
        case 'o':
          return g_variant_type_copy (G_VARIANT_TYPE_OBJECT_PATH);
        case 'g':
          return g_variant_type_copy (G_VARIANT_TYPE_SIGNATURE);
        case '?':
          return g_variant_type_copy (G_VARIANT_TYPE_BASIC);
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      GVariantType *result;

      switch (g_test_rand_int_range (0, definite ? 5 : 7))
        {
        case 0:
          {
            GVariantType *element;

            g_string_append_c (string, 'a');
            g_string_append (description, "a of ");
            element = append_type_string (string, description,
                                          definite, depth);
            result = g_variant_type_new_array (element);
            g_variant_type_free (element);
          }

          g_assert (g_variant_type_is_array (result));
          break;

        case 1:
          {
            GVariantType *element;

            g_string_append_c (string, 'm');
            g_string_append (description, "m of ");
            element = append_type_string (string, description,
                                          definite, depth);
            result = g_variant_type_new_maybe (element);
            g_variant_type_free (element);
          }

          g_assert (g_variant_type_is_maybe (result));
          break;

        case 2:
          result = append_tuple_type_string (string, description,
                                             definite, depth);

          g_assert (g_variant_type_is_tuple (result));
          break;

        case 3:
          {
            GVariantType *key, *value;

            g_string_append_c (string, '{');
            g_string_append (description, "e of [");
            key = append_type_string (string, description, definite, 0);
            g_string_append (description, ", ");
            value = append_type_string (string, description, definite, depth);
            g_string_append_c (description, ']');
            g_string_append_c (string, '}');
            result = g_variant_type_new_dict_entry (key, value);
            g_variant_type_free (key);
            g_variant_type_free (value);
          }

          g_assert (g_variant_type_is_dict_entry (result));
          break;

        case 4:
          g_string_append_c (string, 'v');
          g_string_append_c (description, 'V');
          result = g_variant_type_copy (G_VARIANT_TYPE_VARIANT);
          g_assert (g_variant_type_equal (result, G_VARIANT_TYPE_VARIANT));
          break;

        case 5:
          g_string_append_c (string, '*');
          g_string_append_c (description, 'S');
          result = g_variant_type_copy (G_VARIANT_TYPE_ANY);
          g_assert (g_variant_type_equal (result, G_VARIANT_TYPE_ANY));
          break;

        case 6:
          g_string_append_c (string, 'r');
          g_string_append_c (description, 'R');
          result = g_variant_type_copy (G_VARIANT_TYPE_TUPLE);
          g_assert (g_variant_type_is_tuple (result));
          break;

        default:
          g_assert_not_reached ();
        }

      return result;
    }
}

static GVariantType *
append_tuple_type_string (GString  *string,
                          GString  *description,
                          gboolean  definite,
                          gint      depth)
{
  GVariantType *result, *other_result;
  GVariantType **types;
  gint size;
  gint i;

  g_string_append_c (string, '(');
  g_string_append (description, "t of [");

  size = g_test_rand_int_range (0, 20);
  types = g_new (GVariantType *, size + 1);

  for (i = 0; i < size; i++)
    {
      types[i] = append_type_string (string, description, definite, depth);

      if (i < size - 1)
        g_string_append (description, ", ");
    }

  types[i] = NULL;

  g_string_append_c (description, ']');
  g_string_append_c (string, ')');

  result = g_variant_type_new_tuple ((gpointer) types, size);
  other_result = g_variant_type_new_tuple ((gpointer) types, -1);
  g_assert (g_variant_type_equal (result, other_result));
  g_variant_type_free (other_result);
  for (i = 0; i < size; i++)
    g_variant_type_free (types[i]);
  g_free (types);

  return result;
}

/* given a valid type string, make it invalid */
static gchar *
invalid_mutation (const gchar *type_string)
{
  gboolean have_parens, have_braces;

  /* it's valid, so '(' implies ')' and same for '{' and '}' */
  have_parens = strchr (type_string, '(') != NULL;
  have_braces = strchr (type_string, '{') != NULL;

  if (have_parens && have_braces && randomly (0.3))
    {
      /* swap a paren and a brace */
      gchar *pp, *bp;
      gint np, nb;
      gchar p, b;
      gchar *new;

      new = g_strdup (type_string);

      if (randomly (0.5))
        p = '(', b = '{';
      else
        p = ')', b = '}';

      np = nb = 0;
      pp = bp = new - 1;

      /* count number of parens/braces */
      while ((pp = strchr (pp + 1, p))) np++;
      while ((bp = strchr (bp + 1, b))) nb++;

      /* randomly pick one of each */
      np = g_test_rand_int_range (0, np) + 1;
      nb = g_test_rand_int_range (0, nb) + 1;

      /* find it */
      pp = bp = new - 1;
      while (np--) pp = strchr (pp + 1, p);
      while (nb--) bp = strchr (bp + 1, b);

      /* swap */
      g_assert (*bp == b && *pp == p);
      *bp = p;
      *pp = b;

      return new;
    }

  if ((have_parens || have_braces) && randomly (0.3))
    {
      /* drop a paren/brace */
      gchar *new;
      gchar *pp;
      gint np;
      gchar p;

      if (have_parens)
        if (randomly (0.5)) p = '('; else p = ')';
      else
        if (randomly (0.5)) p = '{'; else p = '}';

      new = g_strdup (type_string);

      np = 0;
      pp = new - 1;
      while ((pp = strchr (pp + 1, p))) np++;
      np = g_test_rand_int_range (0, np) + 1;
      pp = new - 1;
      while (np--) pp = strchr (pp + 1, p);
      g_assert (*pp == p);

      while (*pp)
        {
          *pp = *(pp + 1);
          pp++;
        }

      return new;
    }

  /* else, perform a random mutation at a random point */
  {
    gint length, n;
    gchar *new;
    gchar p;

    if (randomly (0.3))
      {
        /* insert a paren/brace */
        if (randomly (0.5))
          if (randomly (0.5)) p = '('; else p = ')';
        else
          if (randomly (0.5)) p = '{'; else p = '}';
      }
    else if (randomly (0.5))
      {
        /* insert junk */
        p = INVALIDS[g_test_rand_int_range (0, N_INVALIDS)];
      }
    else
      {
        /* truncate */
        p = '\0';
      }


    length = strlen (type_string);
    new = g_malloc (length + 2);
    n = g_test_rand_int_range (0, length);
    memcpy (new, type_string, n);
    new[n] = p;
    memcpy (new + n + 1, type_string + n, length - n);
    new[length + 1] = '\0';

    return new;
  }
}

/* describe a type using the same language as is generated
 * while generating the type with append_type_string
 */
static gchar *
describe_type (const GVariantType *type)
{
  gchar *result;

  if (g_variant_type_is_container (type))
    {
      g_assert (!g_variant_type_is_basic (type));

      if (g_variant_type_is_array (type))
        {
          gchar *subtype = describe_type (g_variant_type_element (type));
          result = g_strdup_printf ("a of %s", subtype);
          g_free (subtype);
        }
      else if (g_variant_type_is_maybe (type))
        {
          gchar *subtype = describe_type (g_variant_type_element (type));
          result = g_strdup_printf ("m of %s", subtype);
          g_free (subtype);
        }
      else if (g_variant_type_is_tuple (type))
        {
          if (!g_variant_type_equal (type, G_VARIANT_TYPE_TUPLE))
            {
              const GVariantType *sub;
              GString *string;
              gint length;
              gint i;

              string = g_string_new ("t of [");

              length = g_variant_type_n_items (type);
              sub = g_variant_type_first (type);
              for (i = 0; i < length; i++)
                {
                  gchar *subtype = describe_type (sub);
                  g_string_append (string, subtype);
                  g_free (subtype);

                  if ((sub = g_variant_type_next (sub)))
                    g_string_append (string, ", ");
                }
              g_assert (sub == NULL);
              g_string_append_c (string, ']');

              result = g_string_free (string, FALSE);
            }
          else
            result = g_strdup ("R");
        }
      else if (g_variant_type_is_dict_entry (type))
        {
          gchar *key, *value, *key2, *value2;

          key = describe_type (g_variant_type_key (type));
          value = describe_type (g_variant_type_value (type));
          key2 = describe_type (g_variant_type_first (type));
          value2 = describe_type (
            g_variant_type_next (g_variant_type_first (type)));
          g_assert (g_variant_type_next (g_variant_type_next (
            g_variant_type_first (type))) == NULL);
          g_assert_cmpstr (key, ==, key2);
          g_assert_cmpstr (value, ==, value2);
          result = g_strjoin ("", "e of [", key, ", ", value, "]", NULL);
          g_free (key2);
          g_free (value2);
          g_free (key);
          g_free (value);
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_VARIANT))
        {
          result = g_strdup ("V");
        }
      else
        g_assert_not_reached ();
    }
  else
    {
      if (g_variant_type_is_definite (type))
        {
          g_assert (g_variant_type_is_basic (type));

          if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
            result = g_strdup ("b");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
            result = g_strdup ("y");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
            result = g_strdup ("n");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
            result = g_strdup ("q");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
            result = g_strdup ("i");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
            result = g_strdup ("u");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
            result = g_strdup ("x");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
            result = g_strdup ("t");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_HANDLE))
            result = g_strdup ("h");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
            result = g_strdup ("d");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
            result = g_strdup ("s");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
            result = g_strdup ("o");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
            result = g_strdup ("g");
          else
            g_assert_not_reached ();
        }
      else
        {
          if (g_variant_type_equal (type, G_VARIANT_TYPE_ANY))
            {
              result = g_strdup ("S");
            }
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_BASIC))
            {
              result = g_strdup ("?");
            }
          else
            g_assert_not_reached ();
        }
    }

  return result;
}

/* given a type string, replace one of the indefinite type characters in
 * it with a matching type (possibly the same type).
 */
static gchar *
generate_subtype (const gchar *type_string)
{
  GVariantType *replacement;
  GString *result, *junk;
  gint length, n = 0, l;

  result = g_string_new (NULL);
  junk = g_string_new (NULL);

  /* count the number of indefinite type characters */
  for (length = 0; type_string[length]; length++)
    n += type_string[length] == 'r' ||
         type_string[length] == '?' ||
         type_string[length] == '*';
  /* length now is strlen (type_string) */

  /* pick one at random to replace */
  n = g_test_rand_int_range (0, n) + 1;

  /* find it */
  l = -1;
  while (n--) l += strcspn (type_string + l + 1, "r?*") + 1;
  g_assert (type_string[l] == 'r' ||
            type_string[l] == '?' ||
            type_string[l] == '*');

  /* store up to that point in a GString */
  g_string_append_len (result, type_string, l);

  /* then store the replacement in the GString */
  if (type_string[l] == 'r')
    replacement = append_tuple_type_string (result, junk, FALSE, 3);

  else if (type_string[l] == '?')
    replacement = append_type_string (result, junk, FALSE, 0);

  else if (type_string[l] == '*')
    replacement = append_type_string (result, junk, FALSE, 3);

  else
    g_assert_not_reached ();

  /* ensure the replacement has the proper type */
  g_assert (g_variant_type_is_subtype_of (replacement,
                                          (gpointer) &type_string[l]));

  /* store the rest from the original type string */
  g_string_append (result, type_string + l + 1);

  g_variant_type_free (replacement);
  g_string_free (junk, TRUE);

  return g_string_free (result, FALSE);
}

struct typestack
{
  const GVariantType *type;
  struct typestack *parent;
};

/* given an indefinite type string, replace one of the indefinite
 * characters in it with a matching type and ensure that the result is a
 * subtype of the original.  repeat.
 */
static void
subtype_check (const gchar      *type_string,
               struct typestack *parent_ts)
{
  struct typestack ts, *node;
  gchar *subtype;
  gint depth = 0;

  subtype = generate_subtype (type_string);

  ts.type = G_VARIANT_TYPE (subtype);
  ts.parent = parent_ts;

  for (node = &ts; node; node = node->parent)
    {
      /* this type should be a subtype of each parent type */
      g_assert (g_variant_type_is_subtype_of (ts.type, node->type));

      /* it should only be a supertype when it is exactly equal */
      g_assert (g_variant_type_is_subtype_of (node->type, ts.type) ==
                g_variant_type_equal (ts.type, node->type));

      depth++;
    }

  if (!g_variant_type_is_definite (ts.type) && depth < 5)
    {
      /* the type is still indefinite and we haven't repeated too many
       * times.  go once more.
       */

      subtype_check (subtype, &ts);
    }

  g_free (subtype);
}

static void
test_gvarianttype (void)
{
  gint i;

  for (i = 0; i < 2000; i++)
    {
      GString *type_string, *description;
      GVariantType *type, *other_type;
      const GVariantType *ctype;
      gchar *invalid;
      gchar *desc;

      type_string = g_string_new (NULL);
      description = g_string_new (NULL);

      /* generate a random type, its type string and a description
       *
       * exercises type constructor functions and g_variant_type_copy()
       */
      type = append_type_string (type_string, description, FALSE, 6);

      /* convert the type string to a type and ensure that it is equal
       * to the one produced with the type constructor routines
       */
      ctype = G_VARIANT_TYPE (type_string->str);
      g_assert (g_variant_type_equal (ctype, type));
      g_assert (g_variant_type_hash (ctype) == g_variant_type_hash (type));
      g_assert (g_variant_type_is_subtype_of (ctype, type));
      g_assert (g_variant_type_is_subtype_of (type, ctype));

      /* check if the type is indefinite */
      if (!g_variant_type_is_definite (type))
        {
          struct typestack ts = { type, NULL };

          /* if it is indefinite, then replace one of the indefinite
           * characters with a matching type and ensure that the result
           * is a subtype of the original type.  repeat.
           */
          subtype_check (type_string->str, &ts);
        }
      else
        /* ensure that no indefinite characters appear */
        g_assert (strcspn (type_string->str, "r?*") == type_string->len);


      /* describe the type.
       *
       * exercises the type iterator interface
       */
      desc = describe_type (type);

      /* make sure the description matches */
      g_assert_cmpstr (desc, ==, description->str);
      g_free (desc);

      /* make an invalid mutation to the type and make sure the type
       * validation routines catch it */
      invalid = invalid_mutation (type_string->str);
      g_assert (g_variant_type_string_is_valid (type_string->str));
      g_assert (!g_variant_type_string_is_valid (invalid));
      g_free (invalid);

      /* concatenate another type to the type string and ensure that
       * the result is recognised as being invalid
       */
      other_type = append_type_string (type_string, description, FALSE, 2);

      g_string_free (description, TRUE);
      g_string_free (type_string, TRUE);
      g_variant_type_free (other_type);
      g_variant_type_free (type);
    }
}

#define ALIGNED(x, y)   (((x + (y - 1)) / y) * y)

/* do our own calculation of the fixed_size and alignment of a type
 * using a simple algorithm to make sure the "fancy" one in the
 * implementation is correct.
 */
static void
calculate_type_info (const GVariantType *type,
                     gsize              *fixed_size,
                     guint              *alignment)
{
  if (g_variant_type_is_array (type) ||
      g_variant_type_is_maybe (type))
    {
      calculate_type_info (g_variant_type_element (type), NULL, alignment);

      if (fixed_size)
        *fixed_size = 0;
    }
  else if (g_variant_type_is_tuple (type) ||
           g_variant_type_is_dict_entry (type))
    {
      if (g_variant_type_n_items (type))
        {
          const GVariantType *sub;
          gboolean variable;
          gsize size;
          guint al;

          variable = FALSE;
          size = 0;
          al = 0;

          sub = g_variant_type_first (type);
          do
            {
              gsize this_fs;
              guint this_al;

              calculate_type_info (sub, &this_fs, &this_al);

              al = MAX (al, this_al);

              if (!this_fs)
                {
                  variable = TRUE;
                  size = 0;
                }

              if (!variable)
                {
                  size = ALIGNED (size, this_al);
                  size += this_fs;
                }
            }
          while ((sub = g_variant_type_next (sub)));

          size = ALIGNED (size, al);

          if (alignment)
            *alignment = al;

          if (fixed_size)
            *fixed_size = size;
        }
      else
        {
          if (fixed_size)
            *fixed_size = 1;

          if (alignment)
            *alignment = 1;
        }
    }
  else
    {
      gint fs, al;

      if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN) ||
          g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
        {
          al = fs = 1;
        }

      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
        {
          al = fs = 2;
        }

      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_UINT32) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_HANDLE))
        {
          al = fs = 4;
        }

      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_UINT64) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
        {
          al = fs = 8;
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
        {
          al = 1;
          fs = 0;
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_VARIANT))
        {
          al = 8;
          fs = 0;
        }
      else
        g_assert_not_reached ();

      if (fixed_size)
        *fixed_size = fs;

      if (alignment)
        *alignment = al;
    }
}

/* same as the describe_type() function above, but iterates over
 * typeinfo instead of types.
 */
static gchar *
describe_info (GVariantTypeInfo *info)
{
  gchar *result;

  switch (g_variant_type_info_get_type_char (info))
    {
    case G_VARIANT_TYPE_INFO_CHAR_MAYBE:
      {
        gchar *element;

        element = describe_info (g_variant_type_info_element (info));
        result = g_strdup_printf ("m of %s", element);
        g_free (element);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_ARRAY:
      {
        gchar *element;

        element = describe_info (g_variant_type_info_element (info));
        result = g_strdup_printf ("a of %s", element);
        g_free (element);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_TUPLE:
      {
        const gchar *sep = "";
        GString *string;
        gint length;
        gint i;

        string = g_string_new ("t of [");
        length = g_variant_type_info_n_members (info);

        for (i = 0; i < length; i++)
          {
            const GVariantMemberInfo *minfo;
            gchar *subtype;

            g_string_append (string, sep);
            sep = ", ";

            minfo = g_variant_type_info_member_info (info, i);
            subtype = describe_info (minfo->type_info);
            g_string_append (string, subtype);
            g_free (subtype);
          }

        g_string_append_c (string, ']');

        result = g_string_free (string, FALSE);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY:
      {
        const GVariantMemberInfo *keyinfo, *valueinfo;
        gchar *key, *value;

        g_assert_cmpint (g_variant_type_info_n_members (info), ==, 2);
        keyinfo = g_variant_type_info_member_info (info, 0);
        valueinfo = g_variant_type_info_member_info (info, 1);
        key = describe_info (keyinfo->type_info);
        value = describe_info (valueinfo->type_info);
        result = g_strjoin ("", "e of [", key, ", ", value, "]", NULL);
        g_free (key);
        g_free (value);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_VARIANT:
      result = g_strdup ("V");
      break;

    default:
      result = g_strdup (g_variant_type_info_get_type_string (info));
      g_assert_cmpint (strlen (result), ==, 1);
      break;
    }

  return result;
}

/* check that the O(1) method of calculating offsets meshes with the
 * results of simple iteration.
 */
static void
check_offsets (GVariantTypeInfo   *info,
               const GVariantType *type)
{
  gint flavour;
  gint length;

  length = g_variant_type_info_n_members (info);
  g_assert_cmpint (length, ==, g_variant_type_n_items (type));

  /* the 'flavour' is the low order bits of the ending point of
   * variable-size items in the tuple.  this lets us test that the type
   * info is correct for various starting alignments.
   */
  for (flavour = 0; flavour < 8; flavour++)
    {
      const GVariantType *subtype;
      gsize last_offset_index;
      gsize last_offset;
      gsize position;
      gint i;

      subtype = g_variant_type_first (type);
      last_offset_index = -1;
      last_offset = 0;
      position = 0;

      /* go through the tuple, keeping track of our position */
      for (i = 0; i < length; i++)
        {
          gsize fixed_size;
          guint alignment;

          calculate_type_info (subtype, &fixed_size, &alignment);

          position = ALIGNED (position, alignment);

          /* compare our current aligned position (ie: the start of this
           * item) to the start offset that would be calculated if we
           * used the type info
           */
          {
            const GVariantMemberInfo *member;
            gsize start;

            member = g_variant_type_info_member_info (info, i);
            g_assert_cmpint (member->i, ==, last_offset_index);

            /* do the calculation using the typeinfo */
            start = last_offset;
            start += member->a;
            start &= member->b;
            start |= member->c;

            /* did we reach the same spot? */
            g_assert_cmpint (start, ==, position);
          }

          if (fixed_size)
            {
              /* fixed size.  add that size. */
              position += fixed_size;
            }
          else
            {
              /* variable size.  do the flavouring. */
              while ((position & 0x7) != flavour)
                position++;

              /* and store the offset, just like it would be in the
               * serialised data.
               */
              last_offset = position;
              last_offset_index++;
            }

          /* next type */
          subtype = g_variant_type_next (subtype);
        }

      /* make sure we used up exactly all the types */
      g_assert (subtype == NULL);
    }
}

static void
test_gvarianttypeinfo (void)
{
  gint i;

  for (i = 0; i < 2000; i++)
    {
      GString *type_string, *description;
      gsize fixed_size1, fixed_size2;
      guint alignment1, alignment2;
      GVariantTypeInfo *info;
      GVariantType *type;
      gchar *desc;

      type_string = g_string_new (NULL);
      description = g_string_new (NULL);

      /* random type */
      type = append_type_string (type_string, description, TRUE, 6);

      /* create a typeinfo for it */
      info = g_variant_type_info_get (type);

      /* make sure the typeinfo has the right type string */
      g_assert_cmpstr (g_variant_type_info_get_type_string (info), ==,
                       type_string->str);

      /* calculate the alignment and fixed size, compare to the
       * typeinfo's calculations
       */
      calculate_type_info (type, &fixed_size1, &alignment1);
      g_variant_type_info_query (info, &alignment2, &fixed_size2);
      g_assert_cmpint (fixed_size1, ==, fixed_size2);
      g_assert_cmpint (alignment1, ==, alignment2 + 1);

      /* test the iteration functions over typeinfo structures by
       * "describing" the typeinfo and verifying equality.
       */
      desc = describe_info (info);
      g_assert_cmpstr (desc, ==, description->str);

      /* do extra checks for containers */
      if (g_variant_type_is_array (type) ||
          g_variant_type_is_maybe (type))
        {
          const GVariantType *element;
          gsize efs1, efs2;
          guint ea1, ea2;

          element = g_variant_type_element (type);
          calculate_type_info (element, &efs1, &ea1);
          g_variant_type_info_query_element (info, &ea2, &efs2);
          g_assert_cmpint (efs1, ==, efs2);
          g_assert_cmpint (ea1, ==, ea2 + 1);

          g_assert_cmpint (ea1, ==, alignment1);
          g_assert_cmpint (0, ==, fixed_size1);
        }
      else if (g_variant_type_is_tuple (type) ||
               g_variant_type_is_dict_entry (type))
        {
          /* make sure the "magic constants" are working */
          check_offsets (info, type);
        }

      g_string_free (type_string, TRUE);
      g_string_free (description, TRUE);
      g_variant_type_info_unref (info);
      g_variant_type_free (type);
      g_free (desc);
    }

  g_variant_type_info_assert_no_infos ();
}

#define MAX_FIXED_MULTIPLIER    256
#define MAX_INSTANCE_SIZE       1024
#define MAX_ARRAY_CHILDREN      128
#define MAX_TUPLE_CHILDREN      128

/* this function generates a random type such that all characteristics
 * that are "interesting" to the serialiser are tested.
 *
 * this basically means:
 *   - test different alignments
 *   - test variable sized items and fixed sized items
 *   - test different fixed sizes
 */
static gchar *
random_type_string (void)
{
  const guchar base_types[] = "ynix";
  guchar base_type;

  base_type = base_types[g_test_rand_int_range (0, 4)];

  if (g_test_rand_bit ())
    /* construct a fixed-sized type */
    {
      char type_string[MAX_FIXED_MULTIPLIER];
      guint multiplier;
      guint i = 0;

      multiplier = g_test_rand_int_range (1, sizeof type_string - 1);

      type_string[i++] = '(';
      while (multiplier--)
        type_string[i++] = base_type;
      type_string[i++] = ')';

      return g_strndup (type_string, i);
    }
  else
    /* construct a variable-sized type */
    {
      char type_string[2] = { 'a', base_type };

      return g_strndup (type_string, 2);
    }
}

typedef struct
{
  GVariantTypeInfo *type_info;
  guint alignment;
  gsize size;
  gboolean is_fixed_sized;

  guint32 seed;

#define INSTANCE_MAGIC    1287582829
  guint magic;
} RandomInstance;

static RandomInstance *
random_instance (GVariantTypeInfo *type_info)
{
  RandomInstance *instance;

  instance = g_slice_new (RandomInstance);

  if (type_info == NULL)
    {
      gchar *str = random_type_string ();
      instance->type_info = g_variant_type_info_get (G_VARIANT_TYPE (str));
      g_free (str);
    }
  else
    instance->type_info = g_variant_type_info_ref (type_info);

  instance->seed = g_test_rand_int ();

  g_variant_type_info_query (instance->type_info,
                             &instance->alignment,
                             &instance->size);

  instance->is_fixed_sized = instance->size != 0;

  if (!instance->is_fixed_sized)
    instance->size = g_test_rand_int_range (0, MAX_INSTANCE_SIZE);

  instance->magic = INSTANCE_MAGIC;

  return instance;
}

static void
random_instance_free (RandomInstance *instance)
{
  g_variant_type_info_unref (instance->type_info);
  g_slice_free (RandomInstance, instance);
}

static void
append_instance_size (RandomInstance *instance,
                      gsize          *offset)
{
  *offset += (-*offset) & instance->alignment;
  *offset += instance->size;
}

static void
random_instance_write (RandomInstance *instance,
                       guchar         *buffer)
{
  GRand *rand;
  gint i;

  g_assert_cmpint ((gsize) buffer & ALIGN_BITS & instance->alignment, ==, 0);

  rand = g_rand_new_with_seed (instance->seed);
  for (i = 0; i < instance->size; i++)
    buffer[i] = g_rand_int (rand);
  g_rand_free (rand);
}

static void
append_instance_data (RandomInstance  *instance,
                      guchar         **buffer)
{
  while (((gsize) *buffer) & instance->alignment)
    *(*buffer)++ = '\0';

  random_instance_write (instance, *buffer);
  *buffer += instance->size;
}

static gboolean
random_instance_assert (RandomInstance *instance,
                        guchar         *buffer,
                        gsize           size)
{
  GRand *rand;
  gint i;

  g_assert_cmpint ((gsize) buffer & ALIGN_BITS & instance->alignment, ==, 0);
  g_assert_cmpint (size, ==, instance->size);

  rand = g_rand_new_with_seed (instance->seed);
  for (i = 0; i < instance->size; i++)
    {
      guchar byte = g_rand_int (rand);

      g_assert (buffer[i] == byte);
    }
  g_rand_free (rand);

  return i == instance->size;
}

static gboolean
random_instance_check (RandomInstance *instance,
                       guchar         *buffer,
                       gsize           size)
{
  GRand *rand;
  gint i;

  g_assert_cmpint ((gsize) buffer & ALIGN_BITS & instance->alignment, ==, 0);

  if (size != instance->size)
    return FALSE;

  rand = g_rand_new_with_seed (instance->seed);
  for (i = 0; i < instance->size; i++)
    if (buffer[i] != (guchar) g_rand_int (rand))
      break;
  g_rand_free (rand);

  return i == instance->size;
}

static void
random_instance_filler (GVariantSerialised *serialised,
                        gpointer            data)
{
  RandomInstance *instance = data;

  g_assert (instance->magic == INSTANCE_MAGIC);

  if (serialised->type_info == NULL)
    serialised->type_info = instance->type_info;

  if (serialised->size == 0)
    serialised->size = instance->size;

  g_assert (serialised->type_info == instance->type_info);
  g_assert (serialised->size == instance->size);

  if (serialised->data)
    random_instance_write (instance, serialised->data);
}

static gsize
calculate_offset_size (gsize body_size,
                       gsize n_offsets)
{
  if (body_size == 0)
    return 0;

  if (body_size + n_offsets <= G_MAXUINT8)
    return 1;

  if (body_size + 2 * n_offsets <= G_MAXUINT16)
    return 2;

  if (body_size + 4 * n_offsets <= G_MAXUINT32)
    return 4;

  /* the test case won't generate anything bigger */
  g_assert_not_reached ();
}

static gpointer
flavoured_malloc (gsize size, gsize flavour)
{
  g_assert (flavour < 8);

  if (size == 0)
    return NULL;

  return ((gchar *) g_malloc (size + flavour)) + flavour;
}

static void
flavoured_free (gpointer data,
                gsize flavour)
{
  if (!data)
    return;
  g_free (((gchar *) data) - flavour);
}

static gpointer
align_malloc (gsize size)
{
  gpointer mem;

#ifdef HAVE_POSIX_MEMALIGN
  if (posix_memalign (&mem, 8, size))
    g_error ("posix_memalign failed");
#else
  /* NOTE: there may be platforms that lack posix_memalign() and also
   * have malloc() that returns non-8-aligned.  if so, we need to try
   * harder here.
   */
  mem = malloc (size);
#endif

  return mem;
}

static void
align_free (gpointer mem)
{
  free (mem);
}

static void
append_offset (guchar **offset_ptr,
               gsize    offset,
               guint    offset_size)
{
  union
  {
    guchar bytes[sizeof (gsize)];
    gsize integer;
  } tmpvalue;

  tmpvalue.integer = GSIZE_TO_LE (offset);
  memcpy (*offset_ptr, tmpvalue.bytes, offset_size);
  *offset_ptr += offset_size;
}

static void
prepend_offset (guchar **offset_ptr,
                gsize    offset,
                guint    offset_size)
{
  union
  {
    guchar bytes[sizeof (gsize)];
    gsize integer;
  } tmpvalue;

  *offset_ptr -= offset_size;
  tmpvalue.integer = GSIZE_TO_LE (offset);
  memcpy (*offset_ptr, tmpvalue.bytes, offset_size);
}

static void
test_maybe (void)
{
  GVariantTypeInfo *type_info;
  RandomInstance *instance;
  gsize needed_size;
  guchar *data;

  instance = random_instance (NULL);

  {
    const gchar *element;
    gchar *tmp;

    element = g_variant_type_info_get_type_string (instance->type_info);
    tmp = g_strdup_printf ("m%s", element);
    type_info = g_variant_type_info_get (G_VARIANT_TYPE (tmp));
    g_free (tmp);
  }

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  NULL, 0);
  g_assert_cmpint (needed_size, ==, 0);

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  (gpointer *) &instance, 1);

  if (instance->is_fixed_sized)
    g_assert_cmpint (needed_size, ==, instance->size);
  else
    g_assert_cmpint (needed_size, ==, instance->size + 1);

  {
    guchar *ptr;

    ptr = data = align_malloc (needed_size);
    append_instance_data (instance, &ptr);

    if (!instance->is_fixed_sized)
      *ptr++ = '\0';

    g_assert_cmpint (ptr - data, ==, needed_size);
  }

  {
    guint alignment;
    guint flavour;

    alignment = (instance->alignment & ALIGN_BITS) + 1;

    for (flavour = 0; flavour < 8; flavour += alignment)
      {
        GVariantSerialised serialised;
        GVariantSerialised child;

        serialised.type_info = type_info;
        serialised.data = flavoured_malloc (needed_size, flavour);
        serialised.size = needed_size;

        g_variant_serialiser_serialise (serialised,
                                        random_instance_filler,
                                        (gpointer *) &instance, 1);
        child = g_variant_serialised_get_child (serialised, 0);
        g_assert (child.type_info == instance->type_info);
        random_instance_assert (instance, child.data, child.size);
        g_variant_type_info_unref (child.type_info);
        flavoured_free (serialised.data, flavour);
      }
  }

  g_variant_type_info_unref (type_info);
  random_instance_free (instance);
  align_free (data);
}

static void
test_maybes (void)
{
  guint i;

  for (i = 0; i < 1000; i++)
    test_maybe ();

  g_variant_type_info_assert_no_infos ();
}

static void
test_array (void)
{
  GVariantTypeInfo *element_info;
  GVariantTypeInfo *array_info;
  RandomInstance **instances;
  gsize needed_size;
  gsize offset_size;
  guint n_children;
  guchar *data;

  {
    gchar *element_type, *array_type;

    element_type = random_type_string ();
    array_type = g_strdup_printf ("a%s", element_type);

    element_info = g_variant_type_info_get (G_VARIANT_TYPE (element_type));
    array_info = g_variant_type_info_get (G_VARIANT_TYPE (array_type));
    g_assert (g_variant_type_info_element (array_info) == element_info);

    g_free (element_type);
    g_free (array_type);
  }

  {
    guint i;

    n_children = g_test_rand_int_range (0, MAX_ARRAY_CHILDREN);
    instances = g_new (RandomInstance *, n_children);
    for (i = 0; i < n_children; i++)
      instances[i] = random_instance (element_info);
  }

  needed_size = g_variant_serialiser_needed_size (array_info,
                                                  random_instance_filler,
                                                  (gpointer *) instances,
                                                  n_children);

  {
    gsize element_fixed_size;
    gsize body_size = 0;
    guint i;

    for (i = 0; i < n_children; i++)
      append_instance_size (instances[i], &body_size);

    g_variant_type_info_query (element_info, NULL, &element_fixed_size);

    if (!element_fixed_size)
      {
        offset_size = calculate_offset_size (body_size, n_children);

        if (offset_size == 0)
          offset_size = 1;
      }
    else
      offset_size = 0;

    g_assert_cmpint (needed_size, ==, body_size + n_children * offset_size);
  }

  {
    guchar *offset_ptr, *body_ptr;
    guint i;

    body_ptr = data = align_malloc (needed_size);
    offset_ptr = body_ptr + needed_size - offset_size * n_children;

    for (i = 0; i < n_children; i++)
      {
        append_instance_data (instances[i], &body_ptr);
        append_offset (&offset_ptr, body_ptr - data, offset_size);
      }

    g_assert (body_ptr == data + needed_size - offset_size * n_children);
    g_assert (offset_ptr == data + needed_size);
  }

  {
    guint alignment;
    gsize flavour;
    guint i;

    g_variant_type_info_query (array_info, &alignment, NULL);
    alignment = (alignment & ALIGN_BITS) + 1;

    for (flavour = 0; flavour < 8; flavour += alignment)
      {
        GVariantSerialised serialised;

        serialised.type_info = array_info;
        serialised.data = flavoured_malloc (needed_size, flavour);
        serialised.size = needed_size;

        g_variant_serialiser_serialise (serialised, random_instance_filler,
                                        (gpointer *) instances, n_children);

        if (serialised.size)
          g_assert (memcmp (serialised.data, data, serialised.size) == 0);

        g_assert (g_variant_serialised_n_children (serialised) == n_children);

        for (i = 0; i < n_children; i++)
          {
            GVariantSerialised child;

            child = g_variant_serialised_get_child (serialised, i);
            g_assert (child.type_info == instances[i]->type_info);
            random_instance_assert (instances[i], child.data, child.size);
            g_variant_type_info_unref (child.type_info);
          }

        flavoured_free (serialised.data, flavour);
      }
  }

  {
    guint i;

    for (i = 0; i < n_children; i++)
      random_instance_free (instances[i]);
    g_free (instances);
  }

  g_variant_type_info_unref (element_info);
  g_variant_type_info_unref (array_info);
  align_free (data);
}

static void
test_arrays (void)
{
  guint i;

  for (i = 0; i < 100; i++)
    test_array ();

  g_variant_type_info_assert_no_infos ();
}

static void
test_tuple (void)
{
  GVariantTypeInfo *type_info;
  RandomInstance **instances;
  gboolean fixed_size;
  gsize needed_size;
  gsize offset_size;
  guint n_children;
  guint alignment;
  guchar *data;

  n_children = g_test_rand_int_range (0, MAX_TUPLE_CHILDREN);
  instances = g_new (RandomInstance *, n_children);

  {
    GString *type_string;
    guint i;

    fixed_size = TRUE;
    alignment = 0;

    type_string = g_string_new ("(");
    for (i = 0; i < n_children; i++)
      {
        const gchar *str;

        instances[i] = random_instance (NULL);

        alignment |= instances[i]->alignment;
        if (!instances[i]->is_fixed_sized)
          fixed_size = FALSE;

        str = g_variant_type_info_get_type_string (instances[i]->type_info);
        g_string_append (type_string, str);
      }
    g_string_append_c (type_string, ')');

    type_info = g_variant_type_info_get (G_VARIANT_TYPE (type_string->str));
    g_string_free (type_string, TRUE);
  }

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  (gpointer *) instances,
                                                  n_children);
  {
    gsize body_size = 0;
    gsize offsets = 0;
    guint i;

    for (i = 0; i < n_children; i++)
      {
        append_instance_size (instances[i], &body_size);

        if (i != n_children - 1 && !instances[i]->is_fixed_sized)
          offsets++;
      }

    if (fixed_size)
      {
        body_size += (-body_size) & alignment;

        g_assert ((body_size == 0) == (n_children == 0));
        if (n_children == 0)
          body_size = 1;
      }

    offset_size = calculate_offset_size (body_size, offsets);
    g_assert_cmpint (needed_size, ==, body_size + offsets * offset_size);
  }

  {
    guchar *body_ptr;
    guchar *ofs_ptr;
    guint i;

    body_ptr = data = align_malloc (needed_size);
    ofs_ptr = body_ptr + needed_size;

    for (i = 0; i < n_children; i++)
      {
        append_instance_data (instances[i], &body_ptr);

        if (i != n_children - 1 && !instances[i]->is_fixed_sized)
          prepend_offset (&ofs_ptr, body_ptr - data, offset_size);
      }

    if (fixed_size)
      {
        while (((gsize) body_ptr) & alignment)
          *body_ptr++ = '\0';

        g_assert ((body_ptr == data) == (n_children == 0));
        if (n_children == 0)
          *body_ptr++ = '\0';

      }


    g_assert (body_ptr == ofs_ptr);
  }

  {
    gsize flavour;
    guint i;

    alignment = (alignment & ALIGN_BITS) + 1;

    for (flavour = 0; flavour < 8; flavour += alignment)
      {
        GVariantSerialised serialised;

        serialised.type_info = type_info;
        serialised.data = flavoured_malloc (needed_size, flavour);
        serialised.size = needed_size;

        g_variant_serialiser_serialise (serialised, random_instance_filler,
                                        (gpointer *) instances, n_children);

        if (serialised.size)
          g_assert (memcmp (serialised.data, data, serialised.size) == 0);

        g_assert (g_variant_serialised_n_children (serialised) == n_children);

        for (i = 0; i < n_children; i++)
          {
            GVariantSerialised child;

            child = g_variant_serialised_get_child (serialised, i);
            g_assert (child.type_info == instances[i]->type_info);
            random_instance_assert (instances[i], child.data, child.size);
            g_variant_type_info_unref (child.type_info);
          }

        flavoured_free (serialised.data, flavour);
      }
  }

  {
    guint i;

    for (i = 0; i < n_children; i++)
      random_instance_free (instances[i]);
    g_free (instances);
  }

  g_variant_type_info_unref (type_info);
  align_free (data);
}

static void
test_tuples (void)
{
  guint i;

  for (i = 0; i < 100; i++)
    test_tuple ();

  g_variant_type_info_assert_no_infos ();
}

static void
test_variant (void)
{
  GVariantTypeInfo *type_info;
  RandomInstance *instance;
  const gchar *type_string;
  gsize needed_size;
  guchar *data;
  gsize len;

  type_info = g_variant_type_info_get (G_VARIANT_TYPE_VARIANT);
  instance = random_instance (NULL);

  type_string = g_variant_type_info_get_type_string (instance->type_info);
  len = strlen (type_string);

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  (gpointer *) &instance, 1);

  g_assert_cmpint (needed_size, ==, instance->size + 1 + len);

  {
    guchar *ptr;

    ptr = data = align_malloc (needed_size);
    append_instance_data (instance, &ptr);
    *ptr++ = '\0';
    memcpy (ptr, type_string, len);
    ptr += len;

    g_assert (data + needed_size == ptr);
  }

  {
    gsize alignment;
    gsize flavour;

    /* variants are always 8-aligned */
    alignment = ALIGN_BITS + 1;

    for (flavour = 0; flavour < 8; flavour += alignment)
      {
        GVariantSerialised serialised;
        GVariantSerialised child;

        serialised.type_info = type_info;
        serialised.data = flavoured_malloc (needed_size, flavour);
        serialised.size = needed_size;

        g_variant_serialiser_serialise (serialised, random_instance_filler,
                                        (gpointer *) &instance, 1);

        if (serialised.size)
          g_assert (memcmp (serialised.data, data, serialised.size) == 0);

        g_assert (g_variant_serialised_n_children (serialised) == 1);

        child = g_variant_serialised_get_child (serialised, 0);
        g_assert (child.type_info == instance->type_info);
        random_instance_check (instance, child.data, child.size);

        g_variant_type_info_unref (child.type_info);
        flavoured_free (serialised.data, flavour);
      }
  }

  g_variant_type_info_unref (type_info);
  random_instance_free (instance);
  align_free (data);
}

static void
test_variants (void)
{
  guint i;

  for (i = 0; i < 100; i++)
    test_variant ();

  g_variant_type_info_assert_no_infos ();
}

static void
test_strings (void)
{
  struct {
    guint flags;
    guint size;
    gconstpointer data;
  } test_cases[] = {
#define is_nval           0
#define is_string         1
#define is_objpath        is_string | 2
#define is_sig            is_string | 4
    { is_sig,       1, "" },
    { is_nval,      0, NULL },
    { is_nval,     13, "hello\xffworld!" },
    { is_string,   13, "hello world!" },
    { is_nval,     13, "hello world\0" },
    { is_nval,     13, "hello\0world!" },
    { is_nval,     12, "hello world!" },
    { is_nval,     13, "hello world!\xff" },

    { is_objpath,   2, "/" },
    { is_objpath,   3, "/a" },
    { is_string,    3, "//" },
    { is_objpath,  11, "/some/path" },
    { is_string,   12, "/some/path/" },
    { is_nval,     11, "/some\0path" },
    { is_string,   11, "/some\\path" },
    { is_string,   12, "/some//path" },
    { is_string,   12, "/some-/path" },

    { is_sig,       2, "i" },
    { is_sig,       2, "s" },
    { is_sig,       5, "(si)" },
    { is_string,    4, "(si" },
    { is_string,    2, "*" },
    { is_sig,       3, "ai" },
    { is_string,    3, "mi" },
    { is_string,    2, "r" },
    { is_sig,      15, "(yyy{sv}ssiai)" },
    { is_string,   16, "(yyy{yv}ssiai))" },
    { is_string,   15, "(yyy{vv}ssiai)" },
    { is_string,   15, "(yyy{sv)ssiai}" }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
      guint flags;

      flags = g_variant_serialiser_is_string (test_cases[i].data,
                                              test_cases[i].size)
        ? 1 : 0;

      flags |= g_variant_serialiser_is_object_path (test_cases[i].data,
                                                    test_cases[i].size)
        ? 2 : 0;

      flags |= g_variant_serialiser_is_signature (test_cases[i].data,
                                                  test_cases[i].size)
        ? 4 : 0;

      g_assert (flags == test_cases[i].flags);
    }
}

typedef struct _TreeInstance TreeInstance;
struct _TreeInstance
{
  GVariantTypeInfo *info;

  TreeInstance **children;
  gsize n_children;

  union {
    guint64 integer;
    gdouble floating;
    gchar string[200];
  } data;
  gsize data_size;
};

static GVariantType *
make_random_definite_type (int depth)
{
  GString *description;
  GString *type_string;
  GVariantType *type;

  description = g_string_new (NULL);
  type_string = g_string_new (NULL);
  type = append_type_string (type_string, description, TRUE, depth);
  g_string_free (description, TRUE);
  g_string_free (type_string, TRUE);

  return type;
}

static void
make_random_string (gchar              *string,
                    gsize               size,
                    const GVariantType *type)
{
  gint i;

  /* create strings that are valid signature strings */
#define good_chars "bynqiuxthdsog"

  for (i = 0; i < size - 1; i++)
    string[i] = good_chars[g_test_rand_int_range (0, strlen (good_chars))];
  string[i] = '\0';

  /* in case we need an object path, prefix a '/' */
  if (*g_variant_type_peek_string (type) == 'o')
    string[0] = '/';

#undef good_chars
}

static TreeInstance *
tree_instance_new (const GVariantType *type,
                   int                 depth)
{
  const GVariantType *child_type = NULL;
  GVariantType *mytype = NULL;
  TreeInstance *instance;
  gboolean is_tuple_type;

  if (type == NULL)
    type = mytype = make_random_definite_type (depth);

  instance = g_slice_new (TreeInstance);
  instance->info = g_variant_type_info_get (type);
  instance->children = NULL;
  instance->n_children = 0;
  instance->data_size = 0;

  is_tuple_type = FALSE;

  switch (*g_variant_type_peek_string (type))
    {
    case G_VARIANT_TYPE_INFO_CHAR_MAYBE:
      instance->n_children = g_test_rand_int_range (0, 2);
      child_type = g_variant_type_element (type);
      break;

    case G_VARIANT_TYPE_INFO_CHAR_ARRAY:
      instance->n_children = g_test_rand_int_range (0, MAX_ARRAY_CHILDREN);
      child_type = g_variant_type_element (type);
      break;

    case G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY:
    case G_VARIANT_TYPE_INFO_CHAR_TUPLE:
      instance->n_children = g_variant_type_n_items (type);
      child_type = g_variant_type_first (type);
      is_tuple_type = TRUE;
      break;

    case G_VARIANT_TYPE_INFO_CHAR_VARIANT:
      instance->n_children = 1;
      child_type = NULL;
      break;

    case 'b':
      instance->data.integer = g_test_rand_int_range (0, 2);
      instance->data_size = 1;
      break;

    case 'y':
      instance->data.integer = g_test_rand_int ();
      instance->data_size = 1;
      break;

    case 'n': case 'q':
      instance->data.integer = g_test_rand_int ();
      instance->data_size = 2;
      break;

    case 'i': case 'u': case 'h':
      instance->data.integer = g_test_rand_int ();
      instance->data_size = 4;
      break;

    case 'x': case 't':
      instance->data.integer = g_test_rand_int ();
      instance->data.integer <<= 32;
      instance->data.integer |= (guint32) g_test_rand_int ();
      instance->data_size = 8;
      break;

    case 'd':
      instance->data.floating = g_test_rand_double ();
      instance->data_size = 8;
      break;

    case 's': case 'o': case 'g':
      instance->data_size = g_test_rand_int_range (10, 200);
      make_random_string (instance->data.string, instance->data_size, type);
      break;
    }

  if (instance->data_size == 0)
    /* no data -> it is a container */
    {
      guint i;

      instance->children = g_new (TreeInstance *, instance->n_children);

      for (i = 0; i < instance->n_children; i++)
        {
          instance->children[i] = tree_instance_new (child_type, depth - 1);

          if (is_tuple_type)
            child_type = g_variant_type_next (child_type);
        }

      g_assert (!is_tuple_type || child_type == NULL);
    }

  g_variant_type_free (mytype);

  return instance;
}

static void
tree_instance_free (TreeInstance *instance)
{
  gint i;

  g_variant_type_info_unref (instance->info);
  for (i = 0; i < instance->n_children; i++)
    tree_instance_free (instance->children[i]);
  g_free (instance->children);
  g_slice_free (TreeInstance, instance);
}

static gboolean i_am_writing_byteswapped;

static void
tree_filler (GVariantSerialised *serialised,
             gpointer            data)
{
  TreeInstance *instance = data;

  if (serialised->type_info == NULL)
    serialised->type_info = instance->info;

  if (instance->data_size == 0)
    /* is a container */
    {
      if (serialised->size == 0)
        serialised->size =
          g_variant_serialiser_needed_size (instance->info, tree_filler,
                                            (gpointer *) instance->children,
                                            instance->n_children);

      if (serialised->data)
        g_variant_serialiser_serialise (*serialised, tree_filler,
                                        (gpointer *) instance->children,
                                        instance->n_children);
    }
  else
    /* it is a leaf */
    {
      if (serialised->size == 0)
        serialised->size = instance->data_size;

      if (serialised->data)
        {
          switch (instance->data_size)
            {
            case 1:
              *serialised->data = instance->data.integer;
              break;

            case 2:
              {
                guint16 value = instance->data.integer;

                if (i_am_writing_byteswapped)
                  value = GUINT16_SWAP_LE_BE (value);

                *(guint16 *) serialised->data = value;
              }
              break;

            case 4:
              {
                guint32 value = instance->data.integer;

                if (i_am_writing_byteswapped)
                  value = GUINT32_SWAP_LE_BE (value);

                *(guint32 *) serialised->data = value;
              }
              break;

            case 8:
              {
                guint64 value = instance->data.integer;

                if (i_am_writing_byteswapped)
                  value = GUINT64_SWAP_LE_BE (value);

                *(guint64 *) serialised->data = value;
              }
              break;

            default:
              memcpy (serialised->data,
                      instance->data.string,
                      instance->data_size);
              break;
            }
        }
    }
}

static gboolean
check_tree (TreeInstance       *instance,
            GVariantSerialised  serialised)
{
  if (instance->info != serialised.type_info)
    return FALSE;

  if (instance->data_size == 0)
    /* is a container */
    {
      gint i;

      if (g_variant_serialised_n_children (serialised) !=
          instance->n_children)
        return FALSE;

      for (i = 0; i < instance->n_children; i++)
        {
          GVariantSerialised child;
          gpointer data = NULL;
          gboolean ok;

          child = g_variant_serialised_get_child (serialised, i);
          if (child.size && child.data == NULL)
            child.data = data = g_malloc0 (child.size);
          ok = check_tree (instance->children[i], child);
          g_variant_type_info_unref (child.type_info);
          g_free (data);

          if (!ok)
            return FALSE;
        }

      return TRUE;
    }
  else
    /* it is a leaf */
    {
      switch (instance->data_size)
        {
        case 1:
          g_assert (serialised.size == 1);
          return *(guint8 *) serialised.data ==
                  (guint8) instance->data.integer;

        case 2:
          g_assert (serialised.size == 2);
          return *(guint16 *) serialised.data ==
                  (guint16) instance->data.integer;

        case 4:
          g_assert (serialised.size == 4);
          return *(guint32 *) serialised.data ==
                  (guint32) instance->data.integer;

        case 8:
          g_assert (serialised.size == 8);
          return *(guint64 *) serialised.data ==
                  (guint64) instance->data.integer;

        default:
          if (serialised.size != instance->data_size)
            return FALSE;

          return memcmp (serialised.data,
                         instance->data.string,
                         instance->data_size) == 0;
        }
    }
}

static void
serialise_tree (TreeInstance       *tree,
                GVariantSerialised *serialised)
{
  GVariantSerialised empty = {0, };

  *serialised = empty;
  tree_filler (serialised, tree);
  serialised->data = g_malloc (serialised->size);
  tree_filler (serialised, tree);
}

static void
test_byteswap (void)
{
  GVariantSerialised one, two;
  TreeInstance *tree;

  tree = tree_instance_new (NULL, 3);
  serialise_tree (tree, &one);

  i_am_writing_byteswapped = TRUE;
  serialise_tree (tree, &two);
  i_am_writing_byteswapped = FALSE;

  g_variant_serialised_byteswap (two);

  g_assert_cmpmem (one.data, one.size, two.data, two.size);

  tree_instance_free (tree);
  g_free (one.data);
  g_free (two.data);
}

static void
test_byteswaps (void)
{
  int i;

  for (i = 0; i < 200; i++)
    test_byteswap ();

  g_variant_type_info_assert_no_infos ();
}

static void
test_fuzz (gdouble *fuzziness)
{
  GVariantSerialised serialised;
  TreeInstance *tree;

  /* make an instance */
  tree = tree_instance_new (NULL, 3);

  /* serialise it */
  serialise_tree (tree, &serialised);

  g_assert (g_variant_serialised_is_normal (serialised));
  g_assert (check_tree (tree, serialised));

  if (serialised.size)
    {
      gboolean fuzzed = FALSE;
      gboolean a, b;

      while (!fuzzed)
        {
          gint i;

          for (i = 0; i < serialised.size; i++)
            if (randomly (*fuzziness))
              {
                serialised.data[i] += g_test_rand_int_range (1, 256);
                fuzzed = TRUE;
              }
        }

      /* at least one byte in the serialised data has changed.
       *
       * this means that at least one of the following is true:
       *
       *    - the serialised data now represents a different value:
       *        check_tree() will return FALSE
       *
       *    - the serialised data is in non-normal form:
       *        g_variant_serialiser_is_normal() will return FALSE
       *
       * we always do both checks to increase exposure of the serialiser
       * to corrupt data.
       */
      a = g_variant_serialised_is_normal (serialised);
      b = check_tree (tree, serialised);

      g_assert (!a || !b);
    }

  tree_instance_free (tree);
  g_free (serialised.data);
}


static void
test_fuzzes (gpointer data)
{
  gdouble fuzziness;
  int i;

  fuzziness = GPOINTER_TO_INT (data) / 100.;

  for (i = 0; i < 200; i++)
    test_fuzz (&fuzziness);

  g_variant_type_info_assert_no_infos ();
}

static GVariant *
tree_instance_get_gvariant (TreeInstance *tree)
{
  const GVariantType *type;
  GVariant *result;

  type = (GVariantType *) g_variant_type_info_get_type_string (tree->info);

  switch (g_variant_type_info_get_type_char (tree->info))
    {
    case G_VARIANT_TYPE_INFO_CHAR_MAYBE:
      {
        const GVariantType *child_type;
        GVariant *child;

        if (tree->n_children)
          child = tree_instance_get_gvariant (tree->children[0]);
        else
          child = NULL;

        child_type = g_variant_type_element (type);

        if (child != NULL && randomly (0.5))
          child_type = NULL;

        result = g_variant_new_maybe (child_type, child);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_ARRAY:
      {
        const GVariantType *child_type;
        GVariant **children;
        gint i;

        children = g_new (GVariant *, tree->n_children);
        for (i = 0; i < tree->n_children; i++)
          children[i] = tree_instance_get_gvariant (tree->children[i]);

        child_type = g_variant_type_element (type);

        if (i > 0 && randomly (0.5))
          child_type = NULL;

        result = g_variant_new_array (child_type, children, tree->n_children);
        g_free (children);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_TUPLE:
      {
        GVariant **children;
        gint i;

        children = g_new (GVariant *, tree->n_children);
        for (i = 0; i < tree->n_children; i++)
          children[i] = tree_instance_get_gvariant (tree->children[i]);

        result = g_variant_new_tuple (children, tree->n_children);
        g_free (children);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY:
      {
        GVariant *key, *val;

        g_assert (tree->n_children == 2);

        key = tree_instance_get_gvariant (tree->children[0]);
        val = tree_instance_get_gvariant (tree->children[1]);

        result = g_variant_new_dict_entry (key, val);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_VARIANT:
      {
        GVariant *value;

        g_assert (tree->n_children == 1);

        value = tree_instance_get_gvariant (tree->children[0]);
        result = g_variant_new_variant (value);
      }
      break;

    case 'b':
      result = g_variant_new_boolean (tree->data.integer > 0);
      break;

    case 'y':
      result = g_variant_new_byte (tree->data.integer);
      break;

    case 'n':
      result = g_variant_new_int16 (tree->data.integer);
      break;

    case 'q':
      result = g_variant_new_uint16 (tree->data.integer);
      break;

    case 'i':
      result = g_variant_new_int32 (tree->data.integer);
      break;

    case 'u':
      result = g_variant_new_uint32 (tree->data.integer);
      break;

    case 'x':
      result = g_variant_new_int64 (tree->data.integer);
      break;

    case 't':
      result = g_variant_new_uint64 (tree->data.integer);
      break;

    case 'h':
      result = g_variant_new_handle (tree->data.integer);
      break;

    case 'd':
      result = g_variant_new_double (tree->data.floating);
      break;

    case 's':
      result = g_variant_new_string (tree->data.string);
      break;

    case 'o':
      result = g_variant_new_object_path (tree->data.string);
      break;

    case 'g':
      result = g_variant_new_signature (tree->data.string);
      break;

    default:
      g_assert_not_reached ();
    }

  return result;
}

static gboolean
tree_instance_check_gvariant (TreeInstance *tree,
                              GVariant     *value)
{
  const GVariantType *type;

  type = (GVariantType *) g_variant_type_info_get_type_string (tree->info);
  g_assert (g_variant_is_of_type (value, type));

  switch (g_variant_type_info_get_type_char (tree->info))
    {
    case G_VARIANT_TYPE_INFO_CHAR_MAYBE:
      {
        GVariant *child;
        gboolean equal;

        child = g_variant_get_maybe (value);

        if (child != NULL && tree->n_children == 1)
          equal = tree_instance_check_gvariant (tree->children[0], child);
        else if (child == NULL && tree->n_children == 0)
          equal = TRUE;
        else
          equal = FALSE;

        if (child != NULL)
          g_variant_unref (child);

        return equal;
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_ARRAY:
    case G_VARIANT_TYPE_INFO_CHAR_TUPLE:
    case G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY:
      {
        gsize i;

        if (g_variant_n_children (value) != tree->n_children)
          return FALSE;

        for (i = 0; i < tree->n_children; i++)
          {
            GVariant *child;
            gboolean equal;

            child = g_variant_get_child_value (value, i);
            equal = tree_instance_check_gvariant (tree->children[i], child);
            g_variant_unref (child);

            if (!equal)
              return FALSE;
          }

        return TRUE;
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_VARIANT:
      {
        const gchar *str1, *str2;
        GVariant *child;
        gboolean equal;

        child = g_variant_get_variant (value);
        str1 = g_variant_get_type_string (child);
        str2 = g_variant_type_info_get_type_string (tree->children[0]->info);
        /* GVariant only keeps one copy of type strings around */
        equal = str1 == str2 &&
                tree_instance_check_gvariant (tree->children[0], child);

        g_variant_unref (child);

        return equal;
      }
      break;

    case 'b':
      return g_variant_get_boolean (value) == tree->data.integer;

    case 'y':
      return g_variant_get_byte (value) == (guchar) tree->data.integer;

    case 'n':
      return g_variant_get_int16 (value) == (gint16) tree->data.integer;

    case 'q':
      return g_variant_get_uint16 (value) == (guint16) tree->data.integer;

    case 'i':
      return g_variant_get_int32 (value) == (gint32) tree->data.integer;

    case 'u':
      return g_variant_get_uint32 (value) == (guint32) tree->data.integer;

    case 'x':
      return g_variant_get_int64 (value) == (gint64) tree->data.integer;

    case 't':
      return g_variant_get_uint64 (value) == (guint64) tree->data.integer;

    case 'h':
      return g_variant_get_handle (value) == (gint32) tree->data.integer;

    case 'd':
      {
        gdouble floating = g_variant_get_double (value);

        return memcmp (&floating, &tree->data.floating, sizeof floating) == 0;
      }

    case 's':
    case 'o':
    case 'g':
      return strcmp (g_variant_get_string (value, NULL),
                     tree->data.string) == 0;

    default:
      g_assert_not_reached ();
    }
}

static void
tree_instance_build_gvariant (TreeInstance    *tree,
                              GVariantBuilder *builder,
                              gboolean         guess_ok)
{
  const GVariantType *type;

  type = (GVariantType *) g_variant_type_info_get_type_string (tree->info);

  if (g_variant_type_is_container (type))
    {
      gsize i;

      /* force GVariantBuilder to guess the type half the time */
      if (guess_ok && randomly (0.5))
        {
          if (g_variant_type_is_array (type) && tree->n_children)
            type = G_VARIANT_TYPE_ARRAY;

          if (g_variant_type_is_maybe (type) && tree->n_children)
            type = G_VARIANT_TYPE_MAYBE;

          if (g_variant_type_is_tuple (type))
            type = G_VARIANT_TYPE_TUPLE;

          if (g_variant_type_is_dict_entry (type))
            type = G_VARIANT_TYPE_DICT_ENTRY;
        }
      else
        guess_ok = FALSE;

      g_variant_builder_open (builder, type);

      for (i = 0; i < tree->n_children; i++)
        tree_instance_build_gvariant (tree->children[i], builder, guess_ok);

      g_variant_builder_close (builder);
    }
  else
    g_variant_builder_add_value (builder, tree_instance_get_gvariant (tree));
}


static gboolean
tree_instance_check_iter (TreeInstance *tree,
                          GVariantIter *iter)
{
  GVariant *value;

  value = g_variant_iter_next_value (iter);

  if (g_variant_is_container (value))
    {
      gsize i;

      iter = g_variant_iter_new (value);
      g_variant_unref (value);

      if (g_variant_iter_n_children (iter) != tree->n_children)
        {
          g_variant_iter_free (iter);
          return FALSE;
        }

      for (i = 0; i < tree->n_children; i++)
        if (!tree_instance_check_iter (tree->children[i], iter))
          {
            g_variant_iter_free (iter);
            return FALSE;
          }

      g_assert (g_variant_iter_next_value (iter) == NULL);
      g_variant_iter_free (iter);

      return TRUE;
    }

  else
    {
      gboolean equal;

      equal = tree_instance_check_gvariant (tree, value);
      g_variant_unref (value);

      return equal;
    }
}

static void
test_container (void)
{
  TreeInstance *tree;
  GVariant *value;
  gchar *s1, *s2;

  tree = tree_instance_new (NULL, 3);
  value = g_variant_ref_sink (tree_instance_get_gvariant (tree));

  s1 = g_variant_print (value, TRUE);
  g_assert (tree_instance_check_gvariant (tree, value));

  g_variant_get_data (value);

  s2 = g_variant_print (value, TRUE);
  g_assert (tree_instance_check_gvariant (tree, value));

  g_assert_cmpstr (s1, ==, s2);

  if (g_variant_is_container (value))
    {
      GVariantBuilder builder;
      GVariantIter iter;
      GVariant *built;
      GVariant *val;
      gchar *s3;

      g_variant_builder_init (&builder, G_VARIANT_TYPE_VARIANT);
      tree_instance_build_gvariant (tree, &builder, TRUE);
      built = g_variant_builder_end (&builder);
      g_variant_ref_sink (built);
      g_variant_get_data (built);
      val = g_variant_get_variant (built);

      s3 = g_variant_print (val, TRUE);
      g_assert_cmpstr (s1, ==, s3);

      g_variant_iter_init (&iter, built);
      g_assert (tree_instance_check_iter (tree, &iter));
      g_assert (g_variant_iter_next_value (&iter) == NULL);

      g_variant_unref (built);
      g_variant_unref (val);
      g_free (s3);
    }

  tree_instance_free (tree);
  g_variant_unref (value);
  g_free (s2);
  g_free (s1);
}

static void
test_string (void)
{
  /* Test some different methods of creating strings */
  GVariant *v;

  v = g_variant_new_string ("foo");
  g_assert_cmpstr (g_variant_get_string (v, NULL), ==, "foo");
  g_variant_unref (v);


  v = g_variant_new_take_string (g_strdup ("foo"));
  g_assert_cmpstr (g_variant_get_string (v, NULL), ==, "foo");
  g_variant_unref (v);

  v = g_variant_new_printf ("%s %d", "foo", 123);
  g_assert_cmpstr (g_variant_get_string (v, NULL), ==, "foo 123");
  g_variant_unref (v);
}

static void
test_utf8 (void)
{
  const gchar invalid[] = "hello\xffworld";
  GVariant *value;

  /* ensure that the test data is not valid utf8... */
  g_assert (!g_utf8_validate (invalid, -1, NULL));

  /* load the data untrusted */
  value = g_variant_new_from_data (G_VARIANT_TYPE_STRING,
                                   invalid, sizeof invalid,
                                   FALSE, NULL, NULL);

  /* ensure that the problem is caught and we get valid UTF-8 */
  g_assert (g_utf8_validate (g_variant_get_string (value, NULL), -1, NULL));
  g_variant_unref (value);


  /* now load it trusted */
  value = g_variant_new_from_data (G_VARIANT_TYPE_STRING,
                                   invalid, sizeof invalid,
                                   TRUE, NULL, NULL);

  /* ensure we get the invalid data (ie: make sure that time wasn't
   * wasted on validating data that was marked as trusted)
   */
  g_assert (g_variant_get_string (value, NULL) == invalid);
  g_variant_unref (value);
}

static void
test_containers (void)
{
  gint i;

  for (i = 0; i < 100; i++)
    {
      test_container ();
    }

  g_variant_type_info_assert_no_infos ();
}

static void
test_format_strings (void)
{
  GVariantType *type;
  const gchar *end;

  g_assert (g_variant_format_string_scan ("i", NULL, &end) && *end == '\0');
  g_assert (g_variant_format_string_scan ("@i", NULL, &end) && *end == '\0');
  g_assert (g_variant_format_string_scan ("@ii", NULL, &end) && *end == 'i');
  g_assert (g_variant_format_string_scan ("^a&s", NULL, &end) && *end == '\0');
  g_assert (g_variant_format_string_scan ("(^as)", NULL, &end) &&
            *end == '\0');
  g_assert (!g_variant_format_string_scan ("(^s)", NULL, &end));
  g_assert (!g_variant_format_string_scan ("(^a)", NULL, &end));
  g_assert (!g_variant_format_string_scan ("(z)", NULL, &end));
  g_assert (!g_variant_format_string_scan ("az", NULL, &end));
  g_assert (!g_variant_format_string_scan ("{**}", NULL, &end));
  g_assert (!g_variant_format_string_scan ("{@**}", NULL, &end));
  g_assert (g_variant_format_string_scan ("{@y*}", NULL, &end) &&
            *end == '\0');
  g_assert (g_variant_format_string_scan ("{yv}", NULL, &end) &&
            *end == '\0');
  g_assert (!g_variant_format_string_scan ("{&?v}", NULL, &end));
  g_assert (g_variant_format_string_scan ("{@?v}", NULL, &end) &&
            *end == '\0');
  g_assert (!g_variant_format_string_scan ("{&@sv}", NULL, &end));
  g_assert (!g_variant_format_string_scan ("{@&sv}", NULL, &end));
  g_assert (g_variant_format_string_scan ("{&sv}", NULL, &end) &&
            *end == '\0');
  g_assert (!g_variant_format_string_scan ("{vv}", NULL, &end));
  g_assert (!g_variant_format_string_scan ("{y}", NULL, &end));
  g_assert (!g_variant_format_string_scan ("{yyy}", NULL, &end));
  g_assert (!g_variant_format_string_scan ("{ya}", NULL, &end));
  g_assert (g_variant_format_string_scan ("&s", NULL, &end) && *end == '\0');
  g_assert (!g_variant_format_string_scan ("&as", NULL, &end));
  g_assert (!g_variant_format_string_scan ("@z", NULL, &end));
  g_assert (!g_variant_format_string_scan ("az", NULL, &end));
  g_assert (!g_variant_format_string_scan ("a&s", NULL, &end));

  type = g_variant_format_string_scan_type ("mm(@xy^a&s*?@?)", NULL, &end);
  g_assert (type && *end == '\0');
  g_assert (g_variant_type_equal (type, G_VARIANT_TYPE ("mm(xyas*?\?)")));
  g_variant_type_free (type);

  type = g_variant_format_string_scan_type ("mm(@xy^a&*?@?)", NULL, NULL);
  g_assert (type == NULL);
}

static void
do_failed_test (const char *test,
                const gchar *pattern)
{
  g_test_trap_subprocess (test, 1000000, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr (pattern);
}

static void
test_invalid_varargs (void)
{
  GVariant *value;
  const gchar *end;

  if (!g_test_undefined ())
    return;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*GVariant format string*");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*valid_format_string*");
  value = g_variant_new ("z");
  g_test_assert_expected_messages ();
  g_assert (value == NULL);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*valid GVariant format string as a prefix*");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*valid_format_string*");
  value = g_variant_new_va ("z", &end, NULL);
  g_test_assert_expected_messages ();
  g_assert (value == NULL);

  value = g_variant_new ("y", 'a');
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*type of 'q' but * has a type of 'y'*");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*valid_format_string*");
  g_variant_get (value, "q");
  g_test_assert_expected_messages ();
  g_variant_unref (value);
}

static void
check_and_free (GVariant    *value,
                const gchar *str)
{
  gchar *valstr = g_variant_print (value, FALSE);
  g_assert_cmpstr (str, ==, valstr);
  g_variant_unref (value);
  g_free (valstr);
}

static void
test_varargs_empty_array (void)
{
  g_variant_new ("(a{s*})", NULL);

  g_assert_not_reached ();
}

static void
test_varargs (void)
{
  {
    GVariantBuilder array;

    g_variant_builder_init (&array, G_VARIANT_TYPE_ARRAY);
    g_variant_builder_add_parsed (&array, "{'size', <(%i, %i)> }", 800, 600);
    g_variant_builder_add (&array, "{sv}", "title",
                           g_variant_new_string ("Test case"));
    g_variant_builder_add_value (&array,
      g_variant_new_dict_entry (g_variant_new_string ("temperature"),
                                g_variant_new_variant (
                                  g_variant_new_double (37.5))));
    check_and_free (g_variant_new ("(ma{sv}m(a{sv})ma{sv}ii)",
                                   NULL, FALSE, NULL, &array, 7777, 8888),
                    "(nothing, nothing, {'size': <(800, 600)>, "
                                        "'title': <'Test case'>, "
                                        "'temperature': <37.5>}, "
                     "7777, 8888)");

    check_and_free (g_variant_new ("(imimimmimmimmi)",
                                   123,
                                   FALSE, 321,
                                   TRUE, 123,
                                   FALSE, TRUE, 321,
                                   TRUE, FALSE, 321,
                                   TRUE, TRUE, 123),
                    "(123, nothing, 123, nothing, just nothing, 123)");

    check_and_free (g_variant_new ("(ybnixd)",
                                   'a', 1, 22, 33, (guint64) 44, 5.5),
                    "(0x61, true, 22, 33, 44, 5.5)");

    check_and_free (g_variant_new ("(@y?*rv)",
                                   g_variant_new ("y", 'a'),
                                   g_variant_new ("y", 'b'),
                                   g_variant_new ("y", 'c'),
                                   g_variant_new ("(y)", 'd'),
                                   g_variant_new ("y", 'e')),
                    "(0x61, 0x62, 0x63, (0x64,), <byte 0x65>)");
  }

  {
    GVariantBuilder array;
    GVariantIter iter;
    GVariant *value;
    gchar *number;
    gboolean just;
    gint i, val;

    g_variant_builder_init (&array, G_VARIANT_TYPE_ARRAY);
    for (i = 0; i < 100; i++)
      {
        number = g_strdup_printf ("%d", i);
        g_variant_builder_add (&array, "s", number);
        g_free (number);
      }

    value = g_variant_builder_end (&array);
    g_variant_iter_init (&iter, value);

    i = 0;
    while (g_variant_iter_loop (&iter, "s", &number))
      {
        gchar *check = g_strdup_printf ("%d", i++);
        g_assert_cmpstr (number, ==, check);
        g_free (check);
      }
    g_assert (number == NULL);
    g_assert (i == 100);

    g_variant_unref (value);

    g_variant_builder_init (&array, G_VARIANT_TYPE_ARRAY);
    for (i = 0; i < 100; i++)
      g_variant_builder_add (&array, "mi", i % 2 == 0, i);
    value = g_variant_builder_end (&array);

    i = 0;
    g_variant_iter_init (&iter, value);
    while (g_variant_iter_loop (&iter, "mi", NULL, &val))
      g_assert (val == i++ || val == 0);
    g_assert (i == 100);

    i = 0;
    g_variant_iter_init (&iter, value);
    while (g_variant_iter_loop (&iter, "mi", &just, &val))
      {
        gint this = i++;

        if (this % 2 == 0)
          {
            g_assert (just);
            g_assert (val == this);
          }
        else
          {
            g_assert (!just);
            g_assert (val == 0);
          }
      }
    g_assert (i == 100);

    g_variant_unref (value);
  }

  {
    const gchar *strvector[] = {"/hello", "/world", NULL};
    const gchar *test_strs[] = {"/foo", "/bar", "/baz" };
    GVariantBuilder builder;
    GVariantIter *array;
    GVariantIter tuple;
    const gchar **strv;
    gchar **my_strv;
    GVariant *value;
    gchar *str;
    gint i;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&builder, "s", "/foo");
    g_variant_builder_add (&builder, "s", "/bar");
    g_variant_builder_add (&builder, "s", "/baz");
    value = g_variant_new("(as^as^a&s)", &builder, strvector, strvector);
    g_variant_iter_init (&tuple, value);
    g_variant_iter_next (&tuple, "as", &array);

    i = 0;
    while (g_variant_iter_loop (array, "s", &str))
      g_assert_cmpstr (str, ==, test_strs[i++]);
    g_assert (i == 3);

    g_variant_iter_free (array);

    /* start over */
    g_variant_iter_init (&tuple, value);
    g_variant_iter_next (&tuple, "as", &array);

    i = 0;
    while (g_variant_iter_loop (array, "&s", &str))
      g_assert_cmpstr (str, ==, test_strs[i++]);
    g_assert (i == 3);

    g_variant_iter_free (array);

    g_variant_iter_next (&tuple, "^a&s", &strv);
    g_variant_iter_next (&tuple, "^as", &my_strv);

    g_assert_cmpstr (strv[0], ==, "/hello");
    g_assert_cmpstr (strv[1], ==, "/world");
    g_assert (strv[2] == NULL);
    g_assert_cmpstr (my_strv[0], ==, "/hello");
    g_assert_cmpstr (my_strv[1], ==, "/world");
    g_assert (my_strv[2] == NULL);

    g_variant_unref (value);
    g_strfreev (my_strv);
    g_free (strv);
  }

  {
    const gchar *strvector[] = {"/hello", "/world", NULL};
    const gchar *test_strs[] = {"/foo", "/bar", "/baz" };
    GVariantBuilder builder;
    GVariantIter *array;
    GVariantIter tuple;
    const gchar **strv;
    gchar **my_strv;
    GVariant *value;
    gchar *str;
    gint i;

    g_variant_builder_init (&builder, G_VARIANT_TYPE_OBJECT_PATH_ARRAY);
    g_variant_builder_add (&builder, "o", "/foo");
    g_variant_builder_add (&builder, "o", "/bar");
    g_variant_builder_add (&builder, "o", "/baz");
    value = g_variant_new("(ao^ao^a&o)", &builder, strvector, strvector);
    g_variant_iter_init (&tuple, value);
    g_variant_iter_next (&tuple, "ao", &array);

    i = 0;
    while (g_variant_iter_loop (array, "o", &str))
      g_assert_cmpstr (str, ==, test_strs[i++]);
    g_assert (i == 3);

    g_variant_iter_free (array);

    /* start over */
    g_variant_iter_init (&tuple, value);
    g_variant_iter_next (&tuple, "ao", &array);

    i = 0;
    while (g_variant_iter_loop (array, "&o", &str))
      g_assert_cmpstr (str, ==, test_strs[i++]);
    g_assert (i == 3);

    g_variant_iter_free (array);

    g_variant_iter_next (&tuple, "^a&o", &strv);
    g_variant_iter_next (&tuple, "^ao", &my_strv);

    g_assert_cmpstr (strv[0], ==, "/hello");
    g_assert_cmpstr (strv[1], ==, "/world");
    g_assert (strv[2] == NULL);
    g_assert_cmpstr (my_strv[0], ==, "/hello");
    g_assert_cmpstr (my_strv[1], ==, "/world");
    g_assert (my_strv[2] == NULL);

    g_variant_unref (value);
    g_strfreev (my_strv);
    g_free (strv);
  }

  {
    const gchar *strvector[] = { "i", "ii", "iii", "iv", "v", "vi", NULL };
    GVariantBuilder builder;
    GVariantIter iter;
    GVariantIter *i2;
    GVariantIter *i3;
    GVariant *value;
    GVariant *sub;
    gchar **strv;
    gint i;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aas"));
    g_variant_builder_open (&builder, G_VARIANT_TYPE ("as"));
    for (i = 0; i < 6; i++)
      if (i & 1)
        g_variant_builder_add (&builder, "s", strvector[i]);
      else
        g_variant_builder_add (&builder, "&s", strvector[i]);
    g_variant_builder_close (&builder);
    g_variant_builder_add (&builder, "^as", strvector);
    g_variant_builder_add (&builder, "^as", strvector);
    value = g_variant_new ("aas", &builder);

    g_variant_iter_init (&iter, value);
    while (g_variant_iter_loop (&iter, "^as", &strv))
      for (i = 0; i < 6; i++)
        g_assert_cmpstr (strv[i], ==, strvector[i]);

    g_variant_iter_init (&iter, value);
    while (g_variant_iter_loop (&iter, "^a&s", &strv))
      for (i = 0; i < 6; i++)
        g_assert_cmpstr (strv[i], ==, strvector[i]);

    g_variant_iter_init (&iter, value);
    while (g_variant_iter_loop (&iter, "as", &i2))
      {
        gchar *str;

        i = 0;
        while (g_variant_iter_loop (i2, "s", &str))
          g_assert_cmpstr (str, ==, strvector[i++]);
        g_assert (i == 6);
      }

    g_variant_iter_init (&iter, value);
    i3 = g_variant_iter_copy (&iter);
    while (g_variant_iter_loop (&iter, "@as", &sub))
      {
        gchar *str = g_variant_print (sub, TRUE);
        g_assert_cmpstr (str, ==,
                         "['i', 'ii', 'iii', 'iv', 'v', 'vi']");
        g_free (str);
      }

    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                           "*NULL has already been returned*");
    g_variant_iter_next_value (&iter);
    g_test_assert_expected_messages ();

    while (g_variant_iter_loop (i3, "*", &sub))
      {
        gchar *str = g_variant_print (sub, TRUE);
        g_assert_cmpstr (str, ==,
                         "['i', 'ii', 'iii', 'iv', 'v', 'vi']");
        g_free (str);
      }

    g_variant_iter_free (i3);

    for (i = 0; i < g_variant_n_children (value); i++)
      {
        gint j;

        g_variant_get_child (value, i, "*", &sub);

        for (j = 0; j < g_variant_n_children (sub); j++)
          {
            const gchar *str = NULL;
            GVariant *cval;

            g_variant_get_child (sub, j, "&s", &str);
            g_assert_cmpstr (str, ==, strvector[j]);

            cval = g_variant_get_child_value (sub, j);
            g_variant_get (cval, "&s", &str);
            g_assert_cmpstr (str, ==, strvector[j]);
            g_variant_unref (cval);
          }

        g_variant_unref (sub);
      }

    g_variant_unref (value);
  }

  {
    gboolean justs[10];
    GVariant *value;

    GVariant *vval;
    guchar byteval;
    gboolean bval;
    gint16 i16val;
    guint16 u16val;
    gint32 i32val;
    guint32 u32val;
    gint64 i64val;
    guint64 u64val;
    gdouble dval;
    gint32 hval;

    /* test all 'nothing' */
    value = g_variant_new ("(mymbmnmqmimumxmtmhmdmv)",
                           FALSE, 'a',
                           FALSE, TRUE,
                           FALSE, (gint16) 123,
                           FALSE, (guint16) 123,
                           FALSE, (gint32) 123,
                           FALSE, (guint32) 123,
                           FALSE, (gint64) 123,
                           FALSE, (guint64) 123,
                           FALSE, (gint32) -1,
                           FALSE, (gdouble) 37.5,
                           NULL);

    /* both NULL */
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL);

    /* NULL values */
    memset (justs, 1, sizeof justs);
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   &justs[0], NULL,
                   &justs[1], NULL,
                   &justs[2], NULL,
                   &justs[3], NULL,
                   &justs[4], NULL,
                   &justs[5], NULL,
                   &justs[6], NULL,
                   &justs[7], NULL,
                   &justs[8], NULL,
                   &justs[9], NULL,
                   NULL);
    g_assert (!(justs[0] || justs[1] || justs[2] || justs[3] || justs[4] ||
                justs[5] || justs[6] || justs[7] || justs[8] || justs[9]));

    /* both non-NULL */
    memset (justs, 1, sizeof justs);
    byteval = i16val = u16val = i32val = u32val = i64val = u64val = hval = 88;
    vval = (void *) 1;
    bval = TRUE;
    dval = 88.88;
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   &justs[0], &byteval,
                   &justs[1], &bval,
                   &justs[2], &i16val,
                   &justs[3], &u16val,
                   &justs[4], &i32val,
                   &justs[5], &u32val,
                   &justs[6], &i64val,
                   &justs[7], &u64val,
                   &justs[8], &hval,
                   &justs[9], &dval,
                   &vval);
    g_assert (!(justs[0] || justs[1] || justs[2] || justs[3] || justs[4] ||
                justs[5] || justs[6] || justs[7] || justs[8] || justs[9]));
    g_assert (byteval == '\0' && bval == FALSE);
    g_assert (i16val == 0 && u16val == 0 && i32val == 0 &&
              u32val == 0 && i64val == 0 && u64val == 0 &&
              hval == 0 && dval == 0.0);
    g_assert (vval == NULL);

    /* NULL justs */
    byteval = i16val = u16val = i32val = u32val = i64val = u64val = hval = 88;
    vval = (void *) 1;
    bval = TRUE;
    dval = 88.88;
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   NULL, &byteval,
                   NULL, &bval,
                   NULL, &i16val,
                   NULL, &u16val,
                   NULL, &i32val,
                   NULL, &u32val,
                   NULL, &i64val,
                   NULL, &u64val,
                   NULL, &hval,
                   NULL, &dval,
                   &vval);
    g_assert (byteval == '\0' && bval == FALSE);
    g_assert (i16val == 0 && u16val == 0 && i32val == 0 &&
              u32val == 0 && i64val == 0 && u64val == 0 &&
              hval == 0 && dval == 0.0);
    g_assert (vval == NULL);

    g_variant_unref (value);


    /* test all 'just' */
    value = g_variant_new ("(mymbmnmqmimumxmtmhmdmv)",
                           TRUE, 'a',
                           TRUE, TRUE,
                           TRUE, (gint16) 123,
                           TRUE, (guint16) 123,
                           TRUE, (gint32) 123,
                           TRUE, (guint32) 123,
                           TRUE, (gint64) 123,
                           TRUE, (guint64) 123,
                           TRUE, (gint32) -1,
                           TRUE, (gdouble) 37.5,
                           g_variant_new ("()"));

    /* both NULL */
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL, NULL,
                   NULL);

    /* NULL values */
    memset (justs, 0, sizeof justs);
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   &justs[0], NULL,
                   &justs[1], NULL,
                   &justs[2], NULL,
                   &justs[3], NULL,
                   &justs[4], NULL,
                   &justs[5], NULL,
                   &justs[6], NULL,
                   &justs[7], NULL,
                   &justs[8], NULL,
                   &justs[9], NULL,
                   NULL);
    g_assert (justs[0] && justs[1] && justs[2] && justs[3] && justs[4] &&
              justs[5] && justs[6] && justs[7] && justs[8] && justs[9]);

    /* both non-NULL */
    memset (justs, 0, sizeof justs);
    byteval = i16val = u16val = i32val = u32val = i64val = u64val = hval = 88;
    vval = (void *) 1;
    bval = FALSE;
    dval = 88.88;
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   &justs[0], &byteval,
                   &justs[1], &bval,
                   &justs[2], &i16val,
                   &justs[3], &u16val,
                   &justs[4], &i32val,
                   &justs[5], &u32val,
                   &justs[6], &i64val,
                   &justs[7], &u64val,
                   &justs[8], &hval,
                   &justs[9], &dval,
                   &vval);
    g_assert (justs[0] && justs[1] && justs[2] && justs[3] && justs[4] &&
              justs[5] && justs[6] && justs[7] && justs[8] && justs[9]);
    g_assert (byteval == 'a' && bval == TRUE);
    g_assert (i16val == 123 && u16val == 123 && i32val == 123 &&
              u32val == 123 && i64val == 123 && u64val == 123 &&
              hval == -1 && dval == 37.5);
    g_assert (g_variant_is_of_type (vval, G_VARIANT_TYPE_UNIT));
    g_variant_unref (vval);

    /* NULL justs */
    byteval = i16val = u16val = i32val = u32val = i64val = u64val = hval = 88;
    vval = (void *) 1;
    bval = TRUE;
    dval = 88.88;
    g_variant_get (value, "(mymbmnmqmimumxmtmhmdmv)",
                   NULL, &byteval,
                   NULL, &bval,
                   NULL, &i16val,
                   NULL, &u16val,
                   NULL, &i32val,
                   NULL, &u32val,
                   NULL, &i64val,
                   NULL, &u64val,
                   NULL, &hval,
                   NULL, &dval,
                   &vval);
    g_assert (byteval == 'a' && bval == TRUE);
    g_assert (i16val == 123 && u16val == 123 && i32val == 123 &&
              u32val == 123 && i64val == 123 && u64val == 123 &&
              hval == -1 && dval == 37.5);
    g_assert (g_variant_is_of_type (vval, G_VARIANT_TYPE_UNIT));
    g_variant_unref (vval);

    g_variant_unref (value);
  }

  {
    GVariant *value;
    gchar *str;

    value = g_variant_new ("(masas)", NULL, NULL);
    g_variant_ref_sink (value);

    str = g_variant_print (value, TRUE);
    g_assert_cmpstr (str, ==, "(@mas nothing, @as [])");
    g_variant_unref (value);
    g_free (str);

    do_failed_test ("/gvariant/varargs/subprocess/empty-array",
                    "*which type of empty array*");
  }

  g_variant_type_info_assert_no_infos ();
}

static void
hash_get (GVariant    *value,
          const gchar *format,
          ...)
{
  const gchar *endptr = NULL;
  gboolean hash;
  va_list ap;

  hash = g_str_has_suffix (format, "#");

  va_start (ap, format);
  g_variant_get_va (value, format, hash ? &endptr : NULL, &ap);
  va_end (ap);

  if (hash)
    g_assert (*endptr == '#');
}

static GVariant *
hash_new (const gchar *format,
          ...)
{
  const gchar *endptr = NULL;
  GVariant *value;
  gboolean hash;
  va_list ap;

  hash = g_str_has_suffix (format, "#");

  va_start (ap, format);
  value = g_variant_new_va (format, hash ? &endptr : NULL, &ap);
  va_end (ap);

  if (hash)
    g_assert (*endptr == '#');

  return value;
}

static void
test_valist (void)
{
  GVariant *value;
  gint32 x;

  x = 0;
  value = hash_new ("i", 234);
  hash_get (value, "i", &x);
  g_assert (x == 234);
  g_variant_unref (value);

  x = 0;
  value = hash_new ("i#", 234);
  hash_get (value, "i#", &x);
  g_assert (x == 234);
  g_variant_unref (value);

  g_variant_type_info_assert_no_infos ();
}

static void
test_builder_memory (void)
{
  GVariantBuilder *hb;
  GVariantBuilder sb;

  hb = g_variant_builder_new  (G_VARIANT_TYPE_ARRAY);
  g_variant_builder_open (hb, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_open (hb, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_open (hb, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add (hb, "s", "some value");
  g_variant_builder_ref (hb);
  g_variant_builder_unref (hb);
  g_variant_builder_unref (hb);

  hb = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  g_variant_builder_unref (hb);

  hb = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  g_variant_builder_clear (hb);
  g_variant_builder_unref (hb);

  g_variant_builder_init (&sb, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_open (&sb, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_open (&sb, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add (&sb, "s", "some value");
  g_variant_builder_clear (&sb);

  g_variant_type_info_assert_no_infos ();
}

static void
test_hashing (void)
{
  GVariant *items[4096];
  GHashTable *table;
  gint i;

  table = g_hash_table_new_full (g_variant_hash, g_variant_equal,
                                 (GDestroyNotify ) g_variant_unref,
                                 NULL);

  for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
      TreeInstance *tree;
      gint j;

 again:
      tree = tree_instance_new (NULL, 0);
      items[i] = tree_instance_get_gvariant (tree);
      tree_instance_free (tree);

      for (j = 0; j < i; j++)
        if (g_variant_equal (items[i], items[j]))
          {
            g_variant_unref (items[i]);
            goto again;
          }

      g_hash_table_insert (table,
                           g_variant_ref_sink (items[i]),
                           GINT_TO_POINTER (i));
    }

  for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
      gpointer result;

      result = g_hash_table_lookup (table, items[i]);
      g_assert_cmpint (GPOINTER_TO_INT (result), ==, i);
    }

  g_hash_table_unref (table);

  g_variant_type_info_assert_no_infos ();
}

static void
test_gv_byteswap (void)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
# define native16(x)  x, 0
# define swapped16(x) 0, x
#else
# define native16(x)  0, x
# define swapped16(x) x, 0
#endif
  /* all kinds of of crazy randomised testing already performed on the
   * byteswapper in the /gvariant/serialiser/byteswap test and all kinds
   * of crazy randomised testing performed against the serialiser
   * normalisation functions in the /gvariant/serialiser/fuzz/ tests.
   *
   * just test a few simple cases here to make sure they each work
   */
  guchar validbytes[] = { 'a', '\0', swapped16(66), 2,
                          0,
                          'b', '\0', swapped16(77), 2,
                          5, 11 };
  guchar corruptbytes[] = { 'a', '\0', swapped16(66), 2,
                            0,
                            'b', '\0', swapped16(77), 2,
                            6, 11 };
  guint valid_data[4], corrupt_data[4];
  GVariant *value, *swapped;
  gchar *string, *string2;

  memcpy (valid_data, validbytes, sizeof validbytes);
  memcpy (corrupt_data, corruptbytes, sizeof corruptbytes);

  /* trusted */
  value = g_variant_new_from_data (G_VARIANT_TYPE ("a(sn)"),
                                   valid_data, sizeof validbytes, TRUE,
                                   NULL, NULL);
  swapped = g_variant_byteswap (value);
  g_variant_unref (value);
  g_assert (g_variant_get_size (swapped) == 13);
  string = g_variant_print (swapped, FALSE);
  g_variant_unref (swapped);
  g_assert_cmpstr (string, ==, "[('a', 66), ('b', 77)]");
  g_free (string);

  /* untrusted but valid */
  value = g_variant_new_from_data (G_VARIANT_TYPE ("a(sn)"),
                                   valid_data, sizeof validbytes, FALSE,
                                   NULL, NULL);
  swapped = g_variant_byteswap (value);
  g_variant_unref (value);
  g_assert (g_variant_get_size (swapped) == 13);
  string = g_variant_print (swapped, FALSE);
  g_variant_unref (swapped);
  g_assert_cmpstr (string, ==, "[('a', 66), ('b', 77)]");
  g_free (string);

  /* untrusted, invalid */
  value = g_variant_new_from_data (G_VARIANT_TYPE ("a(sn)"),
                                   corrupt_data, sizeof corruptbytes, FALSE,
                                   NULL, NULL);
  string = g_variant_print (value, FALSE);
  swapped = g_variant_byteswap (value);
  g_variant_unref (value);
  g_assert (g_variant_get_size (swapped) == 13);
  value = g_variant_byteswap (swapped);
  g_variant_unref (swapped);
  string2 = g_variant_print (value, FALSE);
  g_assert (g_variant_get_size (value) == 13);
  g_variant_unref (value);
  g_assert_cmpstr (string, ==, string2);
  g_free (string2);
  g_free (string);
}

static void
test_parser (void)
{
  TreeInstance *tree;
  GVariant *parsed;
  GVariant *value;
  gchar *pt, *p;
  gchar *res;

  tree = tree_instance_new (NULL, 3);
  value = tree_instance_get_gvariant (tree);
  tree_instance_free (tree);

  pt = g_variant_print (value, TRUE);
  p = g_variant_print (value, FALSE);

  parsed = g_variant_parse (NULL, pt, NULL, NULL, NULL);
  res = g_variant_print (parsed, FALSE);
  g_assert_cmpstr (p, ==, res);
  g_variant_unref (parsed);
  g_free (res);

  parsed = g_variant_parse (g_variant_get_type (value), p,
                            NULL, NULL, NULL);
  res = g_variant_print (parsed, TRUE);
  g_assert_cmpstr (pt, ==, res);
  g_variant_unref (parsed);
  g_free (res);

  g_variant_unref (value);
  g_free (pt);
  g_free (p);
}

static void
test_parses (void)
{
  gint i;

  for (i = 0; i < 100; i++)
    {
      test_parser ();
    }

  /* mini test */
  {
    GError *error = NULL;
    gchar str[128];
    GVariant *val;
    gchar *p, *p2;

    for (i = 0; i < 127; i++)
      str[i] = i + 1;
    str[i] = 0;

    val = g_variant_new_string (str);
    p = g_variant_print (val, FALSE);
    g_variant_unref (val);

    val = g_variant_parse (NULL, p, NULL, NULL, &error);
    p2 = g_variant_print (val, FALSE);

    g_assert_cmpstr (str, ==, g_variant_get_string (val, NULL));
    g_assert_cmpstr (p, ==, p2);

    g_variant_unref (val);
    g_free (p2);
    g_free (p);
  }

  /* another mini test */
  {
    const gchar *end;
    GVariant *value;

    value = g_variant_parse (G_VARIANT_TYPE_INT32, "1 2 3", NULL, &end, NULL);
    g_assert_cmpint (g_variant_get_int32 (value), ==, 1);
    /* make sure endptr returning works */
    g_assert_cmpstr (end, ==, " 2 3");
    g_variant_unref (value);
  }

  /* unicode mini test */
  {
    /* ał𝄞 */
    const gchar orig[] = "a\xc5\x82\xf0\x9d\x84\x9e \t\n";
    GVariant *value;
    gchar *printed;

    value = g_variant_new_string (orig);
    printed = g_variant_print (value, FALSE);
    g_variant_unref (value);

    g_assert_cmpstr (printed, ==, "'a\xc5\x82\xf0\x9d\x84\x9e \\t\\n'");
    value = g_variant_parse (NULL, printed, NULL, NULL, NULL);
    g_assert_cmpstr (g_variant_get_string (value, NULL), ==, orig);
    g_variant_unref (value);
    g_free (printed);
  }

  /* escapes */
  {
    const gchar orig[] = " \342\200\254 \360\220\210\240 \a \b \f \n \r \t \v ";
    GVariant *value;
    gchar *printed;

    value = g_variant_new_string (orig);
    printed = g_variant_print (value, FALSE);
    g_variant_unref (value);

    g_assert_cmpstr (printed, ==, "' \\u202c \\U00010220 \\a \\b \\f \\n \\r \\t \\v '");
    value = g_variant_parse (NULL, printed, NULL, NULL, NULL);
    g_assert_cmpstr (g_variant_get_string (value, NULL), ==, orig);
    g_variant_unref (value);
    g_free (printed);
  }

#ifndef _MSC_VER
  /* inf/nan strings are C99 features which Visual C++ does not support */
  /* inf/nan mini test */
  {
    const gchar *tests[] = { "inf", "-inf", "nan" };
    GVariant *value;
    gchar *printed;
    gchar *printed_down;
    gint i;

    for (i = 0; i < G_N_ELEMENTS (tests); i++)
      {
        GError *error = NULL;
        value = g_variant_parse (NULL, tests[i], NULL, NULL, &error);
        printed = g_variant_print (value, FALSE);
        /* Canonicalize to lowercase; https://bugzilla.gnome.org/show_bug.cgi?id=704585 */
        printed_down = g_ascii_strdown (printed, -1);
        g_assert (g_str_has_prefix (printed_down, tests[i]));
        g_free (printed);
        g_free (printed_down);
        g_variant_unref (value);
      }
  }
#endif

  g_variant_type_info_assert_no_infos ();
}

static void
test_parse_failures (void)
{
  const gchar *test[] = {
    "[1, 2,",                   "6:",              "expected value",
    "",                         "0:",              "expected value",
    "(1, 2,",                   "6:",              "expected value",
    "<1",                       "2:",              "expected '>'",
    "[]",                       "0-2:",            "unable to infer",
    "(,",                       "1:",              "expected value",
    "[4,'']",                   "1-2,3-5:",        "common type",
    "[4, '', 5]",               "1-2,4-6:",        "common type",
    "['', 4, 5]",               "1-3,5-6:",        "common type",
    "[4, 5, '']",               "1-2,7-9:",        "common type",
    "[[4], [], ['']]",          "1-4,10-14:",      "common type",
    "[[], [4], ['']]",          "5-8,10-14:",      "common type",
    "just",                     "4:",              "expected value",
    "nothing",                  "0-7:",            "unable to infer",
    "just [4, '']",             "6-7,9-11:",       "common type",
    "[[4,'']]",                 "2-3,4-6:",        "common type",
    "([4,''],)",                "2-3,4-6:",        "common type",
    "(4)",                      "2:",              "','",
    "{}",                       "0-2:",            "unable to infer",
    "{[1,2],[3,4]}",            "0-13:",           "basic types",
    "{[1,2]:[3,4]}",            "0-13:",           "basic types",
    "justt",                    "0-5:",            "unknown keyword",
    "nothng",                   "0-6:",            "unknown keyword",
    "uint33",                   "0-6:",            "unknown keyword",
    "@mi just ''",              "9-11:",           "can not parse as",
    "@ai ['']",                 "5-7:",            "can not parse as",
    "@(i) ('',)",               "6-8:",            "can not parse as",
    "[[], 5]",                  "1-3,5-6:",        "common type",
    "[[5], 5]",                 "1-4,6-7:",        "common type",
    "5 5",                      "2:",              "expected end of input",
    "[5, [5, '']]",             "5-6,8-10:",       "common type",
    "@i just 5",                "3-9:",            "can not parse as",
    "@i nothing",               "3-10:",           "can not parse as",
    "@i []",                    "3-5:",            "can not parse as",
    "@i ()",                    "3-5:",            "can not parse as",
    "@ai (4,)",                 "4-8:",            "can not parse as",
    "@(i) []",                  "5-7:",            "can not parse as",
    "(5 5)",                    "3:",              "expected ','",
    "[5 5]",                    "3:",              "expected ',' or ']'",
    "(5, 5 5)",                 "6:",              "expected ',' or ')'",
    "[5, 5 5]",                 "6:",              "expected ',' or ']'",
    "<@i []>",                  "4-6:",            "can not parse as",
    "<[5 5]>",                  "4:",              "expected ',' or ']'",
    "{[4,''],5}",               "2-3,4-6:",        "common type",
    "{5,[4,'']}",               "4-5,6-8:",        "common type",
    "@i {1,2}",                 "3-8:",            "can not parse as",
    "{@i '', 5}",               "4-6:",            "can not parse as",
    "{5, @i ''}",               "7-9:",            "can not parse as",
    "@ai {}",                   "4-6:",            "can not parse as",
    "{@i '': 5}",               "4-6:",            "can not parse as",
    "{5: @i ''}",               "7-9:",            "can not parse as",
    "{<4,5}",                   "3:",              "expected '>'",
    "{4,<5}",                   "5:",              "expected '>'",
    "{4,5,6}",                  "4:",              "expected '}'",
    "{5 5}",                    "3:",              "expected ':' or ','",
    "{4: 5: 6}",                "5:",              "expected ',' or '}'",
    "{4:5,<6:7}",               "7:",              "expected '>'",
    "{4:5,6:<7}",               "9:",              "expected '>'",
    "{4:5,6 7}",                "7:",              "expected ':'",
    "@o 'foo'",                 "3-8:",            "object path",
    "@g 'zzz'",                 "3-8:",            "signature",
    "@i true",                  "3-7:",            "can not parse as",
    "@z 4",                     "0-2:",            "invalid type",
    "@a* []",                   "0-3:",            "definite",
    "@ai [3 3]",                "7:",              "expected ',' or ']'",
    "18446744073709551616",     "0-20:",           "too big for any type",
    "-18446744073709551616",    "0-21:",           "too big for any type",
    "byte 256",                 "5-8:",            "out of range for type",
    "byte -1",                  "5-7:",            "out of range for type",
    "int16 32768",              "6-11:",           "out of range for type",
    "int16 -32769",             "6-12:",           "out of range for type",
    "uint16 -1",                "7-9:",            "out of range for type",
    "uint16 65536",             "7-12:",           "out of range for type",
    "2147483648",               "0-10:",           "out of range for type",
    "-2147483649",              "0-11:",           "out of range for type",
    "uint32 -1",                "7-9:",            "out of range for type",
    "uint32 4294967296",        "7-17:",           "out of range for type",
    "@x 9223372036854775808",   "3-22:",           "out of range for type",
    "@x -9223372036854775809",  "3-23:",           "out of range for type",
    "@t -1",                    "3-5:",            "out of range for type",
    "@t 18446744073709551616",  "3-23:",           "too big for any type",
    "handle 2147483648",        "7-17:",           "out of range for type",
    "handle -2147483649",       "7-18:",           "out of range for type",
    "1.798e308",                "0-9:",            "too big for any type",
    "37.5a488",                 "4-5:",            "invalid character",
    "0x7ffgf",                  "5-6:",            "invalid character",
    "07758",                    "4-5:",            "invalid character",
    "123a5",                    "3-4:",            "invalid character",
    "@ai 123",                  "4-7:",            "can not parse as",
    "'\"\\'",                   "0-4:",            "unterminated string",
    "'\"\\'\\",                 "0-5:",            "unterminated string",
    "boolean 4",                "8-9:",            "can not parse as",
    "int32 true",               "6-10:",           "can not parse as",
    "[double 5, int32 5]",      "1-9,11-18:",      "common type",
    "string 4",                 "7-8:",            "can not parse as"
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (test); i += 3)
    {
      GError *error = NULL;
      GVariant *value;

      value = g_variant_parse (NULL, test[i], NULL, NULL, &error);
      g_assert (value == NULL);

      if (!strstr (error->message, test[i+2]))
        g_error ("test %d: Can't find '%s' in '%s'", i / 3,
                 test[i+2], error->message);

      if (!g_str_has_prefix (error->message, test[i+1]))
        g_error ("test %d: Expected location '%s' in '%s'", i / 3,
                 test[i+1], error->message);

      g_error_free (error);
    }
}

static void
test_parse_bad_format_char (void)
{
  g_variant_new_parsed ("%z");

  g_assert_not_reached ();
}

static void
test_parse_bad_format_string (void)
{
  g_variant_new_parsed ("uint32 %i", 2);

  g_assert_not_reached ();
}

static void
test_parse_bad_args (void)
{
  g_variant_new_parsed ("%@i", g_variant_new_uint32 (2));

  g_assert_not_reached ();
}

static void
test_parse_positional (void)
{
  GVariant *value;
  check_and_free (g_variant_new_parsed ("[('one', 1), (%s, 2),"
                                        " ('three', %i)]", "two", 3),
                  "[('one', 1), ('two', 2), ('three', 3)]");
  value = g_variant_new_parsed ("[('one', 1), (%s, 2),"
                                " ('three', %u)]", "two", 3);
  g_assert (g_variant_is_of_type (value, G_VARIANT_TYPE ("a(su)")));
  check_and_free (value, "[('one', 1), ('two', 2), ('three', 3)]");
  check_and_free (g_variant_new_parsed ("{%s:%i}", "one", 1), "{'one': 1}");

  if (g_test_undefined ())
    {
      do_failed_test ("/gvariant/parse/subprocess/bad-format-char",
                      "*GVariant format string*");

      do_failed_test ("/gvariant/parse/subprocess/bad-format-string",
                      "*can not parse as*");

      do_failed_test ("/gvariant/parse/subprocess/bad-args",
                      "*expected GVariant of type 'i'*");
    }
}

static void
test_floating (void)
{
  GVariant *value;

  value = g_variant_new_int32 (42);
  g_assert (g_variant_is_floating (value));
  g_variant_ref_sink (value);
  g_assert (!g_variant_is_floating (value));
  g_variant_unref (value);
}

static void
test_bytestring (void)
{
  const gchar *test_string = "foo,bar,baz,quux,\xffoooo";
  GVariant *value;
  gchar **strv;
  gchar *str;
  const gchar *const_str;
  GVariant *untrusted_empty;

  strv = g_strsplit (test_string, ",", 0);

  value = g_variant_new_bytestring_array ((const gchar **) strv, -1);
  g_assert (g_variant_is_floating (value));
  g_strfreev (strv);

  str = g_variant_print (value, FALSE);
  g_variant_unref (value);

  value = g_variant_parse (NULL, str, NULL, NULL, NULL);
  g_free (str);

  strv = g_variant_dup_bytestring_array (value, NULL);
  g_variant_unref (value);

  str = g_strjoinv (",", strv);
  g_strfreev (strv);

  g_assert_cmpstr (str, ==, test_string);
  g_free (str);

  strv = g_strsplit (test_string, ",", 0);
  value = g_variant_new ("(^aay^a&ay^ay^&ay)",
                         strv, strv, strv[0], strv[0]);
  g_strfreev (strv);

  g_variant_get_child (value, 0, "^a&ay", &strv);
  str = g_strjoinv (",", strv);
  g_free (strv);
  g_assert_cmpstr (str, ==, test_string);
  g_free (str);

  g_variant_get_child (value, 0, "^aay", &strv);
  str = g_strjoinv (",", strv);
  g_strfreev (strv);
  g_assert_cmpstr (str, ==, test_string);
  g_free (str);

  g_variant_get_child (value, 1, "^a&ay", &strv);
  str = g_strjoinv (",", strv);
  g_free (strv);
  g_assert_cmpstr (str, ==, test_string);
  g_free (str);

  g_variant_get_child (value, 1, "^aay", &strv);
  str = g_strjoinv (",", strv);
  g_strfreev (strv);
  g_assert_cmpstr (str, ==, test_string);
  g_free (str);

  g_variant_get_child (value, 2, "^ay", &str);
  g_assert_cmpstr (str, ==, "foo");
  g_free (str);

  g_variant_get_child (value, 2, "^&ay", &str);
  g_assert_cmpstr (str, ==, "foo");

  g_variant_get_child (value, 3, "^ay", &str);
  g_assert_cmpstr (str, ==, "foo");
  g_free (str);

  g_variant_get_child (value, 3, "^&ay", &str);
  g_assert_cmpstr (str, ==, "foo");
  g_variant_unref (value);

  untrusted_empty = g_variant_new_from_data (G_VARIANT_TYPE ("ay"), NULL, 0, FALSE, NULL, NULL);
  value = g_variant_get_normal_form (untrusted_empty);
  const_str = g_variant_get_bytestring (value);
  (void) const_str;
  g_variant_unref (value);
  g_variant_unref (untrusted_empty);
}

static void
test_lookup_value (void)
{
  struct {
    const gchar *dict, *key, *value;
  } cases[] = {
    { "@a{ss} {'x':  'y'}",   "x",  "'y'" },
    { "@a{ss} {'x':  'y'}",   "y"         },
    { "@a{os} {'/x': 'y'}",   "/x", "'y'" },
    { "@a{os} {'/x': 'y'}",   "/y"        },
    { "@a{sv} {'x':  <'y'>}", "x",  "'y'" },
    { "@a{sv} {'x':  <5>}",   "x",  "5"   },
    { "@a{sv} {'x':  <'y'>}", "y"         }
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      GVariant *dictionary;
      GVariant *value;
      gchar *p;
      
      dictionary = g_variant_parse (NULL, cases[i].dict, NULL, NULL, NULL);
      value = g_variant_lookup_value (dictionary, cases[i].key, NULL);
      g_variant_unref (dictionary);

      if (value == NULL && cases[i].value == NULL)
        continue;

      g_assert (value && cases[i].value);
      p = g_variant_print (value, FALSE);
      g_assert_cmpstr (cases[i].value, ==, p);
      g_variant_unref (value);
      g_free (p);
    }
}

static void
test_lookup (void)
{
  const gchar *str;
  GVariant *dict;
  gboolean ok;
  gint num;

  dict = g_variant_parse (NULL,
                          "{'a': <5>, 'b': <'c'>}",
                          NULL, NULL, NULL);

  ok = g_variant_lookup (dict, "a", "i", &num);
  g_assert (ok);
  g_assert_cmpint (num, ==, 5);

  ok = g_variant_lookup (dict, "a", "&s", &str);
  g_assert (!ok);

  ok = g_variant_lookup (dict, "q", "&s", &str);
  g_assert (!ok);

  ok = g_variant_lookup (dict, "b", "i", &num);
  g_assert (!ok);

  ok = g_variant_lookup (dict, "b", "&s", &str);
  g_assert (ok);
  g_assert_cmpstr (str, ==, "c");

  ok = g_variant_lookup (dict, "q", "&s", &str);
  g_assert (!ok);

  g_variant_unref (dict);
}

static GVariant *
untrusted (GVariant *a)
{
  GVariant *b;
  const GVariantType *type;
  GBytes *bytes;

  type = g_variant_get_type (a);
  bytes = g_variant_get_data_as_bytes (a);
  b = g_variant_new_from_bytes (type, bytes, FALSE);
  g_bytes_unref (bytes);
  g_variant_unref (a);

  return b;
}

static void
test_compare (void)
{
  GVariant *a;
  GVariant *b;

  a = untrusted (g_variant_new_byte (5));
  b = g_variant_new_byte (6);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_int16 (G_MININT16));
  b = g_variant_new_int16 (G_MAXINT16);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_uint16 (0));
  b = g_variant_new_uint16 (G_MAXUINT16);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_int32 (G_MININT32));
  b = g_variant_new_int32 (G_MAXINT32);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_uint32 (0));
  b = g_variant_new_uint32 (G_MAXUINT32);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_int64 (G_MININT64));
  b = g_variant_new_int64 (G_MAXINT64);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_uint64 (0));
  b = g_variant_new_uint64 (G_MAXUINT64);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_double (G_MINDOUBLE));
  b = g_variant_new_double (G_MAXDOUBLE);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_string ("abc"));
  b = g_variant_new_string ("abd");
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_object_path ("/abc"));
  b = g_variant_new_object_path ("/abd");
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_signature ("g"));
  b = g_variant_new_signature ("o");
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_boolean (FALSE));
  b = g_variant_new_boolean (TRUE);
  g_assert (g_variant_compare (a, b) < 0);
  g_variant_unref (a);
  g_variant_unref (b);
}

static void
test_equal (void)
{
  GVariant *a;
  GVariant *b;

  a = untrusted (g_variant_new_byte (5));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_int16 (G_MININT16));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_uint16 (0));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_int32 (G_MININT32));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_uint32 (0));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_int64 (G_MININT64));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_uint64 (0));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_double (G_MINDOUBLE));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_string ("abc"));
  g_assert (g_variant_equal (a, a));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_object_path ("/abc"));
  g_assert (g_variant_equal (a, a));
  b = g_variant_get_normal_form (a);
  a = untrusted (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_signature ("g"));
  g_assert (g_variant_equal (a, a));
  b = g_variant_get_normal_form (a);
  a = untrusted (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
  a = untrusted (g_variant_new_boolean (FALSE));
  b = g_variant_get_normal_form (a);
  g_assert (g_variant_equal (a, b));
  g_variant_unref (a);
  g_variant_unref (b);
}

static void
test_fixed_array (void)
{
  GVariant *a;
  gint32 values[5];
  const gint32 *elts;
  gsize n_elts;
  gint i;

  n_elts = 0;
  a = g_variant_new_parsed ("[1,2,3,4,5]");
  elts = g_variant_get_fixed_array (a, &n_elts, sizeof (gint32));
  g_assert (n_elts == 5);
  for (i = 0; i < 5; i++)
    g_assert_cmpint (elts[i], ==, i + 1);
  g_variant_unref (a);

  n_elts = 0;
  for (i = 0; i < 5; i++)
    values[i] = i + 1;
  a = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32, values,
                                 G_N_ELEMENTS (values), sizeof (values[0]));
  g_assert_cmpstr (g_variant_get_type_string (a), ==, "ai");
  elts = g_variant_get_fixed_array (a, &n_elts, sizeof (gint32));
  g_assert (n_elts == 5);
  for (i = 0; i < 5; i++)
    g_assert_cmpint (elts[i], ==, i + 1);
  g_variant_unref (a);
}

static void
test_check_format_string (void)
{
  GVariant *value;

  value = g_variant_new ("(sas)", "foo", NULL);
  g_variant_ref_sink (value);

  g_assert (g_variant_check_format_string (value, "(s*)", TRUE));
  g_assert (g_variant_check_format_string (value, "(s*)", FALSE));
  g_assert (!g_variant_check_format_string (value, "(u*)", TRUE));
  g_assert (!g_variant_check_format_string (value, "(u*)", FALSE));

  g_assert (g_variant_check_format_string (value, "(&s*)", FALSE));
  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL, "*contains a '&' character*");
  g_assert (!g_variant_check_format_string (value, "(&s*)", TRUE));
  g_test_assert_expected_messages ();

  g_assert (g_variant_check_format_string (value, "(s^as)", TRUE));
  g_assert (g_variant_check_format_string (value, "(s^as)", FALSE));

  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL, "*contains a '&' character*");
  g_assert (!g_variant_check_format_string (value, "(s^a&s)", TRUE));
  g_test_assert_expected_messages ();
  g_assert (g_variant_check_format_string (value, "(s^a&s)", FALSE));

  g_variant_unref (value);

  /* Do it again with a type that will let us put a '&' after a '^' */
  value = g_variant_new ("(say)", "foo", NULL);
  g_variant_ref_sink (value);

  g_assert (g_variant_check_format_string (value, "(s*)", TRUE));
  g_assert (g_variant_check_format_string (value, "(s*)", FALSE));
  g_assert (!g_variant_check_format_string (value, "(u*)", TRUE));
  g_assert (!g_variant_check_format_string (value, "(u*)", FALSE));

  g_assert (g_variant_check_format_string (value, "(&s*)", FALSE));
  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL, "*contains a '&' character*");
  g_assert (!g_variant_check_format_string (value, "(&s*)", TRUE));
  g_test_assert_expected_messages ();

  g_assert (g_variant_check_format_string (value, "(s^ay)", TRUE));
  g_assert (g_variant_check_format_string (value, "(s^ay)", FALSE));

  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL, "*contains a '&' character*");
  g_assert (!g_variant_check_format_string (value, "(s^&ay)", TRUE));
  g_test_assert_expected_messages ();
  g_assert (g_variant_check_format_string (value, "(s^&ay)", FALSE));

  g_assert (g_variant_check_format_string (value, "r", FALSE));
  g_assert (g_variant_check_format_string (value, "(?a?)", FALSE));

  g_variant_unref (value);
}

static void
verify_gvariant_checksum (const gchar  *sha256,
			  GVariant     *v)
	     
{
  gchar *checksum;
  checksum = g_compute_checksum_for_data (G_CHECKSUM_SHA256,
					  g_variant_get_data (v),
					  g_variant_get_size (v));
  g_assert_cmpstr (sha256, ==, checksum);
  g_free (checksum);
}

static void
verify_gvariant_checksum_va (const gchar *sha256,
			     const gchar *fmt,
			     ...)
{
  va_list args;
  GVariant *v;

  va_start (args, fmt);

  v = g_variant_new_va (fmt, NULL, &args);
  g_variant_ref_sink (v);
#if G_BYTE_ORDER == G_BIG_ENDIAN
  {
    GVariant *byteswapped = g_variant_byteswap (v);
    g_variant_unref (v);
    v = byteswapped;
  }
#endif

  va_end (args);

  verify_gvariant_checksum (sha256, v);

  g_variant_unref (v);
}

static void
test_checksum_basic (void)
{
  verify_gvariant_checksum_va ("e8a4b2ee7ede79a3afb332b5b6cc3d952a65fd8cffb897f5d18016577c33d7cc",
			       "u", 42);
  verify_gvariant_checksum_va ("c53e363c33b00cfce298229ee83856b8a98c2e6126cab13f65899f62473b0df5",
			       "s", "moocow");
  verify_gvariant_checksum_va ("2b4c342f5433ebe591a1da77e013d1b72475562d48578dca8b84bac6651c3cb9",
			       "y", 9);
  verify_gvariant_checksum_va ("12a3ae445661ce5dee78d0650d33362dec29c4f82af05e7e57fb595bbbacf0ca",
			       "t", G_MAXUINT64);
  verify_gvariant_checksum_va ("e25a59b24440eb6c833aa79c93b9840e6eab6966add0dacf31df7e9e7000f5b3",
			       "d", 3.14159);
  verify_gvariant_checksum_va ("4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a",
			       "b", TRUE);
  verify_gvariant_checksum_va ("ca2fd00fa001190744c15c317643ab092e7048ce086a243e2be9437c898de1bb",
			       "q", G_MAXUINT16);
}

static void
test_checksum_nested (void)
{
  static const char* const strv[] = {"foo", "bar", "baz", NULL};

  verify_gvariant_checksum_va ("31fbc92f08fddaca716188fe4b5d44ae122fc6306fd3c6925af53cfa47ea596d",
			       "(uu)", 41, 43);
  verify_gvariant_checksum_va ("01759d683cead856d1d386d59af0578841698a424a265345ad5413122f220de8",
			       "(su)", "moocow", 79);
  verify_gvariant_checksum_va ("52b3ae95f19b3e642ea1d01185aea14a09004c1d1712672644427403a8a0afe6",
			       "(qyst)", G_MAXUINT16, 9, "moocow", G_MAXUINT64);
  verify_gvariant_checksum_va ("6fc6f4524161c3ae0d316812d7088e3fcd372023edaea2d7821093be40ae1060",
			       "(@ay)", g_variant_new_bytestring ("\xFF\xFF\xFF"));
  verify_gvariant_checksum_va ("572aca386e1a983dd23bb6eb6e3dfa72eef9ca7c7744581aa800e18d7d9d0b0b",
			       "(^as)", strv);
  verify_gvariant_checksum_va ("4bddf6174c791bb44fc6a4106573031690064df34b741033a0122ed8dc05bcf3",
			       "(yvu)", 254, g_variant_new ("(^as)", strv), 42);
}

static void
test_gbytes (void)
{
  GVariant *a;
  GVariant *tuple;
  GBytes *bytes;
  GBytes *bytes2;
  const guint8 values[5] = { 1, 2, 3, 4, 5 };
  const guint8 *elts;
  gsize n_elts;
  gint i;

  bytes = g_bytes_new (&values, 5);
  a = g_variant_new_from_bytes (G_VARIANT_TYPE_BYTESTRING, bytes, TRUE);
  g_bytes_unref (bytes);
  n_elts = 0;
  elts = g_variant_get_fixed_array (a, &n_elts, sizeof (guint8));
  g_assert (n_elts == 5);
  for (i = 0; i < 5; i++)
    g_assert_cmpint (elts[i], ==, i + 1);

  bytes2 = g_variant_get_data_as_bytes (a);
  g_variant_unref (a);

  bytes = g_bytes_new (&values, 5);
  g_assert (g_bytes_equal (bytes, bytes2));
  g_bytes_unref (bytes);
  g_bytes_unref (bytes2);

  tuple = g_variant_new_parsed ("['foo', 'bar']");
  bytes = g_variant_get_data_as_bytes (tuple); /* force serialisation */
  a = g_variant_get_child_value (tuple, 1);
  bytes2 = g_variant_get_data_as_bytes (a);
  g_assert (!g_bytes_equal (bytes, bytes2));

  g_bytes_unref (bytes);
  g_bytes_unref (bytes2);
  g_variant_unref (a);
  g_variant_unref (tuple);
}

typedef struct {
  const GVariantType *type;
  const gchar *in;
  const gchar *out;
} ContextTest;

static void
test_print_context (void)
{
  ContextTest tests[] = {
    { NULL, "(1, 2, 3, 'abc", "          ^^^^" },
    { NULL, "[1, 2, 3, 'str']", " ^        ^^^^^" },
    { G_VARIANT_TYPE_UINT16, "{ 'abc':'def' }", "  ^^^^^^^^^^^^^^^" },
    { NULL, "<5", "    ^" },
    { NULL, "'ab\\ux'", "  ^^^^^^^" },
    { NULL, "'ab\\U00efx'", "  ^^^^^^^^^^^" }
  };
  GVariant *v;
  gchar *s;
  gint i;
  GError *error = NULL;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      v = g_variant_parse (tests[i].type, tests[i].in, NULL, NULL, &error);
      g_assert_null (v);
      s = g_variant_parse_error_print_context (error, tests[i].in);
      g_assert (strstr (s, tests[i].out) != NULL);
      g_free (s);
      g_clear_error (&error);
    }
}

static void
test_error_quark (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_assert (g_variant_parser_get_error_quark () == g_variant_parse_error_quark ());
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
test_stack_builder_init (void)
{
  GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_BYTESTRING);
  GVariant *variant;

  g_variant_builder_add_value (&builder, g_variant_new_byte ('g'));
  g_variant_builder_add_value (&builder, g_variant_new_byte ('l'));
  g_variant_builder_add_value (&builder, g_variant_new_byte ('i'));
  g_variant_builder_add_value (&builder, g_variant_new_byte ('b'));
  g_variant_builder_add_value (&builder, g_variant_new_byte ('\0'));

  variant = g_variant_ref_sink (g_variant_builder_end (&builder));
  g_assert_nonnull (variant);
  g_assert (g_variant_type_equal (g_variant_get_type (variant),
                                  G_VARIANT_TYPE_BYTESTRING));
  g_assert_cmpuint (g_variant_n_children (variant), ==, 5);
  g_assert_cmpstr (g_variant_get_bytestring (variant), ==, "glib");
  g_variant_unref (variant);
}

static GVariant *
get_asv (void)
{
  GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);

  g_variant_builder_add (&builder, "{s@v}", "foo", g_variant_new_variant (g_variant_new_string ("FOO")));
  g_variant_builder_add (&builder, "{s@v}", "bar", g_variant_new_variant (g_variant_new_string ("BAR")));

  return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static void
test_stack_dict_init (void)
{
  GVariant *asv = get_asv ();
  GVariantDict dict = G_VARIANT_DICT_INIT (asv);
  GVariant *variant;
  GVariantIter iter;
  gchar *key;
  GVariant *value;

  g_variant_dict_insert_value (&dict, "baz", g_variant_new_string ("BAZ"));
  g_variant_dict_insert_value (&dict, "quux", g_variant_new_string ("QUUX"));

  variant = g_variant_ref_sink (g_variant_dict_end (&dict));
  g_assert_nonnull (variant);
  g_assert (g_variant_type_equal (g_variant_get_type (variant),
                                  G_VARIANT_TYPE_VARDICT));
  g_assert_cmpuint (g_variant_n_children (variant), ==, 4);

  g_variant_iter_init (&iter, variant);
  while (g_variant_iter_next (&iter, "{sv}", &key, &value))
    {
      gchar *strup = g_ascii_strup (key, -1);

      g_assert_cmpstr (strup, ==, g_variant_get_string (value, NULL));
      g_free (key);
      g_free (strup);
      g_variant_unref (value);
    }

  g_variant_unref (asv);
  g_variant_unref (variant);
}

int
main (int argc, char **argv)
{
  gint i;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gvariant/type", test_gvarianttype);
  g_test_add_func ("/gvariant/typeinfo", test_gvarianttypeinfo);
  g_test_add_func ("/gvariant/serialiser/maybe", test_maybes);
  g_test_add_func ("/gvariant/serialiser/array", test_arrays);
  g_test_add_func ("/gvariant/serialiser/tuple", test_tuples);
  g_test_add_func ("/gvariant/serialiser/variant", test_variants);
  g_test_add_func ("/gvariant/serialiser/strings", test_strings);
  g_test_add_func ("/gvariant/serialiser/byteswap", test_byteswaps);

  for (i = 1; i <= 20; i += 4)
    {
      char *testname;

      testname = g_strdup_printf ("/gvariant/serialiser/fuzz/%d%%", i);
      g_test_add_data_func (testname, GINT_TO_POINTER (i),
                            (gpointer) test_fuzzes);
      g_free (testname);
    }

  g_test_add_func ("/gvariant/string", test_string);
  g_test_add_func ("/gvariant/utf8", test_utf8);
  g_test_add_func ("/gvariant/containers", test_containers);
  g_test_add_func ("/gvariant/format-strings", test_format_strings);
  g_test_add_func ("/gvariant/invalid-varargs", test_invalid_varargs);
  g_test_add_func ("/gvariant/varargs", test_varargs);
  g_test_add_func ("/gvariant/varargs/subprocess/empty-array", test_varargs_empty_array);
  g_test_add_func ("/gvariant/valist", test_valist);
  g_test_add_func ("/gvariant/builder-memory", test_builder_memory);
  g_test_add_func ("/gvariant/hashing", test_hashing);
  g_test_add_func ("/gvariant/byteswap", test_gv_byteswap);
  g_test_add_func ("/gvariant/parser", test_parses);
  g_test_add_func ("/gvariant/parse-failures", test_parse_failures);
  g_test_add_func ("/gvariant/parse-positional", test_parse_positional);
  g_test_add_func ("/gvariant/parse/subprocess/bad-format-char", test_parse_bad_format_char);
  g_test_add_func ("/gvariant/parse/subprocess/bad-format-string", test_parse_bad_format_string);
  g_test_add_func ("/gvariant/parse/subprocess/bad-args", test_parse_bad_args);
  g_test_add_func ("/gvariant/floating", test_floating);
  g_test_add_func ("/gvariant/bytestring", test_bytestring);
  g_test_add_func ("/gvariant/lookup-value", test_lookup_value);
  g_test_add_func ("/gvariant/lookup", test_lookup);
  g_test_add_func ("/gvariant/compare", test_compare);
  g_test_add_func ("/gvariant/equal", test_equal);
  g_test_add_func ("/gvariant/fixed-array", test_fixed_array);
  g_test_add_func ("/gvariant/check-format-string", test_check_format_string);

  g_test_add_func ("/gvariant/checksum-basic", test_checksum_basic);
  g_test_add_func ("/gvariant/checksum-nested", test_checksum_nested);

  g_test_add_func ("/gvariant/gbytes", test_gbytes);
  g_test_add_func ("/gvariant/print-context", test_print_context);
  g_test_add_func ("/gvariant/error-quark", test_error_quark);

  g_test_add_func ("/gvariant/stack-builder-init", test_stack_builder_init);
  g_test_add_func ("/gvariant/stack-dict-init", test_stack_dict_init);
  return g_test_run ();
}
