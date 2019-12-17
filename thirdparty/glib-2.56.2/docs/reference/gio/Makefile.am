NULL =

# The name of the module.
DOC_MODULE=gio

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gio-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = \
	--deprecated-guards="G_DISABLE_DEPRECATED" \
	--ignore-decorators="GLIB_VAR|G_GNUC_INTERNAL|G_GNUC_WARN_UNUSED_RESULT|GLIB_AVAILABLE_IN_ALL|GLIB_AVAILABLE_IN_2_26|GLIB_AVAILABLE_IN_2_28|GLIB_AVAILABLE_IN_2_30|GLIB_AVAILABLE_IN_2_32|GLIB_AVAILABLE_IN_2_34|GLIB_AVAILABLE_IN_2_36|GLIB_AVAILABLE_IN_2_38|GLIB_AVAILABLE_IN_2_40|GLIB_AVAILABLE_IN_2_42|GLIB_AVAILABLE_IN_2_44|GLIB_AVAILABLE_IN_2_46|GLIB_AVAILABLE_IN_2_48|GLIB_AVAILABLE_IN_2_50|GLIB_AVAILABLE_IN_2_52|GLIB_AVAILABLE_IN_2_54|GLIB_AVAILABLE_IN_2_56|GLIB_DEPRECATED_IN_2_26|GLIB_DEPRECATED_IN_2_26_FOR|GLIB_DEPRECATED_IN_2_28|GLIB_DEPRECATED_IN_2_28_FOR|GLIB_DEPRECATED_IN_2_30|GLIB_DEPRECATED_IN_2_30_FOR|GLIB_DEPRECATED_IN_2_32|GLIB_DEPRECATED_IN_2_32_FOR|GLIB_DEPRECATED_IN_2_34|GLIB_DEPRECATED_IN_2_34_FOR|GLIB_DEPRECATED_IN_2_36|GLIB_DEPRECATED_IN_2_36_FOR|GLIB_DEPRECATED_IN_2_38|GLIB_DEPRECATED_IN_2_38_FOR|GLIB_DEPRECATED_IN_2_40|GLIB_DEPRECATED_IN_2_40_FOR|GLIB_DEPRECATED_IN_2_42|GLIB_DEPRECATED_IN_2_42_FOR|GLIB_DEPRECATED_IN_2_44|GLIB_DEPRECATED_IN_2_44_FOR|GLIB_DEPRECATED_IN_2_46|GLIB_DEPRECATED_IN_2_46_FOR|GLIB_DEPRECATED_IN_2_48|GLIB_DEPRECATED_IN_2_48_FOR|GLIB_DEPRECATED_IN_2_50|GLIB_DEPRECATED_IN_2_50_FOR|GLIB_DEPRECATED_IN_2_52|GLIB_DEPRECATED_IN_2_52_FOR|GLIB_DEPRECATED_IN_2_54|GLIB_DEPRECATED_IN_2_54_FOR|GLIB_DEPRECATED_IN_2_56|GLIB_DEPRECATED_IN_2_56_FOR"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR =$(top_srcdir)/gio $(top_builddir)/gio

