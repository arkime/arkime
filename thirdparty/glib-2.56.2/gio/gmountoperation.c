/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <string.h>

#include "gmountoperation.h"
#include "gioenumtypes.h"
#include "glibintl.h"


/**
 * SECTION:gmountoperation
 * @short_description: Object used for authentication and user interaction
 * @include: gio/gio.h
 *
 * #GMountOperation provides a mechanism for interacting with the user.
 * It can be used for authenticating mountable operations, such as loop
 * mounting files, hard drive partitions or server locations. It can
 * also be used to ask the user questions or show a list of applications
 * preventing unmount or eject operations from completing.
 *
 * Note that #GMountOperation is used for more than just #GMount
 * objects – for example it is also used in g_drive_start() and
 * g_drive_stop().
 *
 * Users should instantiate a subclass of this that implements all the
 * various callbacks to show the required dialogs, such as
 * #GtkMountOperation. If no user interaction is desired (for example
 * when automounting filesystems at login time), usually %NULL can be
 * passed, see each method taking a #GMountOperation for details.
 */

enum {
  ASK_PASSWORD,
  ASK_QUESTION,
  REPLY,
  ABORTED,
  SHOW_PROCESSES,
  SHOW_UNMOUNT_PROGRESS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _GMountOperationPrivate {
  char *password;
  char *user;
  char *domain;
  gboolean anonymous;
  GPasswordSave password_save;
  int choice;
};

enum {
  PROP_0,
  PROP_USERNAME,
  PROP_PASSWORD,
  PROP_ANONYMOUS,
  PROP_DOMAIN,
  PROP_PASSWORD_SAVE,
  PROP_CHOICE
};

G_DEFINE_TYPE_WITH_PRIVATE (GMountOperation, g_mount_operation, G_TYPE_OBJECT)

static void 
g_mount_operation_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GMountOperation *operation;

  operation = G_MOUNT_OPERATION (object);

