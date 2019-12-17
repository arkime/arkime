#include <glib.h>

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
 * gslist sort tests
 */
static void
test_slist_sort (void)
{
  GSList *slist = NULL;
  gint    i;

  for (i = 0; i < SIZE; i++)
    slist = g_slist_append (slist, GINT_TO_POINTER (array[i]));

  slist = g_slist_sort (slist, sort);
  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_slist_nth_data (slist, i);
      p2 = g_slist_nth_data (slist, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_slist_free (slist);
}

static void
test_slist_sort_with_data (void)
{
  GSList *slist = NULL;
  gint    i;

  for (i = 0; i < SIZE; i++)
    slist = g_slist_append (slist, GINT_TO_POINTER (array[i]));

  slist = g_slist_sort_with_data (slist, (GCompareDataFunc)sort, NULL);
  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_slist_nth_data (slist, i);
      p2 = g_slist_nth_data (slist, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_slist_free (slist);
}

/* Test that the sort is stable. */
static void
test_slist_sort_stable (void)
{
  GSList *list = NULL;  /* (element-type utf8) */
  GSList *copy = NULL;  /* (element-type utf8) */
  gsize i;

  /* Build a test list, already ordered. */
  for (i = 0; i < SIZE; i++)
    list = g_slist_append (list, g_strdup_printf ("%" G_GSIZE_FORMAT, i / 5));

  /* Take a copy and sort it. */
  copy = g_slist_copy (list);
  copy = g_slist_sort (copy, (GCompareFunc) g_strcmp0);

  /* Compare the two lists, checking pointers are equal to ensure the elements
   * have been kept stable. */
  for (i = 0; i < SIZE; i++)
    {
      gpointer p1, p2;

      p1 = g_slist_nth_data (list, i);
      p2 = g_slist_nth_data (list, i);

      g_assert (p1 == p2);
    }

  g_slist_free (copy);
  g_slist_free_full (list, g_free);
}

static void
test_slist_insert_sorted (void)
{
  GSList *slist = NULL;
  gint    i;

  for (i = 0; i < SIZE; i++)
    slist = g_slist_insert_sorted (slist, GINT_TO_POINTER (array[i]), sort);

  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_slist_nth_data (slist, i);
      p2 = g_slist_nth_data (slist, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_slist_free (slist);
}

static void
test_slist_insert_sorted_with_data (void)
{
  GSList *slist = NULL;
  gint    i;

  for (i = 0; i < SIZE; i++)
    slist = g_slist_insert_sorted_with_data (slist,
                                           GINT_TO_POINTER (array[i]),
                                           (GCompareDataFunc)sort,
                                           NULL);

  for (i = 0; i < SIZE - 1; i++)
    {
      gpointer p1, p2;

      p1 = g_slist_nth_data (slist, i);
      p2 = g_slist_nth_data (slist, i+1);

      g_assert (GPOINTER_TO_INT (p1) <= GPOINTER_TO_INT (p2));
    }

  g_slist_free (slist);
}

static void
test_slist_reverse (void)
{
  GSList *slist = NULL;
  GSList *st;
  gint    nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint    i;

  for (i = 0; i < 10; i++)
    slist = g_slist_append (slist, &nums[i]);

  slist = g_slist_reverse (slist);

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      g_assert (*((gint*) st->data) == (9 - i));
    }

  g_slist_free (slist);
}

static void
test_slist_nth (void)
{
  GSList *slist = NULL;
  GSList *st;
  gint    nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint    i;

  for (i = 0; i < 10; i++)
    slist = g_slist_append (slist, &nums[i]);

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      g_assert (*((gint*) st->data) == i);
    }

  g_slist_free (slist);
}

static void
test_slist_remove (void)
{
  GSList *slist = NULL;
  GSList *st;
  gint    nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint    i;

  for (i = 0; i < 10; i++)
    {
      slist = g_slist_append (slist, &nums[i]);
      slist = g_slist_append (slist, &nums[i]);
    }

  g_assert_cmpint (g_slist_length (slist), ==, 20);

  for (i = 0; i < 10; i++)
    {
      slist = g_slist_remove (slist, &nums[i]);
    }

  g_assert_cmpint (g_slist_length (slist), ==, 10);

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      g_assert (*((gint*) st->data) == i);
    }

  g_slist_free (slist);
}

