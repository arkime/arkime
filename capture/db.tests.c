/******************************************************************************/
/* db.tests.c -- The dbs a dry run uses, nothing is ever written.
 *               tests:// prints every session to stderr for the test suite,
 *               dryrun:// drops them.
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t config;

LOCAL int outputted;
LOCAL ARKIME_LOCK_DEFINE(outputted);

/******************************************************************************/
// maxDocs is 1, so json is a bulk header line and then the session line
LOCAL void arkime_db_tests_send_bulk(char *json, int len)
{
    const char *nl = memchr(json, '\n', len);
    if (nl) {
        const int   hlen = nl - json;
        const char *body = nl + 1;
        int         blen = len - hlen - 1;
        if (blen > 0 && body[blen - 1] == '\n')
            blen--;

        ARKIME_LOCK(outputted);
        outputted++;
        fprintf(stderr, "  %s{\"header\":%.*s,\n  \"body\":%.*s}\n", (outputted == 1 ? "" : ","), hlen, json, blen, body);
        fflush(stderr);
        ARKIME_UNLOCK(outputted);
    }
    arkime_http_free_buffer(json);
}
/******************************************************************************/
LOCAL void arkime_db_tests_sessions_exit()
{
    ARKIME_LOCK(outputted);
    fprintf(stderr, "]}\n");
    fflush(stderr);
    ARKIME_UNLOCK(outputted);
}
/******************************************************************************/
LOCAL const ArkimeSessionsDb_t testsSessionsDb = {
    .exit = arkime_db_tests_sessions_exit
};
/******************************************************************************/
LOCAL const ArkimeSessionsDb_t *arkime_db_tests_sessions_init(const char *UNUSED(url))
{
    ARKIME_LOCK(outputted);
    fprintf(stderr, "{\"sessions3\": [\n");
    fflush(stderr);
    ARKIME_UNLOCK(outputted);

    arkime_db_set_send_bulk2(arkime_db_tests_send_bulk, TRUE, FALSE, 1);
    return &testsSessionsDb;
}
/******************************************************************************/
LOCAL void arkime_db_dryrun_send_bulk(char *json, int len)
{
    if (config.debug)
        LOG("%.*s", len, json);
    arkime_http_free_buffer(json);
}
/******************************************************************************/
LOCAL const ArkimeSessionsDb_t dryrunSessionsDb = {
    .exit = NULL
};
/******************************************************************************/
LOCAL const ArkimeSessionsDb_t *arkime_db_dryrun_sessions_init(const char *UNUSED(url))
{
    arkime_db_set_send_bulk2(arkime_db_dryrun_send_bulk, TRUE, FALSE, 0xffff);
    return &dryrunSessionsDb;
}
/******************************************************************************/
LOCAL void arkime_db_null_put(const char *UNUSED(table), const char *UNUSED(id), const char *UNUSED(json), int UNUSED(len), gboolean UNUSED(sync), int UNUSED(priority))
{
}
/******************************************************************************/
LOCAL void arkime_db_null_put_stats(const char *UNUSED(json), int UNUSED(len), gboolean UNUSED(sync), int UNUSED(priority))
{
}
/******************************************************************************/
LOCAL void arkime_db_null_update(const char *UNUSED(table), const char *UNUSED(id), const char *UNUSED(json), int UNUSED(len), int UNUSED(priority))
{
}
/******************************************************************************/
LOCAL void arkime_db_null_del(const char *UNUSED(table), const char *UNUSED(id))
{
}
/******************************************************************************/
LOCAL void arkime_db_null_sequence(const char *UNUSED(name), ArkimeSeqNum_cb func, gpointer uw)
{
    if (func)
        func(0, uw);
}
/******************************************************************************/
LOCAL uint32_t arkime_db_null_sequence_sync(const char *UNUSED(name))
{
    return 0;
}
/******************************************************************************/
LOCAL gboolean arkime_db_null_each(const char *UNUSED(table), ArkimeDbRow_cb UNUSED(func))
{
    return TRUE;
}
/******************************************************************************/
LOCAL gboolean arkime_db_null_file_exists(const char *UNUSED(node), const char *UNUSED(name), uint32_t *UNUSED(num))
{
    return FALSE;
}
/******************************************************************************/
LOCAL int arkime_db_null_queue_length()
{
    return 0;
}
/******************************************************************************/
LOCAL void arkime_db_null_exit()
{
}
/******************************************************************************/
LOCAL const ArkimeClusterDb_t nullClusterDb = {
    .put               = arkime_db_null_put,
    .put_stats         = arkime_db_null_put_stats,
    .update            = arkime_db_null_update,
    .del               = arkime_db_null_del,
    .sequence          = arkime_db_null_sequence,
    .sequence_sync     = arkime_db_null_sequence_sync,
    .each              = arkime_db_null_each,
    .file_exists       = arkime_db_null_file_exists,
    .queue_length      = arkime_db_null_queue_length,
    .queue_length_best = arkime_db_null_queue_length,
    .exit              = arkime_db_null_exit
};
/******************************************************************************/
LOCAL const ArkimeClusterDb_t *arkime_db_null_cluster_init(const char *UNUSED(url))
{
    return &nullClusterDb;
}
/******************************************************************************/
void arkime_db_tests_init()
{
    arkime_db_sessions_register("tests", arkime_db_tests_sessions_init);
    arkime_db_sessions_register("dryrun", arkime_db_dryrun_sessions_init);
    arkime_db_cluster_register("tests", arkime_db_null_cluster_init);
    arkime_db_cluster_register("dryrun", arkime_db_null_cluster_init);
}
