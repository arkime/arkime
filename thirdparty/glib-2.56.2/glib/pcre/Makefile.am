include $(top_srcdir)/glib.mk

noinst_LTLIBRARIES += libpcre.la

libpcre_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-GRegex\" \
	-DHAVE_MEMMOVE \
	-DSUPPORT_UCP \
	-DSUPPORT_UTF \
	-DSUPPORT_UTF8 \
	-DNEWLINE=-1 \
	-DMATCH_LIMIT=10000000 \
	-DMATCH_LIMIT_RECURSION=8192 \
	-DMAX_NAME_SIZE=32 \
	-DMAX_NAME_COUNT=10000 \
	-DMAX_DUPLENGTH=30000 \
	-DLINK_SIZE=2 \
	-DPOSIX_MALLOC_THRESHOLD=10 \
	-DPCRE_STATIC \
	-UBSR_ANYCRLF \
	-UEBCDIC \
	$(glib_INCLUDES) \
	@GLIB_DEBUG_FLAGS@ \
	-DGLIB_COMPILATION \
	$(AM_CPPFLAGS)

libpcre_la_CFLAGS = \
	$(GLIB_HIDDEN_VISIBILITY_CFLAGS) \
	$(PCRE_WARN_CFLAGS) \
	$(DEP_CFLAGS) \
	$(AM_CFLAGS)

libpcre_la_LDFLAGS = \
	-no-undefined \
	$(AM_LDFLAGS)

libpcre_la_SOURCES = \
	pcre_byte_order.c \
	pcre_chartables.c \
	pcre_compile.c \
	pcre_config.c \
	pcre_dfa_exec.c \
	pcre_exec.c \
	pcre_fullinfo.c \
	pcre_get.c \
	pcre_globals.c \
	pcre_jit_compile.c \
	pcre_newline.c \
	pcre_ord2utf8.c \
	pcre_string_utils.c \
	pcre_study.c \
	pcre_tables.c \
	pcre_valid_utf8.c \
	pcre_version.c \
	pcre_xclass.c \
	pcre.h \
	pcre_internal.h \
	ucp.h

libpcre_la_LIBADD = $(DEP_LIBS)

EXTRA_DIST += COPYING
