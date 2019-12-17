#include <glib.h>
#include <stdlib.h>

#define SIZE       50
#define NUMBER_MIN 0000
#define NUMBER_MAX 9999


static guint32 array[SIZE];


static gint
sort (gconstpointer p1, gconstpointer p2)
{
  gint32 a, b;

  a = GPOINTER_TO_INT (p1);
  b = GPOINTER_TO_INT (p2);

  return (a > b ? +1 : a == b ? 0 : -1);
}

/*
 * glist sort tests
 */
static void
test_list_sort (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < SIZE; i++)
    list = g_list_append (list, GINT_TO_POINTER (array[i]));

  list = g_list_sort (list, sort);
  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_list_free (list);
}

static void
test_list_sort_with_data (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < SIZE; i++)
    list = g_list_append (list, GINT_TO_POINTER (array[i]));

  list = g_list_sort_with_data (list, (GCompareDataFunc)sort, NULL);
  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_list_free (list);
}

/* Test that the sort is stable. */
static void
test_list_sort_stable (void)
{
  GList *list = NULL;  /* (element-type utf8) */
  GList *copy = NULL;  /* (element-type utf8) */
  gsize i;

  /* Build a test list, already ordered. */
  for (i = 0; i < SIZE; i++)
    list = g_list_append (list, g_strdup_printf ("%" G_GSIZE_FORMAT, i / 5));

  /* Take a copy and sort it. */
  copy = g_list_copy (list);
  copy = g_list_sort (copy, (GCompareFunc) g_strcmp0);

  /* Compare the two lists, checking pointers are equal to ensure the elements
   * have been kept stable. */
  for (i = 0; i < SIZE; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i);

      g_assert (p1 == p2);
    }

  g_list_free (copy);
  g_list_free_full (list, g_free);
}

static void
test_list_insert_sorted (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < SIZE; i++)
    list = g_list_insert_sorted (list, GINT_TO_POINTER (array[i]), sort);

  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_list_free (list);
}

static void
test_list_insert_sorted_with_data (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < SIZE; i++)
    list = g_list_insert_sorted_with_data (list,
                                           GINT_TO_POINTER (array[i]),
                                           (GCompareDataFunc)sort,
                                           NULL);

  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_list_nth_data (list, i);
      p2 = g_list_nth_data (list, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_list_free (list);
}

static void
test_list_reverse (void)
{
  GList *list = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  for (i = 0; i < 10; i++)
    list = g_list_append (list, &nums[i]);

  list = g_list_reverse (list);

  for (i = 0; i < 10; i++)
    {
      st = g_list_nth (list, i);
      g_assert (*((gint*) st->data) == (9 - i));
    }

  g_list_free (list);
}

static void
test_list_nth (void)
{
  GList *list = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  for (i = 0; i < 10; i++)
    list = g_list_append (list, &nums[i]);

  for (i = 0; i < 10; i++)
    {
      st = g_list_nth (list, i);
      g_assert (*((gint*) st->data) == i);
    }

  g_list_free (list);
}

static void
test_list_concat (void)
{
  GList *list1 = NULL;
  GList *list2 = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint i;

  for (i = 0; i < 5; i++)
    {
      list1 = g_list_append (list1, &nums[i]);
      list2 = g_list_append (list2, &nums[i+5]);
    }

  g_assert_cmpint (g_list_length (list1), ==, 5);
  g_assert_cmpint (g_list_length (list2), ==, 5);

  list1 = g_list_concat (list1, list2);

  g_assert_cmpint (g_list_length (list1), ==, 10);

  for (i = 0; i < 10; i++)
    {
      st = g_list_nth (list1, i);
      g_assert (*((gint*) st->data) == i);
    }

  list2 = g_list_concat (NULL, list1);
  g_assert_cmpint (g_list_length (list2), ==, 10);

  list2 = g_list_concat (list1, NULL);
  g_assert_cmpint (g_list_length (list2), ==, 10);

  list2 = g_list_concat (NULL, NULL);
  g_assert (list2 == NULL);

  g_list_free (list1);
}

static void
test_list_remove (void)
{
  GList *list = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  for (i = 0; i < 10; i++)
    {
      list = g_list_append (list, &nums[i]);
      list = g_list_append (list, &nums[i]);
    }

  g_assert_cmpint (g_list_length (list), ==, 20);

  for (i = 0; i < 10; i++)
    {
      list = g_list_remove (list, &nums[i]);
    }

  g_assert_cmpint (g_list_length (list), ==, 10);

  for (i = 0; i < 10; i++)
    {
      st = g_list_nth (list, i);
      g_assert (*((gint*) st->data) == i);
    }

  g_list_free (list);
}

