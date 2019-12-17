#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>
#include <string.h>

#include "gdbus-sessionbus.h"

static gboolean
time_out (gpointer unused G_GNUC_UNUSED)
{
  g_error ("Timed out");
  /* not reached */
  return FALSE;
}

static guint
add_timeout (guint seconds)
{
#ifdef G_OS_UNIX
  /* Safety-catch against the main loop having blocked */
  alarm (seconds + 5);
#endif
  return g_timeout_add_seconds (seconds, time_out, NULL);
}

static void
cancel_timeout (guint timeout_id)
{
#ifdef G_OS_UNIX
  alarm (0);
#endif
  g_source_remove (timeout_id);
}

/* Markup printing {{{1 */

/* This used to be part of GLib, but it was removed before the stable
 * release because it wasn't generally useful.  We want it here, though.
 */
static void
indent_string (GString *string,
               gint     indent)
{
  while (indent--)
    g_string_append_c (string, ' ');
}

static GString *
g_menu_markup_print_string (GString    *string,
                            GMenuModel *model,
                            gint        indent,
                            gint        tabstop)
{
  gboolean need_nl = FALSE;
  gint i, n;

  if G_UNLIKELY (string == NULL)
    string = g_string_new (NULL);

  n = g_menu_model_get_n_items (model);

  for (i = 0; i < n; i++)
    {
      GMenuAttributeIter *attr_iter;
      GMenuLinkIter *link_iter;
      GString *contents;
      GString *attrs;

      attr_iter = g_menu_model_iterate_item_attributes (model, i);
      link_iter = g_menu_model_iterate_item_links (model, i);
      contents = g_string_new (NULL);
      attrs = g_string_new (NULL);

      while (g_menu_attribute_iter_next (attr_iter))
        {
          const char *name = g_menu_attribute_iter_get_name (attr_iter);
          GVariant *value = g_menu_attribute_iter_get_value (attr_iter);

          if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
            {
              gchar *str;
              str = g_markup_printf_escaped (" %s='%s'", name, g_variant_get_string (value, NULL));
              g_string_append (attrs, str);
              g_free (str);
            }

          else
            {
              gchar *printed;
              gchar *str;
              const gchar *type;

              printed = g_variant_print (value, TRUE);
              type = g_variant_type_peek_string (g_variant_get_type (value));
              str = g_markup_printf_escaped ("<attribute name='%s' type='%s'>%s</attribute>\n", name, type, printed);
              indent_string (contents, indent + tabstop);
              g_string_append (contents, str);
              g_free (printed);
              g_free (str);
            }

          g_variant_unref (value);
        }
      g_object_unref (attr_iter);

      while (g_menu_link_iter_next (link_iter))
        {
          const gchar *name = g_menu_link_iter_get_name (link_iter);
          GMenuModel *menu = g_menu_link_iter_get_value (link_iter);
          gchar *str;

          if (contents->str[0])
            g_string_append_c (contents, '\n');

          str = g_markup_printf_escaped ("<link name='%s'>\n", name);
          indent_string (contents, indent + tabstop);
          g_string_append (contents, str);
          g_free (str);

          g_menu_markup_print_string (contents, menu, indent + 2 * tabstop, tabstop);

          indent_string (contents, indent + tabstop);
          g_string_append (contents, "</link>\n");
          g_object_unref (menu);
        }
      g_object_unref (link_iter);

      if (contents->str[0])
        {
          indent_string (string, indent);
          g_string_append_printf (string, "<item%s>\n", attrs->str);
          g_string_append (string, contents->str);
          indent_string (string, indent);
          g_string_append (string, "</item>\n");
          need_nl = TRUE;
        }

      else
        {
          if (need_nl)
            g_string_append_c (string, '\n');

          indent_string (string, indent);
          g_string_append_printf (string, "<item%s/>\n", attrs->str);
          need_nl = FALSE;
        }

      g_string_free (contents, TRUE);
      g_string_free (attrs, TRUE);
    }

  return string;
}

/* TestItem {{{1 */

/* This utility struct is used by both the RandomMenu and MirrorMenu
 * class implementations below.
 */
typedef struct {
  GHashTable *attributes;
  GHashTable *links;
} TestItem;

static TestItem *
test_item_new (GHashTable *attributes,
               GHashTable *links)
{
  TestItem *item;

  item = g_slice_new (TestItem);
  item->attributes = g_hash_table_ref (attributes);
  item->links = g_hash_table_ref (links);

  return item;
}

static void
test_item_free (gpointer data)
{
  TestItem *item = data;

  g_hash_table_unref (item->attributes);
  g_hash_table_unref (item->links);

  g_slice_free (TestItem, item);
}

/* RandomMenu {{{1 */
#define MAX_ITEMS 5
#define TOP_ORDER 4

typedef struct {
  GMenuModel parent_instance;

  GSequence *items;
  gint order;
} RandomMenu;

typedef GMenuModelClass RandomMenuClass;

static GType random_menu_get_type (void);
G_DEFINE_TYPE (RandomMenu, random_menu, G_TYPE_MENU_MODEL)

static gboolean
random_menu_is_mutable (GMenuModel *model)
{
  return TRUE;
}

static gint
random_menu_get_n_items (GMenuModel *model)
{
  RandomMenu *menu = (RandomMenu *) model;

  return g_sequence_get_length (menu->items);
}

