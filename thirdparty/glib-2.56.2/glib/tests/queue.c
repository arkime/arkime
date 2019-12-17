#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <time.h>
#include <stdlib.h>

#include <glib.h>


static void
check_integrity (GQueue *queue)
{
  GList *list;
  GList *last;
  GList *links;
  GList *link;
  gint n;

  g_assert (queue->length < 4000000000u);

  g_assert (g_queue_get_length (queue) == queue->length);

  if (!queue->head)
    g_assert (!queue->tail);
  if (!queue->tail)
    g_assert (!queue->head);

  n = 0;
  last = NULL;
  for (list = queue->head; list != NULL; list = list->next)
    {
      if (!list->next)
        last = list;
      ++n;
    }
  g_assert (n == queue->length);
  g_assert (last == queue->tail);

  n = 0;
  last = NULL;
  for (list = queue->tail; list != NULL; list = list->prev)
    {
      if (!list->prev)
        last = list;
      ++n;
    }
  g_assert (n == queue->length);
  g_assert (last == queue->head);

  links = NULL;
  for (list = queue->head; list != NULL; list = list->next)
    links = g_list_prepend (links, list);

  link = links;
  for (list = queue->tail; list != NULL; list = list->prev)
    {
      g_assert (list == link->data);
      link = link->next;
    }
  g_list_free (links);

  links = NULL;
  for (list = queue->tail; list != NULL; list = list->prev)
    links = g_list_prepend (links, list);

  link = links;
  for (list = queue->head; list != NULL; list = list->next)
    {
      g_assert (list == link->data);
      link = link->next;
    }
  g_list_free (links);
}

static gboolean
rnd_bool (void)
{
  return g_random_int_range (0, 2);
}

static void
check_max (gpointer elm, gpointer user_data)
{
  gint *best = user_data;
  gint element = GPOINTER_TO_INT (elm);

  if (element > *best)
    *best = element;
}

static void
check_min (gpointer elm, gpointer user_data)
{
  gint *best = user_data;
  gint element = GPOINTER_TO_INT (elm);

  if (element < *best)
    *best = element;
}

static gint
find_min (GQueue *queue)
{
  gint min = G_MAXINT;

  g_queue_foreach (queue, check_min, &min);

  return min;
}

static gint
find_max (GQueue *queue)
{
  gint max = G_MININT;

  g_queue_foreach (queue, check_max, &max);

  return max;
}

static void
delete_elm (gpointer elm, gpointer user_data)
{
  g_queue_remove ((GQueue *)user_data, elm);
  check_integrity ((GQueue *)user_data);
}

static void
delete_all (GQueue *queue)
{
  g_queue_foreach (queue, delete_elm, queue);
}

static int
compare_int (gconstpointer a, gconstpointer b, gpointer data)
{
  int ai = GPOINTER_TO_INT (a);
  int bi = GPOINTER_TO_INT (b);

  if (ai > bi)
    return 1;
  else if (ai == bi)
    return 0;
  else
    return -1;
}

static gint
get_random_position (GQueue *queue, gboolean allow_offlist)
{
  int n;
  enum { OFF_QUEUE, HEAD, TAIL, MIDDLE, LAST } where;

  if (allow_offlist)
    where = g_random_int_range (OFF_QUEUE, LAST);
  else
    where = g_random_int_range (HEAD, LAST);

  switch (where)
    {
    case OFF_QUEUE:
      n = g_random_int ();
      break;

    case HEAD:
      n = 0;
      break;

    case TAIL:
      if (allow_offlist)
        n = queue->length;
      else
        n = queue->length - 1;
      break;

    case MIDDLE:
      if (queue->length == 0)
        n = 0;
      else
        n = g_random_int_range (0, queue->length);
      break;

    default:
      g_assert_not_reached();
      n = 100;
      break;

    }

  return n;
}

