#include <glib.h>
#include <string.h>

static void
test_autofree (void)
{
  g_autofree gchar *p = NULL;
  g_autofree gchar *p2 = NULL;
  g_autofree gchar *alwaysnull = NULL;

  p = g_malloc (10);
  p2 = g_malloc (42);

  if (TRUE)
    {
      g_autofree guint8 *buf = g_malloc (128);
      g_autofree gchar *alwaysnull_again = NULL;

      buf[0] = 1;

      g_assert_null (alwaysnull_again);
    }

  if (TRUE)
    {
      g_autofree guint8 *buf2 = g_malloc (256);

      buf2[255] = 42;
    }

  g_assert_null (alwaysnull);
}

static void
test_g_async_queue (void)
{
  g_autoptr(GAsyncQueue) val = g_async_queue_new ();
  g_assert (val != NULL);
}

static void
test_g_bookmark_file (void)
{
  g_autoptr(GBookmarkFile) val = g_bookmark_file_new ();
  g_assert (val != NULL);
}

static void
test_g_bytes (void)
{
  g_autoptr(GBytes) val = g_bytes_new ("foo", 3);
  g_assert (val != NULL);
}

static void
test_g_checksum (void)
{
  g_autoptr(GChecksum) val = g_checksum_new (G_CHECKSUM_SHA256);
  g_assert (val != NULL);
}

static void
test_g_date_time (void)
{
  g_autoptr(GDateTime) val = g_date_time_new_now_utc ();
  g_assert (val != NULL);
}

static void
test_g_dir (void)
{
  g_autoptr(GDir) val = g_dir_open (".", 0, NULL);
  g_assert (val != NULL);
}

static void
test_g_error (void)
{
  g_autoptr(GError) val = g_error_new_literal (G_FILE_ERROR, G_FILE_ERROR_FAILED, "oops");
  g_assert (val != NULL);
}

static void
test_g_hash_table (void)
{
  g_autoptr(GHashTable) val = g_hash_table_new (NULL, NULL);
  g_assert (val != NULL);
}

static void
test_g_hmac (void)
{
  g_autoptr(GHmac) val = g_hmac_new (G_CHECKSUM_SHA256, (guint8*)"hello", 5);
  g_assert (val != NULL);
}

static void
test_g_io_channel (void)
{
  g_autoptr(GIOChannel) val = g_io_channel_new_file ("/dev/null", "r", NULL);
  g_assert (val != NULL);
}

static void
test_g_key_file (void)
{
  g_autoptr(GKeyFile) val = g_key_file_new ();
  g_assert (val != NULL);
}

static void
test_g_list (void)
{
  g_autoptr(GList) val = NULL;
  g_autoptr(GList) val2 = g_list_prepend (NULL, "foo");
  g_assert (val == NULL);
  g_assert (val2 != NULL);
}

static void
test_g_array (void)
{
  g_autoptr(GArray) val = g_array_new (0, 0, sizeof (gpointer));
  g_assert (val != NULL);
}

static void
test_g_ptr_array (void)
{
  g_autoptr(GPtrArray) val = g_ptr_array_new ();
  g_assert (val != NULL);
}

static void
test_g_byte_array (void)
{
  g_autoptr(GByteArray) val = g_byte_array_new ();
  g_assert (val != NULL);
}

static void
test_g_main_context (void)
{
  g_autoptr(GMainContext) val = g_main_context_new ();
  g_assert (val != NULL);
}

static void
test_g_main_loop (void)
{
  g_autoptr(GMainLoop) val = g_main_loop_new (NULL, TRUE);
  g_assert (val != NULL);
}

static void
test_g_source (void)
{
  g_autoptr(GSource) val = g_timeout_source_new_seconds (2);
  g_assert (val != NULL);
}

static void
test_g_mapped_file (void)
{
  g_autoptr(GMappedFile) val = g_mapped_file_new (g_test_get_filename (G_TEST_DIST, "keyfiletest.ini", NULL), FALSE, NULL);
  g_assert (val != NULL);
}

