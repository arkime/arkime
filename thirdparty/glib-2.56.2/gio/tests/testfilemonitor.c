#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <gio/gio.h>

/* These tests were written for the inotify implementation.
 * Other implementations may require slight adjustments in
 * the tests, e.g. the length of timeouts
 */

typedef struct
{
  gint event_type;
  gchar *file;
  gchar *other_file;
  gint step;
} RecordedEvent;

static void
free_recorded_event (RecordedEvent *event)
{
  g_free (event->file);
  g_free (event->other_file);
  g_free (event);
}

typedef struct
{
  GFile *file;
  GFileMonitor *monitor;
  GMainLoop *loop;
  gint step;
  GList *events;
  GFileOutputStream *output_stream;
} TestData;

#if 0
static void
output_event (RecordedEvent *event)
{
  if (event->step >= 0)
    g_print (">>>> step %d\n", event->step);
  else
    {
      GTypeClass *class;

      class = g_type_class_ref (g_type_from_name ("GFileMonitorEvent"));
      g_print ("%s file=%s other_file=%s\n",
               g_enum_get_value (G_ENUM_CLASS (class), event->event_type)->value_nick,
               event->file,
               event->other_file);
      g_type_class_unref (class);
    }
}

static void
output_events (GList *list)
{
  GList *l;

  g_print (">>>output events\n");
  for (l = list; l; l = l->next)
    output_event ((RecordedEvent *)l->data);
}
#endif

/* a placeholder for temp file names we don't want to compare */
static const gchar DONT_CARE[] = "";

static void
check_expected_event (gint           i,
                      RecordedEvent *e1,
                      RecordedEvent *e2)
{
  g_assert_cmpint (e1->step, ==, e2->step);
  if (e1->step < 0)
    return;

  g_assert_cmpint (e1->event_type, ==, e2->event_type);

  if (e1->file != DONT_CARE)
    g_assert_cmpstr (e1->file, ==, e2->file);

  if (e1->other_file != DONT_CARE)
    g_assert_cmpstr (e1->other_file, ==, e2->other_file);
}

static void
check_expected_events (RecordedEvent *expected,
                       gsize          n_expected,
                       GList         *recorded)
{
  gint i;
  GList *l;

  g_assert_cmpint (n_expected, ==, g_list_length (recorded));

  for (i = 0, l = recorded; i < n_expected; i++, l = l->next)
    {
      RecordedEvent *e1 = &expected[i];
      RecordedEvent *e2 = (RecordedEvent *)l->data;

      check_expected_event (i, e1, e2);
    }
}

static void
record_event (TestData    *data,
              gint         event_type,
              const gchar *file,
              const gchar *other_file,
              gint         step)
{
  RecordedEvent *event;

  event = g_new0 (RecordedEvent, 1);
  event->event_type = event_type;
  event->file = g_strdup (file);
  event->other_file = g_strdup (other_file);
  event->step = step;

  data->events = g_list_append (data->events, event);
}

static void
monitor_changed (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
  TestData *data = user_data;
  gchar *basename, *other_base;

  basename = g_file_get_basename (file);
  if (other_file)
    other_base = g_file_get_basename (other_file);
  else
    other_base = NULL;

  record_event (data, event_type, basename, other_base, -1);

  g_free (basename);
  g_free (other_base);
}

