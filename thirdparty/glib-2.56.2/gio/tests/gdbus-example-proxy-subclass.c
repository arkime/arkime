
#include <gio/gio.h>

/* ---------------------------------------------------------------------------------------------------- */
/* The D-Bus interface definition we want to create a GDBusProxy-derived type for: */
/* ---------------------------------------------------------------------------------------------------- */

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.Accounts.User'>"
  "    <method name='Frobnicate'>"
  "      <arg name='flux' type='s' direction='in'/>"
  "      <arg name='baz' type='s' direction='in'/>"
  "      <arg name='result' type='s' direction='out'/>"
  "    </method>"
  "    <signal name='Changed'/>"
  "    <property name='AutomaticLogin' type='b' access='readwrite'/>"
  "    <property name='RealName' type='s' access='read'/>"
  "    <property name='UserName' type='s' access='read'/>"
  "  </interface>"
  "</node>";

/* ---------------------------------------------------------------------------------------------------- */
/* Definition of the AccountsUser type */
/* ---------------------------------------------------------------------------------------------------- */

#define ACCOUNTS_TYPE_USER         (accounts_user_get_type ())
#define ACCOUNTS_USER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ACCOUNTS_TYPE_USER, AccountsUser))
#define ACCOUNTS_USER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), ACCOUNTS_TYPE_USER, AccountsUserClass))
#define ACCOUNTS_USER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ACCOUNTS_TYPE_USER, AccountsUserClass))
#define ACCOUNTS_IS_USER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ACCOUNTS_TYPE_USER))
#define ACCOUNTS_IS_USER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ACCOUNTS_TYPE_USER))

typedef struct _AccountsUser        AccountsUser;
typedef struct _AccountsUserClass   AccountsUserClass;
typedef struct _AccountsUserPrivate AccountsUserPrivate;

struct _AccountsUser
{
  /*< private >*/
  GDBusProxy parent_instance;
  AccountsUserPrivate *priv;
};

struct _AccountsUserClass
{
  /*< private >*/
  GDBusProxyClass parent_class;
  void (*changed) (AccountsUser *user);
};

GType        accounts_user_get_type            (void) G_GNUC_CONST;

const gchar *accounts_user_get_user_name       (AccountsUser        *user);
const gchar *accounts_user_get_real_name       (AccountsUser        *user);
gboolean     accounts_user_get_automatic_login (AccountsUser        *user);

void         accounts_user_frobnicate          (AccountsUser        *user,
                                                const gchar         *flux,
                                                gint                 baz,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data);
gchar       *accounts_user_frobnicate_finish   (AccountsUser        *user,
                                                GAsyncResult        *res,
                                                GError             **error);
gchar       *accounts_user_frobnicate_sync     (AccountsUser        *user,
                                                const gchar         *flux,
                                                gint                 baz,
                                                GCancellable        *cancellable,
                                                GError             **error);

/* ---------------------------------------------------------------------------------------------------- */
/* Implementation of the AccountsUser type */
/* ---------------------------------------------------------------------------------------------------- */

/* A more efficient approach than parsing XML is to use const static
 * GDBusInterfaceInfo, GDBusMethodInfo, ... structures
 */
static GDBusInterfaceInfo *
accounts_user_get_interface_info (void)
{
  static gsize has_info = 0;
  static GDBusInterfaceInfo *info = NULL;
  if (g_once_init_enter (&has_info))
    {
      GDBusNodeInfo *introspection_data;
      introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
      info = introspection_data->interfaces[0];
      g_once_init_leave (&has_info, 1);
    }
  return info;
}

enum
{
  PROP_0,
  PROP_USER_NAME,
  PROP_REAL_NAME,
  PROP_AUTOMATIC_LOGIN,
};

