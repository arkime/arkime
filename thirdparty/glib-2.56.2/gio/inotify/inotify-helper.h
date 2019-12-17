/* inotify-helper.h - GVFS Directory Monitor using inotify

   Copyright (C) 2007 John McCutchan

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

   Author: John McCutchan <john@johnmccutchan.com>
*/


#ifndef __INOTIFY_HELPER_H
#define __INOTIFY_HELPER_H

#include "inotify-sub.h"

gboolean _ih_startup    (void);
gboolean _ih_sub_add    (inotify_sub *sub);
gboolean _ih_sub_cancel (inotify_sub *sub);

#endif /* __INOTIFY_HELPER_H */