static void
test_slist_remove_all (void)
{
  GSList *slist = NULL;
  gint    nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint    i;

  for (i = 0; i < 10; i++)
    {
      slist = g_slist_append (slist, &nums[i]);
      slist = g_slist_append (slist, &nums[i]);
    }

  g_assert_cmpint (g_slist_length (slist), ==, 20);

  for (i = 0; i < 5; i++)
    {
      slist = g_slist_remove_all (slist, &nums[2 * i + 1]);
      slist = g_slist_remove_all (slist, &nums[8 - 2 * i]);
    }

  g_assert_cmpint (g_slist_length (slist), ==, 0);
  g_assert (slist == NULL);
}

static void
test_slist_insert (void)
{
  GSList *slist = NULL;
  GSList *st;
  gint   nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint   i;

  slist = g_slist_insert_before (NULL, NULL, &nums[1]);
  slist = g_slist_insert (slist, &nums[3], 1);
  slist = g_slist_insert (slist, &nums[4], -1);
  slist = g_slist_insert (slist, &nums[0], 0);
  slist = g_slist_insert (slist, &nums[5], 100);
  slist = g_slist_insert_before (slist, NULL, &nums[6]);
  slist = g_slist_insert_before (slist, slist->next->next, &nums[2]);

  slist = g_slist_insert (slist, &nums[9], 7);
  slist = g_slist_insert (slist, &nums[8], 7);
  slist = g_slist_insert (slist, &nums[7], 7);

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      g_assert (*((gint*) st->data) == i);
    }

  g_slist_free (slist);

  slist = g_slist_insert (NULL, "a", 1);
  g_assert (slist->data == (gpointer)"a");
  g_assert (slist->next == NULL);
  g_slist_free (slist);

  slist = g_slist_append (NULL, "a");
  slist = g_slist_append (slist, "b");
  slist = g_slist_insert (slist, "c", 5);

  g_assert (slist->next->next->data == (gpointer)"c");
  g_assert (slist->next->next->next == NULL);
  g_slist_free (slist);

  slist = g_slist_append (NULL, "a");
  slist = g_slist_insert_before (slist, slist, "b");
  g_assert (slist->data == (gpointer)"b");
  g_assert (slist->next->data == (gpointer)"a");
  g_assert (slist->next->next == NULL);
  g_slist_free (slist);
}

static gint
find_num (gconstpointer l, gconstpointer data)
{
  return *(gint*)l - GPOINTER_TO_INT(data);
}

static void
test_slist_position (void)
{
  GSList *slist = NULL;
  GSList *st;
  gint    nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint    i;

  for (i = 0; i < 10; i++)
    {
      slist = g_slist_append (slist, &nums[i]);
    }

  g_assert_cmpint (g_slist_index (slist, NULL), ==, -1);
  g_assert_cmpint (g_slist_position (slist, NULL), ==, -1);

  for (i = 0; i < 10; i++)
    {
      g_assert_cmpint (g_slist_index (slist, &nums[i]), ==, i);
      st = g_slist_find_custom (slist, GINT_TO_POINTER(i), find_num);
      g_assert (st != NULL);
      g_assert_cmpint (g_slist_position (slist, st), ==, i);
    }

  st = g_slist_find_custom (slist, GINT_TO_POINTER (1000), find_num);
  g_assert (st == NULL);

  g_slist_free (slist);
}

