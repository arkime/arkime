#include <stdio.h>
#include <glib.h>
#include <stdlib.h>

/* Keep this in sync with gsequence.c !!! */
typedef struct _GSequenceNode GSequenceNode;

struct _GSequence
{
  GSequenceNode *       end_node;
  GDestroyNotify        data_destroy_notify;
  gboolean              access_prohibited;
  GSequence *           real_sequence;
};

struct _GSequenceNode
{
  gint                  n_nodes;
  GSequenceNode *       parent;
  GSequenceNode *       left;
  GSequenceNode *       right;
  gpointer              data;
};

static guint
get_priority (GSequenceNode *node)
{
  guint key = GPOINTER_TO_UINT (node);

  key = (key << 15) - key - 1;
  key = key ^ (key >> 12);
  key = key + (key << 2);
  key = key ^ (key >> 4);
  key = key + (key << 3) + (key << 11);
  key = key ^ (key >> 16);

  return key? key : 1;
}

static void
check_node (GSequenceNode *node)
{
  if (node)
    {
      g_assert (node->parent != node);
      if (node->parent)
        g_assert (node->parent->left == node || node->parent->right == node);
      g_assert (node->n_nodes == 1 + (node->left ? node->left->n_nodes : 0) + (node->right ? node->right->n_nodes : 0));
      if (node->left)
          g_assert (get_priority (node) >= get_priority (node->left));
      if (node->right)
          g_assert (get_priority (node) >= get_priority (node->right));
      check_node (node->left);
      check_node (node->right);
    }
}

static void
g_sequence_check (GSequence *seq)
{
  GSequenceNode *node = seq->end_node;

  while (node->parent)
    node = node->parent;

  check_node (node);

  while (node->right)
    node = node->right;

  g_assert (seq->end_node == node);
  g_assert (node->data == seq);

}


enum {
  NEW, FREE, GET_LENGTH, FOREACH, FOREACH_RANGE, SORT, SORT_ITER,

  /* Getting iters */
  GET_BEGIN_ITER, GET_END_ITER, GET_ITER_AT_POS, APPEND, PREPEND,
  INSERT_BEFORE, MOVE, SWAP, INSERT_SORTED, INSERT_SORTED_ITER, SORT_CHANGED,
  SORT_CHANGED_ITER, REMOVE, REMOVE_RANGE, MOVE_RANGE, SEARCH, SEARCH_ITER,
  LOOKUP, LOOKUP_ITER,

  /* dereferencing */
  GET, SET,

  /* operations on GSequenceIter * */
  ITER_IS_BEGIN, ITER_IS_END, ITER_NEXT, ITER_PREV, ITER_GET_POSITION,
  ITER_MOVE, ITER_GET_SEQUENCE,

  /* search */
  ITER_COMPARE, RANGE_GET_MIDPOINT,
  N_OPS
};

typedef struct SequenceInfo
{
  GQueue *      queue;
  GSequence *   sequence;
  int           n_items;
} SequenceInfo;

typedef struct
{
  SequenceInfo *seq;
  int             number;
} Item;

void g_sequence_check (GSequence *sequence);

static Item *
fix_pointer (gconstpointer data)
{
  return (Item *)((char *)data - 1);
}

static Item *
get_item (GSequenceIter *iter)
{
  return fix_pointer (g_sequence_get (iter));
}

static void
check_integrity (SequenceInfo *info)
{
  GList *list;
  GSequenceIter *iter;
  int i;

  g_sequence_check (info->sequence);

#if 0
  if (g_sequence_get_length (info->sequence) != info->n_items)
    g_printerr ("%d %d\n",
             g_sequence_get_length (info->sequence), info->n_items);
#endif
  g_assert (info->n_items == g_queue_get_length (info->queue));
  g_assert (g_sequence_get_length (info->sequence) == info->n_items);

  iter = g_sequence_get_begin_iter (info->sequence);
  list = info->queue->head;
  i = 0;
  while (iter != g_sequence_get_end_iter (info->sequence))
    {
      Item *item;
      g_assert (list->data == iter);
      item = get_item (list->data);
      g_assert (item->seq == info);

      iter = g_sequence_iter_next (iter);
      list = list->next;
      i++;
    }

  g_assert (info->n_items == g_queue_get_length (info->queue));
  g_assert (g_sequence_get_length (info->sequence) == info->n_items);
}

static gpointer
new_item (SequenceInfo *seq)
{
  Item *item = g_new (Item, 1);
  seq->n_items++;
  item->seq = seq;
  item->number = g_random_int ();

  /* There have been bugs in the past where the GSequence would
   * dereference the user pointers. This will make sure such
   * behavior causes crashes
   */
  return ((char *)item + 1);
}

static void
free_item (gpointer data)
{
  Item *item = fix_pointer (data);
  item->seq->n_items--;
  g_free (item);
}

