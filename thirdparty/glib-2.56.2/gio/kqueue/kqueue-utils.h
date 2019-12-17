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

#ifndef __KQUEUE_UTILS_H
#define __KQUEUE_UTILS_H

#include <sys/types.h> /* ino_t */

/**
 * kqueue_notification:
 * @memory: a pointer to the allocated memory
 * @kq_size: the number of used items
 * @kq_allocated: the number of allocated items
 *
 * Represents a pool of (struct kevent) objects.
 */
typedef struct {
  struct kevent *memory;
  gsize kq_size;
  gsize kq_allocated;
} kevents;

void kevents_init_sz   (kevents *kv, gsize n_initial);
void kevents_extend_sz (kevents *kv, gsize n_new);
void kevents_reduce    (kevents *kv);
void kevents_free      (kevents *kv);


gboolean _ku_read             (int fd, gpointer data, gsize size);
gboolean _ku_write            (int fd, gconstpointer data, gsize size);

void     _ku_file_information (int fd, int *is_dir, ino_t *inode);

#endif /* __KQUEUE_UTILS_H */
