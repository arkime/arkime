/* yara.c  -- Functions dealing with yara library
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "moloch.h"
#include "yara.h"

extern MolochConfig_t config;

/******************************************************************************/
char *moloch_yara_version() {
    static char buf[100];
#ifdef YR_MAJOR_VERSION
 #ifdef YR_MINOR_VERSION
  #ifdef YR_MICRO_VERSION
    snprintf(buf, sizeof(buf), "%d.%d.%d", YR_MAJOR_VERSION, YR_MINOR_VERSION, YR_MICRO_VERSION);
  #else /* YR_MICRO_VERSION */
    snprintf(buf, sizeof(buf), "%d.%d", YR_MAJOR_VERSION, YR_MINOR_VERSION);
  #endif /* YR_MICRO_VERSION */
 #else /* YR_MINOR_VERSION */
    snprintf(buf, sizeof(buf), "%d.x", YR_MAJOR_VERSION);
 #endif /* YR_MINOR_VERSION */
#else /* YR_MAJOR_VERSION */
 #ifdef STRING_IS_HEX
    snprintf(buf, sizeof(buf), "2.x");
 #else /* STRING_IS_HEX */
    snprintf(buf, sizeof(buf), "1.x");
 #endif /* STRING_IS_HEX */
#endif /* YR_MAJOR_VERSION */
    return buf;
}


#if YR_MAJOR_VERSION == 3 && YR_MINOR_VERSION >= 4
// Yara 3
static YR_COMPILER *yCompiler = 0;
static YR_COMPILER *yEmailCompiler = 0;
static YR_RULES *yRules = 0;
static YR_RULES *yEmailRules = 0;



/******************************************************************************/
void moloch_yara_report_error(int error_level, const char* file_name, int line_number, const char* error_message, void* UNUSED(user_data))
{
    LOG("%d %s:%d: %s\n", error_level, file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_open(char *filename, YR_COMPILER **compiler, YR_RULES **rules)
{
    yr_compiler_create(compiler);
    (*compiler)->callback = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            int errors = yr_compiler_add_file(*compiler, rule_file, NULL, filename);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
            yr_compiler_get_rules(*compiler, rules);
        } else {
            printf("yara could not open file: %s\n", filename);
            exit(1);
        }
    }
}
/******************************************************************************/
void moloch_yara_init()
{
    yr_initialize();

    moloch_yara_open(config.yara, &yCompiler, &yRules);
    moloch_yara_open(config.emailYara, &yEmailCompiler, &yEmailRules);
}

/******************************************************************************/
int moloch_yara_callback(int message, YR_RULE* rule, MolochSession_t* session)
{
    char tagname[256];
    const char* tag;

    if (message == CALLBACK_MSG_RULE_MATCHING) {
        snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
        moloch_session_add_tag(session, tagname);
        tag = rule->tags;
        while(tag != NULL && *tag) {
            snprintf(tagname, sizeof(tagname), "yara:%s", tag);
            moloch_session_add_tag(session, tagname);
            tag += strlen(tag) + 1;
        }
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yRules, (uint8_t *)data, len, 0, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yEmailRules, (uint8_t *)data, len, 0, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    if (yRules)
        yr_rules_destroy(yRules);
    if (yEmailRules)
        yr_rules_destroy(yEmailRules);

    if (yCompiler)
        yr_compiler_destroy(yCompiler);
    if (yEmailCompiler)
        yr_compiler_destroy(yEmailCompiler);
    yr_finalize();
}
#elif defined(YR_COMPILER_H)
// Yara 3
static YR_COMPILER *yCompiler = 0;
static YR_COMPILER *yEmailCompiler = 0;
static YR_RULES *yRules = 0;
static YR_RULES *yEmailRules = 0;


/******************************************************************************/
void moloch_yara_report_error(int error_level, const char* file_name, int line_number, const char* error_message)
{
    LOG("%d %s:%d: %s\n", error_level, file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_open(char *filename, YR_COMPILER **compiler, YR_RULES **rules)
{
    yr_compiler_create(compiler);
    (*compiler)->callback = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            int errors = yr_compiler_add_file(*compiler, rule_file, NULL, filename);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
            yr_compiler_get_rules(*compiler, rules);
        } else {
            printf("yara could not open file: %s\n", filename);
            exit(1);
        }
    }
}
/******************************************************************************/
void moloch_yara_init()
{
    yr_initialize();

    moloch_yara_open(config.yara, &yCompiler, &yRules);
    moloch_yara_open(config.emailYara, &yEmailCompiler, &yEmailRules);
}

/******************************************************************************/
int moloch_yara_callback(int message, YR_RULE* rule, MolochSession_t* session)
{
    char tagname[256];
    const char* tag;

    if (message == CALLBACK_MSG_RULE_MATCHING) {
        snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
        moloch_session_add_tag(session, tagname);
        tag = rule->tags;
        while(tag != NULL && *tag) {
            snprintf(tagname, sizeof(tagname), "yara:%s", tag);
            moloch_session_add_tag(session, tagname);
            tag += strlen(tag) + 1;
        }
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yRules, (uint8_t *)data, len, 0, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yEmailRules, (uint8_t *)data, len, 0, (YR_CALLBACK_FUNC)moloch_yara_callback, session, 0);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    if (yRules)
        yr_rules_destroy(yRules);
    if (yEmailRules)
        yr_rules_destroy(yEmailRules);

    if (yCompiler)
        yr_compiler_destroy(yCompiler);
    if (yEmailCompiler)
        yr_compiler_destroy(yEmailCompiler);
    yr_finalize();
}
#elif defined(STRING_IS_HEX)
// Yara 2.x
static YR_COMPILER *yCompiler = 0;
static YR_COMPILER *yEmailCompiler = 0;
static YR_RULES *yRules = 0;
static YR_RULES *yEmailRules = 0;


/******************************************************************************/
void moloch_yara_report_error(int error_level, const char* file_name, int line_number, const char* error_message)
{
    LOG("%d %s:%d: %s\n", error_level, file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_open(char *filename, YR_COMPILER **compiler, YR_RULES **rules)
{
    yr_compiler_create(compiler);
    (*compiler)->error_report_function = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            int errors = yr_compiler_add_file(*compiler, rule_file, NULL);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
            yr_compiler_get_rules(*compiler, rules);
        } else {
            printf("yara could not open file: %s\n", filename);
            exit(1);
        }
    }
}
/******************************************************************************/
void moloch_yara_init()
{
    yr_initialize();

    moloch_yara_open(config.yara, &yCompiler, &yRules);
    moloch_yara_open(config.emailYara, &yEmailCompiler, &yEmailRules);
}

/******************************************************************************/
int moloch_yara_callback(int message, YR_RULE* rule, MolochSession_t* session)
{
    char tagname[256];
    char* tag;

    if (message == CALLBACK_MSG_RULE_MATCHING) {
        snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
        moloch_session_add_tag(session, tagname);
        tag = rule->tags;
        while(tag != NULL && *tag) {
            snprintf(tagname, sizeof(tagname), "yara:%s", tag);
            moloch_session_add_tag(session, tagname);
            tag += strlen(tag) + 1;
        }
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yRules, (uint8_t *)data, len, (YR_CALLBACK_FUNC)moloch_yara_callback, session, FALSE, 0);
    return;
}
/******************************************************************************/
void  moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int UNUSED(first))
{
    yr_rules_scan_mem(yEmailRules, (uint8_t *)data, len, (YR_CALLBACK_FUNC)moloch_yara_callback, session, FALSE, 0);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    if (yRules)
        yr_rules_destroy(yRules);
    if (yEmailRules)
        yr_rules_destroy(yEmailRules);

    if (yCompiler)
        yr_compiler_destroy(yCompiler);
    if (yEmailCompiler)
        yr_compiler_destroy(yEmailCompiler);
    yr_finalize();
}
#else
// Yara 1.x