static void
parser_start (GMarkupParseContext  *context,
              const gchar          *element_name,
              const gchar         **attribute_names,
              const gchar         **attribute_values,
              gpointer              user_data,
              GError              **error)
{
}

static void
parser_end (GMarkupParseContext  *context,
            const gchar          *element_name,
            gpointer              user_data,
            GError              **error)
{
}

static GMarkupParser parser = {
  .start_element = parser_start,
  .end_element = parser_end
};

static void
test_g_markup_parse_context (void)
{
  g_autoptr(GMarkupParseContext) val = g_markup_parse_context_new (&parser,  0, NULL, NULL);
  g_assert (val != NULL);
}

static void
test_g_node (void)
{
  g_autoptr(GNode) val = g_node_new ("hello");
  g_assert (val != NULL);
}

static void
test_g_option_context (void)
{
  g_autoptr(GOptionContext) val = g_option_context_new ("hello");
  g_assert (val != NULL);
}

static void
test_g_option_group (void)
{
  g_autoptr(GOptionGroup) val = g_option_group_new ("hello", "world", "helpme", NULL, NULL);
  g_assert (val != NULL);
}

static void
test_g_pattern_spec (void)
{
  g_autoptr(GPatternSpec) val = g_pattern_spec_new ("plaid");
  g_assert (val != NULL);
}

static void
test_g_queue (void)
{
  g_autoptr(GQueue) val = g_queue_new ();
  g_auto(GQueue) stackval = G_QUEUE_INIT;
  g_assert (val != NULL);
  g_assert_null (stackval.head);
}

static void
test_g_rand (void)
{
  g_autoptr(GRand) val = g_rand_new ();
  g_assert (val != NULL);
}

static void
test_g_regex (void)
{
  g_autoptr(GRegex) val = g_regex_new (".*", 0, 0, NULL);
  g_assert (val != NULL);
}

static void
test_g_match_info (void)
{
  g_autoptr(GRegex) regex = g_regex_new (".*", 0, 0, NULL);
  g_autoptr(GMatchInfo) match = NULL;

  if (!g_regex_match (regex, "hello", 0, &match))
    g_assert_not_reached ();
}

static void
test_g_scanner (void)
{
  GScannerConfig config = { 0, };
  g_autoptr(GScanner) val = g_scanner_new (&config);
  g_assert (val != NULL);
}

static void
test_g_sequence (void)
{
  g_autoptr(GSequence) val = g_sequence_new (NULL);
  g_assert (val != NULL);
}

static void
test_g_slist (void)
{
  g_autoptr(GSList) val = NULL;
  g_autoptr(GSList) nonempty_val = g_slist_prepend (NULL, "hello");
  g_assert (val == NULL);
  g_assert (nonempty_val != NULL);
}

static void
test_g_string (void)
{
  g_autoptr(GString) val = g_string_new ("");
  g_assert (val != NULL);
}

static void
test_g_string_chunk (void)
{
  g_autoptr(GStringChunk) val = g_string_chunk_new (42);
  g_assert (val != NULL);
}

static gpointer
mythread (gpointer data)
{
  g_usleep (G_USEC_PER_SEC);
  return NULL;
}

static void
test_g_thread (void)
{
  g_autoptr(GThread) val = g_thread_new ("bob", mythread, NULL);
  g_assert (val != NULL);
}

static void
test_g_mutex (void)
{
  g_auto(GMutex) val;
  
  g_mutex_init (&val);
}

static void
test_g_mutex_locker (void)
{
  GMutex mutex;

  g_mutex_init (&mutex);

  if (TRUE)
    {
      g_autoptr(GMutexLocker) val = g_mutex_locker_new (&mutex);
      
      g_assert (val != NULL);
    }
}