  switch (prop_id)
    {
    case PROP_USERNAME:
      g_mount_operation_set_username (operation, 
                                      g_value_get_string (value));
      break;
   
    case PROP_PASSWORD:
      g_mount_operation_set_password (operation, 
                                      g_value_get_string (value));
      break;

    case PROP_ANONYMOUS:
      g_mount_operation_set_anonymous (operation, 
                                       g_value_get_boolean (value));
      break;

    case PROP_DOMAIN:
      g_mount_operation_set_domain (operation, 
                                    g_value_get_string (value));
      break;

    case PROP_PASSWORD_SAVE:
      g_mount_operation_set_password_save (operation, 
                                           g_value_get_enum (value));
      break;

    case PROP_CHOICE:
      g_mount_operation_set_choice (operation, 
                                    g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void 
g_mount_operation_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GMountOperation *operation;
  GMountOperationPrivate *priv;

  operation = G_MOUNT_OPERATION (object);
  priv = operation->priv;
  
  switch (prop_id)
    {
    case PROP_USERNAME:
      g_value_set_string (value, priv->user);
      break;

    case PROP_PASSWORD:
      g_value_set_string (value, priv->password);
      break;

    case PROP_ANONYMOUS:
      g_value_set_boolean (value, priv->anonymous);
      break;

    case PROP_DOMAIN:
      g_value_set_string (value, priv->domain);
      break;

    case PROP_PASSWORD_SAVE:
      g_value_set_enum (value, priv->password_save);
      break;

    case PROP_CHOICE:
      g_value_set_int (value, priv->choice);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
g_mount_operation_finalize (GObject *object)
{
  GMountOperation *operation;
  GMountOperationPrivate *priv;

  operation = G_MOUNT_OPERATION (object);

  priv = operation->priv;
  
  g_free (priv->password);
  g_free (priv->user);
  g_free (priv->domain);

  G_OBJECT_CLASS (g_mount_operation_parent_class)->finalize (object);
}

static gboolean
reply_non_handled_in_idle (gpointer data)
{
  GMountOperation *op = data;

  g_mount_operation_reply (op, G_MOUNT_OPERATION_UNHANDLED);
  return G_SOURCE_REMOVE;
}

static void
ask_password (GMountOperation *op,
	      const char      *message,
	      const char      *default_user,
	      const char      *default_domain,
	      GAskPasswordFlags flags)
{
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		   reply_non_handled_in_idle,
		   g_object_ref (op),
		   g_object_unref);
}
  
static void
ask_question (GMountOperation *op,
	      const char      *message,
	      const char      *choices[])
{
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		   reply_non_handled_in_idle,
		   g_object_ref (op),
		   g_object_unref);
}

static void
show_processes (GMountOperation      *op,
                const gchar          *message,
                GArray               *processes,
                const gchar          *choices[])
{
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		   reply_non_handled_in_idle,
		   g_object_ref (op),
		   g_object_unref);
}

static void
show_unmount_progress (GMountOperation *op,
                       const gchar     *message,
                       gint64           time_left,
                       gint64           bytes_left)
{
  /* nothing to do */
}

static void
g_mount_operation_class_init (GMountOperationClass *klass)
{
  GObjectClass *object_class;
 
  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = g_mount_operation_finalize;
  object_class->get_property = g_mount_operation_get_property;
  object_class->set_property = g_mount_operation_set_property;
  
  klass->ask_password = ask_password;
  klass->ask_question = ask_question;
  klass->show_processes = show_processes;
  klass->show_unmount_progress = show_unmount_progress;
  
  /**
   * GMountOperation::ask-password:
   * @op: a #GMountOperation requesting a password.
   * @message: string containing a message to display to the user.
   * @default_user: string containing the default user name.
   * @default_domain: string containing the default domain.
   * @flags: a set of #GAskPasswordFlags.
   *
   * Emitted when a mount operation asks the user for a password.
   *
   * If the message contains a line break, the first line should be
   * presented as a heading. For example, it may be used as the
   * primary text in a #GtkMessageDialog.
   */
  signals[ASK_PASSWORD] =
    g_signal_new (I_("ask-password"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, ask_password),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 4,
		  G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ASK_PASSWORD_FLAGS);
		  
  /**
   * GMountOperation::ask-question:
   * @op: a #GMountOperation asking a question.
   * @message: string containing a message to display to the user.
   * @choices: an array of strings for each possible choice.
   *
   * Emitted when asking the user a question and gives a list of
   * choices for the user to choose from.
   *
   * If the message contains a line break, the first line should be
   * presented as a heading. For example, it may be used as the
   * primary text in a #GtkMessageDialog.
   */
  signals[ASK_QUESTION] =
    g_signal_new (I_("ask-question"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, ask_question),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING, G_TYPE_STRV);
		  
  /**
   * GMountOperation::reply:
   * @op: a #GMountOperation.
   * @result: a #GMountOperationResult indicating how the request was handled
   *
   * Emitted when the user has replied to the mount operation.
   */
  signals[REPLY] =
    g_signal_new (I_("reply"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, reply),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  G_TYPE_MOUNT_OPERATION_RESULT);

  /**
   * GMountOperation::aborted:
   *
   * Emitted by the backend when e.g. a device becomes unavailable
   * while a mount operation is in progress.
   *
   * Implementations of GMountOperation should handle this signal
   * by dismissing open password dialogs.
   *
   * Since: 2.20
   */
  signals[ABORTED] =
    g_signal_new (I_("aborted"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, aborted),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GMountOperation::show-processes:
   * @op: a #GMountOperation.
   * @message: string containing a message to display to the user.
   * @processes: (element-type GPid): an array of #GPid for processes
   *   blocking the operation.
   * @choices: an array of strings for each possible choice.
   *
   * Emitted when one or more processes are blocking an operation
   * e.g. unmounting/ejecting a #GMount or stopping a #GDrive.
   *
   * Note that this signal may be emitted several times to update the
   * list of blocking processes as processes close files. The
   * application should only respond with g_mount_operation_reply() to
   * the latest signal (setting #GMountOperation:choice to the choice
   * the user made).
   *
   * If the message contains a line break, the first line should be
   * presented as a heading. For example, it may be used as the
   * primary text in a #GtkMessageDialog.
   *
   * Since: 2.22
   */
  signals[SHOW_PROCESSES] =
    g_signal_new (I_("show-processes"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, show_processes),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 3,
		  G_TYPE_STRING, G_TYPE_ARRAY, G_TYPE_STRV);

  /**
   * GMountOperation::show-unmount-progress:
   * @op: a #GMountOperation:
   * @message: string containing a mesage to display to the user
   * @time_left: the estimated time left before the operation completes,
   *     in microseconds, or -1
   * @bytes_left: the amount of bytes to be written before the operation
   *     completes (or -1 if such amount is not known), or zero if the operation
   *     is completed
   *
   * Emitted when an unmount operation has been busy for more than some time
   * (typically 1.5 seconds).
   *
   * When unmounting or ejecting a volume, the kernel might need to flush
   * pending data in its buffers to the volume stable storage, and this operation
   * can take a considerable amount of time. This signal may be emitted several
   * times as long as the unmount operation is outstanding, and then one
   * last time when the operation is completed, with @bytes_left set to zero.
   *
   * Implementations of GMountOperation should handle this signal by
   * showing an UI notification, and then dismiss it, or show another notification
   * of completion, when @bytes_left reaches zero.
   *
   * If the message contains a line break, the first line should be
   * presented as a heading. For example, it may be used as the
   * primary text in a #GtkMessageDialog.
   *
   * Since: 2.34
   */
  signals[SHOW_UNMOUNT_PROGRESS] =
    g_signal_new (I_("show-unmount-progress"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GMountOperationClass, show_unmount_progress),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING, G_TYPE_INT64, G_TYPE_INT64);

  /**
   * GMountOperation:username:
   *
   * The user name that is used for authentication when carrying out
   * the mount operation.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_USERNAME,
                                   g_param_spec_string ("username",
                                                        P_("Username"),
                                                        P_("The user name"),
                                                        NULL,
                                                        G_PARAM_READWRITE|
                                                        G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:password:
   *
   * The password that is used for authentication when carrying out
   * the mount operation.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_PASSWORD,
                                   g_param_spec_string ("password",
                                                        P_("Password"),
                                                        P_("The password"),
                                                        NULL,
                                                        G_PARAM_READWRITE|
                                                        G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:anonymous:
   * 
   * Whether to use an anonymous user when authenticating.
   */
  g_object_class_install_property (object_class,
                                   PROP_ANONYMOUS,
                                   g_param_spec_boolean ("anonymous",
                                                         P_("Anonymous"),
                                                         P_("Whether to use an anonymous user"),
                                                         FALSE,
                                                         G_PARAM_READWRITE|
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:domain:
   *
   * The domain to use for the mount operation.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_DOMAIN,
                                   g_param_spec_string ("domain",
                                                        P_("Domain"),
                                                        P_("The domain of the mount operation"),
                                                        NULL,
                                                        G_PARAM_READWRITE|
                                                        G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:password-save:
   *
   * Determines if and how the password information should be saved. 
   */ 
  g_object_class_install_property (object_class,
                                   PROP_PASSWORD_SAVE,
                                   g_param_spec_enum ("password-save",
                                                      P_("Password save"),
                                                      P_("How passwords should be saved"),
                                                      G_TYPE_PASSWORD_SAVE,
                                                      G_PASSWORD_SAVE_NEVER,
                                                      G_PARAM_READWRITE|
                                                      G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  /**
   * GMountOperation:choice:
   *
   * The index of the user's choice when a question is asked during the 
   * mount operation. See the #GMountOperation::ask-question signal.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_CHOICE,
                                   g_param_spec_int ("choice",
                                                     P_("Choice"),
                                                     P_("The users choice"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
g_mount_operation_init (GMountOperation *operation)
{
  operation->priv = g_mount_operation_get_instance_private (operation);
}

/**
 * g_mount_operation_new:
 * 
 * Creates a new mount operation.
 * 
 * Returns: a #GMountOperation.
 **/
GMountOperation *
g_mount_operation_new (void)
{
  return g_object_new (G_TYPE_MOUNT_OPERATION, NULL);
}

/**
 * g_mount_operation_get_username:
 * @op: a #GMountOperation.
 * 
 * Get the user name from the mount operation.
 *
 * Returns: a string containing the user name.
 **/
const char *
g_mount_operation_get_username (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->user;
}

/**
 * g_mount_operation_set_username:
 * @op: a #GMountOperation.
 * @username: input username.
 *
 * Sets the user name within @op to @username.
 **/
void
g_mount_operation_set_username (GMountOperation *op,
				const char      *username)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->user);
  op->priv->user = g_strdup (username);
  g_object_notify (G_OBJECT (op), "username");
}

/**
 * g_mount_operation_get_password:
 * @op: a #GMountOperation.
 *
 * Gets a password from the mount operation. 
 *
 * Returns: a string containing the password within @op.
 **/
const char *
g_mount_operation_get_password (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->password;
}

/**
 * g_mount_operation_set_password:
 * @op: a #GMountOperation.
 * @password: password to set.
 * 
 * Sets the mount operation's password to @password.  
 *
 **/
void
g_mount_operation_set_password (GMountOperation *op,
				const char      *password)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->password);
  op->priv->password = g_strdup (password);
  g_object_notify (G_OBJECT (op), "password");
}

/**
 * g_mount_operation_get_anonymous:
 * @op: a #GMountOperation.
 * 
 * Check to see whether the mount operation is being used 
 * for an anonymous user.
 * 
 * Returns: %TRUE if mount operation is anonymous. 
 **/
gboolean
g_mount_operation_get_anonymous (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), FALSE);
  return op->priv->anonymous;
}

/**
 * g_mount_operation_set_anonymous:
 * @op: a #GMountOperation.
 * @anonymous: boolean value.
 * 
 * Sets the mount operation to use an anonymous user if @anonymous is %TRUE.
 **/  
void
g_mount_operation_set_anonymous (GMountOperation *op,
				 gboolean         anonymous)
{
  GMountOperationPrivate *priv;
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  priv = op->priv;

  if (priv->anonymous != anonymous)
    {
      priv->anonymous = anonymous;
      g_object_notify (G_OBJECT (op), "anonymous");
    }
}

/**
 * g_mount_operation_get_domain:
 * @op: a #GMountOperation.
 * 
 * Gets the domain of the mount operation.
 * 
 * Returns: a string set to the domain. 
 **/
const char *
g_mount_operation_get_domain (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->domain;
}

/**
 * g_mount_operation_set_domain:
 * @op: a #GMountOperation.
 * @domain: the domain to set.
 * 
 * Sets the mount operation's domain. 
 **/  
void
g_mount_operation_set_domain (GMountOperation *op,
			      const char      *domain)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->domain);
  op->priv->domain = g_strdup (domain);
  g_object_notify (G_OBJECT (op), "domain");
}