HFILE_GLOB= \
    $(top_srcdir)/gio/*.h \
    $(top_builddir)/gio/gioenumtypes.h
CFILE_GLOB=$(top_srcdir)/gio/*.c

IGNORE_HFILES = \
	fam				\
	fen				\
	gdbus-2.0			\
	gvdb				\
	inotify				\
	kqueue				\
	libasyncns			\
	tests				\
	win32				\
	xdgmime				\
	gappinfoprivate.h		\
	gapplicationimpl.h		\
	gasynchelper.h			\
	gcontenttypeprivate.h		\
	gcontextspecificgroup.h		\
	gcredentialsprivate.h		\
	gdbus-daemon-generated.h	\
	gdbusactiongroup-private.h	\
	gdbusauth.h			\
	gdbusauthmechanismanon.h	\
	gdbusauthmechanismexternal.h	\
	gdbusauthmechanism.h		\
	gdbusauthmechanismsha1.h	\
	gdbusdaemon.h			\
	gdbusprivate.h			\
	gdelayedsettingsbackend.h	\
	gdummyfile.h			\
	gdummyproxyresolver.h		\
	gdummytlsbackend.h		\
	gfileattribute-priv.h		\
	gfileinfo-priv.h		\
	ghttpproxy.h			\
	gio_trace.h			\
	giomodule-priv.h		\
	gioprivate.h			\
	giowin32-priv.h			\
	glocaldirectorymonitor.h	\
	glocalfileenumerator.h		\
	glocalfile.h			\
	glocalfileinfo.h		\
	glocalfileinputstream.h		\
	glocalfileiostream.h		\
	glocalfilemonitor.h		\
	glocalfileoutputstream.h	\
	glocalvfs.h			\
	gmountprivate.h			\
	gnativevolumemonitor.h		\
	gnetworkingprivate.h		\
	gnetworkmonitorbase.h		\
	gnetworkmonitornetlink.h	\
	gnetworkmonitornm.h		\
	gnotificationbackend.h		\
	gnotification-private.h		\
	gpollfilemonitor.h		\
	gregistrysettingsbackend.h	\
	gresourcefile.h			\
	gsettingsbackendinternal.h	\
	gsettings-mapping.h		\
	gsettingsschema-internal.h	\
	gsocketinputstream.h		\
	gsocketoutputstream.h		\
	gsocks4aproxy.h			\
	gsocks4proxy.h			\
	gsocks5proxy.h			\
	gsubprocesslauncher-private.h	\
	gthreadedresolver.h		\
	gunionvolumemonitor.h		\
	gunixmount.h			\
	gunixresolver.h			\
	gunixvolume.h			\
	gunixvolumemonitor.h		\
	gwin32appinfo.h			\
	gwin32mount.h			\
	gwin32resolver.h		\
	gwin32volumemonitor.h		\
	thumbnail-verify.h

MKDB_IGNORE_FILES = \
	gdbus-daemon-generated.c	\
	kqueue				\
	libasyncns			\
	tests				\
	$(NULL)

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS = 				\
	$(gio_INCLUDES)			\
	$(GLIB_DEBUG_FLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/glib/libglib-2.0.la		\
	$(top_builddir)/gobject/libgobject-2.0.la	\
	$(top_builddir)/gmodule/libgmodule-2.0.la	\
	$(top_builddir)/gio/libgio-2.0.la		\
	$(NULL)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --output-format=xml --name-space=g \
	--ignore-files='$(MKDB_IGNORE_FILES)'

# Images to copy into HTML directory
HTML_IMAGES =			\
	gvfs-overview.png	\
	menu-example.png	\
	menu-model.png

content_files =			\
	version.xml		\
	overview.xml		\
	migrating-posix.xml	\
	migrating-gnome-vfs.xml	\
	migrating-gconf.xml	\
	migrating-gdbus.xml	\
	gio-querymodules.xml	\
	glib-compile-schemas.xml\
	glib-compile-resources.xml	\
	gapplication.xml	\
	gsettings.xml		\
	gresource.xml		\
	gdbus.xml		\
	gdbus-codegen.xml	\
	gio.xml			\
	$(NULL)

expand_content_files =		\
	overview.xml		\
	migrating-posix.xml	\
	migrating-gnome-vfs.xml	\
	migrating-gconf.xml	\
	migrating-gdbus.xml	\
	gdbus-codegen.xml	\
	$(NULL)

extra_files =			\
	version.xml.in		\
	gvfs-overview.odg

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=$(srcdir)/../glib/html --extra-dir=$(srcdir)/../gobject/html

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST +=			\
	version.xml.in

man_MANS =

if ENABLE_MAN

man_MANS +=				\
	gapplication.1			\
	gio-querymodules.1		\
	glib-compile-schemas.1		\
	glib-compile-resources.1	\
	gsettings.1			\
	gresource.1			\
	gdbus.1				\
	gdbus-codegen.1			\
	gio.1				\
	$(NULL)

XSLTPROC_FLAGS = \
        --nonet \
        --stringparam man.output.quietly 1 \
        --stringparam funcsynopsis.style ansi \
        --stringparam man.th.extra1.suppress 1 \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0

.xml.1:
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

endif

CLEANFILES ?=
CLEANFILES += $(man_MANS)

EXTRA_DIST += $(man_MANS)

dist-hook-local: all-local

gio-docs-clean: clean
	cd $(srcdir) && rm -rf xml html
