## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gobject

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gobject-docs.xml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR =$(top_srcdir)/gobject $(top_builddir)/gobject

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = \
	--deprecated-guards="G_DISABLE_DEPRECATED" \
	--ignore-decorators="GLIB_VAR|G_GNUC_INTERNAL|G_GNUC_WARN_UNUSED_RESULT|GLIB_AVAILABLE_IN_ALL|GLIB_AVAILABLE_IN_2_26|GLIB_AVAILABLE_IN_2_28|GLIB_AVAILABLE_IN_2_30|GLIB_AVAILABLE_IN_2_32|GLIB_AVAILABLE_IN_2_34|GLIB_AVAILABLE_IN_2_36|GLIB_AVAILABLE_IN_2_38|GLIB_AVAILABLE_IN_2_40|GLIB_AVAILABLE_IN_2_42|GLIB_AVAILABLE_IN_2_44|GLIB_AVAILABLE_IN_2_46|GLIB_AVAILABLE_IN_2_48|GLIB_AVAILABLE_IN_2_50|GLIB_AVAILABLE_IN_2_52|GLIB_AVAILABLE_IN_2_54|GLIB_AVAILABLE_IN_2_56|GLIB_DEPRECATED_IN_2_26|GLIB_DEPRECATED_IN_2_26_FOR|GLIB_DEPRECATED_IN_2_28|GLIB_DEPRECATED_IN_2_28_FOR|GLIB_DEPRECATED_IN_2_30|GLIB_DEPRECATED_IN_2_30_FOR|GLIB_DEPRECATED_IN_2_32|GLIB_DEPRECATED_IN_2_32_FOR|GLIB_DEPRECATED_IN_2_34|GLIB_DEPRECATED_IN_2_34_FOR|GLIB_DEPRECATED_IN_2_36|GLIB_DEPRECATED_IN_2_36_FOR|GLIB_DEPRECATED_IN_2_38|GLIB_DEPRECATED_IN_2_38_FOR|GLIB_DEPRECATED_IN_2_40|GLIB_DEPRECATED_IN_2_40_FOR|GLIB_DEPRECATED_IN_2_42|GLIB_DEPRECATED_IN_2_42_FOR|GLIB_DEPRECATED_IN_2_44|GLIB_DEPRECATED_IN_2_44_FOR|GLIB_DEPRECATED_IN_2_46|GLIB_DEPRECATED_IN_2_46_FOR|GLIB_DEPRECATED_IN_2_48|GLIB_DEPRECATED_IN_2_48_FOR|GLIB_DEPRECATED_IN_2_50|GLIB_DEPRECATED_IN_2_50_FOR|GLIB_DEPRECATED_IN_2_52|GLIB_DEPRECATED_IN_2_52_FOR|GLIB_DEPRECATED_IN_2_54|GLIB_DEPRECATED_IN_2_54_FOR|GLIB_DEPRECATED_IN_2_56|GLIB_DEPRECATED_IN_2_56_FOR"

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--output-format=xml --name-space=g

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gobject/*.h
CFILE_GLOB=$(top_srcdir)/gobject/*.c

# Headers to ignore
IGNORE_HFILES = \
	tests			\
	gatomicarray.h		\
	gobject_trace.h		\
	gtype-private.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS = 				\
	-I$(srcdir) 			\
	$(gobject_INCLUDES) 		\
	$(GLIB_DEBUG_FLAGS)

GTKDOC_LIBS = 					\
	$(top_builddir)/glib/libglib-2.0.la	\
	$(top_builddir)/gobject/libgobject-2.0.la

# Images to copy into HTML directory
HTML_IMAGES =  \
	images/glue.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = version.xml 		\
		glib-mkenums.xml 	\
		glib-genmarshal.xml	\
		gobject-query.xml \
		tut_gobject.xml \
		tut_gsignal.xml \
		tut_gtype.xml \
		tut_howto.xml \
		tut_intro.xml \
		tut_tools.xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=$(srcdir)/../glib/html

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += 		\
	gobject.cI	\
	version.xml.in

########################################################################

man_MANS =

if ENABLE_MAN

man_MANS += 			\
	glib-mkenums.1 		\
	glib-genmarshal.1 	\
	gobject-query.1


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

gobject-docs-clean: clean
	cd $(srcdir) && rm -rf xml html