static void
test_list_remove_all (void)
{
  GList *list = NULL;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  for (i = 0; i < 10; i++)
    {
      list = g_list_append (list, &nums[i]);
      list = g_list_append (list, &nums[i]);
    }

  g_assert_cmpint (g_list_length (list), ==, 20);

  for (i = 0; i < 5; i++)
    {
      list = g_list_remove_all (list, &nums[2 * i + 1]);
      list = g_list_remove_all (list, &nums[8 - 2 * i]);
    }

  g_assert_cmpint (g_list_length (list), ==, 0);
  g_assert (list == NULL);
}

static void
test_list_first_last (void)
{
  GList *list = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  for (i = 0; i < 10; i++)
    list = g_list_append (list, &nums[i]);

  st = g_list_last (list);
  g_assert (*((gint*) st->data) == 9);
  st = g_list_nth_prev (st, 3);
  g_assert (*((gint*) st->data) == 6);
  st = g_list_first (st);
  g_assert (*((gint*) st->data) == 0);

  g_list_free (list);
}

static void
test_list_insert (void)
{
  GList *list = NULL;
  GList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  list = g_list_insert_before (NULL, NULL, &nums[1]);
  list = g_list_insert (list, &nums[3], 1);
  list = g_list_insert (list, &nums[4], -1);
  list = g_list_insert (list, &nums[0], 0);
  list = g_list_insert (list, &nums[5], 100);
  list = g_list_insert_before (list, NULL, &nums[6]);
  list = g_list_insert_before (list, list->next->next, &nums[2]);

  list = g_list_insert (list, &nums[9], 7);
  list = g_list_insert (list, &nums[8], 7);
  list = g_list_insert (list, &nums[7], 7);

  for (i = 0; i < 10; i++)
    {
      st = g_list_nth (list, i);
      g_assert (*((gint*) st->data) == i);
    }

  g_list_free (list);
}

typedef struct
{
  gboolean freed;
  int x;
} ListItem;

static void
free_func (gpointer data)
{
  ListItem *item = data;

  item->freed = TRUE;
}

static ListItem *
new_item (int x)
{
  ListItem *item;

  item = g_slice_new (ListItem);
  item->freed = FALSE;
  item->x = x;

  return item;
}

static void
test_free_full (void)
{
  ListItem *one, *two, *three;
  GSList *slist = NULL;
  GList *list = NULL;

  slist = g_slist_prepend (slist, one = new_item (1));
  slist = g_slist_prepend (slist, two = new_item (2));
  slist = g_slist_prepend (slist, three = new_item (3));
  g_assert (!one->freed);
  g_assert (!two->freed);
  g_assert (!three->freed);
  g_slist_free_full (slist, free_func);
  g_assert (one->freed);
  g_assert (two->freed);
  g_assert (three->freed);
  g_slice_free (ListItem, one);
  g_slice_free (ListItem, two);
  g_slice_free (ListItem, three);

  list = g_list_prepend (list, one = new_item (1));
  list = g_list_prepend (list, two = new_item (2));
  list = g_list_prepend (list, three = new_item (3));
  g_assert (!one->freed);
  g_assert (!two->freed);
  g_assert (!three->freed);
  g_list_free_full (list, free_func);
  g_assert (one->freed);
  g_assert (two->freed);
  g_assert (three->freed);
  g_slice_free (ListItem, one);
  g_slice_free (ListItem, two);
  g_slice_free (ListItem, three);
}

static void
test_list_copy (void)
{
  GList *l, *l2;
  GList *u, *v;

  l = NULL;
  l = g_list_append (l, GINT_TO_POINTER (1));
  l = g_list_append (l, GINT_TO_POINTER (2));
  l = g_list_append (l, GINT_TO_POINTER (3));

  l2 = g_list_copy (l);

  for (u = l, v = l2; u && v; u = u->next, v = v->next)
    {
      g_assert (u->data == v->data);
    }

  g_list_free (l);
  g_list_free (l2);
}

static gpointer
multiply_value (gconstpointer value, gpointer data)
{
  return GINT_TO_POINTER (GPOINTER_TO_INT (value) * GPOINTER_TO_INT (data));
}

static void
test_list_copy_deep (void)
{
  GList *l, *l2;
  GList *u, *v;

  l = NULL;
  l = g_list_append (l, GINT_TO_POINTER (1));
  l = g_list_append (l, GINT_TO_POINTER (2));
  l = g_list_append (l, GINT_TO_POINTER (3));

  l2 = g_list_copy_deep (l, multiply_value, GINT_TO_POINTER (2));

  for (u = l, v = l2; u && v; u = u->next, v = v->next)
    {
      g_assert_cmpint (GPOINTER_TO_INT (u->data) * 2, ==, GPOINTER_TO_INT (v->data));
    }

  g_list_free (l);
  g_list_free (l2);
}

