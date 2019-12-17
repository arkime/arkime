/* GLIB - Library of useful routines for C programming
 * 
 * Copyright (C) 2009,2010 Red Hat, Inc.
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */
 
#ifndef __GOBJECTTRACE_H__
#define __GOBJECTTRACE_H__

#ifndef SIZEOF_CHAR
#error "config.h must be included prior to gobject_trace.h"
#endif

#ifdef HAVE_DTRACE

/* include the generated probes header and put markers in code */
#include "gobject_probes.h"
#define TRACE(probe) probe

#else

/* Wrap the probe to allow it to be removed when no systemtap available */
#define TRACE(probe)

#endif

#endif /* __GOBJECTTRACE_H__ */
