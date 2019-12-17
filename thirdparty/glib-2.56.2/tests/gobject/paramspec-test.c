/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <string.h>

#include <glib.h>
#include <glib-object.h>

static void
test_param_spec_char (void)
{
  GParamSpec *pspec;
  GValue value = G_VALUE_INIT;
  gboolean modified;
 
  pspec = g_param_spec_char ("char", "nick", "blurb",
			     20, 40, 30, G_PARAM_READWRITE);

  g_assert (strcmp (g_param_spec_get_name (pspec), "char") == 0);
  g_assert (strcmp (g_param_spec_get_nick (pspec), "nick") == 0);
  g_assert (strcmp (g_param_spec_get_blurb (pspec), "blurb") == 0);

  g_value_init (&value, G_TYPE_CHAR);
  g_value_set_char (&value, 30);

  g_assert (g_param_value_defaults (pspec, &value));
  
  g_value_set_char (&value, 0);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_char (&value) == 20);

  g_value_set_char (&value, 20);
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_char (&value) == 20);

  g_value_set_char (&value, 40);
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_char (&value) == 40);

  g_value_set_char (&value, 60);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_char (&value) == 40);

  g_value_set_schar (&value, 0);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_schar (&value) == 20);

  g_value_set_schar (&value, 20);
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_schar (&value) == 20);

  g_value_set_schar (&value, 40);
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_schar (&value) == 40);

  g_value_set_schar (&value, 60);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_schar (&value) == 40);

  g_param_spec_unref (pspec);
}

static void
test_param_spec_string (void)
{
  GParamSpec *pspec;
  GValue value = G_VALUE_INIT;
  gboolean modified;

  pspec = g_param_spec_string ("string", "nick", "blurb",
                               NULL, G_PARAM_READWRITE);
  g_value_init (&value, G_TYPE_STRING);

  g_value_set_string (&value, "foobar");
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified);

  g_value_set_string (&value, "");
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_string (&value) != NULL);

  /* test ensure_non_null */

  G_PARAM_SPEC_STRING (pspec)->ensure_non_null = TRUE;

  g_value_set_string (&value, NULL);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_string (&value) != NULL);

  G_PARAM_SPEC_STRING (pspec)->ensure_non_null = FALSE;

  /* test null_fold_if_empty */

  G_PARAM_SPEC_STRING (pspec)->null_fold_if_empty = TRUE;

  g_value_set_string (&value, "");
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_string (&value) == NULL);

  g_value_set_static_string (&value, "");
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_string (&value) == NULL);

  G_PARAM_SPEC_STRING (pspec)->null_fold_if_empty = FALSE;

  /* test cset_first */

  G_PARAM_SPEC_STRING (pspec)->cset_first = g_strdup ("abc");
  G_PARAM_SPEC_STRING (pspec)->substitutor = '-';

  g_value_set_string (&value, "ABC");
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_string (&value)[0] == '-');

  g_value_set_static_string (&value, "ABC");
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_string (&value)[0] == '-');

  /* test cset_nth */

  G_PARAM_SPEC_STRING (pspec)->cset_nth = g_strdup ("abc");

  g_value_set_string (&value, "aBC");
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_string (&value)[1] == '-');

  g_value_set_static_string (&value, "aBC");
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_string (&value)[1] == '-');

  g_value_unset (&value);
  g_param_spec_unref (pspec);
}

static void
test_param_spec_override (void)
{
  GParamSpec *ospec, *pspec;
  GValue value = G_VALUE_INIT;
  gboolean modified;
 
  ospec = g_param_spec_char ("char", "nick", "blurb",
			     20, 40, 30, G_PARAM_READWRITE);

  pspec = g_param_spec_override ("override", ospec);

  g_assert (strcmp (g_param_spec_get_name (pspec), "override") == 0);
  g_assert (strcmp (g_param_spec_get_nick (pspec), "nick") == 0);
  g_assert (strcmp (g_param_spec_get_blurb (pspec), "blurb") == 0);

  g_value_init (&value, G_TYPE_CHAR);
  g_value_set_char (&value, 30);

  g_assert (g_param_value_defaults (pspec, &value));
  
  g_value_set_char (&value, 0);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_char (&value) == 20);

  g_value_set_char (&value, 20);
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_char (&value) == 20);

  g_value_set_char (&value, 40);
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_char (&value) == 40);

  g_value_set_char (&value, 60);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_char (&value) == 40);

  g_param_spec_unref (pspec);
  g_param_spec_unref (ospec);
}

static void
test_param_spec_gtype (void)
{
  GParamSpec *pspec;
  GValue value = G_VALUE_INIT;
  gboolean modified;
  
  pspec = g_param_spec_gtype ("gtype", "nick", "blurb",
			      G_TYPE_PARAM, G_PARAM_READWRITE);
  
  g_value_init (&value, G_TYPE_GTYPE);
  g_value_set_gtype (&value, G_TYPE_PARAM);

  g_assert (g_param_value_defaults (pspec, &value));
  
  g_value_set_gtype (&value, G_TYPE_INT);
  modified = g_param_value_validate (pspec, &value);
  g_assert (modified && g_value_get_gtype (&value) == G_TYPE_PARAM);

  g_value_set_gtype (&value, G_TYPE_PARAM_INT);
  modified = g_param_value_validate (pspec, &value);
  g_assert (!modified && g_value_get_gtype (&value) == G_TYPE_PARAM_INT);

  g_param_spec_unref (pspec);
}