static void
seq_foreach (gpointer data,
             gpointer user_data)
{
  Item *item = fix_pointer (data);
  GList **link = user_data;
  GSequenceIter *iter;

  g_assert (*link != NULL);

  iter = (*link)->data;

  g_assert (get_item (iter) == item);

  item->number = g_random_int();

  *link = (*link)->next;
}

static gint
simple_items_cmp (gconstpointer a,
	          gconstpointer b,
	          gpointer data)
{
  const Item *item_a = fix_pointer (a);
  const Item *item_b = fix_pointer (b);

  if (item_a->number > item_b->number)
    return +1;
  else if (item_a->number < item_b->number)
    return -1;
  else
    return 0;
}

static gint
simple_iters_cmp (gconstpointer a,
	          gconstpointer b,
	          gpointer data)
{
  GSequence *seq = data;
  GSequenceIter *iter_a = (GSequenceIter *)a;
  GSequenceIter *iter_b = (GSequenceIter *)b;
  gpointer item_a = g_sequence_get (iter_a);
  gpointer item_b = g_sequence_get (iter_b);

  if (seq)
    {
      g_assert (g_sequence_iter_get_sequence (iter_a) == seq);
      g_assert (g_sequence_iter_get_sequence (iter_b) == seq);
    }

  return simple_items_cmp (item_a, item_b, data);
}

static gint
compare_items (gconstpointer a,
               gconstpointer b,
               gpointer      data)
{
  const Item *item_a = fix_pointer (a);
  const Item *item_b = fix_pointer (b);

  if (item_a->number < item_b->number)
    {
      return -1;
    }
  else if (item_a->number == item_b->number)
    {
      /* Force an arbitrary order on the items
       * We have to do this, since g_queue_insert_sorted() and
       * g_sequence_insert_sorted() do not agree on the exact
       * position the item is inserted if the new item is
       * equal to an existing one.
       */
      if (item_a < item_b)
        return -1;
      else if (item_a == item_b)
        return 0;
      else
        return 1;
    }
  else
    {
      return 1;
    }
}

static void
check_sorted (SequenceInfo *info)
{
  GList *list;
  int last;
  GSequenceIter *last_iter;

  check_integrity (info);

  last = G_MININT;
  last_iter = NULL;
  for (list = info->queue->head; list != NULL; list = list->next)
    {
      GSequenceIter *iter = list->data;
      Item *item = get_item (iter);

      g_assert (item->number >= last);
      /* Check that the ordering is the same as that of the queue,
       * ie. that the sort is stable
       */
      if (last_iter)
        g_assert (iter == g_sequence_iter_next (last_iter));

      last = item->number;
      last_iter = iter;
    }
}

static gint
compare_iters (gconstpointer a,
               gconstpointer b,
               gpointer      data)
{
  GSequence *seq = data;
  GSequenceIter *iter_a = (GSequenceIter *)a;
  GSequenceIter *iter_b = (GSequenceIter *)b;
  /* compare_items() will fix up the pointers */
  Item *item_a = g_sequence_get (iter_a);
  Item *item_b = g_sequence_get (iter_b);

  if (seq)
    {
      g_assert (g_sequence_iter_get_sequence (iter_a) == seq);
      g_assert (g_sequence_iter_get_sequence (iter_b) == seq);
    }

  return compare_items (item_a, item_b, data);
}

/* A version of g_queue_link_index() that treats NULL as just
 * beyond the queue
 */
static int
queue_link_index (SequenceInfo *seq, GList *link)
{
  if (link)
    return g_queue_link_index (seq->queue, link);
  else
    return g_queue_get_length (seq->queue);
}

static void
get_random_range (SequenceInfo *seq,
                  GSequenceIter **begin_iter,
                  GSequenceIter **end_iter,
                  GList **begin_link,
                  GList **end_link)
{
  int length = g_queue_get_length (seq->queue);
  int b = g_random_int_range (0, length + 1);
  int e = g_random_int_range (b, length + 1);

  g_assert (length == g_sequence_get_length (seq->sequence));

  if (begin_iter)
    *begin_iter = g_sequence_get_iter_at_pos (seq->sequence, b);
  if (end_iter)
    *end_iter = g_sequence_get_iter_at_pos (seq->sequence, e);
  if (begin_link)
    *begin_link = g_queue_peek_nth_link (seq->queue, b);
  if (end_link)
    *end_link = g_queue_peek_nth_link (seq->queue, e);
  if (begin_iter && begin_link)
    {
      g_assert (
                queue_link_index (seq, *begin_link) ==
                g_sequence_iter_get_position (*begin_iter));
    }
  if (end_iter && end_link)
    {
      g_assert (
                queue_link_index (seq, *end_link) ==
                g_sequence_iter_get_position (*end_iter));
    }
}

