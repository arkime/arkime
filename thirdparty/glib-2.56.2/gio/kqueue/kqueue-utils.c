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

#include <sys/types.h>
#include <sys/event.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <errno.h>
#include "kqueue-utils.h"

static gboolean ku_debug_enabled = FALSE;
#define KU_W if (ku_debug_enabled) g_warning



#define KEVENTS_EXTEND_COUNT 10


/**
 * kevents_init_sz:
 * @kv: a #kevents
 * @n_initial: the initial preallocated memory size. If it is less than
 *      %KEVENTS_EXTEND_COUNT, this value will be used instead.
 *
 * Initializes a #kevents object.
 **/
void
kevents_init_sz (kevents *kv, gsize n_initial)
{
  g_assert (kv != NULL);

  memset (kv, 0, sizeof (kevents));

  if (n_initial < KEVENTS_EXTEND_COUNT)
    n_initial = KEVENTS_EXTEND_COUNT;

  kv->memory = g_new0 (struct kevent, n_initial);
  kv->kq_allocated = n_initial;
}


/**
 * kevents_extend_sz:
 * @kv: a #kevents
 * @n_new: the number of new objects to be added
 *
 * Extends the allocated memory, if needed.
 **/
void
kevents_extend_sz (kevents *kv, gsize n_new)
{
  g_assert (kv != NULL);

  if (kv->kq_size + n_new <= kv->kq_allocated)
    return;

  kv->kq_allocated += (n_new + KEVENTS_EXTEND_COUNT);
  kv->memory = g_renew (struct kevent, kv->memory, kv->kq_allocated);
}


/**
 * kevents_reduce:
 * @kv: a #kevents
 *
 * Reduces the allocated heap size, if needed.
 *
 * If the allocated heap size is >= 3*used
 * and 2*used >= %KEVENTS_EXTEND_COUNT, reduce it to 2*used.
 **/
void
kevents_reduce (kevents *kv)
{
  g_assert (kv != NULL);
  gsize candidate_sz;

  if (kv->kq_size == 0 || kv->kq_allocated == 0 || kv->memory == NULL)
    return;

  candidate_sz = 2 * kv->kq_size;

  if (((double) kv->kq_allocated / kv->kq_size) >= 3 &&
      candidate_sz >= KEVENTS_EXTEND_COUNT)
    {
      kv->kq_allocated = candidate_sz;
      kv->memory = g_renew (struct kevent, kv->memory, kv->kq_allocated);
    }
}


/**
 * kevents_free:
 * @kv: a #kevents
 *
 * Resets the kevents object and frees all the associated memory.
 **/
void
kevents_free (kevents *kv)
{
  g_assert (kv != NULL);

  g_free (kv->memory);
  memset (kv, 0, sizeof (kevents));
}


#define SAFE_GENERIC_OP(fcn, fd, data, size) \
  while (size > 0) \
    { \
      gsize retval = fcn (fd, data, size); \
      if (retval == -1) \
        { \
          if (errno == EINTR) \
            continue; \
          else \
            return FALSE; \
        } \
      size -= retval; \
      data += retval; \
    } \
  return TRUE;


/**
 * _ku_read:
 * @fd: a file descriptor
 * @data: the destination buffer
 * @size: how many bytes to read
 *
 * A ready-to-EINTR version of read().
 *
 * This function expects to work with a blocking socket.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 **/
gboolean
_ku_read (int fd, gpointer data, gsize size)
{
  SAFE_GENERIC_OP (read, fd, data, size);
}


/**
 * _ku_write:
 * @fd: a file descriptor
 * @data: the buffer to write
 * @size: how many bytes to write
 *
 * A ready-to-EINTR version of write().
 *
 * This function expects to work with a blocking socket.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 **/
gboolean
_ku_write (int fd, gconstpointer data, gsize size)
{
  SAFE_GENERIC_OP (write, fd, data, size);
}


/**
 * Get some file information by its file descriptor.
 *
 * @param[in]  fd      A file descriptor.
 * @param[out] is_dir  A flag indicating directory.
 * @param[out] inode   A file's inode number.
 **/
void
_ku_file_information (int fd, int *is_dir, ino_t *inode)
{
  g_assert (fd != -1);

  struct stat st;
  memset (&st, 0, sizeof (struct stat));

  if (fstat (fd, &st) == -1)
    {
      KU_W ("fstat failed, assuming it is just a file");
      is_dir = NULL;
      return;
    }

  if (is_dir != NULL)
      *is_dir = ((st.st_mode & S_IFDIR) == S_IFDIR) ? 1 : 0;

  if (inode != NULL)
      *inode = st.st_ino;
}