static void
random_menu_get_item_attributes (GMenuModel  *model,
                                 gint         position,
                                 GHashTable **table)
{
  RandomMenu *menu = (RandomMenu *) model;
  TestItem *item;

  item = g_sequence_get (g_sequence_get_iter_at_pos (menu->items, position));
  *table = g_hash_table_ref (item->attributes);
}

static void
random_menu_get_item_links (GMenuModel  *model,
                            gint         position,
                            GHashTable **table)
{
  RandomMenu *menu = (RandomMenu *) model;
  TestItem *item;

  item = g_sequence_get (g_sequence_get_iter_at_pos (menu->items, position));
  *table = g_hash_table_ref (item->links);
}

static void
random_menu_finalize (GObject *object)
{
  RandomMenu *menu = (RandomMenu *) object;

  g_sequence_free (menu->items);

  G_OBJECT_CLASS (random_menu_parent_class)
    ->finalize (object);
}

static void
random_menu_init (RandomMenu *menu)
{
}

static void
random_menu_class_init (GMenuModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  class->is_mutable = random_menu_is_mutable;
  class->get_n_items = random_menu_get_n_items;
  class->get_item_attributes = random_menu_get_item_attributes;
  class->get_item_links = random_menu_get_item_links;

  object_class->finalize = random_menu_finalize;
}

static RandomMenu * random_menu_new (GRand *rand, gint order);

static void
random_menu_change (RandomMenu *menu,
                    GRand      *rand)
{
  gint position, removes, adds;
  GSequenceIter *point;
  gint n_items;
  gint i;

  n_items = g_sequence_get_length (menu->items);

  do
    {
      position = g_rand_int_range (rand, 0, n_items + 1);
      removes = g_rand_int_range (rand, 0, n_items - position + 1);
      adds = g_rand_int_range (rand, 0, MAX_ITEMS - (n_items - removes) + 1);
    }
  while (removes == 0 && adds == 0);

  point = g_sequence_get_iter_at_pos (menu->items, position + removes);

  if (removes)
    {
      GSequenceIter *start;

      start = g_sequence_get_iter_at_pos (menu->items, position);
      g_sequence_remove_range (start, point);
    }

  for (i = 0; i < adds; i++)
    {
      const gchar *label;
      GHashTable *links;
      GHashTable *attributes;

      attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
      links = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);

      if (menu->order > 0 && g_rand_boolean (rand))
        {
          RandomMenu *child;
	  const gchar *subtype;

          child = random_menu_new (rand, menu->order - 1);

          if (g_rand_boolean (rand))
            {
              subtype = G_MENU_LINK_SECTION;
              /* label some section headers */
              if (g_rand_boolean (rand))
                label = "Section";
              else
                label = NULL;
            }
          else
            {
              /* label all submenus */
              subtype = G_MENU_LINK_SUBMENU;
              label = "Submenu";
            }

          g_hash_table_insert (links, g_strdup (subtype), child);
        }
      else
        /* label all terminals */
        label = "Menu Item";

      if (label)
        g_hash_table_insert (attributes, g_strdup ("label"), g_variant_ref_sink (g_variant_new_string (label)));

      g_sequence_insert_before (point, test_item_new (attributes, links));
      g_hash_table_unref (links);
      g_hash_table_unref (attributes);
    }

  g_menu_model_items_changed (G_MENU_MODEL (menu), position, removes, adds);
}

static RandomMenu *
random_menu_new (GRand *rand,
                 gint   order)
{
  RandomMenu *menu;

  menu = g_object_new (random_menu_get_type (), NULL);
  menu->items = g_sequence_new (test_item_free);
  menu->order = order;

  random_menu_change (menu, rand);

  return menu;
}

/* MirrorMenu {{{1 */
typedef struct {
  GMenuModel parent_instance;

  GMenuModel *clone_of;
  GSequence *items;
  gulong handler_id;
} MirrorMenu;

typedef GMenuModelClass MirrorMenuClass;

static GType mirror_menu_get_type (void);
G_DEFINE_TYPE (MirrorMenu, mirror_menu, G_TYPE_MENU_MODEL)

static gboolean
mirror_menu_is_mutable (GMenuModel *model)
{
  MirrorMenu *menu = (MirrorMenu *) model;

  return menu->handler_id != 0;
}

static gint
mirror_menu_get_n_items (GMenuModel *model)
{
  MirrorMenu *menu = (MirrorMenu *) model;

  return g_sequence_get_length (menu->items);
}

static void
mirror_menu_get_item_attributes (GMenuModel  *model,
                                 gint         position,
                                 GHashTable **table)
{
  MirrorMenu *menu = (MirrorMenu *) model;
  TestItem *item;

  item = g_sequence_get (g_sequence_get_iter_at_pos (menu->items, position));
  *table = g_hash_table_ref (item->attributes);
}

static void
mirror_menu_get_item_links (GMenuModel  *model,
                            gint         position,
                            GHashTable **table)
{
  MirrorMenu *menu = (MirrorMenu *) model;
  TestItem *item;

  item = g_sequence_get (g_sequence_get_iter_at_pos (menu->items, position));
  *table = g_hash_table_ref (item->links);
}

static void
mirror_menu_finalize (GObject *object)
{
  MirrorMenu *menu = (MirrorMenu *) object;

  if (menu->handler_id)
    g_signal_handler_disconnect (menu->clone_of, menu->handler_id);

  g_sequence_free (menu->items);
  g_object_unref (menu->clone_of);

  G_OBJECT_CLASS (mirror_menu_parent_class)
    ->finalize (object);
}