static gint
get_random_position (SequenceInfo *seq)
{
  int length = g_queue_get_length (seq->queue);

  g_assert (length == g_sequence_get_length (seq->sequence));

  return g_random_int_range (-2, length + 5);
}

static GSequenceIter *
get_random_iter (SequenceInfo  *seq,
                 GList        **link)
{
  GSequenceIter *iter;
  int pos = get_random_position (seq);
  if (link)
    *link = g_queue_peek_nth_link (seq->queue, pos);
  iter = g_sequence_get_iter_at_pos (seq->sequence, pos);
  if (link)
    g_assert (queue_link_index (seq, *link) == g_sequence_iter_get_position (iter));
  return iter;
}

static void
dump_info (SequenceInfo *seq)
{
#if 0
  GSequenceIter *iter;
  GList *list;

  iter = g_sequence_get_begin_iter (seq->sequence);
  list = seq->queue->head;

  while (iter != g_sequence_get_end_iter (seq->sequence))
    {
      Item *item = get_item (iter);
      g_printerr ("%p  %p    %d\n", list->data, iter, item->number);

      iter = g_sequence_iter_next (iter);
      list = list->next;
    }
#endif
}

static void
run_random_tests (gconstpointer d)
{
  guint32 seed = GPOINTER_TO_UINT (d);
#define N_ITERATIONS 60000
#define N_SEQUENCES 8
#define N_TIMES 24

  SequenceInfo sequences[N_SEQUENCES];
  int k;

#if 0
  g_printerr ("    seed: %u\n", seed);
#endif

  g_random_set_seed (seed);

  for (k = 0; k < N_SEQUENCES; ++k)
    {
      sequences[k].queue = g_queue_new ();
      sequences[k].sequence = g_sequence_new (free_item);
      sequences[k].n_items = 0;
    }

#define RANDOM_SEQUENCE() &(sequences[g_random_int_range (0, N_SEQUENCES)])

  for (k = 0; k < N_ITERATIONS; ++k)
    {
      int i;
      SequenceInfo *seq = RANDOM_SEQUENCE();
      int op = g_random_int_range (0, N_OPS);

#if 0
      g_printerr ("%d on %p\n", op, seq);
#endif

      switch (op)
        {
        case NEW:
        case FREE:
          {
            g_queue_free (seq->queue);
            g_sequence_free (seq->sequence);

            g_assert (seq->n_items == 0);

            seq->queue = g_queue_new ();
            seq->sequence = g_sequence_new (free_item);

            check_integrity (seq);
          }
          break;
        case GET_LENGTH:
          {
            int slen = g_sequence_get_length (seq->sequence);
            int qlen = g_queue_get_length (seq->queue);

            g_assert (slen == qlen);
          }
          break;
        case FOREACH:
          {
            GList *link = seq->queue->head;
            g_sequence_foreach (seq->sequence, seq_foreach, &link);
            g_assert (link == NULL);
          }
          break;
        case FOREACH_RANGE:
          {
            GSequenceIter *begin_iter, *end_iter;
            GList *begin_link, *end_link;

            get_random_range (seq, &begin_iter, &end_iter, &begin_link, &end_link);

            check_integrity (seq);

            g_sequence_foreach_range (begin_iter, end_iter, seq_foreach, &begin_link);

            g_assert (begin_link == end_link);
          }
          break;
        case SORT:
          {
            dump_info (seq);

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            dump_info (seq);
          }
          break;
        case SORT_ITER:
          {
            check_integrity (seq);
            g_sequence_sort_iter (seq->sequence,
                                  (GSequenceIterCompareFunc)compare_iters, seq->sequence);
            g_queue_sort (seq->queue, compare_iters, NULL);
            check_sorted (seq);
          }
          break;

          /* Getting iters */
        case GET_END_ITER:
        case GET_BEGIN_ITER:
          {
            GSequenceIter *begin_iter;
            GSequenceIter *end_iter;
            GSequenceIter *penultimate_iter;

            begin_iter = g_sequence_get_begin_iter (seq->sequence);
            check_integrity (seq);

            end_iter = g_sequence_get_end_iter (seq->sequence);
            check_integrity (seq);

            penultimate_iter = g_sequence_iter_prev (end_iter);
            check_integrity (seq);

            if (g_sequence_get_length (seq->sequence) > 0)
              {
                g_assert (seq->queue->head);
                g_assert (seq->queue->head->data == begin_iter);
                g_assert (seq->queue->tail);
                g_assert (seq->queue->tail->data == penultimate_iter);
              }
            else
              {
                g_assert (penultimate_iter == end_iter);
                g_assert (begin_iter == end_iter);
                g_assert (penultimate_iter == begin_iter);
                g_assert (seq->queue->head == NULL);
                g_assert (seq->queue->tail == NULL);
              }
          }
          break;
        case GET_ITER_AT_POS:
          {
            int i;

            g_assert (g_queue_get_length (seq->queue) == g_sequence_get_length (seq->sequence));

            for (i = 0; i < 10; ++i)
              {
                int pos = get_random_position (seq);
                GSequenceIter *iter = g_sequence_get_iter_at_pos (seq->sequence, pos);
                GList *link = g_queue_peek_nth_link (seq->queue, pos);
                check_integrity (seq);
                if (pos >= g_sequence_get_length (seq->sequence) || pos < 0)
                  {
                    g_assert (iter == g_sequence_get_end_iter (seq->sequence));
                    g_assert (link == NULL);
                  }
                else
                  {
                    g_assert (link);
                    g_assert (link->data == iter);
                  }
              }
          }
          break;
        case APPEND:
          {
            for (i = 0; i < 10; ++i)
              {
                GSequenceIter *iter = g_sequence_append (seq->sequence, new_item (seq));
                g_queue_push_tail (seq->queue, iter);
              }
          }
          break;
        case PREPEND:
          {
            for (i = 0; i < 10; ++i)
              {
                GSequenceIter *iter = g_sequence_prepend (seq->sequence, new_item (seq));
                g_queue_push_head (seq->queue, iter);
              }
          }
          break;
        case INSERT_BEFORE:
          {
            for (i = 0; i < 10; ++i)
              {
                GList *link;
                GSequenceIter *iter = get_random_iter (seq, &link);
                GSequenceIter *new_iter;
                check_integrity (seq);

                new_iter = g_sequence_insert_before (iter, new_item (seq));

                g_queue_insert_before (seq->queue, link, new_iter);
              }
          }
          break;
        case MOVE:
          {
            GList *link1, *link2;
            SequenceInfo *seq1 = RANDOM_SEQUENCE();
            SequenceInfo *seq2 = RANDOM_SEQUENCE();
            GSequenceIter *iter1 = get_random_iter (seq1, &link1);
            GSequenceIter *iter2 = get_random_iter (seq2, &link2);

            if (!g_sequence_iter_is_end (iter1))
              {
                g_sequence_move (iter1, iter2);

                if (!link2)
                  g_assert (g_sequence_iter_is_end (iter2));

                g_queue_insert_before (seq2->queue, link2, link1->data);

                g_queue_delete_link (seq1->queue, link1);

                get_item (iter1)->seq = seq2;

                seq1->n_items--;
                seq2->n_items++;
              }

            check_integrity (seq);

            iter1 = get_random_iter (seq, NULL);

            /* Moving an iter to itself should have no effect */
            if (!g_sequence_iter_is_end (iter1))
              g_sequence_move (iter1, iter1);
          }
          break;
        case SWAP:
          {
            GList *link1, *link2;
            SequenceInfo *seq1 = RANDOM_SEQUENCE();
            SequenceInfo *seq2 = RANDOM_SEQUENCE();
            GSequenceIter *iter1 = get_random_iter (seq1, &link1);
            GSequenceIter *iter2 = get_random_iter (seq2, &link2);

            if (!g_sequence_iter_is_end (iter1) &&
                !g_sequence_iter_is_end (iter2))
              {
                gpointer tmp;

                g_sequence_swap (iter1, iter2);

                get_item (iter1)->seq = seq2;
                get_item (iter2)->seq = seq1;

                tmp = link1->data;
                link1->data = link2->data;
                link2->data = tmp;
              }
          }
          break;
        case INSERT_SORTED:
          {
            int i;
            dump_info (seq);

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            for (i = 0; i < N_TIMES; ++i)
              {
                GSequenceIter *iter =
                  g_sequence_insert_sorted (seq->sequence, new_item(seq), compare_items, NULL);

                g_queue_insert_sorted (seq->queue, iter, compare_iters, NULL);
              }

            check_sorted (seq);

            dump_info (seq);
          }
          break;
        case INSERT_SORTED_ITER:
          {
            int i;
            dump_info (seq);

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            for (i = 0; i < N_TIMES; ++i)
              {
                GSequenceIter *iter;

                iter = g_sequence_insert_sorted_iter (seq->sequence,
                                                      new_item (seq),
                                                      (GSequenceIterCompareFunc)compare_iters,
                                                      seq->sequence);

                g_queue_insert_sorted (seq->queue, iter, compare_iters, NULL);
              }

            check_sorted (seq);

            dump_info (seq);
          }
          break;
        case SORT_CHANGED:
          {
            int i;

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            for (i = 0; i < N_TIMES; ++i)
              {
                GList *link;
                GSequenceIter *iter = get_random_iter (seq, &link);

                if (!g_sequence_iter_is_end (iter))
                  {
                    g_sequence_set (iter, new_item (seq));
                    g_sequence_sort_changed (iter, compare_items, NULL);

                    g_queue_delete_link (seq->queue, link);
                    g_queue_insert_sorted (seq->queue, iter, compare_iters, NULL);
                  }

                check_sorted (seq);
              }
          }
          break;
        case SORT_CHANGED_ITER:
          {
            int i;

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            for (i = 0; i < N_TIMES; ++i)
              {
                GList *link;
                GSequenceIter *iter = get_random_iter (seq, &link);

                if (!g_sequence_iter_is_end (iter))
                  {
                    g_sequence_set (iter, new_item (seq));
                    g_sequence_sort_changed_iter (iter,
                                                  (GSequenceIterCompareFunc)compare_iters, seq->sequence);

                    g_queue_delete_link (seq->queue, link);
                    g_queue_insert_sorted (seq->queue, iter, compare_iters, NULL);
                  }

                check_sorted (seq);
              }
          }
          break;
        case REMOVE:
          {
            int i;

            for (i = 0; i < N_TIMES; ++i)
              {
                GList *link;
                GSequenceIter *iter = get_random_iter (seq, &link);

                if (!g_sequence_iter_is_end (iter))
                  {
                    g_sequence_remove (iter);
                    g_queue_delete_link (seq->queue, link);
                  }
              }
          }
          break;
        case REMOVE_RANGE:
          {
            GSequenceIter *begin_iter, *end_iter;
            GList *begin_link, *end_link;
            GList *list;

            get_random_range (seq, &begin_iter, &end_iter, &begin_link, &end_link);

            g_sequence_remove_range (begin_iter, end_iter);

            list = begin_link;
            while (list != end_link)
              {
                GList *next = list->next;

                g_queue_delete_link (seq->queue, list);

                list = next;
              }
          }
          break;
        case MOVE_RANGE:
          {
            SequenceInfo *src = RANDOM_SEQUENCE();
            SequenceInfo *dst = RANDOM_SEQUENCE();

            GSequenceIter *begin_iter, *end_iter;
            GList *begin_link, *end_link;

            GSequenceIter *dst_iter;
            GList *dst_link;

            GList *list;

            g_assert (src->queue);
            g_assert (dst->queue);

            get_random_range (src, &begin_iter, &end_iter, &begin_link, &end_link);
            dst_iter = get_random_iter (dst, &dst_link);

            g_sequence_move_range (dst_iter, begin_iter, end_iter);

            if (dst_link == begin_link || (src == dst && dst_link == end_link))
              {
                check_integrity (src);
                check_integrity (dst);
                break;
              }

            if (queue_link_index (src, begin_link) >=
                queue_link_index (src, end_link))
              {
                break;
              }

            if (src == dst &&
                queue_link_index (src, dst_link) >= queue_link_index (src, begin_link) &&
                queue_link_index (src, dst_link) <= queue_link_index (src, end_link))
              {
                break;
              }

            list = begin_link;
            while (list != end_link)
              {
                GList *next = list->next;
                Item *item = get_item (list->data);

                g_assert (dst->queue);
                g_queue_insert_before (dst->queue, dst_link, list->data);
                g_queue_delete_link (src->queue, list);

                g_assert (item->seq == src);

                src->n_items--;
                dst->n_items++;
                item->seq = dst;

                list = next;
              }
          }
          break;
        case SEARCH:
          {
            Item *item;
            GSequenceIter *search_iter;
            GSequenceIter *insert_iter;

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            item = new_item (seq);
            search_iter = g_sequence_search (seq->sequence, item, compare_items, NULL);

            insert_iter = g_sequence_insert_sorted (seq->sequence, item, compare_items, NULL);

            g_assert (search_iter == g_sequence_iter_next (insert_iter));

            g_queue_insert_sorted (seq->queue, insert_iter, compare_iters, NULL);
          }
          break;
        case SEARCH_ITER:
          {
            Item *item;
            GSequenceIter *search_iter;
            GSequenceIter *insert_iter;

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            item = new_item (seq);
            search_iter = g_sequence_search_iter (seq->sequence,
                                                  item,
                                                  (GSequenceIterCompareFunc)compare_iters, seq->sequence);

            insert_iter = g_sequence_insert_sorted (seq->sequence, item, compare_items, NULL);

            g_assert (search_iter == g_sequence_iter_next (insert_iter));

            g_queue_insert_sorted (seq->queue, insert_iter, compare_iters, NULL);
          }
          break;
        case LOOKUP:
          {
            Item *item;
            GSequenceIter *lookup_iter;
            GSequenceIter *insert_iter;

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            item = new_item (seq);
            insert_iter = g_sequence_insert_sorted (seq->sequence, item, compare_items, NULL);
            g_queue_insert_sorted (seq->queue, insert_iter, compare_iters, NULL);

            lookup_iter = g_sequence_lookup (seq->sequence, item, simple_items_cmp, NULL);
            g_assert (simple_iters_cmp (insert_iter, lookup_iter, NULL) == 0);
          }
          break;
        case LOOKUP_ITER:
          {
            Item *item;
            GSequenceIter *lookup_iter;
            GSequenceIter *insert_iter;

            g_sequence_sort (seq->sequence, compare_items, NULL);
            g_queue_sort (seq->queue, compare_iters, NULL);

            check_sorted (seq);

            item = new_item (seq);
            insert_iter = g_sequence_insert_sorted (seq->sequence, item, compare_items, NULL);
            g_queue_insert_sorted (seq->queue, insert_iter, compare_iters, NULL);

            lookup_iter = g_sequence_lookup_iter (seq->sequence, item,
                (GSequenceIterCompareFunc) simple_iters_cmp, NULL);
            g_assert (simple_iters_cmp (insert_iter, lookup_iter, NULL) == 0);
          }
          break;

          /* dereferencing */
        case GET:
        case SET:
          {
            GSequenceIter *iter;
            GList *link;

            iter = get_random_iter (seq, &link);

            if (!g_sequence_iter_is_end (iter))
              {
                Item *item;
                int i;

                check_integrity (seq);

                /* Test basic functionality */
                item = new_item (seq);
                g_sequence_set (iter, item);
                g_assert (g_sequence_get (iter) == item);

                /* Make sure that existing items are freed */
                for (i = 0; i < N_TIMES; ++i)
                  g_sequence_set (iter, new_item (seq));

                check_integrity (seq);

                g_sequence_set (iter, new_item (seq));
              }
          }
          break;

          /* operations on GSequenceIter * */
        case ITER_IS_BEGIN:
          {
            GSequenceIter *iter;

            iter = g_sequence_get_iter_at_pos (seq->sequence, 0);

            g_assert (g_sequence_iter_is_begin (iter));

            check_integrity (seq);

            if (g_sequence_get_length (seq->sequence) > 0)
              {
                g_assert (!g_sequence_iter_is_begin (g_sequence_get_end_iter (seq->sequence)));
              }
            else
              {
                g_assert (g_sequence_iter_is_begin (g_sequence_get_end_iter (seq->sequence)));
              }

            g_assert (g_sequence_iter_is_begin (g_sequence_get_begin_iter (seq->sequence)));
          }
          break;
        case ITER_IS_END:
          {
            GSequenceIter *iter;
            int len = g_sequence_get_length (seq->sequence);

            iter = g_sequence_get_iter_at_pos (seq->sequence, len);

            g_assert (g_sequence_iter_is_end (iter));

            if (len > 0)
              {
                g_assert (!g_sequence_iter_is_end (g_sequence_get_begin_iter (seq->sequence)));
              }
            else
              {
                g_assert (g_sequence_iter_is_end (g_sequence_get_begin_iter (seq->sequence)));
              }

            g_assert (g_sequence_iter_is_end (g_sequence_get_end_iter (seq->sequence)));
          }
          break;
        case ITER_NEXT:
          {
            GSequenceIter *iter1, *iter2, *iter3, *end;

            iter1 = g_sequence_append (seq->sequence, new_item (seq));
            iter2 = g_sequence_append (seq->sequence, new_item (seq));
            iter3 = g_sequence_append (seq->sequence, new_item (seq));

            end = g_sequence_get_end_iter (seq->sequence);

            g_assert (g_sequence_iter_next (iter1) == iter2);
            g_assert (g_sequence_iter_next (iter2) == iter3);
            g_assert (g_sequence_iter_next (iter3) == end);
            g_assert (g_sequence_iter_next (end) == end);

            g_queue_push_tail (seq->queue, iter1);
            g_queue_push_tail (seq->queue, iter2);
            g_queue_push_tail (seq->queue, iter3);
          }
          break;
        case ITER_PREV:
          {
            GSequenceIter *iter1, *iter2, *iter3, *begin;

            iter1 = g_sequence_prepend (seq->sequence, new_item (seq));
            iter2 = g_sequence_prepend (seq->sequence, new_item (seq));
            iter3 = g_sequence_prepend (seq->sequence, new_item (seq));

            begin = g_sequence_get_begin_iter (seq->sequence);

            g_assert (g_sequence_iter_prev (iter1) == iter2);
            g_assert (g_sequence_iter_prev (iter2) == iter3);
            g_assert (iter3 == begin);
            g_assert (g_sequence_iter_prev (iter3) == begin);
            g_assert (g_sequence_iter_prev (begin) == begin);

            g_queue_push_head (seq->queue, iter1);
            g_queue_push_head (seq->queue, iter2);
            g_queue_push_head (seq->queue, iter3);
          }
          break;
        case ITER_GET_POSITION:
          {
            GList *link;
            GSequenceIter *iter = get_random_iter (seq, &link);

            g_assert (g_sequence_iter_get_position (iter) ==
                      queue_link_index (seq, link));
          }
          break;
        case ITER_MOVE:
          {
            int len = g_sequence_get_length (seq->sequence);
            GSequenceIter *iter;
            int pos;

            iter = get_random_iter (seq, NULL);
            pos = g_sequence_iter_get_position (iter);
            iter = g_sequence_iter_move (iter, len - pos);
            g_assert (g_sequence_iter_is_end (iter));


            iter = get_random_iter (seq, NULL);
            pos = g_sequence_iter_get_position (iter);
            while (pos < len)
              {
                g_assert (!g_sequence_iter_is_end (iter));
                pos++;
                iter = g_sequence_iter_move (iter, 1);
              }
            g_assert (g_sequence_iter_is_end (iter));
          }
          break;
        case ITER_GET_SEQUENCE:
          {
            GSequenceIter *iter = get_random_iter (seq, NULL);

            g_assert (g_sequence_iter_get_sequence (iter) == seq->sequence);
          }
          break;

          /* search */
        case ITER_COMPARE:
          {
            GList *link1, *link2;
            GSequenceIter *iter1 = get_random_iter (seq, &link1);
            GSequenceIter *iter2 = get_random_iter (seq, &link2);

            int cmp = g_sequence_iter_compare (iter1, iter2);
            int pos1 = queue_link_index (seq, link1);
            int pos2 = queue_link_index (seq, link2);

            if (cmp == 0)
              {
                g_assert (pos1 == pos2);
              }
            else if (cmp < 0)
              {
                g_assert (pos1 < pos2);
              }
            else
              {
                g_assert (pos1 > pos2);
              }
          }
          break;
        case RANGE_GET_MIDPOINT:
          {
            GSequenceIter *iter1 = get_random_iter (seq, NULL);
            GSequenceIter *iter2 = get_random_iter (seq, NULL);
            GSequenceIter *iter3;
            int cmp;

            cmp = g_sequence_iter_compare (iter1, iter2);

            if (cmp > 0)
              {
                GSequenceIter *tmp;

                tmp = iter1;
                iter1 = iter2;
                iter2 = tmp;
              }

            iter3 = g_sequence_range_get_midpoint (iter1, iter2);

            if (cmp == 0)
              {
                g_assert (iter3 == iter1);
                g_assert (iter3 == iter2);
              }

            g_assert (g_sequence_iter_get_position (iter3) >=
                      g_sequence_iter_get_position (iter1));
            g_assert (g_sequence_iter_get_position (iter2) >=
                      g_sequence_iter_get_position (iter3));
          }
          break;

        }

      check_integrity (seq);
    }

  for (k = 0; k < N_SEQUENCES; ++k)
    {
      g_queue_free (sequences[k].queue);
      g_sequence_free (sequences[k].sequence);
      sequences[k].n_items = 0;
    }
}

