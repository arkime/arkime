/*******************************************************************************
  Copyright (c) 2011, 2012 Dmitry Matveev <me@dmitrymatveev.co.uk>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#ifndef __KQUEUE_SUB_H
#define __KQUEUE_SUB_H

#include "dep-list.h"

/**
 * kqueue_sub:
 * @filename: a name of the file to monitor
 * @user_data: the pointer to user data
 * @pair_moves: unused (currently not implemented)
 * @fd: the associated file descriptor (used by kqueue)
 *
 * Represents a subscription on a file or directory.
 */
typedef struct
{
  gchar*    filename;
  gpointer  user_data;
  gboolean  pair_moves;
  int       fd;
  dep_list* deps;
  int       is_dir;
} kqueue_sub;

kqueue_sub* _kh_sub_new  (const gchar* filename, gboolean pair_moves, gpointer user_data);
void        _kh_sub_free (kqueue_sub* sub);

#endif /* __KQUEUE_SUB_H */
