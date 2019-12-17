/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib.h>
#include "glibintl.h"

#include "gsrvtarget.h"

#include <stdlib.h>
#include <string.h>


/**
 * SECTION:gsrvtarget
 * @short_description: DNS SRV record target
 * @include: gio/gio.h
 *
 * SRV (service) records are used by some network protocols to provide
 * service-specific aliasing and load-balancing. For example, XMPP
 * (Jabber) uses SRV records to locate the XMPP server for a domain;
 * rather than connecting directly to "example.com" or assuming a
 * specific server hostname like "xmpp.example.com", an XMPP client
 * would look up the "xmpp-client" SRV record for "example.com", and
 * then connect to whatever host was pointed to by that record.
 *
 * You can use g_resolver_lookup_service() or
 * g_resolver_lookup_service_async() to find the #GSrvTargets
 * for a given service. However, if you are simply planning to connect
 * to the remote service, you can use #GNetworkService's
 * #GSocketConnectable interface and not need to worry about
 * #GSrvTarget at all.
 */

struct _GSrvTarget {
  gchar   *hostname;
  guint16  port;

  guint16  priority;
  guint16  weight;
};

/**
 * GSrvTarget:
 *
 * A single target host/port that a network service is running on.
 */

G_DEFINE_BOXED_TYPE (GSrvTarget, g_srv_target,
                     g_srv_target_copy, g_srv_target_free)

/**
 * g_srv_target_new:
 * @hostname: the host that the service is running on
 * @port: the port that the service is running on
 * @priority: the target's priority
 * @weight: the target's weight
 *
 * Creates a new #GSrvTarget with the given parameters.
 *
 * You should not need to use this; normally #GSrvTargets are
 * created by #GResolver.
 *
 * Returns: a new #GSrvTarget.
 *
 * Since: 2.22
 */
GSrvTarget *
g_srv_target_new (const gchar *hostname,
                  guint16      port,
                  guint16      priority,
                  guint16      weight)
{
  GSrvTarget *target = g_slice_new0 (GSrvTarget);

  target->hostname = g_strdup (hostname);
  target->port = port;
  target->priority = priority;
  target->weight = weight;

  return target;
}

/**
 * g_srv_target_copy:
 * @target: a #GSrvTarget
 *
 * Copies @target
 *
 * Returns: a copy of @target
 *
 * Since: 2.22
 */
GSrvTarget *
g_srv_target_copy (GSrvTarget *target)
{
  return g_srv_target_new (target->hostname, target->port,
                           target->priority, target->weight);
}

/**
 * g_srv_target_free:
 * @target: a #GSrvTarget
 *
 * Frees @target
 *
 * Since: 2.22
 */
void
g_srv_target_free (GSrvTarget *target)
{
  g_free (target->hostname);
  g_slice_free (GSrvTarget, target);
}

/**
 * g_srv_target_get_hostname:
 * @target: a #GSrvTarget
 *
 * Gets @target's hostname (in ASCII form; if you are going to present
 * this to the user, you should use g_hostname_is_ascii_encoded() to
 * check if it contains encoded Unicode segments, and use
 * g_hostname_to_unicode() to convert it if it does.)
 *
 * Returns: @target's hostname
 *
 * Since: 2.22
 */
const gchar *
g_srv_target_get_hostname (GSrvTarget *target)
{
  return target->hostname;
}

/**
 * g_srv_target_get_port:
 * @target: a #GSrvTarget
 *
 * Gets @target's port
 *
 * Returns: @target's port
 *
 * Since: 2.22
 */
guint16
g_srv_target_get_port (GSrvTarget *target)
{
  return target->port;
}

/**
 * g_srv_target_get_priority:
 * @target: a #GSrvTarget
 *
 * Gets @target's priority. You should not need to look at this;
 * #GResolver already sorts the targets according to the algorithm in
 * RFC 2782.
 *
 * Returns: @target's priority
 *
 * Since: 2.22
 */
guint16
g_srv_target_get_priority (GSrvTarget *target)
{
  return target->priority;
}

/**
 * g_srv_target_get_weight:
 * @target: a #GSrvTarget
 *
 * Gets @target's weight. You should not need to look at this;
 * #GResolver already sorts the targets according to the algorithm in
 * RFC 2782.
 *
 * Returns: @target's weight
 *
 * Since: 2.22
 */
guint16
g_srv_target_get_weight (GSrvTarget *target)
{
  return target->weight;
}

static gint
compare_target (gconstpointer a, gconstpointer b)
{
  GSrvTarget *ta = (GSrvTarget *)a;
  GSrvTarget *tb = (GSrvTarget *)b;

  if (ta->priority == tb->priority)
    {
      /* Arrange targets of the same priority "in any order, except
       * that all those with weight 0 are placed at the beginning of
       * the list"
       */
      return ta->weight - tb->weight;
    }
  else
    return ta->priority - tb->priority;
}

/**
 * g_srv_target_list_sort: (skip)
 * @targets: a #GList of #GSrvTarget
 *
 * Sorts @targets in place according to the algorithm in RFC 2782.
 *
 * Returns: (transfer full): the head of the sorted list.
 *
 * Since: 2.22
 */
GList *
g_srv_target_list_sort (GList *targets)
{
  gint sum, num, val, priority, weight;
  GList *t, *out, *tail;
  GSrvTarget *target;

  if (!targets)
    return NULL;

  if (!targets->next)
    {
      target = targets->data;
      if (!strcmp (target->hostname, "."))
        {
          /* 'A Target of "." means that the service is decidedly not
           * available at this domain.'
           */
          g_srv_target_free (target);
          g_list_free (targets);
          return NULL;
        }
    }

  /* Sort input list by priority, and put the 0-weight targets first
   * in each priority group. Initialize output list to %NULL.
   */
  targets = g_list_sort (targets, compare_target);
  out = tail = NULL;

  /* For each group of targets with the same priority, remove them
   * from @targets and append them to @out in a valid order.
   */
  while (targets)
    {
      priority = ((GSrvTarget *)targets->data)->priority;

      /* Count the number of targets at this priority level, and
       * compute the sum of their weights.
       */
      sum = num = 0;
      for (t = targets; t; t = t->next)
        {
          target = (GSrvTarget *)t->data;
          if (target->priority != priority)
            break;
          sum += target->weight;
          num++;
        }

      /* While there are still targets at this priority level... */
      while (num)
        {
          /* Randomly select from the targets at this priority level,
           * giving precedence to the ones with higher weight,
           * according to the rules from RFC 2782.
           */
          val = g_random_int_range (0, sum + 1);
          for (t = targets; ; t = t->next)
            {
              weight = ((GSrvTarget *)t->data)->weight;
              if (weight >= val)
                break;
              val -= weight;
            }

          targets = g_list_remove_link (targets, t);

          if (!out)
            out = t;
          else
            tail->next = t;
          tail = t;

          sum -= weight;
          num--;
        }
    }

  return out;
}