/* Random seeds known to have failed at one point
 */
static gulong seeds[] =
  {
    825541564u,
    801678400u,
    1477639090u,
    3369132895u,
    1192944867u,
    770458294u,
    1099575817u,
    590523467u,
    3583571454u,
    579241222u
  };

/* Single, stand-alone tests */

static void
test_out_of_range_jump (void)
{
  GSequence *seq = g_sequence_new (NULL);
  GSequenceIter *iter = g_sequence_get_begin_iter (seq);

  g_sequence_iter_move (iter, 5);

  g_assert (g_sequence_iter_is_begin (iter));
  g_assert (g_sequence_iter_is_end (iter));

  g_sequence_free (seq);
}

static void
test_iter_move (void)
{
  GSequence *seq = g_sequence_new (NULL);
  GSequenceIter *iter;
  gint i;

  for (i = 0; i < 10; ++i)
    g_sequence_append (seq, GINT_TO_POINTER (i));

  iter = g_sequence_get_begin_iter (seq);
  iter = g_sequence_iter_move (iter, 5);
  g_assert_cmpint (GPOINTER_TO_INT (g_sequence_get (iter)), ==, 5);

  iter = g_sequence_iter_move (iter, -10);
  g_assert (g_sequence_iter_is_begin (iter));

  iter = g_sequence_get_end_iter (seq);
  iter = g_sequence_iter_move (iter, -5);
  g_assert_cmpint (GPOINTER_TO_INT (g_sequence_get (iter)), ==, 5);

  iter = g_sequence_iter_move (iter, 10);
  g_assert (g_sequence_iter_is_end (iter));

  g_sequence_free (seq);
}

