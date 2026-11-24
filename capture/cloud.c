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
// Returns the response body, which the caller must free, only on a 200
LOCAL uint8_t *aws_get_instance_metadata(const char *key, int key_len, size_t *mlen)
{
    char *requestHeaders[2] = {NULL, NULL};
    char  tokenHeader[200];
    int   code = 0;

    if (awsUseTokenForMetadata) {
        char *tokenRequestHeaders[2] = {"X-aws-ec2-metadata-token-ttl-seconds: 30", NULL};
        if (config.debug)
            LOG("Requesting IMDSv2 metadata token");
        size_t tlen = 0;
        uint8_t *token = arkime_http_send_sync(metadataServer, "PUT", "/latest/api/token", -1, NULL, 0, tokenRequestHeaders, &tlen, &code);
        if (token && code == 200 && tlen) {
            snprintf(tokenHeader, sizeof(tokenHeader), "X-aws-ec2-metadata-token: %.*s", (int)tlen, token);
            requestHeaders[0] = tokenHeader;
        } else if (code == 0) {
            // No HTTP response at all, so this isn't an EC2 instance and the IMDSv1 request would wait on the same timeouts
            LOG("WARNING - Cannot reach the instance metadata service for an IMDSv2 token");
            free(token);
            *mlen = 0;
            return NULL;
        } else {
            LOG("WARNING - Failed to get IMDSv2 metadata token %d, trying IMDSv1, set awsUseTokenForMetadata=false to skip", code);
        }
        free(token);
    } else if (config.debug) {
        LOG("Using IMDSv1");
    }

    code = 0;
    uint8_t *data = arkime_http_send_sync(metadataServer, "GET", key, key_len, NULL, 0, requestHeaders, mlen, &code);
    if (data && code != 200) {
        if (config.debug)
            LOG("Metadata request %s failed %d: %.*s", key, code, (int)MIN(*mlen, 1000), data);
        free(data);
        data = NULL;
    }
    return data;
}
/******************************************************************************/
static char credURL[1024];
LOCAL gboolean aws_refresh_creds(gpointer UNUSED(user_data))
{
    size_t clen = 0;
    gboolean found = FALSE;

    uint8_t *credentials = aws_get_instance_metadata(credURL, -1, &clen);

    if (credentials && clen) {
        // Now need to extract access key, secret key and token
        char *id = arkime_js0n_get_str(credentials, clen, "AccessKeyId");
        char *key = arkime_js0n_get_str(credentials, clen, "SecretAccessKey");
        char *token = arkime_js0n_get_str(credentials, clen, "Token");
        if (config.debug)
            LOG("Found AccessKeyId %s", id ? id : "(null)");

        if (id && key) {
            arkime_credentials_set(id, key, token);
            found = TRUE;
        }
        g_free(id);
        g_free(key);
        g_free(token);
    }
    free(credentials);

    // A transient failure keeps the current credentials, the next refresh tries again
    if (!found)
        LOG("WARNING - Cannot retrieve credentials from instance metadata %s", credURL);

    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL int aws_get_credentials_metadata(const char UNUSED(*service))
{
    if (!metadataServer) {
        size_t rlen = 0;

        metadataServer = arkime_http_create_server("http://169.254.169.254", 2, 2, FALSE);
        arkime_http_set_timeout(metadataServer, 10);
        uint8_t *rolename = aws_get_instance_metadata("/latest/meta-data/iam/security-credentials/", -1, &rlen);
        if (!rolename || !rlen || rolename[0] == '<') {
            if (config.debug)
                LOG("ERROR - Cannot retrieve role name from metadata service\n");
            free(rolename);
            return 0;
        }

        snprintf(credURL, sizeof(credURL), "/latest/meta-data/iam/security-credentials/%.*s", (int)rlen, rolename);
        free(rolename);
        arkime_http_set_print_errors(metadataServer);
        g_timeout_add_seconds(280, aws_refresh_creds, 0);
    }
    aws_refresh_creds(NULL);
    return 1;
}
/******************************************************************************/
/* EKS and ECS credentials for the aws provider.
 * IRSA sets AWS_ROLE_ARN and AWS_WEB_IDENTITY_TOKEN_FILE, exchanged with an
 * unsigned STS AssumeRoleWithWebIdentity call. Pod Identity sets
 * AWS_CONTAINER_CREDENTIALS_FULL_URI and an Authorization token file. ECS sets
 * AWS_CONTAINER_CREDENTIALS_RELATIVE_URI on the ECS_CONTAINER_METADATA_URI_V4 host.
 * The container endpoints all return the same json and never get an IMDSv2 token.
 */
enum { EKS_UNKNOWN, EKS_NONE, EKS_WEB_IDENTITY, EKS_CONTAINER };

#define EKS_WEB_ID_DURATION 3600

LOCAL ARKIME_LOCK_DEFINE(eks);
LOCAL int                    eksMode;
LOCAL void                  *eksServer;
LOCAL char                   eksPath[1024];
LOCAL const char            *eksTokenFile;
LOCAL const char            *eksRoleArn;
LOCAL char                  *eksRoleSessionName;
LOCAL ArkimeCredentials_t    eksWebIdCreds;
LOCAL time_t                 eksWebIdExpire;

/******************************************************************************/
// Split a url into the scheme://host[:port] for the http server and the path
LOCAL char *eks_split_url(const char *url, char *path, int pathLen)
{
    const char *hostStart = strstr(url, "://");
    hostStart = hostStart ? hostStart + 3 : url;
    const char *slash = strchr(hostStart, '/');
    g_strlcpy(path, slash ? slash : "/", pathLen);
    return slash ? g_strndup(url, slash - url) : g_strdup(url);
}
/******************************************************************************/
// The token files are rotated, so they are read on every refresh
LOCAL char *eks_read_token(const char *fileName)
{
    gchar *token = NULL;
    GError *error = NULL;
    if (!g_file_get_contents(fileName, &token, NULL, &error)) {
        LOG("ERROR - Couldn't read token file %s: %s", fileName, error->message);
        g_error_free(error);
        return NULL;
    }
    return g_strstrip(token);
}
/******************************************************************************/
LOCAL char *eks_xml_get_str(const uint8_t *data, size_t len, const char *tag)
{
    char open[64], close[64];
    const int olen = snprintf(open, sizeof(open), "<%s>", tag);
    const int clen = snprintf(close, sizeof(close), "</%s>", tag);
    const char *start = arkime_memstr((const char *)data, len, open, olen);
    if (!start)
        return NULL;
    start += olen;
    const char *end = arkime_memstr(start, len - (start - (const char *)data), close, clen);
    if (!end || end == start)
        return NULL;
    return g_strndup(start, end - start);
}
/******************************************************************************/
/* Returns TRUE if IRSA or Pod Identity environment variables are set */
LOCAL gboolean aws_eks_init()
{
    ARKIME_LOCK(eks);
    if (eksMode != EKS_UNKNOWN) {
        ARKIME_UNLOCK(eks);
        return eksMode != EKS_NONE;
    }

    const char *fullURI = getenv("AWS_CONTAINER_CREDENTIALS_FULL_URI");
    const char *relativeURI = getenv("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI");
    const char *ecsURI = getenv("ECS_CONTAINER_METADATA_URI_V4");
    eksRoleArn = getenv("AWS_ROLE_ARN");
    eksTokenFile = getenv("AWS_WEB_IDENTITY_TOKEN_FILE");

    char *base = NULL;
    // Same order as the AWS SDKs, web identity before container credentials
    if (eksRoleArn && eksRoleArn[0] && eksTokenFile && eksTokenFile[0]) {
        eksMode = EKS_WEB_IDENTITY;

        const char *stsURL = getenv("AWS_ENDPOINT_URL_STS");
        const char *region = getenv("AWS_REGION");
        if (!region || !region[0])
            region = getenv("AWS_DEFAULT_REGION");

        if (stsURL && stsURL[0]) {
            base = eks_split_url(stsURL, eksPath, sizeof(eksPath));
        } else if (region && region[0] && g_strcmp0(getenv("AWS_STS_REGIONAL_ENDPOINTS"), "legacy") != 0) {
            base = g_strdup_printf("https://sts.%s.%s", region, g_str_has_prefix(region, "cn-") ? "amazonaws.com.cn" : "amazonaws.com");
            g_strlcpy(eksPath, "/", sizeof(eksPath));
        } else {
            base = g_strdup("https://sts.amazonaws.com");
            g_strlcpy(eksPath, "/", sizeof(eksPath));
        }

        const char *sessionName = getenv("AWS_ROLE_SESSION_NAME");
        if (sessionName && sessionName[0])
            eksRoleSessionName = g_strdup(sessionName);
        else
            eksRoleSessionName = g_strdup_printf("arkime-%s", config.nodeName ? config.nodeName : "capture");
        g_strcanon(eksRoleSessionName, G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "_+=,.@-", '-');
        if (strlen(eksRoleSessionName) > 64)
            eksRoleSessionName[64] = 0;

        LOG("Using IRSA web identity for role %s from %s", eksRoleArn, base);
    } else if (fullURI && fullURI[0]) {
        eksMode = EKS_CONTAINER;
        eksTokenFile = getenv("AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE");
        base = eks_split_url(fullURI, eksPath, sizeof(eksPath));
        LOG("Using container credentials from %s", fullURI);
    } else if (relativeURI && relativeURI[0] && ecsURI && ecsURI[0]) {
        eksMode = EKS_CONTAINER;
        eksTokenFile = getenv("AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE");
        char ignorePath[1024];
        base = eks_split_url(ecsURI, ignorePath, sizeof(ignorePath));
        g_strlcpy(eksPath, relativeURI, sizeof(eksPath));
        LOG("Using ECS container credentials from %s%s", base, eksPath);
    } else {
        eksMode = EKS_NONE;
        ARKIME_UNLOCK(eks);
        return FALSE;
    }

    eksServer = arkime_http_create_server(base, 2, 2, FALSE);
    arkime_http_set_timeout(eksServer, 10);
    arkime_http_set_print_errors(eksServer);
    g_free(base);

    ARKIME_UNLOCK(eks);
    return TRUE;
}
/******************************************************************************/
// Must hold eks lock. Credentials are cached until close to expiring so STS isn't called on every refresh
LOCAL void eks_web_identity_refresh_locked()
{
    const time_t now = time(NULL);
    if (eksWebIdCreds.id && now < eksWebIdExpire - 900)
        return;

    char *webToken = eks_read_token(eksTokenFile);
    if (!webToken)
        return;

    char *roleArn = g_uri_escape_string(eksRoleArn, NULL, FALSE);
    char *sessionName = g_uri_escape_string(eksRoleSessionName, NULL, FALSE);
    char *webTokenEsc = g_uri_escape_string(webToken, NULL, FALSE);
    char *body = g_strdup_printf("Action=AssumeRoleWithWebIdentity&Version=2011-06-15&DurationSeconds=%d&RoleArn=%s&RoleSessionName=%s&WebIdentityToken=%s",
                                 EKS_WEB_ID_DURATION, roleArn, sessionName, webTokenEsc);
    g_free(webToken);
    g_free(roleArn);
    g_free(sessionName);
    g_free(webTokenEsc);

    // The token goes in the body so it is never logged as part of the url
    char *headers[3] = {"Content-Type: application/x-www-form-urlencoded", "Expect:", NULL};
    size_t len = 0;
    int code = 0;
    uint8_t *data = arkime_http_send_sync(eksServer, "POST", eksPath, -1, body, strlen(body), headers, &len, &code);
    g_free(body);

    if (data && code == 200) {
        char *id = eks_xml_get_str(data, len, "AccessKeyId");
        char *key = eks_xml_get_str(data, len, "SecretAccessKey");
        char *token = eks_xml_get_str(data, len, "SessionToken");
        if (id && key && token) {
            g_free(eksWebIdCreds.id);
            g_free(eksWebIdCreds.key);
            g_free(eksWebIdCreds.token);
            eksWebIdCreds.id = id;
            eksWebIdCreds.key = key;
            eksWebIdCreds.token = token;
            eksWebIdExpire = now + EKS_WEB_ID_DURATION;
        } else {
            LOG("ERROR - Couldn't find credentials in STS AssumeRoleWithWebIdentity response for %s", eksRoleArn);
            g_free(id);
            g_free(key);
            g_free(token);
        }
    } else {
        LOG("ERROR - STS AssumeRoleWithWebIdentity for %s failed %d: %.*s", eksRoleArn, code, data ? (int)MIN(len, 1000) : 0, data ? (char *)data : "");
    }
    free(data);
}
/******************************************************************************/
/* Fetch the current IRSA or Pod Identity credentials, returns TRUE and sets
 * id, key and token, which the caller must g_free, on success
 */
LOCAL gboolean aws_eks_get(char **id, char **key, char **token)
{
    *id = *key = *token = NULL;

    if (!aws_eks_init())
        return FALSE;

    ARKIME_LOCK(eks);
    if (eksMode == EKS_WEB_IDENTITY) {
        eks_web_identity_refresh_locked();
        if (eksWebIdCreds.id && time(NULL) < eksWebIdExpire) {
            *id = g_strdup(eksWebIdCreds.id);
            *key = g_strdup(eksWebIdCreds.key);
            *token = g_strdup(eksWebIdCreds.token);
        }
    } else {
        char *authToken = NULL;
        if (eksTokenFile && eksTokenFile[0])
            authToken = eks_read_token(eksTokenFile);
        else if (getenv("AWS_CONTAINER_AUTHORIZATION_TOKEN"))
            authToken = g_strdup(getenv("AWS_CONTAINER_AUTHORIZATION_TOKEN"));

        char *authHeader = authToken ? g_strdup_printf("Authorization: %s", authToken) : NULL;
        char *headers[2] = {authHeader, NULL};
        size_t len = 0;
        int code = 0;
        uint8_t *data = arkime_http_send_sync(eksServer, "GET", eksPath, -1, NULL, 0, headers, &len, &code);

        if (data && code == 200 && len) {
            *id = arkime_js0n_get_str(data, len, "AccessKeyId");
            *key = arkime_js0n_get_str(data, len, "SecretAccessKey");
            *token = arkime_js0n_get_str(data, len, "Token");
        } else {
            LOG("ERROR - Container credentials request failed %d: %.*s", code, data ? (int)MIN(len, 1000) : 0, data ? (char *)data : "");
        }
        free(data);
        g_free(authHeader);
        g_free(authToken);
    }
    ARKIME_UNLOCK(eks);

    if (*id && *key && *token) {
        if (config.debug)
            LOG("Found EKS AccessKeyId %s", *id);
        return TRUE;
    }

    g_free(*id);
    g_free(*key);
    g_free(*token);
    *id = *key = *token = NULL;
    return FALSE;
}
/******************************************************************************/
LOCAL gboolean aws_refresh_eks_creds(gpointer UNUSED(user_data))
{
    char *id, *key, *token;
    if (aws_eks_get(&id, &key, &token)) {
        arkime_credentials_set(id, key, token);
        g_free(id);
        g_free(key);
        g_free(token);
    }
    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
LOCAL int aws_get_credentials_eks(const char UNUSED(*service))
{
    static gboolean timerAdded;

    if (!aws_eks_init())
        return 0;

    // The environment names this source, so like the AWS SDKs don't quietly move on to the node's instance role
    char *id, *key, *token;
    if (!aws_eks_get(&id, &key, &token))
        LOGEXIT("ERROR - No credentials from %s, see the error above", eksMode == EKS_WEB_IDENTITY ? "STS for the IRSA web identity" : "the container credentials endpoint");
    arkime_credentials_set(id, key, token);
    g_free(id);
    g_free(key);
    g_free(token);

    if (!timerAdded) {
        timerAdded = TRUE;
        g_timeout_add_seconds(280, aws_refresh_eks_creds, 0);
    }
    return 1;
}
/******************************************************************************/
LOCAL int aws_get_credentials_env(const char UNUSED(*service))
{
    const char *id = getenv("AWS_ACCESS_KEY_ID");
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
        g_key_file_free(keyFile);
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

    if (aws_get_credentials_eks(service))
        return;

    if (aws_get_credentials_metadata(service))
        return;

    LOGEXIT("No AWS credentials found, try --profile\n");
}
/******************************************************************************/
void arkime_cloud_init()
{
    // IMDSv2 by default, s3UseTokenForMetadata is the older s3 writer name for this
    awsUseTokenForMetadata = arkime_config_boolean(NULL, "awsUseTokenForMetadata", arkime_config_boolean(NULL, "s3UseTokenForMetadata", TRUE));
    arkime_credentials_register("aws", aws_get_credentials);
}
/******************************************************************************/
