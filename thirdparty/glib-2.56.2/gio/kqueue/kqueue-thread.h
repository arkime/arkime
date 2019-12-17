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

#ifndef __KQUEUE_THREAD_H
#define __KQUEUE_THREAD_H

/**
 * kqueue_notification:
 * @fd: file descriptor, on which an activity has occured.
 * @flags: kqueue event flags, see man kevent(2).
 *
 * Represents an event occured on a file descriptor. Used for marshalling from
 * kqueue thread to its subscribers.
 */
struct kqueue_notification {
  /*< public >*/
  int fd;
  uint32_t flags;
};


void* _kqueue_thread_func      (void *arg);
void  _kqueue_thread_push_fd   (int fd);
void  _kqueue_thread_remove_fd (int fd);

#endif /* __KQUEUE_SUB_H */