static void
mirror_menu_init (MirrorMenu *menu)
{
}

static void
mirror_menu_class_init (GMenuModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  class->is_mutable = mirror_menu_is_mutable;
  class->get_n_items = mirror_menu_get_n_items;
  class->get_item_attributes = mirror_menu_get_item_attributes;
  class->get_item_links = mirror_menu_get_item_links;

  object_class->finalize = mirror_menu_finalize;
}

static MirrorMenu * mirror_menu_new (GMenuModel *clone_of);

static void
mirror_menu_changed (GMenuModel *model,
                     gint        position,
                     gint        removed,
                     gint        added,
                     gpointer    user_data)
{
  MirrorMenu *menu = user_data;
  GSequenceIter *point;
  gint i;

  g_assert (model == menu->clone_of);

  point = g_sequence_get_iter_at_pos (menu->items, position + removed);

  if (removed)
    {
      GSequenceIter *start;

      start = g_sequence_get_iter_at_pos (menu->items, position);
      g_sequence_remove_range (start, point);
    }

  for (i = position; i < position + added; i++)
    {
      GMenuAttributeIter *attr_iter;
      GMenuLinkIter *link_iter;
      GHashTable *links;
      GHashTable *attributes;
      const gchar *name;
      GMenuModel *child;
      GVariant *value;

      attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
      links = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);

      attr_iter = g_menu_model_iterate_item_attributes (model, i);
      while (g_menu_attribute_iter_get_next (attr_iter, &name, &value))
        {
          g_hash_table_insert (attributes, g_strdup (name), value);
        }
      g_object_unref (attr_iter);

      link_iter = g_menu_model_iterate_item_links (model, i);
      while (g_menu_link_iter_get_next (link_iter, &name, &child))
        {
          g_hash_table_insert (links, g_strdup (name), mirror_menu_new (child));
          g_object_unref (child);
        }
      g_object_unref (link_iter);

      g_sequence_insert_before (point, test_item_new (attributes, links));
      g_hash_table_unref (attributes);
      g_hash_table_unref (links);
    }

  g_menu_model_items_changed (G_MENU_MODEL (menu), position, removed, added);
}

static MirrorMenu *
mirror_menu_new (GMenuModel *clone_of)
{
  MirrorMenu *menu;

  menu = g_object_new (mirror_menu_get_type (), NULL);
  menu->items = g_sequence_new (test_item_free);
  menu->clone_of = g_object_ref (clone_of);

  if (g_menu_model_is_mutable (clone_of))
    menu->handler_id = g_signal_connect (clone_of, "items-changed", G_CALLBACK (mirror_menu_changed), menu);
  mirror_menu_changed (clone_of, 0, 0, g_menu_model_get_n_items (clone_of), menu);

  return menu;
}

/* check_menus_equal(), assert_menus_equal() {{{1 */
static gboolean
check_menus_equal (GMenuModel *a,
                   GMenuModel *b)
{
  gboolean equal = TRUE;
  gint a_n, b_n;
  gint i;

  a_n = g_menu_model_get_n_items (a);
  b_n = g_menu_model_get_n_items (b);

  if (a_n != b_n)
    return FALSE;

  for (i = 0; i < a_n; i++)
    {
      GMenuAttributeIter *attr_iter;
      GVariant *a_value, *b_value;
      GMenuLinkIter *link_iter;
      GMenuModel *a_menu, *b_menu;
      const gchar *name;

      attr_iter = g_menu_model_iterate_item_attributes (a, i);
      while (g_menu_attribute_iter_get_next (attr_iter, &name, &a_value))
        {
          b_value = g_menu_model_get_item_attribute_value (b, i, name, NULL);
          equal &= b_value && g_variant_equal (a_value, b_value);
          if (b_value)
            g_variant_unref (b_value);
          g_variant_unref (a_value);
        }
      g_object_unref (attr_iter);

      attr_iter = g_menu_model_iterate_item_attributes (b, i);
      while (g_menu_attribute_iter_get_next (attr_iter, &name, &b_value))
        {
          a_value = g_menu_model_get_item_attribute_value (a, i, name, NULL);
          equal &= a_value && g_variant_equal (a_value, b_value);
          if (a_value)
            g_variant_unref (a_value);
          g_variant_unref (b_value);
        }
      g_object_unref (attr_iter);

      link_iter = g_menu_model_iterate_item_links (a, i);
      while (g_menu_link_iter_get_next (link_iter, &name, &a_menu))
        {
          b_menu = g_menu_model_get_item_link (b, i, name);
          equal &= b_menu && check_menus_equal (a_menu, b_menu);
          if (b_menu)
            g_object_unref (b_menu);
          g_object_unref (a_menu);
        }
      g_object_unref (link_iter);

      link_iter = g_menu_model_iterate_item_links (b, i);
      while (g_menu_link_iter_get_next (link_iter, &name, &b_menu))
        {
          a_menu = g_menu_model_get_item_link (a, i, name);
          equal &= a_menu && check_menus_equal (a_menu, b_menu);
          if (a_menu)
            g_object_unref (a_menu);
          g_object_unref (b_menu);
        }
      g_object_unref (link_iter);
    }

  return equal;
}