static void
random_test (gconstpointer d)
{
  guint32 seed = GPOINTER_TO_UINT (d);

  typedef enum {
    IS_EMPTY, GET_LENGTH, REVERSE, COPY,
    FOREACH, FIND, FIND_CUSTOM, SORT,
    PUSH_HEAD, PUSH_TAIL, PUSH_NTH, POP_HEAD,
    POP_TAIL, POP_NTH, PEEK_HEAD, PEEK_TAIL,
    PEEK_NTH, INDEX, REMOVE, REMOVE_ALL,
    INSERT_BEFORE, INSERT_AFTER, INSERT_SORTED, PUSH_HEAD_LINK,
    PUSH_TAIL_LINK, PUSH_NTH_LINK, POP_HEAD_LINK, POP_TAIL_LINK,
    POP_NTH_LINK, PEEK_HEAD_LINK, PEEK_TAIL_LINK, PEEK_NTH_LINK,
    LINK_INDEX, UNLINK, DELETE_LINK, LAST_OP
  } QueueOp;

#define N_ITERATIONS 500000
#define N_QUEUES 3

#define RANDOM_QUEUE() &(queues[g_random_int_range(0, N_QUEUES)])

  typedef struct QueueInfo QueueInfo;
  struct QueueInfo
  {
    GQueue *queue;
    GList *tail;
    GList *head;
    guint length;
  };

  gint i;
  QueueOp op;
  QueueInfo queues[N_QUEUES];

  g_random_set_seed (seed);

  for (i = 0; i < N_QUEUES; ++i)
    {
      queues[i].queue = g_queue_new ();
      queues[i].head = NULL;
      queues[i].tail = NULL;
      queues[i].length = 0;
    }

  for (i = 0; i < N_ITERATIONS; ++i)
    {
      int j;
      QueueInfo *qinf = RANDOM_QUEUE();
      GQueue *q = qinf->queue;
      op = g_random_int_range (IS_EMPTY, LAST_OP);

      g_assert (qinf->head == q->head);
      g_assert (qinf->tail == q->tail);
      g_assert (qinf->length == q->length);

      switch (op)
        {
        case IS_EMPTY:
          {
            if (g_queue_is_empty (qinf->queue))
              {
                g_assert (q->head == NULL);
                g_assert (q->tail == NULL);
                g_assert (q->length == 0);
              }
            else
              {
                g_assert (q->head);
                g_assert (q->tail);
                g_assert (q->length > 0);
              }
          }
          break;
        case GET_LENGTH:
          {
            int l;

            l = g_queue_get_length (q);

            g_assert (qinf->length == q->length);
            g_assert (qinf->length == l);
          }
          break;
        case REVERSE:
          g_queue_reverse (q);
          g_assert (qinf->tail == q->head);
          g_assert (qinf->head == q->tail);
          g_assert (qinf->length == q->length);
          qinf->tail = q->tail;
          qinf->head = q->head;
          break;
        case COPY:
          {
            QueueInfo *random_queue = RANDOM_QUEUE();
            GQueue *new_queue = g_queue_copy (random_queue->queue);

            g_queue_free (qinf->queue);
            q = qinf->queue = new_queue;
            qinf->head = new_queue->head;
            qinf->tail = g_list_last (new_queue->head);
            qinf->length = new_queue->length;
          }
          break;
        case FOREACH:
          delete_all (q);
          qinf->head = NULL;
          qinf->tail = NULL;
          qinf->length = 0;
          break;
        case FIND:
          {
            gboolean find_existing = rnd_bool ();
            int first = find_max (q);
            int second = find_min (q);

            if (q->length == 0)
              find_existing = FALSE;

            if (!find_existing)
              first++;
            if (!find_existing)
              second--;

            if (find_existing)
              {
                g_assert (g_queue_find (q, GINT_TO_POINTER (first)));
                g_assert (g_queue_find (q, GINT_TO_POINTER (second)));
              }
            else
              {
                g_assert (!g_queue_find (q, GINT_TO_POINTER (first)));
                g_assert (!g_queue_find (q, GINT_TO_POINTER (second)));
              }
          }
          break;
        case FIND_CUSTOM:
          break;
        case SORT:
          {
            if (!g_queue_is_empty (q))
              {
                int max = find_max (q);
                int min = find_min (q);
                g_queue_remove_all (q, GINT_TO_POINTER (max));
                check_integrity (q);
                g_queue_remove_all (q, GINT_TO_POINTER (min));
                check_integrity (q);
                g_queue_push_head (q, GINT_TO_POINTER (max));
                if (max != min)
                  g_queue_push_head (q, GINT_TO_POINTER (min));
                qinf->length = q->length;
              }

            check_integrity (q);

            g_queue_sort (q, compare_int, NULL);

            check_integrity (q);

            qinf->head = g_queue_find (q, GINT_TO_POINTER (find_min(q)));
            qinf->tail = g_queue_find (q, GINT_TO_POINTER (find_max(q)));

            g_assert (qinf->tail == q->tail);
          }
          break;
        case PUSH_HEAD:
          {
            int x = g_random_int_range (0, 435435);
            g_queue_push_head (q, GINT_TO_POINTER (x));
            if (!qinf->head)
              qinf->tail = qinf->head = q->head;
            else
              qinf->head = qinf->head->prev;
            qinf->length++;
          }
          break;
        case PUSH_TAIL:
          {
            int x = g_random_int_range (0, 236546);
            g_queue_push_tail (q, GINT_TO_POINTER (x));
            if (!qinf->tail)
              qinf->tail = qinf->head = q->head;
            else
              qinf->tail = qinf->tail->next;
            qinf->length++;
          }
          break;
        case PUSH_NTH:
          {
            int pos = get_random_position (q, TRUE);
            int x = g_random_int_range (0, 236546);
            g_queue_push_nth (q, GINT_TO_POINTER (x), pos);
            if (qinf->head && qinf->head->prev)
              qinf->head = qinf->head->prev;
            else
              qinf->head = q->head;
            if (qinf->tail && qinf->tail->next)
              qinf->tail = qinf->tail->next;
            else
              qinf->tail = g_list_last (qinf->head);
            qinf->length++;
          }
          break;
        case POP_HEAD:
          if (qinf->head)
            qinf->head = qinf->head->next;
          if (!qinf->head)
            qinf->tail = NULL;
          qinf->length = (qinf->length == 0)? 0 : qinf->length - 1;
          g_queue_pop_head (q);
          break;
        case POP_TAIL:
          if (qinf->tail)
            qinf->tail = qinf->tail->prev;
          if (!qinf->tail)
            qinf->head = NULL;
          qinf->length = (qinf->length == 0)? 0 : qinf->length - 1;
          g_queue_pop_tail (q);
          break;
        case POP_NTH:
          if (!g_queue_is_empty (q))
            {
              int n = get_random_position (q, TRUE);
              gpointer elm = g_queue_peek_nth (q, n);

              if (n == q->length - 1)
                qinf->tail = qinf->tail->prev;

              if (n == 0)
                qinf->head = qinf->head->next;

              if (n >= 0 && n < q->length)
                qinf->length--;

              g_assert (elm == g_queue_pop_nth (q, n));
            }
          break;
        case PEEK_HEAD:
          if (qinf->head)
            g_assert (qinf->head->data == g_queue_peek_head (q));
          else
            g_assert (g_queue_peek_head (q) == NULL);
          break;
        case PEEK_TAIL:
          if (qinf->head)
            g_assert (qinf->tail->data == g_queue_peek_tail (q));
          else
            g_assert (g_queue_peek_tail (q) == NULL);
          break;
        case PEEK_NTH:
          if (g_queue_is_empty (q))
            {
              for (j = -10; j < 10; ++j)
                g_assert (g_queue_peek_nth (q, j) == NULL);
            }
          else
            {
              GList *list;
              int n = get_random_position (q, TRUE);
              if (n < 0 || n >= q->length)
                {
                  g_assert (g_queue_peek_nth (q, n) == NULL);
                }
              else
                {
                  list = qinf->head;
                  for (j = 0; j < n; ++j)
                    list = list->next;

                  g_assert (list->data == g_queue_peek_nth (q, n));
                }
            }
          break;
        case INDEX:
        case LINK_INDEX:
          {
            int x = g_random_int_range (0, 386538);
            int n;
            GList *list;

            g_queue_remove_all (q, GINT_TO_POINTER (x));
            check_integrity (q);
            g_queue_push_tail (q, GINT_TO_POINTER (x));
            check_integrity (q);
            g_queue_sort (q, compare_int, NULL);
            check_integrity (q);

            n = 0;
            for (list = q->head; list != NULL; list = list->next)
              {
                if (list->data == GINT_TO_POINTER (x))
                  break;
                n++;
              }
            g_assert (list);
            g_assert (g_queue_index (q, GINT_TO_POINTER (x)) ==
                      g_queue_link_index (q, list));
            g_assert (g_queue_link_index (q, list) == n);

            qinf->head = q->head;
            qinf->tail = q->tail;
            qinf->length = q->length;
          }
          break;
        case REMOVE:
          if (!g_queue_is_empty (q))
            g_queue_remove (q, qinf->tail->data);
          /* qinf->head/qinf->tail may be invalid at this point */
          if (!g_queue_is_empty (q))
            g_queue_remove (q, q->head->data);
          if (!g_queue_is_empty (q))
            g_queue_remove (q, g_queue_peek_nth (q, get_random_position (q, TRUE)));

          qinf->head = q->head;
          qinf->tail = q->tail;
          qinf->length = q->length;
          break;
        case REMOVE_ALL:
          if (!g_queue_is_empty (q))
            g_queue_remove_all (q, qinf->tail->data);
          /* qinf->head/qinf->tail may be invalid at this point */
          if (!g_queue_is_empty (q))
            g_queue_remove_all (q, q->head->data);
          if (!g_queue_is_empty (q))
            g_queue_remove_all (q, g_queue_peek_nth (q, get_random_position (q, TRUE)));

          qinf->head = q->head;
          qinf->tail = q->tail;
          qinf->length = q->length;
          break;
        case INSERT_BEFORE:
          if (!g_queue_is_empty (q))
            {
              gpointer x = GINT_TO_POINTER (g_random_int_range (0, 386538));

              g_queue_insert_before (q, qinf->tail, x);
              g_queue_insert_before (q, qinf->head, x);
              g_queue_insert_before (q, g_queue_find (q, x), x);
              g_queue_insert_before (q, NULL, x);
            }
          qinf->head = q->head;
          qinf->tail = q->tail;
          qinf->length = q->length;
          break;
        case INSERT_AFTER:
          if (!g_queue_is_empty (q))
            {
              gpointer x = GINT_TO_POINTER (g_random_int_range (0, 386538));

              g_queue_insert_after (q, qinf->tail, x);
              g_queue_insert_after (q, qinf->head, x);
              g_queue_insert_after (q, g_queue_find (q, x), x);
              g_queue_insert_after (q, NULL, x);
            }
          qinf->head = q->head;
          qinf->tail = q->tail;
          qinf->length = q->length;
          break;
        case INSERT_SORTED:
          {
            int max = find_max (q);
            int min = find_min (q);

            if (g_queue_is_empty (q))
              {
                max = 345;
                min = -12;
              }

            g_queue_sort (q, compare_int, NULL);
            check_integrity (q);
            g_queue_insert_sorted (q, GINT_TO_POINTER (max + 1), compare_int, NULL);
            check_integrity (q);
            g_assert (GPOINTER_TO_INT (q->tail->data) == max + 1);
            g_queue_insert_sorted (q, GINT_TO_POINTER (min - 1), compare_int, NULL);
            check_integrity (q);
            g_assert (GPOINTER_TO_INT (q->head->data) == min - 1);
            qinf->head = q->head;
            qinf->tail = q->tail;
            qinf->length = q->length;
          }
          break;
        case PUSH_HEAD_LINK:
          {
            GList *link = g_list_prepend (NULL, GINT_TO_POINTER (i));
            g_queue_push_head_link (q, link);
            if (!qinf->tail)
              qinf->tail = link;
            qinf->head = link;
            qinf->length++;
          }
          break;
        case PUSH_TAIL_LINK:
          {
            GList *link = g_list_prepend (NULL, GINT_TO_POINTER (i));
            g_queue_push_tail_link (q, link);
            if (!qinf->head)
              qinf->head = link;
            qinf->tail = link;
            qinf->length++;
          }
          break;
        case PUSH_NTH_LINK:
          {
            GList *link = g_list_prepend (NULL, GINT_TO_POINTER (i));
            gint n = get_random_position (q, TRUE);
            g_queue_push_nth_link (q, n, link);

            if (qinf->head && qinf->head->prev)
              qinf->head = qinf->head->prev;
            else
              qinf->head = q->head;
            if (qinf->tail && qinf->tail->next)
              qinf->tail = qinf->tail->next;
            else
              qinf->tail = g_list_last (qinf->head);
            qinf->length++;
          }
          break;
        case POP_HEAD_LINK:
          if (!g_queue_is_empty (q))
            {
              qinf->head = qinf->head->next;
              if (!qinf->head)
                qinf->tail = NULL;
              qinf->length--;
              g_list_free (g_queue_pop_head_link (q));
            }
          break;
        case POP_TAIL_LINK:
          if (!g_queue_is_empty (q))
            {
              qinf->tail = qinf->tail->prev;
              if (!qinf->tail)
                qinf->head = NULL;
              qinf->length--;
              g_list_free (g_queue_pop_tail_link (q));
            }
          break;
        case POP_NTH_LINK:
          if (g_queue_is_empty (q))
            g_assert (g_queue_pop_nth_link (q, 200) == NULL);
          else
            {
              int n = get_random_position (q, FALSE);

              if (n == g_queue_get_length (q) - 1)
                qinf->tail = qinf->tail->prev;

              if (n == 0)
                qinf->head = qinf->head->next;

              qinf->length--;

              g_list_free (g_queue_pop_nth_link (q, n));
            }
          break;
        case PEEK_HEAD_LINK:
          if (g_queue_is_empty (q))
            g_assert (g_queue_peek_head_link (q) == NULL);
          else
            g_assert (g_queue_peek_head_link (q) == qinf->head);
          break;
        case PEEK_TAIL_LINK:
          if (g_queue_is_empty (q))
            g_assert (g_queue_peek_tail_link (q) == NULL);
          else
            g_assert (g_queue_peek_tail_link (q) == qinf->tail);
          break;
        case PEEK_NTH_LINK:
          if (g_queue_is_empty(q))
            g_assert (g_queue_peek_nth_link (q, 1000) == NULL);
          else
            {
              gint n = get_random_position (q, FALSE);
              GList *link;

              link = q->head;
              for (j = 0; j < n; ++j)
                link = link->next;

              g_assert (g_queue_peek_nth_link (q, n) == link);
            }
          break;
        case UNLINK:
          if (!g_queue_is_empty (q))
            {
              gint n = g_random_int_range (0, g_queue_get_length (q));
              GList *link;

              link = q->head;
              for (j = 0; j < n; ++j)
                link = link->next;

              g_queue_unlink (q, link);
              check_integrity (q);

              g_list_free (link);

              qinf->head = q->head;
              qinf->tail = q->tail;
              qinf->length--;
            }
          break;
        case DELETE_LINK:
          if (!g_queue_is_empty (q))
            {
              gint n = g_random_int_range (0, g_queue_get_length (q));
              GList *link;

              link = q->head;
              for (j = 0; j < n; ++j)
                link = link->next;

              g_queue_delete_link (q, link);
              check_integrity (q);

              qinf->head = q->head;
              qinf->tail = q->tail;
              qinf->length--;
            }
          break;
        case LAST_OP:
        default:
          g_assert_not_reached();
          break;
        }

      if (qinf->head != q->head ||
          qinf->tail != q->tail ||
          qinf->length != q->length)
        g_printerr ("op: %d\n", op);

      g_assert (qinf->head == q->head);
      g_assert (qinf->tail == q->tail);
      g_assert (qinf->length == q->length);

      for (j = 0; j < N_QUEUES; ++j)
        check_integrity (queues[j].queue);
    }

  for (i = 0; i < N_QUEUES; ++i)
    g_queue_free (queues[i].queue);
}

