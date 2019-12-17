/* Unit tests for GMarkup
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 * Author: Matthias Clasen
 */

#include "glib.h"

typedef struct {
  GSList *stack;
} ParseData;

static void
start (GMarkupParseContext  *context,
       const gchar          *element_name,
       const gchar         **attribute_names,
       const gchar         **attribute_values,
       gpointer              user_data,
       GError              **error)
{
  ParseData *data = user_data;

  data->stack = g_slist_prepend (data->stack, g_strdup (element_name));
}

static void
end (GMarkupParseContext  *context,
     const gchar          *element_name,
     gpointer              user_data,
     GError              **error)
{
  ParseData *data = user_data;
  const GSList *stack;
  const GSList *s1, *s2;
  GSList *s;
  
  stack = g_markup_parse_context_get_element_stack (context);
  for (s1 = stack, s2 = data->stack; s1 && s2; s1 = s1->next, s2 = s2->next)
    g_assert_cmpstr (s1->data, ==, s2->data);
  g_assert (s1 == NULL && s2 == NULL);

  s = data->stack;
  data->stack = data->stack->next;
  s->next = NULL;
  g_slist_free_full (s, g_free);
}

const gchar content[] = 
 "<e1><foo><bar></bar> bla <l>fff</l></foo></e1>";

static void
test_markup_stack (void)
{
  GMarkupParser parser = {
    start,
    end,
    NULL,
    NULL,
    NULL
  };
  GMarkupParseContext *context;
  ParseData data = { NULL };
  gboolean res;
  GError *error = NULL;

  context = g_markup_parse_context_new (&parser, 0, &data, NULL);
  res = g_markup_parse_context_parse (context, content, -1, &error);
  g_assert (res);
  g_assert_no_error (error);
  g_markup_parse_context_free (context);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/markup/stack", test_markup_stack);

  return g_test_run ();
}