static void
assert_menus_equal (GMenuModel *a,
                    GMenuModel *b)
{
  if (!check_menus_equal (a, b))
    {
      GString *string;

      string = g_string_new ("\n  <a>\n");
      g_menu_markup_print_string (string, G_MENU_MODEL (a), 4, 2);
      g_string_append (string, "  </a>\n\n-------------\n  <b>\n");
      g_menu_markup_print_string (string, G_MENU_MODEL (b), 4, 2);
      g_string_append (string, "  </b>\n");
      g_error ("%s", string->str);
    }
}

static void
assert_menuitem_equal (GMenuItem  *item,
                       GMenuModel *model,
                       gint        index)
{
  GMenuAttributeIter *attr_iter;
  GMenuLinkIter *link_iter;
  const gchar *name;
  GVariant *value;
  GMenuModel *linked_model;

  /* NOTE we can't yet test whether item has attributes or links that
   * are not in the model, because there's no iterator API for menu
   * items */

  attr_iter = g_menu_model_iterate_item_attributes (model, index);
  while (g_menu_attribute_iter_get_next (attr_iter, &name, &value))
    {
      GVariant *item_value;

      item_value = g_menu_item_get_attribute_value (item, name, g_variant_get_type (value));
      g_assert (item_value && g_variant_equal (item_value, value));

      g_variant_unref (item_value);
      g_variant_unref (value);
    }

  link_iter = g_menu_model_iterate_item_links (model, index);
  while (g_menu_link_iter_get_next (link_iter, &name, &linked_model))
    {
      GMenuModel *item_linked_model;

      item_linked_model = g_menu_item_get_link (item, name);
      g_assert (linked_model == item_linked_model);

      g_object_unref (item_linked_model);
      g_object_unref (linked_model);
    }

  g_object_unref (attr_iter);
  g_object_unref (link_iter);
}

/* Test cases {{{1 */
static void
test_equality (void)
{
  GRand *randa, *randb;
  guint32 seed;
  gint i;

  seed = g_test_rand_int ();

  randa = g_rand_new_with_seed (seed);
  randb = g_rand_new_with_seed (seed);

  for (i = 0; i < 500; i++)
    {
      RandomMenu *a, *b;

      a = random_menu_new (randa, TOP_ORDER);
      b = random_menu_new (randb, TOP_ORDER);
      assert_menus_equal (G_MENU_MODEL (a), G_MENU_MODEL (b));
      g_object_unref (b);
      g_object_unref (a);
    }

  g_rand_int (randa);

  for (i = 0; i < 500;)
    {
      RandomMenu *a, *b;

      a = random_menu_new (randa, TOP_ORDER);
      b = random_menu_new (randb, TOP_ORDER);
      if (check_menus_equal (G_MENU_MODEL (a), G_MENU_MODEL (b)))
        {
          /* by chance, they may really be equal.  double check. */
          GString *as, *bs;

          as = g_menu_markup_print_string (NULL, G_MENU_MODEL (a), 4, 2);
          bs = g_menu_markup_print_string (NULL, G_MENU_MODEL (b), 4, 2);
          g_assert_cmpstr (as->str, ==, bs->str);
          g_string_free (bs, TRUE);
          g_string_free (as, TRUE);

          /* we're here because randa and randb just generated equal
           * menus.  they may do it again, so throw away randb and make
           * a fresh one.
           */
          g_rand_free (randb);
          randb = g_rand_new_with_seed (g_rand_int (randa));
        }
      else
        /* make sure we get enough unequals (ie: no GRand failure) */
        i++;

      g_object_unref (b);
      g_object_unref (a);
    }

  g_rand_free (randb);
  g_rand_free (randa);
}

static void
test_random (void)
{
  RandomMenu *random;
  MirrorMenu *mirror;
  GRand *rand;
  gint i;

  rand = g_rand_new_with_seed (g_test_rand_int ());
  random = random_menu_new (rand, TOP_ORDER);
  mirror = mirror_menu_new (G_MENU_MODEL (random));

  for (i = 0; i < 500; i++)
    {
      assert_menus_equal (G_MENU_MODEL (random), G_MENU_MODEL (mirror));
      random_menu_change (random, rand);
    }

  g_object_unref (mirror);
  g_object_unref (random);

  g_rand_free (rand);
}

typedef struct
{
  GDBusConnection *client_connection;
  GDBusConnection *server_connection;
  GDBusServer *server;

  GThread *service_thread;
  GMutex service_loop_lock;
  GCond service_loop_cond;

  GMainLoop *service_loop;
  GMainLoop *loop;
} PeerConnection;

static gboolean
on_new_connection (GDBusServer *server,
                   GDBusConnection *connection,
                   gpointer user_data)
{
  PeerConnection *data = user_data;

  data->server_connection = g_object_ref (connection);

  g_main_loop_quit (data->loop);

  return TRUE;
}

static void
create_service_loop (GMainContext   *service_context,
                     PeerConnection *data)
{
  g_assert (data->service_loop == NULL);
  g_mutex_lock (&data->service_loop_lock);
  data->service_loop = g_main_loop_new (service_context, FALSE);
  g_cond_broadcast (&data->service_loop_cond);
  g_mutex_unlock (&data->service_loop_lock);
}