static void
test_g_cond (void)
{
  g_auto(GCond) val;
  g_cond_init (&val);
}

static void
test_g_timer (void)
{
  g_autoptr(GTimer) val = g_timer_new ();
  g_assert (val != NULL);
}

static void
test_g_time_zone (void)
{
  g_autoptr(GTimeZone) val = g_time_zone_new ("UTC");
  g_assert (val != NULL);
}

static void
test_g_tree (void)
{
  g_autoptr(GTree) val = g_tree_new ((GCompareFunc)strcmp);
  g_assert (val != NULL);
}

static void
test_g_variant (void)
{
  g_autoptr(GVariant) val = g_variant_new_string ("hello");
  g_assert (val != NULL);
}

static void
test_g_variant_builder (void)
{
  g_autoptr(GVariantBuilder) val = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  g_auto(GVariantBuilder) stackval;

  g_assert (val != NULL);
  g_variant_builder_init (&stackval, G_VARIANT_TYPE ("as"));
}

static void
test_g_variant_iter (void)
{
  g_autoptr(GVariant) var = g_variant_new_fixed_array (G_VARIANT_TYPE_UINT32, "", 0, sizeof(guint32));
  g_autoptr(GVariantIter) val = g_variant_iter_new (var);
  g_assert (val != NULL);
}

static void
test_g_variant_dict (void)
{
  g_autoptr(GVariant) data = g_variant_new_from_data (G_VARIANT_TYPE ("a{sv}"), "", 0, FALSE, NULL, NULL);
  g_auto(GVariantDict) stackval;
  g_autoptr(GVariantDict) val = g_variant_dict_new (data);

  g_variant_dict_init (&stackval, data);
  g_assert (val != NULL);
}

static void
test_g_variant_type (void)
{
  g_autoptr(GVariantType) val = g_variant_type_new ("s");
  g_assert (val != NULL);
}

static void
test_strv (void)
{
  g_auto(GStrv) val = g_strsplit("a:b:c", ":", -1);
  g_assert (val != NULL);
}

static void
mark_freed (gpointer ptr)
{
  gboolean *freed = ptr;
  *freed = TRUE;
}

static void
test_autolist (void)
{
  char data[1] = {0};
  gboolean freed1 = FALSE;
  gboolean freed2 = FALSE;
  gboolean freed3 = FALSE;
  GBytes *b1 = g_bytes_new_with_free_func (data, sizeof(data), mark_freed, &freed1);
  GBytes *b2 = g_bytes_new_with_free_func (data, sizeof(data), mark_freed, &freed2);
  GBytes *b3 = g_bytes_new_with_free_func (data, sizeof(data), mark_freed, &freed3);

  {
    g_autolist(GBytes) l = NULL;

    l = g_list_prepend (l, b1);
    l = g_list_prepend (l, b3);
  }

  /* Only assert if autoptr works */
#ifdef __GNUC__
  g_assert (freed1);
  g_assert (freed3);
#endif
  g_assert (!freed2);

  g_bytes_unref (b2);
  g_assert (freed2);
}

static void
test_autoslist (void)
{
  char data[1] = {0};
  gboolean freed1 = FALSE;
  gboolean freed2 = FALSE;
  gboolean freed3 = FALSE;
  GBytes *b1 = g_bytes_new_with_free_func (data, sizeof(data), mark_freed, &freed1);
  GBytes *b2 = g_bytes_new_with_free_func (data, sizeof(data), mark_freed, &freed2);
  GBytes *b3 = g_bytes_new_with_free_func (data, sizeof(data), mark_freed, &freed3);

  {
    g_autoslist(GBytes) l = NULL;

    l = g_slist_prepend (l, b1);
    l = g_slist_prepend (l, b3);
  }

  /* Only assert if autoptr works */
#ifdef __GNUC__
  g_assert (freed1);
  g_assert (freed3);
#endif
  g_assert (!freed2);

  g_bytes_unref (b2);
  g_assert (freed2);
}

