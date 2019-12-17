## Process this file with automake to produce Makefile.in
include $(top_srcdir)/glib.mk

AM_CPPFLAGS = 							\
	$(gmodule_INCLUDES)					\
	-DGLIB_CHARSETALIAS_DIR=\"$(GLIB_CHARSETALIAS_DIR)\"	\
	$(config_h_INCLUDES)

noinst_LTLIBRARIES += libcharset.la

libcharset_la_CFLAGS = $(GLIB_HIDDEN_VISIBILITY_CFLAGS)
libcharset_la_SOURCES =    	\
	libcharset.h		\
	localcharset.h		\
	localcharset.c

EXTRA_DIST += 			\
	README			\
	config.charset		\
	ref-add.sin		\
	ref-del.sin		\
	glibc21.m4		\
	codeset.m4		\
	update.sh		\
	make-patch.sh		\
	libcharset-glib.patch

charset_alias = $(DESTDIR)$(GLIB_CHARSETALIAS_DIR)/charset.alias
charset_tmp = $(DESTDIR)$(GLIB_CHARSETALIAS_DIR)/charset.tmp
install-exec-local: all-local
	$(mkinstalldirs) $(DESTDIR)$(GLIB_CHARSETALIAS_DIR)
	if test -f $(charset_alias); then \
	  sed -f ref-add.sed $(charset_alias) > $(charset_tmp) ; \
	  $(INSTALL_DATA) $(charset_tmp) $(charset_alias) ; \
	  rm -f $(charset_tmp) ; \
	else \
	  if test @GLIBC21@ = no; then \
	    sed -f ref-add.sed charset.alias > $(charset_tmp) ; \
	    $(INSTALL_DATA) $(charset_tmp) $(charset_alias) ; \
	    rm -f $(charset_tmp) ; \
	  fi ; \
	fi

uninstall-local: all-local
	if test -f $(charset_alias); then \
	  sed -f ref-del.sed $(charset_alias) > $(charset_tmp); \
	  if grep '^# Packages using this file: $$' $(charset_tmp) \
	      > /dev/null; then \
	    rm -f $(charset_alias); \
	  else \
	    $(INSTALL_DATA) $(charset_tmp) $(charset_alias); \
	  fi; \
	  rm -f $(charset_tmp); \
	fi

charset.alias: config.charset
	$(AM_V_GEN) $(SHELL) $(srcdir)/config.charset '@host@' > t-$@
	@mv t-$@ $@

all-local: ref-add.sed ref-del.sed charset.alias

SUFFIXES = .sed .sin
.sin.sed:
	$(AM_V_GEN) $(SED) -e '/^#/d' -e 's/@''PACKAGE''@/@PACKAGE@/g' $< > t-$@
	@mv t-$@ $@

CLEANFILES += charset.alias ref-add.sed ref-del.sed
