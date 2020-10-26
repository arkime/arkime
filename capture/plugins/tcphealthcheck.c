/* tcphealthcheck.c  -- TCP Health Check plugin
Listens on a TCP socket and immediately closes it to confirm that the capture service is running.
 */


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include "moloch.h"


/******************************************************************************/

extern MolochConfig_t        config;

LOCAL  int                   tcp_port;

/******************************************************************************/
void tcp_server(void) {
    int server_fd, err;
    struct sockaddr_in server, client;
   
    server.sin_family = AF_INET;
    server.sin_port = htons(tcp_port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG("Error creating socket: %d", server_fd);
        return;
    }
    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (err < 0) {
        LOG("Error binding socket: %d", err);
        return;
    }

    err = listen(server_fd, 128);
    if (err < 0) {
        LOG("Error listening on socket: %d", err);
        return;
    }

    LOG("Listening for TCP health checks on port %d", tcp_port);

    while (1) {
        socklen_t client_len = sizeof(client);
        int client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
            if (client_fd < 0) {
                LOG("Error establishing new connection: %d", client_fd);
            }
            else {
                close(client_fd);
            }
    }
}

/******************************************************************************/
void *tcp_listener(void *vargp) 
{
    (void) vargp;
    LOG("Starting TCP server");
    tcp_server();
    return NULL;
}


/******************************************************************************/
/*
 * Called by moloch when the plugin is loaded
 */
void moloch_plugin_init()
{
  	pthread_t thread_id;

    tcp_port = moloch_config_int(NULL, "tcpHealthCheckPort", 0, 0, 65535);
    if (tcp_port) {
        LOG("tcpHealthCheckPort set to %d", tcp_port);
        pthread_create(&thread_id, NULL, tcp_listener, NULL);
        LOG("TCP listener thread created"); 
        moloch_plugins_register("tcphealthcheck", FALSE);
    }
    else {
      LOG("To use TCP health checks, set tcpHealthCheckPort to a value between 1 and 65535");
    }
}
