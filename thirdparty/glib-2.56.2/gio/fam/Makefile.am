include $(top_srcdir)/glib.mk

module_flags = -export_dynamic -avoid-version -module -no-undefined -export-symbols-regex '^g_io_module_(load|unload|query)'

giomodule_LTLIBRARIES = libgiofam.la
giomoduledir = $(GIO_MODULE_DIR)

libgiofam_la_SOURCES = gfamfilemonitor.c

libgiofam_la_CFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-GIO\"	\
	$(gio_INCLUDES) 		\
	$(GLIB_DEBUG_FLAGS)		\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	\
	-DGIO_COMPILATION		\
	-DG_DISABLE_DEPRECATED

libgiofam_la_LDFLAGS = $(module_flags)
libgiofam_la_LIBADD = \
		$(top_builddir)/gio/libgio-2.0.la \
		$(top_builddir)/gobject/libgobject-2.0.la \
		$(top_builddir)/glib/libglib-2.0.la \
		$(GLIB_LIBS) \
		$(FAM_LIBS) \
		$(NULL)

if CROSS_COMPILING
RUN_QUERY_MODULES=false
else
RUN_QUERY_MODULES=true
endif

install-data-hook:
	if $(RUN_QUERY_MODULES) && test -z "$(DESTDIR)" ; then					\
		$(top_builddir)/gio/gio-querymodules$(EXEEXT) $(DESTDIR)$(GIO_MODULE_DIR) ;	\
	fi

uninstall-local:
	$(RM) $(DESTDIR)$(GIO_MODULE_DIR)/giomodule.cache