enum
{
  CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (AccountsUser, accounts_user, G_TYPE_DBUS_PROXY)

static void
accounts_user_finalize (GObject *object)
{
  G_GNUC_UNUSED AccountsUser *user = ACCOUNTS_USER (object);

  if (G_OBJECT_CLASS (accounts_user_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (accounts_user_parent_class)->finalize (object);
}

static void
accounts_user_init (AccountsUser *user)
{
  /* Sets the expected interface */
  g_dbus_proxy_set_interface_info (G_DBUS_PROXY (user), accounts_user_get_interface_info ());
}

static void
accounts_user_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  AccountsUser *user = ACCOUNTS_USER (object);

  switch (prop_id)
    {
    case PROP_USER_NAME:
      g_value_set_string (value, accounts_user_get_user_name (user));
      break;

    case PROP_REAL_NAME:
      g_value_set_string (value, accounts_user_get_real_name (user));
      break;

    case PROP_AUTOMATIC_LOGIN:
      g_value_set_boolean (value, accounts_user_get_automatic_login (user));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

const gchar *
accounts_user_get_user_name (AccountsUser *user)
{
  GVariant *value;
  const gchar *ret;
  g_return_val_if_fail (ACCOUNTS_IS_USER (user), NULL);
  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (user), "UserName");
  ret = g_variant_get_string (value, NULL);
  g_variant_unref (value);
  return ret;
}

const gchar *
accounts_user_get_real_name (AccountsUser *user)
{
  GVariant *value;
  const gchar *ret;
  g_return_val_if_fail (ACCOUNTS_IS_USER (user), NULL);
  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (user), "RealName");
  ret = g_variant_get_string (value, NULL);
  g_variant_unref (value);
  return ret;
}

gboolean
accounts_user_get_automatic_login (AccountsUser *user)
{
  GVariant *value;
  gboolean ret;
  g_return_val_if_fail (ACCOUNTS_IS_USER (user), FALSE);
  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (user), "AutomaticLogin");
  ret = g_variant_get_boolean (value);
  g_variant_unref (value);
  return ret;
}

static void
accounts_user_g_signal (GDBusProxy   *proxy,
                        const gchar  *sender_name,
                        const gchar  *signal_name,
                        GVariant     *parameters)
{
  AccountsUser *user = ACCOUNTS_USER (proxy);
  if (g_strcmp0 (signal_name, "Changed") == 0)
    g_signal_emit (user, signals[CHANGED_SIGNAL], 0);
}

static void
accounts_user_g_properties_changed (GDBusProxy          *proxy,
                                    GVariant            *changed_properties,
                                    const gchar* const  *invalidated_properties)
{
  AccountsUser *user = ACCOUNTS_USER (proxy);
  GVariantIter *iter;
  const gchar *key;

  if (changed_properties != NULL)
    {
      g_variant_get (changed_properties, "a{sv}", &iter);
      while (g_variant_iter_next (iter, "{&sv}", &key, NULL))
        {
          if (g_strcmp0 (key, "AutomaticLogin") == 0)
            g_object_notify (G_OBJECT (user), "automatic-login");
          else if (g_strcmp0 (key, "RealName") == 0)
            g_object_notify (G_OBJECT (user), "real-name");
          else if (g_strcmp0 (key, "UserName") == 0)
            g_object_notify (G_OBJECT (user), "user-name");
        }
      g_variant_iter_free (iter);
    }
}

static void
accounts_user_class_init (AccountsUserClass *klass)
{
  GObjectClass *gobject_class;
  GDBusProxyClass *proxy_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = accounts_user_get_property;
  gobject_class->finalize = accounts_user_finalize;

  proxy_class = G_DBUS_PROXY_CLASS (klass);
  proxy_class->g_signal             = accounts_user_g_signal;
  proxy_class->g_properties_changed = accounts_user_g_properties_changed;

  g_object_class_install_property (gobject_class,
                                   PROP_USER_NAME,
                                   g_param_spec_string ("user-name",
                                                        "User Name",
                                                        "The user name of the user",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_REAL_NAME,
                                   g_param_spec_string ("real-name",
                                                        "Real Name",
                                                        "The real name of the user",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_AUTOMATIC_LOGIN,
                                   g_param_spec_boolean ("automatic-login",
                                                         "Automatic Login",
                                                         "Whether the user is automatically logged in",
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  signals[CHANGED_SIGNAL] = g_signal_new ("changed",
                                          ACCOUNTS_TYPE_USER,
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (AccountsUserClass, changed),
                                          NULL,
                                          NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE,
                                          0);
}

gchar *
accounts_user_frobnicate_sync (AccountsUser        *user,
                               const gchar         *flux,
                               gint                 baz,
                               GCancellable        *cancellable,
                               GError             **error)
{
  gchar *ret;
  GVariant *value;

  g_return_val_if_fail (ACCOUNTS_IS_USER (user), NULL);

  ret = NULL;

  value = g_dbus_proxy_call_sync (G_DBUS_PROXY (user),
                                  "Frobnicate",
                                  g_variant_new ("(si)",
                                                 flux,
                                                 baz),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  cancellable,
                                  error);
  if (value != NULL)
    {
      g_variant_get (value, "(s)", &ret);
      g_variant_unref (value);
    }
  return ret;
}

void
accounts_user_frobnicate (AccountsUser        *user,
                          const gchar         *flux,
                          gint                 baz,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_return_if_fail (ACCOUNTS_IS_USER (user));
  g_dbus_proxy_call (G_DBUS_PROXY (user),
                     "Frobnicate",
                     g_variant_new ("(si)",
                                    flux,
                                    baz),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cancellable,
                     callback,
                     user_data);
}


gchar *
accounts_user_frobnicate_finish (AccountsUser        *user,
                                 GAsyncResult        *res,
                                 GError             **error)
{
  gchar *ret;
  GVariant *value;

  ret = NULL;
  value = g_dbus_proxy_call_finish (G_DBUS_PROXY (user), res, error);
  if (value != NULL)
    {
      g_variant_get (value, "(s)", &ret);
      g_variant_unref (value);
    }
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

gint
main (gint argc, gchar *argv[])
{
  return 0;
}