static void
remove_item (gpointer data, gpointer q)
{
  GQueue *queue = q;

  g_queue_remove (queue, data);
}

static void
test_basic (void)
{
  GQueue *q;
  GList *node;
  gpointer data;

  q = g_queue_new ();

  g_assert (g_queue_is_empty (q));
  g_queue_push_head (q, GINT_TO_POINTER (2));
  check_integrity (q);
  g_assert (g_queue_peek_head (q) == GINT_TO_POINTER (2));
  check_integrity (q);
  g_assert (!g_queue_is_empty (q));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 1);
  g_assert (q->head == q->tail);
  g_queue_push_head (q, GINT_TO_POINTER (1));
  check_integrity (q);
  g_assert (q->head->next == q->tail);
  g_assert (q->tail->prev == q->head);
  g_assert_cmpint (g_list_length (q->head), ==, 2);
  check_integrity (q);
  g_assert (q->tail->data == GINT_TO_POINTER (2));
  g_assert (q->head->data == GINT_TO_POINTER (1));
  check_integrity (q);
  g_queue_push_tail (q, GINT_TO_POINTER (3));
  g_assert_cmpint (g_list_length (q->head), ==, 3);
  g_assert (q->head->data == GINT_TO_POINTER (1));
  g_assert (q->head->next->data == GINT_TO_POINTER (2));
  g_assert (q->head->next->next == q->tail);
  g_assert (q->head->next == q->tail->prev);
  g_assert (q->tail->data == GINT_TO_POINTER (3));
  g_queue_push_tail (q, GINT_TO_POINTER (4));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 4);
  g_assert (q->head->data == GINT_TO_POINTER (1));
  g_assert (g_queue_peek_tail (q) == GINT_TO_POINTER (4));
  g_queue_push_tail (q, GINT_TO_POINTER (5));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 5);
  g_assert (g_queue_is_empty (q) == FALSE);
  check_integrity (q);
  g_assert_cmpint (q->length, ==, 5);
  g_assert (q->head->prev == NULL);
  g_assert (q->head->data == GINT_TO_POINTER (1));
  g_assert (q->head->next->data == GINT_TO_POINTER (2));
  g_assert (q->head->next->next->data == GINT_TO_POINTER (3));
  g_assert (q->head->next->next->next->data == GINT_TO_POINTER (4));
  g_assert (q->head->next->next->next->next->data == GINT_TO_POINTER (5));
  g_assert (q->head->next->next->next->next->next == NULL);
  g_assert (q->head->next->next->next->next == q->tail);
  g_assert (q->tail->data == GINT_TO_POINTER (5));
  g_assert (q->tail->prev->data == GINT_TO_POINTER (4));
  g_assert (q->tail->prev->prev->data == GINT_TO_POINTER (3));
  g_assert (q->tail->prev->prev->prev->data == GINT_TO_POINTER (2));
  g_assert (q->tail->prev->prev->prev->prev->data == GINT_TO_POINTER (1));
  g_assert (q->tail->prev->prev->prev->prev->prev == NULL);
  g_assert (q->tail->prev->prev->prev->prev == q->head);
  g_assert (g_queue_peek_tail (q) == GINT_TO_POINTER (5));
  g_assert (g_queue_peek_head (q) == GINT_TO_POINTER (1));
  g_assert (g_queue_pop_head (q) == GINT_TO_POINTER (1));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 4);
  g_assert_cmpint (q->length, ==, 4);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (5));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 3);

  node = g_queue_pop_head_link (q);
  g_assert (node->data == GINT_TO_POINTER (2));
  g_list_free_1 (node);

  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 2);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (4));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 1);
  node = g_queue_pop_head_link (q);
  g_assert (node->data == GINT_TO_POINTER (3));
  g_list_free_1 (node);
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 0);
  g_assert (g_queue_pop_tail (q) == NULL);
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 0);
  g_assert (g_queue_pop_head (q) == NULL);
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 0);
  g_assert (g_queue_is_empty (q));
  check_integrity (q);

  g_queue_push_head (q, GINT_TO_POINTER (1));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 1);
  g_assert_cmpint (q->length, ==, 1);
  g_queue_push_head (q, GINT_TO_POINTER (2));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 2);
  g_assert_cmpint (q->length, ==, 2);
  g_queue_push_head (q, GINT_TO_POINTER (3));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 3);
  g_assert_cmpint (q->length, ==, 3);
  g_queue_push_head (q, GINT_TO_POINTER (4));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 4);
  g_assert_cmpint (q->length, ==, 4);
  g_queue_push_head (q, GINT_TO_POINTER (5));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 5);
  g_assert_cmpint (q->length, ==, 5);
  g_assert (g_queue_pop_head (q) == GINT_TO_POINTER (5));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 4);
  node = q->tail;
  g_assert (node == g_queue_pop_tail_link (q));
  check_integrity (q);
  g_list_free_1 (node);
  g_assert_cmpint (g_list_length (q->head), ==, 3);
  data = q->head->data;
  g_assert (data == g_queue_pop_head (q));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 2);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (2));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 1);
  g_assert (q->head == q->tail);
  g_assert (g_queue_pop_tail (q) == GINT_TO_POINTER (3));
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 0);
  g_assert (g_queue_pop_head (q) == NULL);
  check_integrity (q);
  g_assert (g_queue_pop_head_link (q) == NULL);
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 0);
  g_assert (g_queue_pop_tail_link (q) == NULL);
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 0);

  g_queue_reverse (q);
  check_integrity (q);
  g_assert_cmpint (g_list_length (q->head), ==, 0);
  g_queue_free (q);
}

