/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1991, 1992, 1996, 1997,1999,2004 Free Software Foundation, Inc.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "galloca.h"
#include "gmem.h"

#include "gqsort.h"

#include "gtestutils.h"

/* This file was originally from stdlib/msort.c in gnu libc, just changed
   to build inside glib and to not fall back to an unstable quicksort
   for large arrays. */

/* An alternative to qsort, with an identical interface.
   This file is part of the GNU C Library.
   Copyright (C) 1992,95-97,99,2000,01,02,04,07 Free Software Foundation, Inc.
   Written by Mike Haertel, September 1988.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */


struct msort_param
{
  size_t s;
  size_t var;
  GCompareDataFunc cmp;
  void *arg;
  char *t;
};

static void msort_with_tmp (const struct msort_param *p, void *b, size_t n);

static void
msort_with_tmp (const struct msort_param *p, void *b, size_t n)
{
  char *b1, *b2;
  size_t n1, n2;
  char *tmp = p->t;
  const size_t s = p->s;
  GCompareDataFunc cmp = p->cmp;
  void *arg = p->arg;

  if (n <= 1)
    return;

  n1 = n / 2;
  n2 = n - n1;
  b1 = b;
  b2 = (char *) b + (n1 * p->s);

  msort_with_tmp (p, b1, n1);
  msort_with_tmp (p, b2, n2);

  switch (p->var)
    {
    case 0:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      *(guint32 *) tmp = *(guint32 *) b1;
	      b1 += sizeof (guint32);
	      --n1;
	    }
	  else
	    {
	      *(guint32 *) tmp = *(guint32 *) b2;
	      b2 += sizeof (guint32);
	      --n2;
	    }
	  tmp += sizeof (guint32);
	}
      break;
    case 1:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      *(guint64 *) tmp = *(guint64 *) b1;
	      b1 += sizeof (guint64);
	      --n1;
	    }
	  else
	    {
	      *(guint64 *) tmp = *(guint64 *) b2;
	      b2 += sizeof (guint64);
	      --n2;
	    }
	  tmp += sizeof (guint64);
	}
      break;
    case 2:
      while (n1 > 0 && n2 > 0)
	{
	  unsigned long *tmpl = (unsigned long *) tmp;
	  unsigned long *bl;

	  tmp += s;
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      bl = (unsigned long *) b1;
	      b1 += s;
	      --n1;
	    }
	  else
	    {
	      bl = (unsigned long *) b2;
	      b2 += s;
	      --n2;
	    }
	  while (tmpl < (unsigned long *) tmp)
	    *tmpl++ = *bl++;
	}
      break;
    case 3:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (*(const void **) b1, *(const void **) b2, arg) <= 0)
	    {
	      *(void **) tmp = *(void **) b1;
	      b1 += sizeof (void *);
	      --n1;
	    }
	  else
	    {
	      *(void **) tmp = *(void **) b2;
	      b2 += sizeof (void *);
	      --n2;
	    }
	  tmp += sizeof (void *);
	}
      break;
    default:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      memcpy (tmp, b1, s);
	      tmp += s;
	      b1 += s;
	      --n1;
	    }
	  else
	    {
	      memcpy (tmp, b2, s);
	      tmp += s;
	      b2 += s;
	      --n2;
	    }
	}
      break;
    }

  if (n1 > 0)
    memcpy (tmp, b1, n1 * s);
  memcpy (b, p->t, (n - n2) * s);
}


static void
msort_r (void *b, size_t n, size_t s, GCompareDataFunc cmp, void *arg)
{
  size_t size = n * s;
  char *tmp = NULL;
  struct msort_param p;

  /* For large object sizes use indirect sorting.  */
  if (s > 32)
    size = 2 * n * sizeof (void *) + s;

  if (size < 1024)
    /* The temporary array is small, so put it on the stack.  */
    p.t = g_alloca (size);
  else
    {
      /* It's large, so malloc it.  */
      tmp = g_malloc (size);
      p.t = tmp;
    }

  p.s = s;
  p.var = 4;
  p.cmp = cmp;
  p.arg = arg;

  if (s > 32)
    {
      /* Indirect sorting.  */
      char *ip = (char *) b;
      void **tp = (void **) (p.t + n * sizeof (void *));
      void **t = tp;
      void *tmp_storage = (void *) (tp + n);
      char *kp;
      size_t i;

      while ((void *) t < tmp_storage)
	{
	  *t++ = ip;
	  ip += s;
	}
      p.s = sizeof (void *);
      p.var = 3;
      msort_with_tmp (&p, p.t + n * sizeof (void *), n);

      /* tp[0] .. tp[n - 1] is now sorted, copy around entries of
	 the original array.  Knuth vol. 3 (2nd ed.) exercise 5.2-10.  */
      for (i = 0, ip = (char *) b; i < n; i++, ip += s)
	if ((kp = tp[i]) != ip)
	  {
	    size_t j = i;
	    char *jp = ip;
	    memcpy (tmp_storage, ip, s);

	    do
	      {
		size_t k = (kp - (char *) b) / s;
		tp[j] = jp;
		memcpy (jp, kp, s);
		j = k;
		jp = kp;
		kp = tp[k];
	      }
	    while (kp != ip);

	    tp[j] = jp;
	    memcpy (jp, tmp_storage, s);
	  }
    }
  else
    {
      if ((s & (sizeof (guint32) - 1)) == 0
	  && ((char *) b - (char *) 0) % ALIGNOF_GUINT32 == 0)
	{
	  if (s == sizeof (guint32))
	    p.var = 0;
	  else if (s == sizeof (guint64)
		   && ((char *) b - (char *) 0) % ALIGNOF_GUINT64 == 0)
	    p.var = 1;
	  else if ((s & (sizeof (unsigned long) - 1)) == 0
		   && ((char *) b - (char *) 0)
		      % ALIGNOF_UNSIGNED_LONG == 0)
	    p.var = 2;
	}
      msort_with_tmp (&p, b, n);
    }
  g_free (tmp);
}

/**
 * g_qsort_with_data:
 * @pbase: (not nullable): start of array to sort
 * @total_elems: elements in the array
 * @size: size of each element
 * @compare_func: function to compare elements
 * @user_data: data to pass to @compare_func
 *
 * This is just like the standard C qsort() function, but
 * the comparison routine accepts a user data argument.
 *
 * This is guaranteed to be a stable sort since version 2.32.
 */
void
g_qsort_with_data (gconstpointer    pbase,
                   gint             total_elems,
                   gsize            size,
                   GCompareDataFunc compare_func,
                   gpointer         user_data)
{
  msort_r ((gpointer)pbase, total_elems, size, compare_func, user_data);
}
