include $(top_srcdir)/glib.mk

AM_CPPFLAGS = -DXDG_PREFIX=_gio_xdg

noinst_LTLIBRARIES += libxdgmime.la

libxdgmime_la_CFLAGS = $(GLIB_HIDDEN_VISIBILITY_CFLAGS)
libxdgmime_la_SOURCES = \
	xdgmime.c 	\
	xdgmime.h	\
	xdgmimealias.c	\
	xdgmimealias.h	\
	xdgmimecache.c	\
	xdgmimecache.h	\
	xdgmimeglob.c 	\
	xdgmimeglob.h 	\
	xdgmimeicon.c	\
	xdgmimeicon.h	\
	xdgmimeint.c 	\
	xdgmimeint.h 	\
	xdgmimemagic.c  \
	xdgmimemagic.h	\
	xdgmimeparent.c	\
	xdgmimeparent.h