/**
 * g_mount_operation_get_password_save:
 * @op: a #GMountOperation.
 * 
 * Gets the state of saving passwords for the mount operation.
 *
 * Returns: a #GPasswordSave flag. 
 **/  

GPasswordSave
g_mount_operation_get_password_save (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), G_PASSWORD_SAVE_NEVER);
  return op->priv->password_save;
}

/**
 * g_mount_operation_set_password_save:
 * @op: a #GMountOperation.
 * @save: a set of #GPasswordSave flags.
 * 
 * Sets the state of saving passwords for the mount operation.
 * 
 **/   
void
g_mount_operation_set_password_save (GMountOperation *op,
				     GPasswordSave    save)
{
  GMountOperationPrivate *priv;
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  priv = op->priv;
 
  if (priv->password_save != save)
    {
      priv->password_save = save;
      g_object_notify (G_OBJECT (op), "password-save");
    }
}

/**
 * g_mount_operation_get_choice:
 * @op: a #GMountOperation.
 * 
 * Gets a choice from the mount operation.
 *
 * Returns: an integer containing an index of the user's choice from 
 * the choice's list, or %0.
 **/
int
g_mount_operation_get_choice (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), 0);
  return op->priv->choice;
}

/**
 * g_mount_operation_set_choice:
 * @op: a #GMountOperation.
 * @choice: an integer.
 *
 * Sets a default choice for the mount operation.
 **/
void
g_mount_operation_set_choice (GMountOperation *op,
			      int              choice)
{
  GMountOperationPrivate *priv;
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  priv = op->priv;
  if (priv->choice != choice)
    {
      priv->choice = choice;
      g_object_notify (G_OBJECT (op), "choice");
    }
}

/**
 * g_mount_operation_reply:
 * @op: a #GMountOperation
 * @result: a #GMountOperationResult
 * 
 * Emits the #GMountOperation::reply signal.
 **/
void
g_mount_operation_reply (GMountOperation *op,
			 GMountOperationResult result)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_signal_emit (op, signals[REPLY], 0, result);
}