static int
compare (gconstpointer a, gconstpointer b, gpointer userdata)
{
  int ai, bi;

  ai = GPOINTER_TO_INT (a);
  bi = GPOINTER_TO_INT (b);

  if (ai < bi)
    return -1;
  else if (ai > bi)
    return 1;
  else
    return 0;
}

static int
compare_iter (GSequenceIter *a,
              GSequenceIter *b,
              gpointer data)
{
  return compare (g_sequence_get (a),
                  g_sequence_get (b),
                  data);
}

static void
test_insert_sorted_non_pointer (void)
{
  int i;

  for (i = 0; i < 10; i++)
    {
      GSequence *seq = g_sequence_new (NULL);
      int j;

      for (j = 0; j < 10000; j++)
        {
          g_sequence_insert_sorted (seq, GINT_TO_POINTER (g_random_int()),
                                    compare, NULL);

          g_sequence_insert_sorted_iter (seq, GINT_TO_POINTER (g_random_int()),
                                         compare_iter, NULL);
        }

      g_sequence_check (seq);

      g_sequence_free (seq);
    }
}

static void
test_stable_sort (void)
{
  int i;
  GSequence *seq = g_sequence_new (NULL);

#define N_ITEMS 1000

  GSequenceIter *iters[N_ITEMS];
  GSequenceIter *iter;

  for (i = 0; i < N_ITEMS; ++i)
    {
      iters[i] = g_sequence_append (seq, GINT_TO_POINTER (3000));
      g_sequence_check (seq);
      g_assert (g_sequence_iter_get_sequence (iters[i]) == seq);
   }

  i = 0;
  iter = g_sequence_get_begin_iter (seq);
  g_assert (g_sequence_iter_get_sequence (iter) == seq);
  g_sequence_check (seq);
  while (!g_sequence_iter_is_end (iter))
    {
      g_assert (g_sequence_iter_get_sequence (iters[i]) == seq);
      g_assert (iters[i++] == iter);

      iter = g_sequence_iter_next (iter);
      g_sequence_check (seq);
    }

  g_sequence_sort (seq, compare, NULL);

  i = 0;
  iter = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (iter))
    {
      g_assert (g_sequence_iter_get_sequence (iters[i]) == seq);
      g_assert (iters[i] == iter);

      iter = g_sequence_iter_next (iter);
      g_sequence_check (seq);

      i++;
    }

  for (i = N_ITEMS - 1; i >= 0; --i)
    {
      g_sequence_check (seq);
      g_assert (g_sequence_iter_get_sequence (iters[i]) == seq);
      g_assert (g_sequence_get_end_iter (seq) != iters[i]);
      g_sequence_sort_changed (iters[i], compare, NULL);
    }

  i = 0;
  iter = g_sequence_get_begin_iter (seq);
  while (!g_sequence_iter_is_end (iter))
    {
      g_assert (iters[i++] == iter);

      iter = g_sequence_iter_next (iter);
      g_sequence_check (seq);
    }

  g_sequence_free (seq);
}

