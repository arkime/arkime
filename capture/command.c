/******************************************************************************/
/* command.c
 *
 * Copyright 2024 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gio/gio.h"
#include "glib-object.h"
#include "arkime.h"
extern ArkimeConfig_t        config;

typedef struct {
    char                    *name;
    ArkimeCommandFunc        func;
    char                    *help;
} Command_t;

LOCAL int         maxCommandLen = 0;
LOCAL Command_t  *commandsArray[1000];
LOCAL int         commandArrayLen;
LOCAL gboolean    commandsArraySorted = FALSE;
LOCAL GHashTable *commandsHash;

typedef struct {
    GSocket                *socket;
    char                    data[1024];
    uint32_t                len;
    int                     readWatch;
} CommandClient_t;


/******************************************************************************/
LOCAL void arkime_command_client_free(CommandClient_t *cc)
{
    g_source_remove(cc->readWatch);
    g_object_unref (cc->socket);

    ARKIME_TYPE_FREE(CommandClient_t, cc);
}
/******************************************************************************/
LOCAL void arkime_command_run(char *line, gpointer cc)
{
    gint    argcp;
    gchar **argvp = NULL;
    GError *error = NULL;

    if (!g_shell_parse_argv(line, &argcp, &argvp, &error)) {
        arkime_command_respond(cc, "No command sent\n", -1);
        return;
    }

    Command_t *cmd = g_hash_table_lookup(commandsHash, argvp[0]);
    if (cmd) {
        cmd->func(argcp, argvp, cc);
    } else {
        char error[1024];
        snprintf(error, sizeof(error), "Unknown command %s\n", argvp[0]);
        arkime_command_respond(cc, error, -1);
    }
    g_strfreev(argvp);
}
/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL gboolean arkime_command_data_read_cb(gint UNUSED(fd), GIOCondition cond, gpointer data)
{
    CommandClient_t *cc = (CommandClient_t *)data;
    // LOG("fd: %d cond: %x data: %p", fd, cond, data);

    GError              *error = 0;

    int len = g_socket_receive(cc->socket, cc->data + cc->len, sizeof(cc->data) - cc->len, NULL, &error);

    if (error || cond & (G_IO_HUP | G_IO_ERR) || len <= 0) {
        if (error) {
            LOG("ERROR: Receive Error: %s", error->message);
            g_error_free(error);
        }
        arkime_command_client_free(cc);
        return FALSE;
    }
    cc->len += len;

    while (1) {
        char *pos = memchr(cc->data, '\n', cc->len);
        if (!pos)
            break;
        *pos = 0;
        arkime_command_run(cc->data, cc);
        memmove(cc->data, pos + 1, cc->len - (pos - cc->data + 1));
        cc->len -= pos - cc->data + 1;
    }

    return TRUE;
}
/******************************************************************************/
LOCAL gboolean arkime_command_server_read_cb(gint UNUSED(fd), GIOCondition UNUSED(cond), gpointer data)
{
    GError                   *error = NULL;

    // LOG("fd: %d cond: %x data: %p", fd, cond, data);

    GSocket *client = g_socket_accept((GSocket *)data, NULL, &error);
    if (!client || error) {
        LOGEXIT("ERROR - Error accepting command: %s", error->message);
    }

    CommandClient_t *cc = ARKIME_TYPE_ALLOC0(CommandClient_t);
    cc->socket = client;

    int cfd = g_socket_get_fd(client);
    cc->readWatch = arkime_watch_fd(cfd, ARKIME_GIO_READ_COND, arkime_command_data_read_cb, cc);
    return TRUE;
}
/******************************************************************************/
LOCAL void arkime_command_free(gpointer data)
{
    Command_t *cmd = (Command_t *)data;

    g_free(cmd->name);
    g_free(cmd->help);
    ARKIME_TYPE_FREE(Command_t, cmd);
}
/******************************************************************************/
void arkime_command_register(char *name, ArkimeCommandFunc func, char *help)
{
    if (!config.command) {
        return;
    }

    if (!commandsHash) {
        commandsHash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, arkime_command_free);
    }

    Command_t *cmd = ARKIME_TYPE_ALLOC0(Command_t);
    cmd->name = g_strdup(name);
    cmd->func = func;
    cmd->help = g_strdup(help);
    g_hash_table_insert(commandsHash, cmd->name, cmd);
    maxCommandLen = MIN(60, MAX(maxCommandLen, (int)strlen(name)));
    commandsArray[commandArrayLen++] = cmd;
    commandsArraySorted = FALSE;
}
/******************************************************************************/
void arkime_command_respond(gpointer cc, char *data, int len)
{
    CommandClient_t *client = (CommandClient_t *)cc;
    GError *error = NULL;

    if (len == -1) {
        len = strlen(data);
    }

    if (g_socket_send(client->socket, data, len, NULL, &error) != len) {
        LOG("ERROR - Sending response: %s", error->message);
    }
}
/******************************************************************************/
LOCAL int arkime_command_cmp(const void *a, const void *b)
{
    return strcmp((*(Command_t **)a)->name, (*(Command_t **)b)->name);
}
/******************************************************************************/
void arkime_command_help(int UNUSED(argc), char UNUSED(**argv), gpointer cc)
{
    static const char indent[] = "                                                             ";
    char help[10000];
    BSB bsb;
    BSB_INIT(bsb, help, sizeof(help));

    if (!commandsArraySorted) {
        qsort(commandsArray, commandArrayLen, sizeof(Command_t *), arkime_command_cmp);
        commandsArraySorted = TRUE;
    }

    for (int i = 0; i < commandArrayLen; i++) {
        Command_t *cmd = commandsArray[i];
        int len = maxCommandLen - MIN(60, strlen(cmd->name));
        BSB_EXPORT_sprintf(bsb, "%s %.*s - %s\n", cmd->name, len, indent, cmd->help);
    }

    arkime_command_respond(cc, help, BSB_LENGTH(bsb));
}
/******************************************************************************/
void arkime_command_exit(int UNUSED(argc), char UNUSED(**argv), gpointer cc)
{
    arkime_command_client_free(cc);
}
/******************************************************************************/
void arkime_command_init()
{
    if (!config.command) {
        return;
    }

    GError                   *error = NULL;
    GSocket                  *socket;
    GSocketAddress           *addr;

    socket = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, 0, &error);

    if (!socket || error) {
        CONFIGEXIT("Error creating command: %s", error->message);
    }

    unlink(config.command);
    addr = g_unix_socket_address_new (config.command);

    if (!g_socket_bind (socket, addr, TRUE, &error)) {
        CONFIGEXIT("Error binding command socket: %s", error->message);
    }
    g_object_unref (addr);

    if (!g_socket_listen (socket, &error)) {
        CONFIGEXIT("Error listening command socket: %s", error->message);
    }

    int fd = g_socket_get_fd(socket);

    arkime_watch_fd(fd, ARKIME_GIO_READ_COND, arkime_command_server_read_cb, socket);
    arkime_command_register("help", arkime_command_help, "This help");
    arkime_command_register("exit", arkime_command_exit, "Close the connection, can also use Ctrl-D");
}
/******************************************************************************/
