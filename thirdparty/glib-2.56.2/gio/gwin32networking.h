/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2015 Chun-wei Fan
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

#ifndef __G_WIN32_NETWORKING_H__
#define __G_WIN32_NETWORKING_H__

G_BEGIN_DECLS

/* Check if more ANSI-compliant Winsock2 functions are provided */
/* For run-time compatibility with Windows XP, remove when XP support dropped */

typedef INT (WSAAPI *PFN_InetPton) (INT, PCTSTR, PVOID);
typedef PCTSTR (WSAAPI *PFN_InetNtop) (INT, PVOID, PTSTR, size_t);
typedef NET_IFINDEX (WINAPI *PFN_IfNameToIndex) (PCSTR);

typedef struct _GWin32WinsockFuncs
{
  PFN_InetPton pInetPton;
  PFN_InetNtop pInetNtop;
  PFN_IfNameToIndex pIfNameToIndex;
} GWin32WinsockFuncs;

extern GWin32WinsockFuncs ws2funcs;

G_END_DECLS /* __G_WIN32_NETWORKING_H__ */

#endif