static void
test_delete_link (void)
{
  GList *l, *l2;

  l = NULL;
  l = g_list_append (l, GINT_TO_POINTER (1));
  l = g_list_append (l, GINT_TO_POINTER (2));
  l = g_list_append (l, GINT_TO_POINTER (3));

  l2 = l->next;

  l = g_list_delete_link (l, l2);
  g_assert (l->data == GINT_TO_POINTER (1));
  g_assert (l->next->data == GINT_TO_POINTER (3));

  g_list_free (l);
}

static void
test_prepend (void)
{
  GList *l, *l2;

  l = NULL;
  l = g_list_prepend (l, "c");
  l = g_list_prepend (l, "a");

  g_assert (l->data == (gpointer)"a");
  g_assert (l->next->data == (gpointer)"c");
  g_assert (l->next->next == NULL);

  l2 = l->next;
  l2 = g_list_prepend (l2, "b");
  g_assert (l2->prev == l);

  g_assert (l->data == (gpointer)"a");
  g_assert (l->next->data == (gpointer)"b");
  g_assert (l->next->next->data == (gpointer)"c");
  g_assert (l->next->next->next == NULL);

  g_list_free (l);
}

static void
test_position (void)
{
  GList *l, *ll;

  l = NULL;
  l = g_list_append (l, "a");
  l = g_list_append (l, "b");
  l = g_list_append (l, "c");

  ll = g_list_find (l, "a");
  g_assert_cmpint (g_list_position (l, ll), ==, 0);
  g_assert_cmpint (g_list_index (l, "a"), ==, 0);
  ll = g_list_find (l, "b");
  g_assert_cmpint (g_list_position (l, ll), ==, 1);
  g_assert_cmpint (g_list_index (l, "b"), ==, 1);
  ll = g_list_find (l, "c");
  g_assert_cmpint (g_list_position (l, ll), ==, 2);
  g_assert_cmpint (g_list_index (l, "c"), ==, 2);

  ll = g_list_append (NULL, "d");
  g_assert_cmpint (g_list_position (l, ll), ==, -1);
  g_assert_cmpint (g_list_index (l, "d"), ==, -1);

  g_list_free (l);
  g_list_free (ll);
}

static void
test_double_free (void)
{
  GList *list, *link;
  GList  intruder = { NULL, (gpointer)0xDEADBEEF, (gpointer)0xDEADBEEF };

  if (g_test_subprocess ())
    {
      list = NULL;
      list = g_list_append (list, "a");
      link = list = g_list_append (list, "b");
      list = g_list_append (list, "c");

      list = g_list_remove_link (list, link);
      link->prev = list;
      link->next = &intruder;
      list = g_list_remove_link (list, link);

      g_list_free (list);
      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*corrupted double-linked list detected*");
}

int
main (int argc, char *argv[])
{
  gint i;

  g_test_init (&argc, &argv, NULL);

  /* Create an array of random numbers. */
  for (i = 0; i < SIZE; i++)
    array[i] = g_test_rand_int_range (NUMBER_MIN, NUMBER_MAX);

  g_test_add_func ("/list/sort", test_list_sort);
  g_test_add_func ("/list/sort-with-data", test_list_sort_with_data);
  g_test_add_func ("/list/sort/stable", test_list_sort_stable);
  g_test_add_func ("/list/insert-sorted", test_list_insert_sorted);
  g_test_add_func ("/list/insert-sorted-with-data", test_list_insert_sorted_with_data);
  g_test_add_func ("/list/reverse", test_list_reverse);
  g_test_add_func ("/list/nth", test_list_nth);
  g_test_add_func ("/list/concat", test_list_concat);
  g_test_add_func ("/list/remove", test_list_remove);
  g_test_add_func ("/list/remove-all", test_list_remove_all);
  g_test_add_func ("/list/first-last", test_list_first_last);
  g_test_add_func ("/list/insert", test_list_insert);
  g_test_add_func ("/list/free-full", test_free_full);
  g_test_add_func ("/list/copy", test_list_copy);
  g_test_add_func ("/list/copy-deep", test_list_copy_deep);
  g_test_add_func ("/list/delete-link", test_delete_link);
  g_test_add_func ("/list/prepend", test_prepend);
  g_test_add_func ("/list/position", test_position);
  g_test_add_func ("/list/double-free", test_double_free);

  return g_test_run ();
}
