/******************************************************************************/
/* cloud.c
 *
 * Common cloud code
 *
 * Copyright 2024 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <curl/curl.h>
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL  void                 *metadataServer = 0;

LOCAL  char                  awsUseTokenForMetadata;

/******************************************************************************/
LOCAL uint8_t *aws_get_instance_metadata(const char *key, int key_len, size_t *mlen)
{
    char *requestHeaders[2];
    char  tokenHeader[200];
    requestHeaders[1] = NULL;
    if (awsUseTokenForMetadata) {
        char *tokenRequestHeaders[2] = {"X-aws-ec2-metadata-token-ttl-seconds: 30", NULL};
        if (config.debug)
            LOG("Requesting IMDSv2 metadata token");
        const uint8_t *token = arkime_http_send_sync(metadataServer, "PUT", "/latest/api/token", -1, NULL, 0, tokenRequestHeaders, mlen, NULL);
        if (config.debug)
            LOG("IMDSv2 metadata token received");
        if (token) {
            snprintf(tokenHeader, sizeof(tokenHeader), "X-aws-ec2-metadata-token: %s", token);
            requestHeaders[0] = tokenHeader;
        } else {
            LOG("WARNING - Failed to get IMDSv2 metadata token");
            requestHeaders[0] = NULL;
        }
    } else {
        if (config.debug)
            LOG("Using IMDSv1");
        requestHeaders[0] = NULL;
    }
    return arkime_http_send_sync(metadataServer, "GET", key, key_len, NULL, 0, requestHeaders, mlen, NULL);
}
/******************************************************************************/
static char credURL[1024];
LOCAL gboolean aws_refresh_creds(gpointer UNUSED(user_data))
{
    size_t clen;

    const uint8_t *credentials = aws_get_instance_metadata(credURL, -1, &clen);

    if (credentials && clen) {
        // Now need to extract access key, secret key and token
        const char *id = arkime_js0n_get_str(credentials, clen, "AccessKeyId");
        const char *key = arkime_js0n_get_str(credentials, clen, "SecretAccessKey");
        const char *token = arkime_js0n_get_str(credentials, clen, "Token");
        if (config.debug)
            LOG("Found AccessKeyId %s", id);

        if (id && key) {
            arkime_credentials_set(id, key, token);
        }
    }

    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL int aws_get_credentials_metadata(const char UNUSED(*service))
{
    if (!metadataServer) {
        const char *uri = getenv("ECS_CONTAINER_METADATA_URI_V4");
        const char *relativeURI = getenv("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI");
        if (uri && relativeURI) {
            char *uriDup = g_strdup(uri);
            char *slash = strchr(uriDup + 8, '/');
            if (slash) {
                *slash = 0;
            }
            g_strlcpy(credURL, relativeURI, sizeof(credURL));
            metadataServer = arkime_http_create_server(uriDup, 2, 2, FALSE);
            g_free(uriDup);
        } else {
            size_t rlen;

            metadataServer = arkime_http_create_server("http://169.254.169.254", 2, 2, FALSE);
            uint8_t *rolename = aws_get_instance_metadata("/latest/meta-data/iam/security-credentials/", -1, &rlen);
            if (!rolename || !rlen || rolename[0] == '<') {
                if (config.debug)
                    LOG("ERROR - Cannot retrieve role name from metadata service\n");
                return 0;
            }

            snprintf(credURL, sizeof(credURL), "/latest/meta-data/iam/security-credentials/%.*s", (int)rlen, rolename);
            free(rolename);
        }
        arkime_http_set_print_errors(metadataServer);
        g_timeout_add_seconds(280, aws_refresh_creds, 0);
    }
    aws_refresh_creds(NULL);
    return 1;
}
/******************************************************************************/
LOCAL int aws_get_credentials_env(const char UNUSED(*service))
{
    const char *id = getenv("AWS_ACCESS_KEY_ID");;
    const char *key = getenv("AWS_SECRET_ACCESS_KEY");
    const char *token = getenv("AWS_SESSION_TOKEN");

    if (id && key) {
        arkime_credentials_set(id, key, token);
        return 1;
    }
    return 0;
}
/******************************************************************************/
LOCAL int aws_get_credentials_file(const char UNUSED(*service), const char *envName, char *fileName, gboolean leadingProfile)
{
    const char *credFileEnv = getenv(envName);

    char *credFilename;
    if (credFileEnv)
        credFilename = g_strdup(credFileEnv);
    else
        credFilename = g_build_filename(g_get_home_dir(), ".aws", fileName, NULL);

    if (!g_file_test(credFilename, G_FILE_TEST_EXISTS)) {
        g_free(credFilename);
        return 0;
    }

    GKeyFile *keyFile = g_key_file_new();

    // Load the credentials file
    GError *error = NULL;
    if (!g_key_file_load_from_file(keyFile, credFilename, G_KEY_FILE_NONE, &error)) {
        g_free(credFilename);
        g_printerr("Error loading credentials file: %s\n", error->message);
        g_error_free(error);
        return 0;
    }
    g_free(credFilename);

    int good = 0;

    // Get the credentials
    if (config.profile) {
        char profileSection[200];
        if (leadingProfile)
            snprintf(profileSection, sizeof(profileSection), "%s %s", "profile", config.profile);
        else
            snprintf(profileSection, sizeof(profileSection), "%s", config.profile);
        gchar *idp = g_key_file_get_string(keyFile, profileSection, "aws_access_key_id", NULL);
        gchar *keyp = g_key_file_get_string(keyFile, profileSection, "aws_secret_access_key", NULL);
        gchar *tokenp = g_key_file_get_string(keyFile, profileSection, "aws_session_token", NULL);
        if (idp && keyp) {
            arkime_credentials_set(idp, keyp, tokenp);
            good = 1;
        }

        g_free(idp);
        g_free(keyp);
        g_free(tokenp);
        if (good) {
            g_key_file_free(keyFile);
            return 1;
        }
    }

    gchar *id = g_key_file_get_string(keyFile, "default", "aws_access_key_id", NULL);
    gchar *key = g_key_file_get_string(keyFile, "default", "aws_secret_access_key", NULL);
    gchar *token = g_key_file_get_string(keyFile, "default", "aws_session_token", NULL);
    if (id && key) {
        arkime_credentials_set(id, key, token);
        good = 1;
    }
    g_free(id);
    g_free(key);
    g_free(token);
    g_key_file_free(keyFile);
    return good;
}

/******************************************************************************/
LOCAL void aws_get_credentials(const char *service)
{
    if (aws_get_credentials_env(service))
        return;

    if (aws_get_credentials_file(service, "AWS_SHARED_CREDENTIALS_FILE", "credentials", FALSE))
        return;

    if (aws_get_credentials_file(service, "AWS_CONFIG_FILE", "config", TRUE))
        return;

    if (aws_get_credentials_metadata(service))
        return;

    LOGEXIT("No AWS credentials found, try --profile\n");
}
/******************************************************************************/
void arkime_cloud_init()
{
    arkime_credentials_register("aws", aws_get_credentials);
}
/******************************************************************************/
