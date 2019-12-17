## Process this file with automake to produce Makefile.in
include $(top_srcdir)/glib.mk

AM_CPPFLAGS = 				\
	$(glib_INCLUDES) 		\
	-DG_LOG_DOMAIN=\"GLib\" 	\
	$(GLIB_DEBUG_FLAGS) 		\
	-DG_DISABLE_DEPRECATED 		\
	-DGLIB_COMPILATION

noinst_LTLIBRARIES += libgnulib.la

libgnulib_la_CFLAGS = $(GLIB_HIDDEN_VISIBILITY_CFLAGS)
libgnulib_la_SOURCES = 	\
	asnprintf.c \
	printf-args.c \
	printf-args.h \
	printf-parse.c \
	printf-parse.h \
	vasnprintf.c \
	vasnprintf.h \
	printf.c \
	printf.h \
	verify.h \
	xsize.h \
	g-gnulib.h
