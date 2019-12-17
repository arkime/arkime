#include <gio/gio.h>

#define G_TYPE_DBUS_DAEMON (_g_dbus_daemon_get_type ())
#define G_DBUS_DAEMON(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_DAEMON, GDBusDaemon))
#define G_DBUS_DAEMON_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_DBUS_DAEMON, GDBusDaemonClass))
#define G_DBUS_DAEMON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_DAEMON, GDBusDaemonClass))
#define G_IS_DBUS_DAEMON(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_DAEMON))
#define G_IS_DBUS_DAEMON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_DAEMON))

typedef struct _GDBusDaemon GDBusDaemon;
typedef struct _GDBusDaemonClass GDBusDaemonClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GDBusDaemon, g_object_unref)

GType _g_dbus_daemon_get_type (void) G_GNUC_CONST;

GDBusDaemon *_g_dbus_daemon_new (const char *address,
				 GCancellable *cancellable,
				 GError **error);

const char *_g_dbus_daemon_get_address (GDBusDaemon *daemon);