static void
test_empty (void)
{
  GSequence *seq;
  int i;

  seq = g_sequence_new (NULL);
  g_assert_true (g_sequence_is_empty (seq));

  for (i = 0; i < 1000; i++)
    {
      g_sequence_append (seq, GINT_TO_POINTER (i));
      g_assert_false (g_sequence_is_empty (seq));
    }

  for (i = 0; i < 1000; i++)
    {
      GSequenceIter *end = g_sequence_get_end_iter (seq);
      g_assert_false (g_sequence_is_empty (seq));
      g_sequence_remove (g_sequence_iter_prev (end));
    }

  g_assert_true (g_sequence_is_empty (seq));
}

int
main (int argc,
      char **argv)
{
  gint i;
  guint32 seed;
  gchar *path;

  g_test_init (&argc, &argv, NULL);

  /* Standalone tests */
  g_test_add_func ("/sequence/out-of-range-jump", test_out_of_range_jump);
  g_test_add_func ("/sequence/iter-move", test_iter_move);
  g_test_add_func ("/sequence/insert-sorted-non-pointer", test_insert_sorted_non_pointer);
  g_test_add_func ("/sequence/stable-sort", test_stable_sort);
  g_test_add_func ("/sequence/is_empty", test_empty);

  /* Regression tests */
  for (i = 0; i < G_N_ELEMENTS (seeds); ++i)
    {
      path = g_strdup_printf ("/sequence/random/seed:%lu", seeds[i]);
      g_test_add_data_func (path, GUINT_TO_POINTER (seeds[i]), run_random_tests);
      g_free (path);
    }

  /* New random seed */
  seed = g_test_rand_int_range (0, G_MAXINT);
  path = g_strdup_printf ("/sequence/random/seed:%u", seed);
  g_test_add_data_func (path, GUINT_TO_POINTER (seed), run_random_tests);
  g_free (path);

  return g_test_run ();
}

