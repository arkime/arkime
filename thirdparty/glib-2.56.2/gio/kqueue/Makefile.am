include $(top_srcdir)/glib.mk

noinst_LTLIBRARIES += libkqueue.la

libkqueue_la_SOURCES = \
       gkqueuefilemonitor.c \
       gkqueuefilemonitor.h \
       kqueue-helper.c \
       kqueue-helper.h \
       kqueue-thread.c \
       kqueue-thread.h \
       kqueue-sub.c \
       kqueue-sub.h \
       kqueue-missing.c \
       kqueue-missing.h \
       kqueue-utils.c \
       kqueue-utils.h \
       kqueue-exclusions.c \
       kqueue-exclusions.h \
       dep-list.c \
       dep-list.h \
       $(NULL)

libkqueue_la_CFLAGS = \
       $(GLIB_HIDDEN_VISIBILITY_CFLAGS) \
       -DG_LOG_DOMAIN=\"GLib-GIO\" \
       $(gio_INCLUDES) \
       $(GLIB_DEBUG_FLAGS) \
       -DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\" \
       -DGIO_COMPILATION \
       -DG_DISABLE_DEPRECATED
