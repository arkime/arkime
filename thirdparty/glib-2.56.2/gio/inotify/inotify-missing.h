/* inotify-helper.h - GNOME VFS Monitor using inotify

   Copyright (C) 2006 John McCutchan <john@johnmccutchan.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library; if not, see <http://www.gnu.org/licenses/>.

   Author: John McCutchan <ttb@tentacle.dhs.org>
*/


#ifndef __INOTIFY_MISSING_H
#define __INOTIFY_MISSING_H

#include "inotify-sub.h"

void _im_startup   (void (*missing_cb)(inotify_sub *sub));
void _im_add       (inotify_sub *sub);
void _im_rm        (inotify_sub *sub);
void _im_diag_dump (GIOChannel  *ioc);


#endif /* __INOTIFY_MISSING_H */
