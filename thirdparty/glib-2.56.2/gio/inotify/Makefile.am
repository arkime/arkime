include $(top_srcdir)/glib.mk

noinst_LTLIBRARIES += libinotify.la

libinotify_la_SOURCES = 		\
	inotify-kernel.c 		\
	inotify-sub.c 			\
	inotify-path.c 			\
	inotify-missing.c 		\
	inotify-helper.c 		\
	inotify-kernel.h 		\
	inotify-missing.h		\
	inotify-path.h			\
	inotify-sub.h			\
	inotify-helper.h		\
	ginotifyfilemonitor.c		\
	ginotifyfilemonitor.h		\
	$(NULL)

libinotify_la_CFLAGS = \
	$(GLIB_HIDDEN_VISIBILITY_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GLib-GIO\"	\
	$(gio_INCLUDES) 		\
	$(GLIB_DEBUG_FLAGS)		\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	\
	-DGIO_COMPILATION		\
	-DG_DISABLE_DEPRECATED

