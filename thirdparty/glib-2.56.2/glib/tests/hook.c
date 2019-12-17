/* Unit tests for hook lists
 * Copyright (C) 2011 Red Hat, Inc.
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

static void
hook_func (gpointer data)
{
}

static void
destroy (gpointer data)
{
}

static void
test_hook1 (void)
{
  GHookList *hl;
  GHook *hook;
  gulong id;
  GHook *h;

  hl = g_new (GHookList, 1);
  g_hook_list_init (hl, sizeof (GHook));

  hook = g_hook_alloc (hl);
  hook->data = GINT_TO_POINTER(1);
  hook->func = hook_func;
  hook->flags = G_HOOK_FLAG_ACTIVE;
  hook->destroy = destroy;
  g_hook_append (hl, hook);
  id = hook->hook_id;

  h = g_hook_get (hl, id);
  g_assert (h == hook);

  h = hook = g_hook_alloc (hl);
  hook->data = GINT_TO_POINTER(2);
  hook->func = hook_func;
  hook->flags = G_HOOK_FLAG_ACTIVE;
  hook->destroy = destroy;
  g_hook_prepend (hl, hook);

  g_hook_destroy (hl, id);

  hook = g_hook_alloc (hl);
  hook->data = GINT_TO_POINTER(3);
  hook->func = hook_func;
  hook->flags = G_HOOK_FLAG_ACTIVE;
  hook->destroy = destroy;
  g_hook_insert_sorted (hl, hook, g_hook_compare_ids);

  hook = g_hook_alloc (hl);
  hook->data = GINT_TO_POINTER(4);
  hook->func = hook_func;
  hook->flags = G_HOOK_FLAG_ACTIVE;
  hook->destroy = destroy;
  g_hook_insert_before (hl, h, hook);

  g_hook_list_invoke (hl, TRUE);

  g_hook_list_clear (hl);
  g_free (hl);
}

int main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/hook/test1", test_hook1);

  return g_test_run ();
}

