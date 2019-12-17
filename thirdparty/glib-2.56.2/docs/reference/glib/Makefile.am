## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=glib

# The top-level SGML file.
DOC_MAIN_SGML_FILE=glib-docs.xml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = \
    $(top_srcdir)/glib $(top_srcdir)/gmodule \
    $(top_builddir)/glib $(top_builddir)/gmodule

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = \
	--deprecated-guards="G_DISABLE_DEPRECATED" \
	--ignore-decorators="GLIB_VAR|G_GNUC_INTERNAL|G_GNUC_WARN_UNUSED_RESULT|GLIB_AVAILABLE_IN_ALL|GLIB_AVAILABLE_IN_2_26|GLIB_AVAILABLE_IN_2_28|GLIB_AVAILABLE_IN_2_30|GLIB_AVAILABLE_IN_2_32|GLIB_AVAILABLE_IN_2_34|GLIB_AVAILABLE_IN_2_36|GLIB_AVAILABLE_IN_2_38|GLIB_AVAILABLE_IN_2_40|GLIB_AVAILABLE_IN_2_42|GLIB_AVAILABLE_IN_2_44|GLIB_AVAILABLE_IN_2_46|GLIB_AVAILABLE_IN_2_48|GLIB_AVAILABLE_IN_2_50|GLIB_AVAILABLE_IN_2_52|GLIB_AVAILABLE_IN_2_54|GLIB_AVAILABLE_IN_2_56|GLIB_DEPRECATED_IN_2_26|GLIB_DEPRECATED_IN_2_26_FOR|GLIB_DEPRECATED_IN_2_28|GLIB_DEPRECATED_IN_2_28_FOR|GLIB_DEPRECATED_IN_2_30|GLIB_DEPRECATED_IN_2_30_FOR|GLIB_DEPRECATED_IN_2_32|GLIB_DEPRECATED_IN_2_32_FOR|GLIB_DEPRECATED_IN_2_34|GLIB_DEPRECATED_IN_2_34_FOR|GLIB_DEPRECATED_IN_2_36|GLIB_DEPRECATED_IN_2_36_FOR|GLIB_DEPRECATED_IN_2_38|GLIB_DEPRECATED_IN_2_38_FOR|GLIB_DEPRECATED_IN_2_40|GLIB_DEPRECATED_IN_2_40_FOR|GLIB_DEPRECATED_IN_2_42|GLIB_DEPRECATED_IN_2_42_FOR|GLIB_DEPRECATED_IN_2_44|GLIB_DEPRECATED_IN_2_44_FOR|GLIB_DEPRECATED_IN_2_46|GLIB_DEPRECATED_IN_2_46_FOR|GLIB_DEPRECATED_IN_2_48|GLIB_DEPRECATED_IN_2_48_FOR|GLIB_DEPRECATED_IN_2_50|GLIB_DEPRECATED_IN_2_50_FOR|GLIB_DEPRECATED_IN_2_52|GLIB_DEPRECATED_IN_2_52_FOR|GLIB_DEPRECATED_IN_2_54|GLIB_DEPRECATED_IN_2_54_FOR|GLIB_DEPRECATED_IN_2_56|GLIB_DEPRECATED_IN_2_56_FOR"

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--output-format=xml --name-space=g

# Used for dependencies
HFILE_GLOB = \
    $(top_srcdir)/glib/*.h \
    $(top_srcdir)/gmodule/*.h \
    $(top_builddir)/glib/glibconfig.h
CFILE_GLOB= $(top_srcdir)/glib/*.c $(top_srcdir)/gmodule/*.c

# Ignore some private headers
IGNORE_HFILES = \
	gallocator.h		\
	gdatasetprivate.h	\
	glibintl.h		\
	gbsearcharray.h		\
	glib-private.h		\
	gmoduleconf.h		\
	gthreadprivate.h	\
	gunibreak.h		\
	gunicomp.h		\
	gunidecomp.h		\
	gunichartables.h	\
	glib_probes.h		\
	glib_trace.h		\
	libcharset.h		\
	gdebug.h		\
	gprintfint.h		\
	gmirroringtable.h	\
	gscripttable.h		\
	glib-mirroring-tab	\
	gnulib			\
	pcre			\
	update-pcre		\
	gbytesprivate.h		\
	gvariant-internal.h	\
	gvariant-serialiser.h	\
	gvariant-core.h		\
	gvarianttypeinfo.h	\
	gwakeup.h		\
	gtranslit-data.h	\
	glib-init.h		\
	gconstructor.h		\
	valgrind.h

# Images to copy into HTML directory
HTML_IMAGES =  						\
	file-name-encodings.png				\
	mainloop-states.gif				\
	Sorted_binary_tree_breadth-first_traversal.svg 	\
	Sorted_binary_tree_inorder.svg 			\
	Sorted_binary_tree_postorder.svg 		\
	Sorted_binary_tree_preorder.svg

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =				\
	cross.xml			\
	running.xml			\
	building.xml			\
	changes.xml			\
	compiling.xml			\
	programming.xml			\
	resources.xml			\
	regex-syntax.xml		\
	version.xml			\
	glib-gettextize.xml		\
	gtester.xml			\
	gtester-report.xml		\
	gvariant-varargs.xml		\
	gvariant-text.xml

expand_content_files = 			\
	compiling.xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=$(srcdir)/../gobject/html --extra-dir=$(srcdir)/../gio/html

# include common portion ...
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST +=				\
	file-name-encodings.png		\
	file-name-encodings.sxd		\
	mainloop-states.fig		\
	mainloop-states.png		\
	mainloop-states.eps		\
	Sorted_binary_tree_breadth-first_traversal.svg 	\
	Sorted_binary_tree_inorder.svg 			\
	Sorted_binary_tree_postorder.svg 		\
	Sorted_binary_tree_preorder.svg			\
	version.xml.in

########################################################################

man_MANS =

if ENABLE_MAN

man_MANS += \
	glib-gettextize.1 	\
	gtester.1		\
	gtester-report.1

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

glib-docs-clean: clean
	cd $(srcdir) && rm -rf xml html