static void
test_copy (void)
{
  GQueue *q, *q2;
  gint i;

  q = g_queue_new ();
  q2 = g_queue_copy (q);
  check_integrity (q);
  check_integrity (q2);
  g_assert_cmpint (g_list_length (q->head), ==, 0);
  g_assert_cmpint (g_list_length (q2->head), ==, 0);
  g_queue_sort (q, compare_int, NULL);
  check_integrity (q2);
  check_integrity (q);
  g_queue_sort (q2, compare_int, NULL);
  check_integrity (q2);
  check_integrity (q);

  for (i = 0; i < 200; ++i)
    {
      g_queue_push_nth (q, GINT_TO_POINTER (i), i);
      g_assert (g_queue_find (q, GINT_TO_POINTER (i)));
      check_integrity (q);
      check_integrity (q2);
    }

  for (i = 0; i < 200; ++i)
    {
      g_queue_remove (q, GINT_TO_POINTER (i));
      check_integrity (q);
      check_integrity (q2);
    }

  for (i = 0; i < 200; ++i)
    {
      GList *l = g_list_prepend (NULL, GINT_TO_POINTER (i));

      g_queue_push_nth_link (q, i, l);
      check_integrity (q);
      check_integrity (q2);
      g_queue_reverse (q);
      check_integrity (q);
      check_integrity (q2);
    }

  g_queue_free (q2);
  q2 = g_queue_copy (q);

  g_queue_foreach (q2, remove_item, q2);
  check_integrity (q2);
  check_integrity (q);

  g_queue_free (q);
  g_queue_free (q2);
}