static void
test_slist_concat (void)
{
  GSList *s1, *s2, *s;

  s1 = g_slist_append (NULL, "a");
  s2 = g_slist_append (NULL, "b");
  s = g_slist_concat (s1, s2);
  g_assert (s->data == (gpointer)"a");
  g_assert (s->next->data == (gpointer)"b");
  g_assert (s->next->next == NULL);
  g_slist_free (s);

  s1 = g_slist_append (NULL, "a");

  s = g_slist_concat (NULL, s1);
  g_assert_cmpint (g_slist_length (s), ==, 1);
  s = g_slist_concat (s1, NULL);
  g_assert_cmpint (g_slist_length (s), ==, 1);

  g_slist_free (s);

  s = g_slist_concat (NULL, NULL);
  g_assert (s == NULL);
}

static void
test_slist_copy (void)
{
  GSList *slist = NULL, *copy;
  GSList *s1, *s2;
  guint nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gsize i;

  /* Copy and test a many-element list. */
  for (i = 0; i < 10; i++)
    slist = g_slist_append (slist, &nums[i]);

  copy = g_slist_copy (slist);

  g_assert_cmpuint (g_slist_length (copy), ==, g_slist_length (slist));

  for (s1 = copy, s2 = slist; s1 != NULL && s2 != NULL; s1 = s1->next, s2 = s2->next)
    g_assert (s1->data == s2->data);

  g_slist_free (copy);
  g_slist_free (slist);

  /* Copy a NULL list. */
  copy = g_slist_copy (NULL);
  g_assert_null (copy);
}

static gpointer
copy_and_count_string (gconstpointer src,
                       gpointer      data)
{
  const gchar *str = src;
  gsize *count = data;

  *count = *count + 1;
  return g_strdup (str);
}

static void
test_slist_copy_deep (void)
{
  GSList *slist = NULL, *copy;
  GSList *s1, *s2;
  gsize count;

  /* Deep-copy a simple list. */
  slist = g_slist_append (slist, "a");
  slist = g_slist_append (slist, "b");
  slist = g_slist_append (slist, "c");

  count = 0;
  copy = g_slist_copy_deep (slist, copy_and_count_string, &count);

  g_assert_cmpuint (count, ==, g_slist_length (slist));
  g_assert_cmpuint (g_slist_length (copy), ==, count);
  for (s1 = slist, s2 = copy; s1 != NULL && s2 != NULL; s1 = s1->next, s2 = s2->next)
    {
      g_assert_cmpstr (s1->data, ==, s2->data);
      g_assert (s1->data != s2->data);
    }

  g_slist_free_full (copy, g_free);
  g_slist_free (slist);

  /* Try with an empty list. */
  count = 0;
  copy = g_slist_copy_deep (NULL, copy_and_count_string, &count);
  g_assert_cmpuint (count, ==, 0);
  g_assert_null (copy);
}

int
main (int argc, char *argv[])
{
  gint i;

  g_test_init (&argc, &argv, NULL);

  /* Create an array of random numbers. */
  for (i = 0; i < SIZE; i++)
    array[i] = g_test_rand_int_range (NUMBER_MIN, NUMBER_MAX);

  g_test_add_func ("/slist/sort", test_slist_sort);
  g_test_add_func ("/slist/sort-with-data", test_slist_sort_with_data);
  g_test_add_func ("/slist/sort/stable", test_slist_sort_stable);
  g_test_add_func ("/slist/insert-sorted", test_slist_insert_sorted);
  g_test_add_func ("/slist/insert-sorted-with-data", test_slist_insert_sorted_with_data);
  g_test_add_func ("/slist/reverse", test_slist_reverse);
  g_test_add_func ("/slist/nth", test_slist_nth);
  g_test_add_func ("/slist/remove", test_slist_remove);
  g_test_add_func ("/slist/remove-all", test_slist_remove_all);
  g_test_add_func ("/slist/insert", test_slist_insert);
  g_test_add_func ("/slist/position", test_slist_position);
  g_test_add_func ("/slist/concat", test_slist_concat);
  g_test_add_func ("/slist/copy", test_slist_copy);
  g_test_add_func ("/slist/copy/deep", test_slist_copy_deep);

  return g_test_run ();
}