static void
teardown_service_loop (PeerConnection *data)
{
  g_mutex_lock (&data->service_loop_lock);
  g_clear_pointer (&data->service_loop, g_main_loop_unref);
  g_mutex_unlock (&data->service_loop_lock);
}

static void
await_service_loop (PeerConnection *data)
{
  g_mutex_lock (&data->service_loop_lock);
  while (data->service_loop == NULL)
    g_cond_wait (&data->service_loop_cond, &data->service_loop_lock);
  g_mutex_unlock (&data->service_loop_lock);
}

static gpointer
service_thread_func (gpointer user_data)
{
  PeerConnection *data = user_data;
  GMainContext *service_context;
  GError *error;
  gchar *address;
  gchar *tmpdir;
  GDBusServerFlags flags;
  gchar *guid;

  service_context = g_main_context_new ();
  g_main_context_push_thread_default (service_context);

  tmpdir = NULL;
  flags = G_DBUS_SERVER_FLAGS_NONE;

#ifdef G_OS_UNIX
  if (g_unix_socket_address_abstract_names_supported ())
    address = g_strdup ("unix:tmpdir=/tmp/test-dbus-peer");
  else
    {
      tmpdir = g_dir_make_tmp ("test-dbus-peer-XXXXXX", NULL);
      address = g_strdup_printf ("unix:tmpdir=%s", tmpdir);
    }
#else
  address = g_strdup ("nonce-tcp:");
  flags |= G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS;
#endif

  guid = g_dbus_generate_guid ();

  error = NULL;
  data->server = g_dbus_server_new_sync (address,
                                         flags,
                                         guid,
                                         NULL,
                                         NULL,
                                         &error);
  g_assert_no_error (error);
  g_free (address);
  g_free (guid);

  g_signal_connect (data->server,
                    "new-connection",
                    G_CALLBACK (on_new_connection),
                    data);

  g_dbus_server_start (data->server);

  create_service_loop (service_context, data);
  g_main_loop_run (data->service_loop);

  g_main_context_pop_thread_default (service_context);

  teardown_service_loop (data);
  g_main_context_unref (service_context);

  if (tmpdir)
    {
      g_rmdir (tmpdir);
      g_free (tmpdir);
    }

  return NULL;
}

static void
peer_connection_up (PeerConnection *data)
{
  GError *error;

  memset (data, '\0', sizeof (PeerConnection));
  data->loop = g_main_loop_new (NULL, FALSE);

  g_mutex_init (&data->service_loop_lock);
  g_cond_init (&data->service_loop_cond);

  /* bring up a server - we run the server in a different thread to
     avoid deadlocks */
  data->service_thread = g_thread_new ("test_dbus_peer",
                                       service_thread_func,
                                       data);
  await_service_loop (data);
  g_assert (data->server != NULL);

  /* bring up a connection and accept it */
  error = NULL;
  data->client_connection =
    g_dbus_connection_new_for_address_sync (g_dbus_server_get_client_address (data->server),
                                            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                            NULL, /* GDBusAuthObserver */
                                            NULL, /* cancellable */
                                            &error);
  g_assert_no_error (error);
  g_assert (data->client_connection != NULL);
  while (data->server_connection == NULL)
    g_main_loop_run (data->loop);
}

static void
peer_connection_down (PeerConnection *data)
{
  g_object_unref (data->client_connection);
  g_object_unref (data->server_connection);

  g_dbus_server_stop (data->server);
  g_object_unref (data->server);

  g_main_loop_quit (data->service_loop);
  g_thread_join (data->service_thread);

  g_mutex_clear (&data->service_loop_lock);
  g_cond_clear (&data->service_loop_cond);

  g_main_loop_unref (data->loop);
}

struct roundtrip_state
{
  RandomMenu *random;
  MirrorMenu *proxy_mirror;
  GDBusMenuModel *proxy;
  GMainLoop *loop;
  GRand *rand;
  gint success;
  gint count;
};

static gboolean
roundtrip_step (gpointer data)
{
  struct roundtrip_state *state = data;

  if (check_menus_equal (G_MENU_MODEL (state->random), G_MENU_MODEL (state->proxy)) &&
      check_menus_equal (G_MENU_MODEL (state->random), G_MENU_MODEL (state->proxy_mirror)))
    {
      state->success++;
      state->count = 0;

      if (state->success < 100)
        random_menu_change (state->random, state->rand);
      else
        g_main_loop_quit (state->loop);
    }
  else if (state->count == 100)
    {
      assert_menus_equal (G_MENU_MODEL (state->random), G_MENU_MODEL (state->proxy));
      g_assert_not_reached ();
    }
  else
    state->count++;

  return G_SOURCE_CONTINUE;
}

static void
do_roundtrip (GDBusConnection *exporter_connection,
              GDBusConnection *proxy_connection)
{
  struct roundtrip_state state;
  guint export_id;
  guint id;

  state.rand = g_rand_new_with_seed (g_test_rand_int ());

  state.random = random_menu_new (state.rand, 2);
  export_id = g_dbus_connection_export_menu_model (exporter_connection,
                                                   "/",
                                                   G_MENU_MODEL (state.random),
                                                   NULL);
  state.proxy = g_dbus_menu_model_get (proxy_connection,
                                       g_dbus_connection_get_unique_name (proxy_connection),
                                       "/");
  state.proxy_mirror = mirror_menu_new (G_MENU_MODEL (state.proxy));
  state.count = 0;
  state.success = 0;

  id = g_timeout_add (10, roundtrip_step, &state);

  state.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (state.loop);

  g_main_loop_unref (state.loop);
  g_source_remove (id);
  g_object_unref (state.proxy);
  g_dbus_connection_unexport_menu_model (exporter_connection, export_id);
  g_object_unref (state.random);
  g_object_unref (state.proxy_mirror);
  g_rand_free (state.rand);
}