int
main (int argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/autoptr/autofree", test_autofree);
  g_test_add_func ("/autoptr/g_async_queue", test_g_async_queue);
  g_test_add_func ("/autoptr/g_bookmark_file", test_g_bookmark_file);
  g_test_add_func ("/autoptr/g_bytes", test_g_bytes);
  g_test_add_func ("/autoptr/g_checksum", test_g_checksum);
  g_test_add_func ("/autoptr/g_date_time", test_g_date_time);
  g_test_add_func ("/autoptr/g_dir", test_g_dir);
  g_test_add_func ("/autoptr/g_error", test_g_error);
  g_test_add_func ("/autoptr/g_hash_table", test_g_hash_table);
  g_test_add_func ("/autoptr/g_hmac", test_g_hmac);
  g_test_add_func ("/autoptr/g_io_channel", test_g_io_channel);
  g_test_add_func ("/autoptr/g_key_file", test_g_key_file);
  g_test_add_func ("/autoptr/g_list", test_g_list);
  g_test_add_func ("/autoptr/g_array", test_g_array);
  g_test_add_func ("/autoptr/g_ptr_array", test_g_ptr_array);
  g_test_add_func ("/autoptr/g_byte_array", test_g_byte_array);
  g_test_add_func ("/autoptr/g_main_context", test_g_main_context);
  g_test_add_func ("/autoptr/g_main_loop", test_g_main_loop);
  g_test_add_func ("/autoptr/g_source", test_g_source);
  g_test_add_func ("/autoptr/g_mapped_file", test_g_mapped_file);
  g_test_add_func ("/autoptr/g_markup_parse_context", test_g_markup_parse_context);
  g_test_add_func ("/autoptr/g_node", test_g_node);
  g_test_add_func ("/autoptr/g_option_context", test_g_option_context);
  g_test_add_func ("/autoptr/g_option_group", test_g_option_group);
  g_test_add_func ("/autoptr/g_pattern_spec", test_g_pattern_spec);
  g_test_add_func ("/autoptr/g_queue", test_g_queue);
  g_test_add_func ("/autoptr/g_rand", test_g_rand);
  g_test_add_func ("/autoptr/g_regex", test_g_regex);
  g_test_add_func ("/autoptr/g_match_info", test_g_match_info);
  g_test_add_func ("/autoptr/g_scanner", test_g_scanner);
  g_test_add_func ("/autoptr/g_sequence", test_g_sequence);
  g_test_add_func ("/autoptr/g_slist", test_g_slist);
  g_test_add_func ("/autoptr/g_string", test_g_string);
  g_test_add_func ("/autoptr/g_string_chunk", test_g_string_chunk);
  g_test_add_func ("/autoptr/g_thread", test_g_thread);
  g_test_add_func ("/autoptr/g_mutex", test_g_mutex);
  g_test_add_func ("/autoptr/g_mutex_locker", test_g_mutex_locker);
  g_test_add_func ("/autoptr/g_cond", test_g_cond);
  g_test_add_func ("/autoptr/g_timer", test_g_timer);
  g_test_add_func ("/autoptr/g_time_zone", test_g_time_zone);
  g_test_add_func ("/autoptr/g_tree", test_g_tree);
  g_test_add_func ("/autoptr/g_variant", test_g_variant);
  g_test_add_func ("/autoptr/g_variant_builder", test_g_variant_builder);
  g_test_add_func ("/autoptr/g_variant_iter", test_g_variant_iter);
  g_test_add_func ("/autoptr/g_variant_dict", test_g_variant_dict);
  g_test_add_func ("/autoptr/g_variant_type", test_g_variant_type);
  g_test_add_func ("/autoptr/strv", test_strv);
  g_test_add_func ("/autoptr/autolist", test_autolist);
  g_test_add_func ("/autoptr/autoslist", test_autoslist);

  return g_test_run ();
}