static void
test_off_by_one (void)
{
  GQueue *q;
  GList *node;

  q = g_queue_new ();

  g_queue_push_tail (q, GINT_TO_POINTER (1234));
  check_integrity (q);
  node = g_queue_peek_tail_link (q);
  g_assert (node != NULL && node->data == GINT_TO_POINTER (1234));
  node = g_queue_peek_nth_link (q, g_queue_get_length (q));
  g_assert (node == NULL);
  node = g_queue_peek_nth_link (q, g_queue_get_length (q) - 1);
  g_assert (node->data == GINT_TO_POINTER (1234));
  node = g_queue_pop_nth_link (q, g_queue_get_length (q));
  g_assert (node == NULL);
  node = g_queue_pop_nth_link (q, g_queue_get_length (q) - 1);
  g_assert (node != NULL && node->data == GINT_TO_POINTER (1234));
  g_list_free_1 (node);

  g_queue_free (q);
}

static gint
find_custom (gconstpointer a, gconstpointer b)
{
  return GPOINTER_TO_INT (a) - GPOINTER_TO_INT (b);
}

static void
test_find_custom (void)
{
  GQueue *q;
  GList *node;
  q = g_queue_new ();

  g_queue_push_tail (q, GINT_TO_POINTER (1234));
  g_queue_push_tail (q, GINT_TO_POINTER (1));
  g_queue_push_tail (q, GINT_TO_POINTER (2));
  node = g_queue_find_custom  (q, GINT_TO_POINTER (1), find_custom);
  g_assert (node != NULL);
  node = g_queue_find_custom  (q, GINT_TO_POINTER (2), find_custom);
  g_assert (node != NULL);
  node = g_queue_find_custom  (q, GINT_TO_POINTER (3), find_custom);
  g_assert (node == NULL);

  g_queue_free (q);
}

