#include "giotypes.h"

typedef struct _GApplicationImpl GApplicationImpl;

typedef struct
{
  gchar *name;

  GVariantType *parameter_type;
  gboolean      enabled;
  GVariant     *state;
} RemoteActionInfo;

void                    g_application_impl_destroy                      (GApplicationImpl   *impl);

GApplicationImpl *      g_application_impl_register                     (GApplication        *application,
                                                                         const gchar         *appid,
                                                                         GApplicationFlags    flags,
                                                                         GActionGroup        *exported_actions,
                                                                         GRemoteActionGroup **remote_actions,
                                                                         GCancellable        *cancellable,
                                                                         GError             **error);

void                    g_application_impl_activate                     (GApplicationImpl   *impl,
                                                                         GVariant           *platform_data);

void                    g_application_impl_open                         (GApplicationImpl   *impl,
                                                                         GFile             **files,
                                                                         gint                n_files,
                                                                         const gchar        *hint,
                                                                         GVariant           *platform_data);

int                     g_application_impl_command_line                 (GApplicationImpl   *impl,
                                                                         const gchar *const *arguments,
                                                                         GVariant           *platform_data);

void                    g_application_impl_flush                        (GApplicationImpl   *impl);

GDBusConnection *       g_application_impl_get_dbus_connection          (GApplicationImpl   *impl);

const gchar *           g_application_impl_get_dbus_object_path         (GApplicationImpl   *impl);

void                    g_application_impl_set_busy_state               (GApplicationImpl   *impl,
                                                                         gboolean            busy);
