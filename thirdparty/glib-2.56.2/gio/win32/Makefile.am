include $(top_srcdir)/glib.mk

noinst_LTLIBRARIES += libgiowin32.la

libgiowin32_la_SOURCES =			\
	gwin32fsmonitorutils.c			\
	gwin32fsmonitorutils.h			\
	gwin32filemonitor.c			\
	gwin32filemonitor.h			\
	gwinhttpvfs.c				\
	gwinhttpvfs.h				\
	gwinhttpfile.c				\
	gwinhttpfile.h				\
	gwinhttpfileinputstream.c		\
	gwinhttpfileinputstream.h		\
	gwinhttpfileoutputstream.c		\
	gwinhttpfileoutputstream.h		\
	winhttp.h				\
	$(NULL)

libgiowin32_la_CFLAGS = \
	$(GLIB_HIDDEN_VISIBILITY_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GLib-GIO\"	\
	$(gio_INCLUDES)			\
	$(GLIB_DEBUG_FLAGS)		\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	\
	-DGIO_COMPILATION		\
	-DG_DISABLE_DEPRECATED