static void
test_dbus_roundtrip (void)
{
  GDBusConnection *bus;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  do_roundtrip (bus, bus);
  g_object_unref (bus);
}

static void
test_dbus_peer_roundtrip (void)
{
  PeerConnection peer;

  peer_connection_up (&peer);
  do_roundtrip (peer.server_connection, peer.client_connection);
  peer_connection_down (&peer);
}

static gint items_changed_count;

static void
items_changed (GMenuModel *model,
               gint        position,
               gint        removed,
               gint        added,
               gpointer    data)
{
  items_changed_count++;
}

static gboolean
stop_loop (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
do_subscriptions (GDBusConnection *exporter_connection,
                  GDBusConnection *proxy_connection)
{
  GMenu *menu;
  GDBusMenuModel *proxy;
  GMainLoop *loop;
  GError *error = NULL;
  guint export_id;
  guint timeout_id;

  timeout_id = add_timeout (60);
  loop = g_main_loop_new (NULL, FALSE);

  menu = g_menu_new ();

  export_id = g_dbus_connection_export_menu_model (exporter_connection,
                                                   "/",
                                                   G_MENU_MODEL (menu),
                                                   &error);
  g_assert_no_error (error);

  proxy = g_dbus_menu_model_get (proxy_connection,
                                 g_dbus_connection_get_unique_name (proxy_connection),
                                 "/");
  items_changed_count = 0;
  g_signal_connect (proxy, "items-changed",
                    G_CALLBACK (items_changed), NULL);

  g_menu_append (menu, "item1", NULL);
  g_menu_append (menu, "item2", NULL);
  g_menu_append (menu, "item3", NULL);

  g_assert_cmpint (items_changed_count, ==, 0);

  /* We don't subscribe to change-notification until we look at the items */
  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  /* Looking at the items triggers subscription */
  g_menu_model_get_n_items (G_MENU_MODEL (proxy));

  while (items_changed_count < 1)
    g_main_context_iteration (NULL, TRUE);

  /* We get all three items in one batch */
  g_assert_cmpint (items_changed_count, ==, 1);
  g_assert_cmpint (g_menu_model_get_n_items (G_MENU_MODEL (proxy)), ==, 3);

  /* If we wait, we don't get any more */
  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);
  g_assert_cmpint (items_changed_count, ==, 1);
  g_assert_cmpint (g_menu_model_get_n_items (G_MENU_MODEL (proxy)), ==, 3);

  /* Now we're subscribed, we get changes individually */
  g_menu_append (menu, "item4", NULL);
  g_menu_append (menu, "item5", NULL);
  g_menu_append (menu, "item6", NULL);
  g_menu_remove (menu, 0);
  g_menu_remove (menu, 0);

  while (items_changed_count < 6)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpint (items_changed_count, ==, 6);

  g_assert_cmpint (g_menu_model_get_n_items (G_MENU_MODEL (proxy)), ==, 4);

  /* After destroying the proxy and waiting a bit, we don't get any more
   * items-changed signals */
  g_object_unref (proxy);

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_menu_remove (menu, 0);
  g_menu_remove (menu, 0);

  g_timeout_add (100, stop_loop, loop);
  g_main_loop_run (loop);

  g_assert_cmpint (items_changed_count, ==, 6);

  g_dbus_connection_unexport_menu_model (exporter_connection, export_id);
  g_object_unref (menu);

  g_main_loop_unref (loop);
  cancel_timeout (timeout_id);
}

static void
test_dbus_subscriptions (void)
{
  GDBusConnection *bus;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  do_subscriptions (bus, bus);
  g_object_unref (bus);
}

static void
test_dbus_peer_subscriptions (void)
{
  PeerConnection peer;

  peer_connection_up (&peer);
  do_subscriptions (peer.server_connection, peer.client_connection);
  peer_connection_down (&peer);
}

static gpointer
do_modify (gpointer data)
{
  RandomMenu *menu = data;
  GRand *rand;
  gint i;

  rand = g_rand_new_with_seed (g_test_rand_int ());

  for (i = 0; i < 10000; i++)
    {
      random_menu_change (menu, rand);
    }

  return NULL;
}

static gpointer
do_export (gpointer data)
{
  GMenuModel *menu = data;
  gint i;
  GDBusConnection *bus;
  gchar *path;
  GError *error = NULL;
  guint id;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  path = g_strdup_printf ("/%p", data);

  for (i = 0; i < 10000; i++)
    {
      id = g_dbus_connection_export_menu_model (bus, path, menu, &error);
      g_assert_no_error (error);
      g_dbus_connection_unexport_menu_model (bus, id);
      while (g_main_context_iteration (NULL, FALSE));
    }

  g_free (path);

  g_object_unref (bus);

  return NULL;
}