static void
test_param_spec_variant (void)
{
  GParamSpec *pspec;
  GValue value = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  GValue value3 = G_VALUE_INIT;
  GValue value4 = G_VALUE_INIT;
  GValue value5 = G_VALUE_INIT;
  gboolean modified;

  pspec = g_param_spec_variant ("variant", "nick", "blurb",
                                G_VARIANT_TYPE ("i"),
                                g_variant_new_int32 (42),
                                G_PARAM_READWRITE);

  g_value_init (&value, G_TYPE_VARIANT);
  g_value_set_variant (&value, g_variant_new_int32 (42));

  g_value_init (&value2, G_TYPE_VARIANT);
  g_value_set_variant (&value2, g_variant_new_int32 (43));

  g_value_init (&value3, G_TYPE_VARIANT);
  g_value_set_variant (&value3, g_variant_new_int16 (42));

  g_value_init (&value4, G_TYPE_VARIANT);
  g_value_set_variant (&value4, g_variant_new_parsed ("[@u 15, @u 10]"));

  g_value_init (&value5, G_TYPE_VARIANT);
  g_value_set_variant (&value5, NULL);

  g_assert_true (g_param_value_defaults (pspec, &value));
  g_assert_false (g_param_value_defaults (pspec, &value2));
  g_assert_false (g_param_value_defaults (pspec, &value3));
  g_assert_false (g_param_value_defaults (pspec, &value4));
  g_assert_false (g_param_value_defaults (pspec, &value5));

  modified = g_param_value_validate (pspec, &value);
  g_assert_false (modified);

  g_value_reset (&value);
  g_value_set_variant (&value, g_variant_new_uint32 (41));
  modified = g_param_value_validate (pspec, &value);
  g_assert_true (modified);
  g_assert_cmpint (g_variant_get_int32 (g_value_get_variant (&value)), ==, 42);
  g_value_unset (&value);

  g_value_unset (&value5);
  g_value_unset (&value4);
  g_value_unset (&value3);
  g_value_unset (&value2);

  g_param_spec_unref (pspec);
}

/* Test g_param_values_cmp() for #GParamSpecVariant. */
static void
test_param_spec_variant_cmp (void)
{
  const struct
    {
      const GVariantType *pspec_type;
      const gchar *v1;
      enum
        {
          LESS_THAN = -1,
          EQUAL = 0,
          GREATER_THAN = 1,
          NOT_EQUAL,
        } expected_result;
      const gchar *v2;
    }
  vectors[] =
    {
      { G_VARIANT_TYPE ("i"), "@i 1", LESS_THAN, "@i 2" },
      { G_VARIANT_TYPE ("i"), "@i 2", EQUAL, "@i 2" },
      { G_VARIANT_TYPE ("i"), "@i 3", GREATER_THAN, "@i 2" },
      { G_VARIANT_TYPE ("i"), NULL, LESS_THAN, "@i 2" },
      { G_VARIANT_TYPE ("i"), NULL, EQUAL, NULL },
      { G_VARIANT_TYPE ("i"), "@i 1", GREATER_THAN, NULL },
      { G_VARIANT_TYPE ("i"), "@u 1", LESS_THAN, "@u 2" },
      { G_VARIANT_TYPE ("i"), "@as ['hi']", NOT_EQUAL, "@u 2" },
      { G_VARIANT_TYPE ("i"), "@as ['hi']", NOT_EQUAL, "@as ['there']" },
      { G_VARIANT_TYPE ("i"), "@as ['hi']", EQUAL, "@as ['hi']" },
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (vectors); i++)
    {
      GParamSpec *pspec;
      GValue v1 = G_VALUE_INIT;
      GValue v2 = G_VALUE_INIT;
      gint cmp;

      pspec = g_param_spec_variant ("variant", "nick", "blurb",
                                    vectors[i].pspec_type,
                                    NULL,
                                    G_PARAM_READWRITE);

      g_value_init (&v1, G_TYPE_VARIANT);
      g_value_set_variant (&v1, (vectors[i].v1 != NULL) ? g_variant_new_parsed (vectors[i].v1) : NULL);

      g_value_init (&v2, G_TYPE_VARIANT);
      g_value_set_variant (&v2, (vectors[i].v2 != NULL) ? g_variant_new_parsed (vectors[i].v2) : NULL);

      cmp = g_param_values_cmp (pspec, &v1, &v2);

      switch (vectors[i].expected_result)
        {
        case LESS_THAN:
        case EQUAL:
        case GREATER_THAN:
          g_assert_cmpint (cmp, ==, vectors[i].expected_result);
          break;
        case NOT_EQUAL:
          g_assert_cmpint (cmp, !=, 0);
          break;
        default:
          g_assert_not_reached ();
        }

      g_value_unset (&v2);
      g_value_unset (&v1);
      g_param_spec_unref (pspec);
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/paramspec/char", test_param_spec_char);
  g_test_add_func ("/paramspec/string", test_param_spec_string);
  g_test_add_func ("/paramspec/override", test_param_spec_override);
  g_test_add_func ("/paramspec/gtype", test_param_spec_gtype);
  g_test_add_func ("/paramspec/variant", test_param_spec_variant);
  g_test_add_func ("/paramspec/variant/cmp", test_param_spec_variant_cmp);

  return g_test_run ();
}