static void
test_static (void)
{
  GQueue q;
  GQueue q2 = G_QUEUE_INIT;

  g_queue_init (&q);

  check_integrity (&q);
  g_assert (g_queue_is_empty (&q));

  check_integrity (&q2);
  g_assert (g_queue_is_empty (&q2));
}

static void
test_clear (void)
{
  GQueue *q;
  q = g_queue_new ();

  g_queue_push_tail (q, GINT_TO_POINTER (1234));
  g_queue_push_tail (q, GINT_TO_POINTER (1));
  g_queue_push_tail (q, GINT_TO_POINTER (2));
  g_assert_cmpint (g_queue_get_length (q), ==, 3);

  g_queue_clear (q);
  check_integrity (q);
  g_assert (g_queue_is_empty (q));

  g_queue_free (q);
}

typedef struct
{
  gboolean freed;
  int x;
} QueueItem;

static void
free_func (gpointer data)
{
  QueueItem *item = data;

  item->freed = TRUE;
}

static QueueItem *
new_item (int x)
{
  QueueItem *item;

  item = g_slice_new (QueueItem);
  item->freed = FALSE;
  item->x = x;

  return item;
}

static void
test_free_full (void)
{
  QueueItem *one, *two, *three;
  GQueue *queue = NULL;

  queue = g_queue_new();
  g_queue_push_tail (queue, one = new_item (1));
  g_queue_push_tail (queue, two = new_item (2));
  g_queue_push_tail (queue, three = new_item (3));
  g_assert (!one->freed);
  g_assert (!two->freed);
  g_assert (!three->freed);
  g_queue_free_full (queue, free_func);
  g_assert (one->freed);
  g_assert (two->freed);
  g_assert (three->freed);
  g_slice_free (QueueItem, one);
  g_slice_free (QueueItem, two);
  g_slice_free (QueueItem, three);
}


int main (int argc, char *argv[])
{
  guint32 seed;
  gchar *path;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/queue/basic", test_basic);
  g_test_add_func ("/queue/copy", test_copy);
  g_test_add_func ("/queue/off-by-one", test_off_by_one);
  g_test_add_func ("/queue/find-custom", test_find_custom);
  g_test_add_func ("/queue/static", test_static);
  g_test_add_func ("/queue/clear", test_clear);
  g_test_add_func ("/queue/free-full", test_free_full);

  seed = g_test_rand_int_range (0, G_MAXINT);
  path = g_strdup_printf ("/queue/random/seed:%u", seed);
  g_test_add_data_func (path, GUINT_TO_POINTER (seed), random_test);
  g_free (path);

  return g_test_run ();
}
