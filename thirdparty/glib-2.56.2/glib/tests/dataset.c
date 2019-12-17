#include <glib.h>
#include <stdlib.h>

static void
test_quark_basic (void)
{
  GQuark quark;
  const gchar *orig = "blargh";
  gchar *copy;
  const gchar *str;

  quark = g_quark_try_string ("no-such-quark");
  g_assert (quark == 0);

  copy = g_strdup (orig);
  quark = g_quark_from_static_string (orig);
  g_assert (quark != 0);
  g_assert (g_quark_from_string (orig) == quark);
  g_assert (g_quark_from_string (copy) == quark);
  g_assert (g_quark_try_string (orig) == quark);

  str = g_quark_to_string (quark);
  g_assert_cmpstr (str, ==, orig);

  g_free (copy);
}

static void
test_quark_string (void)
{
  const gchar *orig = "string1";
  gchar *copy;
  const gchar *str1;
  const gchar *str2;

  copy = g_strdup (orig);

  str1 = g_intern_static_string (orig);
  str2 = g_intern_string (copy);
  g_assert (str1 == str2);
  g_assert (str1 == orig);

  g_free (copy);
}

static void
test_dataset_basic (void)
{
  gpointer location = (gpointer)test_dataset_basic;
  gpointer other = (gpointer)test_quark_basic;
  gpointer data = "test1";
  gpointer ret;

  g_dataset_set_data (location, "test1", data);

  ret = g_dataset_get_data (location, "test1");
  g_assert (ret == data);

  ret = g_dataset_get_data (location, "test2");
  g_assert (ret == NULL);

  ret = g_dataset_get_data (other, "test1");
  g_assert (ret == NULL);

  g_dataset_set_data (location, "test1", "new-value");
  ret = g_dataset_get_data (location, "test1");
  g_assert (ret != data);

  g_dataset_remove_data (location, "test1");
  ret = g_dataset_get_data (location, "test1");
  g_assert (ret == NULL);

  ret = g_dataset_get_data (location, NULL);
  g_assert (ret == NULL);
}

static gint destroy_count;

static void
notify (gpointer data)
{
  destroy_count++;
}

static void
test_dataset_full (void)
{
  gpointer location = (gpointer)test_dataset_full;

  g_dataset_set_data_full (location, "test1", "test1", notify);

  destroy_count = 0;
  g_dataset_set_data (location, "test1", NULL);
  g_assert (destroy_count == 1);

  g_dataset_set_data_full (location, "test1", "test1", notify);

  destroy_count = 0;
  g_dataset_remove_data (location, "test1");
  g_assert (destroy_count == 1);

  g_dataset_set_data_full (location, "test1", "test1", notify);

  destroy_count = 0;
  g_dataset_remove_no_notify (location, "test1");
  g_assert (destroy_count == 0);
}

static void
foreach (GQuark   id,
         gpointer data,
         gpointer user_data)
{
  gint *counter = user_data;

  *counter += 1;
}

static void
test_dataset_foreach (void)
{
  gpointer location = (gpointer)test_dataset_foreach;
  gint my_count;

  my_count = 0;
  g_dataset_set_data_full (location, "test1", "test1", notify);
  g_dataset_set_data_full (location, "test2", "test2", notify);
  g_dataset_set_data_full (location, "test3", "test3", notify);
  g_dataset_foreach (location, foreach, &my_count);
  g_assert (my_count == 3);

  g_dataset_destroy (location);
}

static void
test_dataset_destroy (void)
{
  gpointer location = (gpointer)test_dataset_destroy;

  destroy_count = 0;
  g_dataset_set_data_full (location, "test1", "test1", notify);
  g_dataset_set_data_full (location, "test2", "test2", notify);
  g_dataset_set_data_full (location, "test3", "test3", notify);
  g_dataset_destroy (location);
  g_assert (destroy_count == 3);
}

static void
test_dataset_id (void)
{
  gpointer location = (gpointer)test_dataset_id;
  gpointer other = (gpointer)test_quark_basic;
  gpointer data = "test1";
  gpointer ret;
  GQuark quark;

  quark = g_quark_from_string ("test1");

  g_dataset_id_set_data (location, quark, data);

  ret = g_dataset_id_get_data (location, quark);
  g_assert (ret == data);

  ret = g_dataset_id_get_data (location, g_quark_from_string ("test2"));
  g_assert (ret == NULL);

  ret = g_dataset_id_get_data (other, quark);
  g_assert (ret == NULL);

  g_dataset_id_set_data (location, quark, "new-value");
  ret = g_dataset_id_get_data (location, quark);
  g_assert (ret != data);

  g_dataset_id_remove_data (location, quark);
  ret = g_dataset_id_get_data (location, quark);
  g_assert (ret == NULL);

  ret = g_dataset_id_get_data (location, 0);
  g_assert (ret == NULL);
}

static GData *list;

static void
free_one (gpointer data)
{
  /* recurse */
  g_datalist_clear (&list);
}

static void
test_datalist_clear (void)
{
  /* Need to use a subprocess because it will deadlock if it fails */
  if (g_test_subprocess ())
    {
      g_datalist_init (&list);
      g_datalist_set_data_full (&list, "one", GINT_TO_POINTER (1), free_one);
      g_datalist_set_data_full (&list, "two", GINT_TO_POINTER (2), NULL);
      g_datalist_clear (&list);
      g_assert (list == NULL);
      return;
    }

  g_test_trap_subprocess (NULL, 500000, 0);
  g_test_trap_assert_passed ();
}

static void
test_datalist_basic (void)
{
  GData *list = NULL;
  gpointer data;
  gpointer ret;

  g_datalist_init (&list);
  data = "one";
  g_datalist_set_data (&list, "one", data);
  ret = g_datalist_get_data (&list, "one");
  g_assert (ret == data);

  ret = g_datalist_get_data (&list, "two");
  g_assert (ret == NULL);

  ret = g_datalist_get_data (&list, NULL);
  g_assert (ret == NULL);

  g_datalist_clear (&list);
}

static void
test_datalist_id (void)
{
  GData *list = NULL;
  gpointer data;
  gpointer ret;

  g_datalist_init (&list);
  data = "one";
  g_datalist_id_set_data (&list, g_quark_from_string ("one"), data);
  ret = g_datalist_id_get_data (&list, g_quark_from_string ("one"));
  g_assert (ret == data);

  ret = g_datalist_id_get_data (&list, g_quark_from_string ("two"));
  g_assert (ret == NULL);

  ret = g_datalist_id_get_data (&list, 0);
  g_assert (ret == NULL);

  g_datalist_clear (&list);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/quark/basic", test_quark_basic);
  g_test_add_func ("/quark/string", test_quark_string);
  g_test_add_func ("/dataset/basic", test_dataset_basic);
  g_test_add_func ("/dataset/id", test_dataset_id);
  g_test_add_func ("/dataset/full", test_dataset_full);
  g_test_add_func ("/dataset/foreach", test_dataset_foreach);
  g_test_add_func ("/dataset/destroy", test_dataset_destroy);
  g_test_add_func ("/datalist/basic", test_datalist_basic);
  g_test_add_func ("/datalist/id", test_datalist_id);
  g_test_add_func ("/datalist/recursive-clear", test_datalist_clear);

  return g_test_run ();
}