static YARA_CONTEXT *yContext[MOLOCH_MAX_PACKET_THREADS];
static YARA_CONTEXT *yEmailContext[MOLOCH_MAX_PACKET_THREADS];


/******************************************************************************/
void moloch_yara_report_error(const char* file_name, int line_number, const char* error_message)
{
    LOG("%s:%d: %s\n", file_name, line_number, error_message);
}
/******************************************************************************/
YARA_CONTEXT *moloch_yara_open(char *filename)
{
    YARA_CONTEXT *context;

    context = yr_create_context();
    context->error_report_function = moloch_yara_report_error;

    if (filename) {
        FILE *rule_file;

        rule_file = fopen(filename, "r");

        if (rule_file != NULL) {
            yr_push_file_name(context, filename);

            int errors = yr_compile_file(rule_file, context);

            fclose(rule_file);

            if (errors) {
                exit (0);
            }
        } else {
            printf("yara could not open file: %s\n", filename);
            exit(1);
        }
    }
    return context;
}
/******************************************************************************/
void moloch_yara_init()
{
    yr_init();

    int t;

    for (t = 0; t < config.packetThreads; t++) {
        yContext[t] = moloch_yara_open(config.yara);
        yEmailContext[t] = moloch_yara_open(config.emailYara);
    }
}

/******************************************************************************/
int moloch_yara_callback(RULE* rule, MolochSession_t* session)
{
    char tagname[256];
    TAG* tag;

    if (rule->flags & RULE_FLAGS_MATCH) {
        snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier);
        moloch_session_add_tag(session, tagname);
        tag = rule->tag_list_head;
        while(tag != NULL) {
            if (tag->identifier) {
                snprintf(tagname, sizeof(tagname), "yara:%s", tag->identifier);
                moloch_session_add_tag(session, tagname);
            }
            tag = tag->next;
        }
    }

    return CALLBACK_CONTINUE;
}
/******************************************************************************/
int yr_scan_mem_blocks(MEMORY_BLOCK* block, YARA_CONTEXT* context, YARACALLBACK callback, void* user_data);

void  moloch_yara_execute(MolochSession_t *session, const uint8_t *data, int len, int first)
{
    MEMORY_BLOCK block;

    block.data = (uint8_t *)data;
    block.size = len;
    block.base = 0;
    block.next = NULL;

    yr_scan_mem_blocks(&block, yContext[session->thread], (YARACALLBACK)moloch_yara_callback, session);
    return;
}
/******************************************************************************/
void moloch_yara_email_execute(MolochSession_t *session, const uint8_t *data, int len, int first)
{
    MEMORY_BLOCK block;

    if (!config.emailYara)
        return;

    block.data = (uint8_t *)data;
    block.size = len;
    block.base = 0;
    block.next = NULL;

    yr_scan_mem_blocks(&block, yEmailContext[session->thread], (YARACALLBACK)moloch_yara_callback, session);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        yr_destroy_context(yContext[t]);
        yr_destroy_context(yEmailContext[t]);
    }
}
#endif