static gboolean
atomic_replace_step (gpointer user_data)
{
  TestData *data = user_data;
  GError *error = NULL;

  switch (data->step)
    {
    case 0:
      record_event (data, -1, NULL, NULL, 0);
      g_file_replace_contents (data->file, "step 0", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      break;
    case 1:
      record_event (data, -1, NULL, NULL, 1);
      g_file_replace_contents (data->file, "step 1", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      break;
    case 2:
      record_event (data, -1, NULL, NULL, 2);
      g_file_delete (data->file, NULL, NULL);
      break;
    case 3:
      record_event (data, -1, NULL, NULL, 3);
      g_main_loop_quit (data->loop);
      return G_SOURCE_REMOVE;
    }

  data->step++;

  return G_SOURCE_CONTINUE;
}

/* this is the output we expect from the above steps */
static RecordedEvent atomic_replace_output[] = {
  { -1, NULL, NULL, 0 },
  { G_FILE_MONITOR_EVENT_CREATED, "atomic_replace_file", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGED, "atomic_replace_file", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "atomic_replace_file", NULL, -1 },
  { -1, NULL, NULL, 1 },
  { G_FILE_MONITOR_EVENT_RENAMED, (gchar*)DONT_CARE, "atomic_replace_file", -1 },
  { -1, NULL, NULL, 2 },
  { G_FILE_MONITOR_EVENT_DELETED, "atomic_replace_file", NULL, -1 },
  { -1, NULL, NULL, 3 }
};

static void
test_atomic_replace (void)
{
  GError *error = NULL;
  TestData data;

  data.step = 0;
  data.events = NULL;

  data.file = g_file_new_for_path ("atomic_replace_file");
  g_file_delete (data.file, NULL, NULL);

  data.monitor = g_file_monitor_file (data.file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);

  g_file_monitor_set_rate_limit (data.monitor, 200);
  g_signal_connect (data.monitor, "changed", G_CALLBACK (monitor_changed), &data);

  data.loop = g_main_loop_new (NULL, TRUE);

  g_timeout_add (500, atomic_replace_step, &data);

  g_main_loop_run (data.loop);

  /*output_events (data.events);*/
  check_expected_events (atomic_replace_output, G_N_ELEMENTS (atomic_replace_output), data.events);

  g_list_free_full (data.events, (GDestroyNotify)free_recorded_event);
  g_main_loop_unref (data.loop);
  g_object_unref (data.monitor);
  g_object_unref (data.file);
}

static gboolean
change_step (gpointer user_data)
{
  TestData *data = user_data;
  GOutputStream *stream;
  GError *error = NULL;
  guint32 mode = 0660;

  switch (data->step)
    {
    case 0:
      record_event (data, -1, NULL, NULL, 0);
      g_file_replace_contents (data->file, "step 0", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      break;
    case 1:
      record_event (data, -1, NULL, NULL, 1);
      stream = (GOutputStream *)g_file_append_to (data->file, G_FILE_CREATE_NONE, NULL, &error);
      g_assert_no_error (error);
      g_output_stream_write_all (stream, " step 1", 7, NULL, NULL, &error);
      g_assert_no_error (error);
      g_output_stream_close (stream, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (stream);
      break;
    case 2:
      record_event (data, -1, NULL, NULL, 2);
      g_file_set_attribute (data->file,
                            G_FILE_ATTRIBUTE_UNIX_MODE,
                            G_FILE_ATTRIBUTE_TYPE_UINT32,
                            &mode,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
      g_assert_no_error (error);
      break;
    case 3:
      record_event (data, -1, NULL, NULL, 3);
      g_file_delete (data->file, NULL, NULL);
      break;
    case 4:
      record_event (data, -1, NULL, NULL, 4);
      g_main_loop_quit (data->loop);
      return G_SOURCE_REMOVE;
    }

  data->step++;

  return G_SOURCE_CONTINUE;
}

/* this is the output we expect from the above steps */
static RecordedEvent change_output[] = {
  { -1, NULL, NULL, 0 },
  { G_FILE_MONITOR_EVENT_CREATED, "change_file", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGED, "change_file", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "change_file", NULL, -1 },
  { -1, NULL, NULL, 1 },
  { G_FILE_MONITOR_EVENT_CHANGED, "change_file", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "change_file", NULL, -1 },
  { -1, NULL, NULL, 2 },
  { G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED, "change_file", NULL, -1 },
  { -1, NULL, NULL, 3 },
  { G_FILE_MONITOR_EVENT_DELETED, "change_file", NULL, -1 },
  { -1, NULL, NULL, 4 }
};

static void
test_file_changes (void)
{
  GError *error = NULL;
  TestData data;

  data.step = 0;
  data.events = NULL;

  data.file = g_file_new_for_path ("change_file");
  g_file_delete (data.file, NULL, NULL);

  data.monitor = g_file_monitor_file (data.file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);

  g_file_monitor_set_rate_limit (data.monitor, 200);
  g_signal_connect (data.monitor, "changed", G_CALLBACK (monitor_changed), &data);

  data.loop = g_main_loop_new (NULL, TRUE);

  g_timeout_add (500, change_step, &data);

  g_main_loop_run (data.loop);

  /*output_events (data.events);*/
  check_expected_events (change_output, G_N_ELEMENTS (change_output), data.events);

  g_list_free_full (data.events, (GDestroyNotify)free_recorded_event);
  g_main_loop_unref (data.loop);
  g_object_unref (data.monitor);
  g_object_unref (data.file);
}

static gboolean
dir_step (gpointer user_data)
{
  TestData *data = user_data;
  GFile *parent, *file, *file2;
  GError *error = NULL;

  switch (data->step)
    {
    case 1:
      record_event (data, -1, NULL, NULL, 1);
      parent = g_file_get_parent (data->file);
      file = g_file_get_child (parent, "dir_test_file");
      g_file_replace_contents (file, "step 1", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (file);
      g_object_unref (parent);
      break;
    case 2:
      record_event (data, -1, NULL, NULL, 2);
      parent = g_file_get_parent (data->file);
      file = g_file_get_child (parent, "dir_test_file");
      file2 = g_file_get_child (data->file, "dir_test_file");
      g_file_move (file, file2, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (file);
      g_object_unref (file2);
      g_object_unref (parent);
      break;
    case 3:
      record_event (data, -1, NULL, NULL, 3);
      file = g_file_get_child (data->file, "dir_test_file");
      file2 = g_file_get_child (data->file, "dir_test_file2");
      g_file_move (file, file2, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (file);
      g_object_unref (file2);
      break;
    case 4:
      record_event (data, -1, NULL, NULL, 4);
      parent = g_file_get_parent (data->file);
      file = g_file_get_child (data->file, "dir_test_file2");
      file2 = g_file_get_child (parent, "dir_test_file2");
      g_file_move (file, file2, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
      g_assert_no_error (error);
      g_file_delete (file2, NULL, NULL);
      g_object_unref (file);
      g_object_unref (file2);
      g_object_unref (parent);
      break;
    case 5:
      record_event (data, -1, NULL, NULL, 5);
      g_file_delete (data->file, NULL, NULL);
      break;
    case 6:
      record_event (data, -1, NULL, NULL, 6);
      g_main_loop_quit (data->loop);
      return G_SOURCE_REMOVE;
    }

  data->step++;

  return G_SOURCE_CONTINUE;
}

/* this is the output we expect from the above steps */
static RecordedEvent dir_output[] = {
  { -1, NULL, NULL, 1 },
  { -1, NULL, NULL, 2 },
  { G_FILE_MONITOR_EVENT_MOVED_IN, "dir_test_file", NULL, -1 },
  { -1, NULL, NULL, 3 },
  { G_FILE_MONITOR_EVENT_RENAMED, "dir_test_file", "dir_test_file2", -1 },
  { -1, NULL, NULL, 4 },
  { G_FILE_MONITOR_EVENT_MOVED_OUT, "dir_test_file2", NULL, -1 },
  { -1, NULL, NULL, 5 },
  { G_FILE_MONITOR_EVENT_DELETED, "dir_monitor_test", NULL, -1 },
  { -1, NULL, NULL, 6 }
};

static void
test_dir_monitor (void)
{
  GError *error = NULL;
  TestData data;

  data.step = 0;
  data.events = NULL;

  data.file = g_file_new_for_path ("dir_monitor_test");
  g_file_delete (data.file, NULL, NULL);
  g_file_make_directory (data.file, NULL, &error);

  data.monitor = g_file_monitor_directory (data.file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);

  g_file_monitor_set_rate_limit (data.monitor, 200);
  g_signal_connect (data.monitor, "changed", G_CALLBACK (monitor_changed), &data);

  data.loop = g_main_loop_new (NULL, TRUE);

  g_timeout_add (500, dir_step, &data);

  g_main_loop_run (data.loop);

  /*output_events (data.events);*/
  check_expected_events (dir_output, G_N_ELEMENTS (dir_output), data.events);

  g_list_free_full (data.events, (GDestroyNotify)free_recorded_event);
  g_main_loop_unref (data.loop);
  g_object_unref (data.monitor);
  g_object_unref (data.file);
}

static gboolean
nodir_step (gpointer user_data)
{
  TestData *data = user_data;
  GFile *parent;
  GError *error = NULL;

  switch (data->step)
    {
    case 0:
      record_event (data, -1, NULL, NULL, 0);
      parent = g_file_get_parent (data->file);
      g_file_make_directory (parent, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (parent);
      break;
    case 1:
      record_event (data, -1, NULL, NULL, 1);
      g_file_replace_contents (data->file, "step 1", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      break;
    case 2:
      record_event (data, -1, NULL, NULL, 2);
      g_file_delete (data->file, NULL, &error);
      g_assert_no_error (error);
      break;
    case 3:
      record_event (data, -1, NULL, NULL, 3);
      parent = g_file_get_parent (data->file);
      g_file_delete (parent, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (parent);
      break;
    case 4:
      record_event (data, -1, NULL, NULL, 4);
      g_main_loop_quit (data->loop);
      return G_SOURCE_REMOVE;
    }

  data->step++;

  return G_SOURCE_CONTINUE;
}

static RecordedEvent nodir_output[] = {
  { -1, NULL, NULL, 0 },
  { G_FILE_MONITOR_EVENT_CREATED, "nosuchfile", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "nosuchfile", NULL, -1 },
  { -1, NULL, NULL, 1 },
  { G_FILE_MONITOR_EVENT_CREATED, "nosuchfile", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGED, "nosuchfile", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "nosuchfile", NULL, -1 },
  { -1, NULL, NULL, 2 },
  { G_FILE_MONITOR_EVENT_DELETED, "nosuchfile", NULL, -1 },
  { -1, NULL, NULL, 3 },
  { -1, NULL, NULL, 4 }
};

static void
test_dir_non_existent (void)
{
  TestData data;
  GError *error = NULL;

  data.step = 0;
  data.events = NULL;

  data.file = g_file_new_for_path ("nosuchdir/nosuchfile");
  data.monitor = g_file_monitor_file (data.file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);

  g_file_monitor_set_rate_limit (data.monitor, 200);
  g_signal_connect (data.monitor, "changed", G_CALLBACK (monitor_changed), &data);

  data.loop = g_main_loop_new (NULL, TRUE);

  /* we need a long timeout here, since the inotify implementation only scans
   * for missing files every 4 seconds.
   */
  g_timeout_add (5000, nodir_step, &data);

  g_main_loop_run (data.loop);

  /*output_events (data.events);*/
  check_expected_events (nodir_output, G_N_ELEMENTS (nodir_output), data.events);

  g_list_free_full (data.events, (GDestroyNotify)free_recorded_event);
  g_main_loop_unref (data.loop);
  g_object_unref (data.monitor);
  g_object_unref (data.file);
}

static gboolean
cross_dir_step (gpointer user_data)
{
  TestData *data = user_data;
  GFile *file, *file2;
  GError *error = NULL;

  switch (data[0].step)
    {
    case 0:
      record_event (&data[0], -1, NULL, NULL, 0);
      record_event (&data[1], -1, NULL, NULL, 0);
      file = g_file_get_child (data[1].file, "a");
      g_file_replace_contents (file, "step 0", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (file);
      break;
    case 1:
      record_event (&data[0], -1, NULL, NULL, 1);
      record_event (&data[1], -1, NULL, NULL, 1);
      file = g_file_get_child (data[1].file, "a");
      file2 = g_file_get_child (data[0].file, "a");
      g_file_move (file, file2, 0, NULL, NULL, NULL, &error);
      g_assert_no_error (error);
      g_object_unref (file);
      g_object_unref (file2);
      break;
    case 2:
      record_event (&data[0], -1, NULL, NULL, 2);
      record_event (&data[1], -1, NULL, NULL, 2);
      file2 = g_file_get_child (data[0].file, "a");
      g_file_delete (file2, NULL, NULL);
      g_file_delete (data[0].file, NULL, NULL);
      g_file_delete (data[1].file, NULL, NULL);
      g_object_unref (file2);
      break;
    case 3:
      record_event (&data[0], -1, NULL, NULL, 3);
      record_event (&data[1], -1, NULL, NULL, 3);
      g_main_loop_quit (data->loop);
      return G_SOURCE_REMOVE;
    }

  data->step++;

  return G_SOURCE_CONTINUE;
}

static RecordedEvent cross_dir_a_output[] = {
  { -1, NULL, NULL, 0 },
  { -1, NULL, NULL, 1 },
  { G_FILE_MONITOR_EVENT_CREATED, "a", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "a", NULL, -1 },
  { -1, NULL, NULL, 2 },
  { G_FILE_MONITOR_EVENT_DELETED, "a", NULL, -1 },
  { G_FILE_MONITOR_EVENT_DELETED, "cross_dir_a", NULL, -1 },
  { -1, NULL, NULL, 3 },
};

static RecordedEvent cross_dir_b_output[] = {
  { -1, NULL, NULL, 0 },
  { G_FILE_MONITOR_EVENT_CREATED, "a", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGED, "a", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "a", NULL, -1 },
  { -1, NULL, NULL, 1 },
  { G_FILE_MONITOR_EVENT_MOVED_OUT, "a", "a", -1 },
  { -1, NULL, NULL, 2 },
  { G_FILE_MONITOR_EVENT_DELETED, "cross_dir_b", NULL, -1 },
  { -1, NULL, NULL, 3 },
};
static void
test_cross_dir_moves (void)
{
  GError *error = NULL;
  TestData data[2];

  data[0].step = 0;
  data[0].events = NULL;

  data[0].file = g_file_new_for_path ("cross_dir_a");
  g_file_delete (data[0].file, NULL, NULL);
  g_file_make_directory (data[0].file, NULL, &error);

  data[0].monitor = g_file_monitor_directory (data[0].file, 0, NULL, &error);
  g_assert_no_error (error);

  g_file_monitor_set_rate_limit (data[0].monitor, 200);
  g_signal_connect (data[0].monitor, "changed", G_CALLBACK (monitor_changed), &data[0]);

  data[1].step = 0;
  data[1].events = NULL;

  data[1].file = g_file_new_for_path ("cross_dir_b");
  g_file_delete (data[1].file, NULL, NULL);
  g_file_make_directory (data[1].file, NULL, &error);

  data[1].monitor = g_file_monitor_directory (data[1].file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);

  g_file_monitor_set_rate_limit (data[1].monitor, 200);
  g_signal_connect (data[1].monitor, "changed", G_CALLBACK (monitor_changed), &data[1]);

  data[0].loop = g_main_loop_new (NULL, TRUE);

  g_timeout_add (500, cross_dir_step, data);

  g_main_loop_run (data[0].loop);

#if 0
  g_print ("monitor a:\n");
  output_events (data[0].events);
  g_print ("monitor b:\n");
  output_events (data[1].events);
#endif

  check_expected_events (cross_dir_a_output, G_N_ELEMENTS (cross_dir_a_output), data[0].events);
  check_expected_events (cross_dir_b_output, G_N_ELEMENTS (cross_dir_b_output), data[1].events);

  g_list_free_full (data[0].events, (GDestroyNotify)free_recorded_event);
  g_main_loop_unref (data[0].loop);
  g_object_unref (data[0].monitor);
  g_object_unref (data[0].file);

  g_list_free_full (data[1].events, (GDestroyNotify)free_recorded_event);
  g_object_unref (data[1].monitor);
  g_object_unref (data[1].file);
}

static gboolean
file_hard_links_step (gpointer user_data)
{
  gboolean retval = G_SOURCE_CONTINUE;
  TestData *data = user_data;
  GError *error = NULL;

  gchar *filename = g_file_get_path (data->file);
  gchar *hard_link_name = g_strdup_printf ("%s2", filename);
  GFile *hard_link_file = g_file_new_for_path (hard_link_name);

  switch (data->step)
    {
    case 0:
      record_event (data, -1, NULL, NULL, 0);
      g_output_stream_write_all (G_OUTPUT_STREAM (data->output_stream),
                                 "hello, step 0", 13, NULL, NULL, &error);
      g_assert_no_error (error);
      g_output_stream_close (G_OUTPUT_STREAM (data->output_stream), NULL, &error);
      g_assert_no_error (error);
      break;
    case 1:
      record_event (data, -1, NULL, NULL, 1);
      g_file_replace_contents (data->file, "step 1", 6, NULL, FALSE,
                               G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      break;
    case 2:
      record_event (data, -1, NULL, NULL, 2);
#ifdef HAVE_LINK
      if (link (filename, hard_link_name) < 0)
        {
          g_error ("link(%s, %s) failed: %s", filename, hard_link_name, g_strerror (errno));
        }
#endif  /* HAVE_LINK */
      break;
    case 3:
      record_event (data, -1, NULL, NULL, 3);
#ifdef HAVE_LINK
      {
        GOutputStream *hard_link_stream = NULL;

        /* Deliberately don’t do an atomic swap on the hard-linked file. */
        hard_link_stream = G_OUTPUT_STREAM (g_file_append_to (hard_link_file,
                                                              G_FILE_CREATE_NONE,
                                                              NULL, &error));
        g_assert_no_error (error);
        g_output_stream_write_all (hard_link_stream, " step 3", 7, NULL, NULL, &error);
        g_assert_no_error (error);
        g_output_stream_close (hard_link_stream, NULL, &error);
        g_assert_no_error (error);
        g_object_unref (hard_link_stream);
      }
#endif  /* HAVE_LINK */
      break;
    case 4:
      record_event (data, -1, NULL, NULL, 4);
      g_file_delete (data->file, NULL, &error);
      g_assert_no_error (error);
      break;
    case 5:
      record_event (data, -1, NULL, NULL, 5);
#ifdef HAVE_LINK
      g_file_delete (hard_link_file, NULL, &error);
      g_assert_no_error (error);
#endif  /* HAVE_LINK */
      break;
    case 6:
      record_event (data, -1, NULL, NULL, 6);
      g_main_loop_quit (data->loop);
      retval = G_SOURCE_REMOVE;
      break;
    }

  if (retval != G_SOURCE_REMOVE)
    data->step++;

  g_object_unref (hard_link_file);
  g_free (hard_link_name);
  g_free (filename);

  return retval;
}

static RecordedEvent file_hard_links_output[] = {
  { -1, NULL, NULL, 0 },
  { G_FILE_MONITOR_EVENT_CHANGED, "testfilemonitor.db", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "testfilemonitor.db", NULL, -1 },
  { -1, NULL, NULL, 1 },
  { G_FILE_MONITOR_EVENT_RENAMED, NULL  /* .goutputstream-XXXXXX */, "testfilemonitor.db", -1 },
  { -1, NULL, NULL, 2 },
  { -1, NULL, NULL, 3 },
  /* FIXME: There should be a EVENT_CHANGED and EVENT_CHANGES_DONE_HINT here
   * from the modification of the hard link. */
  { -1, NULL, NULL, 4 },
  { G_FILE_MONITOR_EVENT_DELETED, "testfilemonitor.db", NULL, -1 },
  { -1, NULL, NULL, 5 },
  { -1, NULL, NULL, 6 },
};

static void
test_file_hard_links (void)
{
  GError *error = NULL;
  TestData data;

  g_test_bug ("755721");

#ifdef HAVE_LINK
  g_test_message ("Running with hard link tests");
#else  /* if !HAVE_LINK */
  g_test_message ("Running without hard link tests");
#endif  /* !HAVE_LINK */

  data.step = 0;
  data.events = NULL;

  /* Create a file which exists and is not a directory. */
  data.file = g_file_new_for_path ("testfilemonitor.db");
  data.output_stream = g_file_replace (data.file, NULL, FALSE,
                                       G_FILE_CREATE_NONE, NULL, &error);
  g_assert_no_error (error);

  /* Monitor it. Creating the monitor should not crash (bug #755721). */
  data.monitor = g_file_monitor_file (data.file,
                                      G_FILE_MONITOR_WATCH_MOUNTS |
                                      G_FILE_MONITOR_WATCH_MOVES |
                                      G_FILE_MONITOR_WATCH_HARD_LINKS,
                                      NULL,
                                      &error);
  g_assert_no_error (error);
  g_assert_nonnull (data.monitor);

  /* Change the file a bit. */
  g_file_monitor_set_rate_limit (data.monitor, 200);
  g_signal_connect (data.monitor, "changed", (GCallback) monitor_changed, &data);

  data.loop = g_main_loop_new (NULL, TRUE);
  g_timeout_add (500, file_hard_links_step, &data);
  g_main_loop_run (data.loop);

  /* output_events (data.events); */
  check_expected_events (file_hard_links_output,
                         G_N_ELEMENTS (file_hard_links_output), data.events);

  g_list_free_full (data.events, (GDestroyNotify) free_recorded_event);
  g_main_loop_unref (data.loop);
  g_object_unref (data.monitor);
  g_object_unref (data.file);
  g_object_unref (data.output_stream);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("https://bugzilla.gnome.org/show_bug.cgi?id=");

  g_test_add_func ("/monitor/atomic-replace", test_atomic_replace);
  g_test_add_func ("/monitor/file-changes", test_file_changes);
  g_test_add_func ("/monitor/dir-monitor", test_dir_monitor);
  g_test_add_func ("/monitor/dir-not-existent", test_dir_non_existent);
  g_test_add_func ("/monitor/cross-dir-moves", test_cross_dir_moves);
  g_test_add_func ("/monitor/file/hard-links", test_file_hard_links);

  return g_test_run ();
}