static void
test_dbus_threaded (void)
{
  RandomMenu *menu[10];
  GThread *call[10];
  GThread *export[10];
  gint i;

  for (i = 0; i < 10; i++)
    {
      menu[i] = random_menu_new (g_rand_new_with_seed (g_test_rand_int ()), 2);
      call[i] = g_thread_new ("call", do_modify, menu[i]);
      export[i] = g_thread_new ("export", do_export, menu[i]);
    }

  for (i = 0; i < 10; i++)
    {
      g_thread_join (call[i]);
      g_thread_join (export[i]);
    }

  for (i = 0; i < 10; i++)
    g_object_unref (menu[i]);
}

static void
test_attributes (void)
{
  GMenu *menu;
  GMenuItem *item;
  GVariant *v;

  menu = g_menu_new ();

  item = g_menu_item_new ("test", NULL);
  g_menu_item_set_attribute_value (item, "boolean", g_variant_new_boolean (FALSE));
  g_menu_item_set_attribute_value (item, "string", g_variant_new_string ("bla"));

  g_menu_item_set_attribute (item, "double", "d", 1.5);
  v = g_variant_new_parsed ("[('one', 1), ('two', %i), (%s, 3)]", 2, "three");
  g_menu_item_set_attribute_value (item, "complex", v);
  g_menu_item_set_attribute_value (item, "test-123", g_variant_new_string ("test-123"));

  g_menu_append_item (menu, item);

  g_menu_item_set_attribute (item, "double", "d", G_PI);

  g_assert_cmpint (g_menu_model_get_n_items (G_MENU_MODEL (menu)), ==, 1);

  v = g_menu_model_get_item_attribute_value (G_MENU_MODEL (menu), 0, "boolean", NULL);
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_BOOLEAN));
  g_variant_unref (v);

  v = g_menu_model_get_item_attribute_value (G_MENU_MODEL (menu), 0, "string", NULL);
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING));
  g_variant_unref (v);

  v = g_menu_model_get_item_attribute_value (G_MENU_MODEL (menu), 0, "double", NULL);
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_DOUBLE));
  g_variant_unref (v);

  v = g_menu_model_get_item_attribute_value (G_MENU_MODEL (menu), 0, "complex", NULL);
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE("a(si)")));
  g_variant_unref (v);

  g_menu_remove_all (menu);

  g_object_unref (menu);
  g_object_unref (item);
}

static void
test_attribute_iter (void)
{
  GMenu *menu;
  GMenuItem *item;
  const gchar *name;
  GVariant *v;
  GMenuAttributeIter *iter;
  GHashTable *found;

  menu = g_menu_new ();

  item = g_menu_item_new ("test", NULL);
  g_menu_item_set_attribute_value (item, "boolean", g_variant_new_boolean (FALSE));
  g_menu_item_set_attribute_value (item, "string", g_variant_new_string ("bla"));

  g_menu_item_set_attribute (item, "double", "d", 1.5);
  v = g_variant_new_parsed ("[('one', 1), ('two', %i), (%s, 3)]", 2, "three");
  g_menu_item_set_attribute_value (item, "complex", v);
  g_menu_item_set_attribute_value (item, "test-123", g_variant_new_string ("test-123"));

  g_menu_append_item (menu, item);

  g_menu_item_set_attribute (item, "double", "d", G_PI);

  g_assert_cmpint (g_menu_model_get_n_items (G_MENU_MODEL (menu)), ==, 1);

  found = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref); 

  iter = g_menu_model_iterate_item_attributes (G_MENU_MODEL (menu), 0);
  while (g_menu_attribute_iter_get_next (iter, &name, &v))
    g_hash_table_insert (found, g_strdup (name), v);

  g_assert_cmpint (g_hash_table_size (found), ==, 6);
  
  v = g_hash_table_lookup (found, "label");
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING));

  v = g_hash_table_lookup (found, "boolean");
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_BOOLEAN));
 
  v = g_hash_table_lookup (found, "string");
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING));

  v = g_hash_table_lookup (found, "double");
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_DOUBLE));

  v = g_hash_table_lookup (found, "complex");
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE("a(si)")));

  v = g_hash_table_lookup (found, "test-123");
  g_assert (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING));

  g_hash_table_unref (found);

  g_menu_remove_all (menu);

  g_object_unref (menu);
  g_object_unref (item);
}

static void
test_links (void)
{
  GMenu *menu;
  GMenuModel *m;
  GMenuModel *x;
  GMenuItem *item;

  m = G_MENU_MODEL (g_menu_new ());
  g_menu_append (G_MENU (m), "test", NULL);

  menu = g_menu_new ();

  item = g_menu_item_new ("test2", NULL);
  g_menu_item_set_link (item, "submenu", m);
  g_menu_prepend_item (menu, item);

  item = g_menu_item_new ("test1", NULL);
  g_menu_item_set_link (item, "section", m);
  g_menu_insert_item (menu, 0, item);

  item = g_menu_item_new ("test3", NULL);
  g_menu_item_set_link (item, "wallet", m);
  g_menu_insert_item (menu, 1000, item);

  item = g_menu_item_new ("test4", NULL);
  g_menu_item_set_link (item, "purse", m);
  g_menu_item_set_link (item, "purse", NULL);
  g_menu_append_item (menu, item);

  g_assert_cmpint (g_menu_model_get_n_items (G_MENU_MODEL (menu)), ==, 4);

  x = g_menu_model_get_item_link (G_MENU_MODEL (menu), 0, "section");
  g_assert (x == m);
  g_object_unref (x);

  x = g_menu_model_get_item_link (G_MENU_MODEL (menu), 1, "submenu");
  g_assert (x == m);
  g_object_unref (x);

  x = g_menu_model_get_item_link (G_MENU_MODEL (menu), 2, "wallet");
  g_assert (x == m);
  g_object_unref (x);

  x = g_menu_model_get_item_link (G_MENU_MODEL (menu), 3, "purse");
  g_assert (x == NULL);

  g_object_unref (m);
  g_object_unref (menu);
}

static void
test_mutable (void)
{
  GMenu *menu;

  menu = g_menu_new ();
  g_menu_append (menu, "test", "test");

  g_assert (g_menu_model_is_mutable (G_MENU_MODEL (menu)));
  g_menu_freeze (menu);
  g_assert (!g_menu_model_is_mutable (G_MENU_MODEL (menu)));

  g_object_unref (menu);
}

static void
test_convenience (void)
{
  GMenu *m1, *m2;
  GMenu *sub;
  GMenuItem *item;

  m1 = g_menu_new ();
  m2 = g_menu_new ();
  sub = g_menu_new ();

  g_menu_prepend (m1, "label1", "do::something");
  g_menu_insert (m2, 0, "label1", "do::something");

  g_menu_append (m1, "label2", "do::somethingelse");
  g_menu_insert (m2, -1, "label2", "do::somethingelse");

  g_menu_insert_section (m1, 10, "label3", G_MENU_MODEL (sub));
  item = g_menu_item_new_section ("label3", G_MENU_MODEL (sub));
  g_menu_insert_item (m2, 10, item);
  g_object_unref (item);

  g_menu_prepend_section (m1, "label4", G_MENU_MODEL (sub));
  g_menu_insert_section (m2, 0, "label4", G_MENU_MODEL (sub));

  g_menu_append_section (m1, "label5", G_MENU_MODEL (sub));
  g_menu_insert_section (m2, -1, "label5", G_MENU_MODEL (sub));

  g_menu_insert_submenu (m1, 5, "label6", G_MENU_MODEL (sub));
  item = g_menu_item_new_submenu ("label6", G_MENU_MODEL (sub));
  g_menu_insert_item (m2, 5, item);
  g_object_unref (item);

  g_menu_prepend_submenu (m1, "label7", G_MENU_MODEL (sub));
  g_menu_insert_submenu (m2, 0, "label7", G_MENU_MODEL (sub));

  g_menu_append_submenu (m1, "label8", G_MENU_MODEL (sub));
  g_menu_insert_submenu (m2, -1, "label8", G_MENU_MODEL (sub));

  assert_menus_equal (G_MENU_MODEL (m1), G_MENU_MODEL (m2));

  g_object_unref (m1);
  g_object_unref (m2);
}

static void
test_menuitem (void)
{
  GMenu *menu;
  GMenu *submenu;
  GMenuItem *item;
  GIcon *icon;
  gboolean b;
  gchar *s;

  menu = g_menu_new ();
  submenu = g_menu_new ();

  item = g_menu_item_new ("label", "action");
  g_menu_item_set_attribute (item, "attribute", "b", TRUE);
  g_menu_item_set_link (item, G_MENU_LINK_SUBMENU, G_MENU_MODEL (submenu));
  g_menu_append_item (menu, item);

  icon = g_themed_icon_new ("bla");
  g_menu_item_set_icon (item, icon);
  g_object_unref (icon);

  g_assert (g_menu_item_get_attribute (item, "attribute", "b", &b));
  g_assert (b);

  g_menu_item_set_action_and_target (item, "action", "(bs)", TRUE, "string");
  g_assert (g_menu_item_get_attribute (item, "target", "(bs)", &b, &s));
  g_assert (b);
  g_assert_cmpstr (s, ==, "string");
  g_free (s);

  g_object_unref (item);

  item = g_menu_item_new_from_model (G_MENU_MODEL (menu), 0);
  assert_menuitem_equal (item, G_MENU_MODEL (menu), 0);
  g_object_unref (item);

  g_object_unref (menu);
  g_object_unref (submenu);
}

/* Epilogue {{{1 */
int
main (int argc, char **argv)
{
  gboolean ret;

  g_test_init (&argc, &argv, NULL);

  session_bus_up ();

  g_test_add_func ("/gmenu/equality", test_equality);
  g_test_add_func ("/gmenu/random", test_random);
  g_test_add_func ("/gmenu/dbus/roundtrip", test_dbus_roundtrip);
  g_test_add_func ("/gmenu/dbus/subscriptions", test_dbus_subscriptions);
  g_test_add_func ("/gmenu/dbus/threaded", test_dbus_threaded);
  g_test_add_func ("/gmenu/dbus/peer/roundtrip", test_dbus_peer_roundtrip);
  g_test_add_func ("/gmenu/dbus/peer/subscriptions", test_dbus_peer_subscriptions);
  g_test_add_func ("/gmenu/attributes", test_attributes);
  g_test_add_func ("/gmenu/attributes/iterate", test_attribute_iter);
  g_test_add_func ("/gmenu/links", test_links);
  g_test_add_func ("/gmenu/mutable", test_mutable);
  g_test_add_func ("/gmenu/convenience", test_convenience);
  g_test_add_func ("/gmenu/menuitem", test_menuitem);

  ret = g_test_run ();

  session_bus_down ();

  return ret;
}
/* vim:set foldmethod=marker: */
